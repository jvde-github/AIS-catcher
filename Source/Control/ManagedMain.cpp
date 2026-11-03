/*
        Copyright(c) 2021-2026 jvde.github@gmail.com

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <memory>
#include <stdexcept>
#include <thread>

#include "AIS-catcher.h"

#include "Config.h"
#include "ControlServer.h"
#include "Helper.h"
#include "JSON.h"
#include "Logger.h"
#include "ManagedMain.h"
#include "JSON/Parser.h"

#if defined(HASWEBVIEWER) && defined(HASAPI)
#include "Server.h"
#include "Site.h"
#include "SiteConfig.h"
#endif

namespace Managed
{

	bool isInvocation(const std::vector<std::string> &args)
	{
		for (const auto &a : args)
			if (a == "-E")
				return true;
		return false;
	}

#if !defined(HASWEBVIEWER) || !defined(HASAPI)

	int run(const std::vector<std::string> &)
	{
		throw std::runtime_error(
			"managed mode (-E) is not available in this build: it requires the web "
			"viewer and the API (build with WEBVIEWER=ON and API=ON)");
	}

#else

	// one catalogue the panel's editor edits and the tracker reads, wherever the
	// configuration keeps the places
	static void usePanelPlaces(ControlCore &core, SiteConfig &site)
	{
		site.places.clear();
		site.catalogue = core.getPlaces();
	}

	struct Saved
	{
		SiteConfig site;
		StationRegistry stations;
	};

	static Saved readSaved(ControlCore &core)
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		auto doc = parser.parse(core.getConfig());
		Saved saved;
		StationDefaults defaults;
		// the listed stations first: a server's station_id describes one only where the file does not
		if (const auto *v = doc.root[AIS::KEY_SETTING_STATIONS])
			SiteReader::stations(*v, saved.stations);
		SiteReader::managedSite(doc.root, Config::baseDir(), core.getControlPort(), core.getViewerPort(), core.getBindAddress(), saved.site, defaults, saved.stations);
		usePanelPlaces(core, saved.site);
		return saved;
	}

	// applies the site and mounts the viewer beside the panel once; a taken port leaves
	// the panel up without a viewer, and the next apply tries again
	static void applySite(ControlCore &core, ControlServer &server, Site &site, const SiteConfig &cfg)
	{
		site.configure(cfg);
		Services::Server *viewer = site.server(core.getViewerPort());
		if (!viewer)
			Error() << "Control: viewer not started on port " << core.getViewerPort() << "; the control panel stays available";
		// one exposed port is enough for a station reached through a VPN or proxy
		else if (!viewer->isMounted())
			server.mount("/viewer", viewer);
		core.setViewerServing(viewer != nullptr);
	}

	static void applyConfig(ControlCore &core, ControlServer &server, Site &site, Engine *engine)
	{
		try
		{
			const Saved saved = readSaved(core);
			applySite(core, server, site, saved.site);
			if (engine)
				engine->refreshStations(saved.stations);
		}
		catch (const std::exception &e)
		{
			Error() << "Control: configuration not applied: " << e.what();
		}
	}

	static void syncSite(ControlCore &core, ControlServer &server, Site &site, Engine *engine = nullptr)
	{
		auto places = core.getPlaces();
		if (places && places->consumeChanged())
			site.refreshPlaces();
		if (core.consumeConfigChanged())
			applyConfig(core, server, site, engine);
	}

	// construct before the Engine so the session covers ~Engine's device close
	struct EngineSessionGuard
	{
		ControlCore &core;
		EngineSessionGuard(ControlCore &c) : core(c) { core.beginEngineSession(); }
		~EngineSessionGuard() { core.endEngineSession(); }
	};

	// One engine session: read the config, start the receivers, run until stopped
	// or failed. The site and its viewer serve throughout.
	static void runEngine(ControlCore &core, ControlServer &server,
						  const std::string &config_file, Site &site)
	{
		EngineSessionGuard session(core);
		Engine engine;
		Config c(engine);
		c.setManaged(core.getControlPort(), core.getViewerPort(), core.getBindAddress());
		// stdout is the journal or the container log here: silent unless "screen"
		// in the config says otherwise, where the command line prints NMEA by default
		engine.screen.setScreen("0");

		try
		{
			engine.scanner().refreshDevices();

			core.consumeConfigChanged();
			c.read(config_file);
			c.finish();
			for (auto &r : engine.receivers)
				if (r->inputType() == Type::NONE && r->devices().serial().empty())
					throw std::runtime_error(
						"no input device selected, configure one under Inputs");

			usePanelPlaces(core, engine.site_config);
			engine.loop_hook = [&] { syncSite(core, server, site, &engine); };
			engine.managed = true;
			engine.activity = &core.getChannelActivity();
			engine.on_started = [&] {
				core.reportRunning();
				Info() << "Control: engine started";
			};

			engine.run(site);
		}
		catch (std::exception const &e)
		{
			Error() << e.what();

			// the throw skipped run()'s own detach() and ~Engine only runs at the end
			// of this call
			engine.detach();
			core.engineFailed();
		}
		core.reportStopped();
		Info() << "Control: engine stopped";
	}

	// The back-off before an automatic restart. False when a command or the
	// shutdown cut it short.
	static bool waitForRetry(ControlCore &core, ControlServer &server, Site &site, int delay)
	{
		Warning() << "Control: engine stopped unexpectedly, restarting in " << delay
				  << " seconds";
		const int seq = core.commandSequence();
		for (int i = 0;
			 i < delay * 4 && !stop_process && core.commandSequence() == seq; i++)
		{
			// the engine is down for the whole wait, so the panel's changes apply here
			// as well as when idle
			syncSite(core, server, site);
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		return !stop_process && core.engineDesired();
	}

	static int serve(const std::string &config_file, int port,
					 const std::string &bind, const std::string &tls_cert,
					 const std::string &tls_key)
	{
		Logger::getInstance().setMinLevel(LogLevel::DEBUG);
		// the file's own folder for what it names, from the first reading on
		Config::setConfigBaseDir(config_file);

		ControlCore core(config_file, port, bind);
		if (!tls_cert.empty())
			core.setPanelTLSFromCommandLine(tls_cert, tls_key);
		// outlives every engine session, and serves the viewer between them
		Site site;
		ControlServer server(core);
		server.setStationsLive([&site] { return site.stationsLive(); });
		server.setServerAddress([&site](int port, std::string &host) { return site.serverAddress(port, host); });
		server.setServerPorts([&site] { return site.serverPorts(); });
		server.setServerTLS([&site] { return site.serverTLS(); });
		server.setReceiverStates([&site] { return site.receiverStates(); });

		if (core.authRequired())
			Info() << "Control: managed mode, config file \"" << config_file
				   << "\", bound to " << core.getBindAddress() << ", password required";
		else
			Info() << "Control: managed mode, config file \"" << config_file
				   << "\", local access only";

		server.start();
		applyConfig(core, server, site, nullptr);

		while (!stop_process)
		{
			if (!core.engineDesired())
			{
				// the panel's changes apply while the engine is stopped; the rest is
				// picked up by the engine start
				syncSite(core, server, site);
				core.waitForCommand();
				continue;
			}

			const int delay = core.consumeRetryDelay();
			if (delay > 0 && !waitForRetry(core, server, site, delay))
				continue;

			// the flag the engine's run() ends on, left set by the last session's stop
			// or its watchdog
			stop = false;
			runEngine(core, server, config_file, site);
		}

		server.close();
		site.stop();
		return 0;
	}

	int run(const std::vector<std::string> &all)
	{
		const std::string usage = "AIS-catcher -E [config file] [bind address:port] [TLS_CERT file TLS_KEY file]";

		// the panel's TLS_CERT and TLS_KEY, for this run only, end the line
		std::vector<std::string> args = all;
		std::string tls_cert, tls_key;
		while (args.size() >= 4 && (args[args.size() - 2] == "TLS_CERT" || args[args.size() - 2] == "TLS_KEY"))
		{
			(args[args.size() - 2] == "TLS_CERT" ? tls_cert : tls_key) = args.back();
			args.resize(args.size() - 2);
		}
		if (tls_cert.empty() != tls_key.empty())
			throw std::runtime_error("Control: HTTPS needs both TLS_CERT and TLS_KEY: " + usage);

		if (args.size() < 2 || args.size() > 4 || args[1] != "-E")
			throw std::runtime_error(
				"in managed mode all settings live in the config file: " + usage);

		auto allDigits = [](const std::string &s) {
			return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
		};

		std::string file = "config.json", bind = "127.0.0.1";
		int port = 0;

		std::string addr;
		if (args.size() == 3)
			addr = args[2];
		else if (args.size() == 4)
		{
			file = args[2];
			addr = args[3];
		}

		if (!addr.empty())
		{
			const std::size_t colon = addr.rfind(':');
			const std::string host = colon == std::string::npos ? "" : addr.substr(0, colon);
			const std::string portstr = colon == std::string::npos ? addr : addr.substr(colon + 1);
			if (!allDigits(portstr) || (colon != std::string::npos && host.empty()))
				throw std::runtime_error(
					"Control: listen address must be [ip:]port, e.g. 127.0.0.1:8118: " +
					usage);
			port = Util::Parse::Integer(portstr, 1, 65535);
			if (!host.empty())
				bind = host;
		}

		return serve(file, port, bind, tls_cert, tls_key);
	}

#endif

}
