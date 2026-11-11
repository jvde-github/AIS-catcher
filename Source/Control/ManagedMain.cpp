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
#include "Logger.h"
#include "ManagedMain.h"

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

	// Managed mode: the site lives as long as the program, the engine comes and goes on it.
	class ManagedInstallation : public Installation
	{
		ControlCore &core;
		Supervisor supervisor;
		Site site;
		ControlServer server;

		// one catalogue the panel's editor edits and the tracker reads, wherever the
		// configuration keeps the places
		void usePanelPlaces(SiteConfig &cfg)
		{
			cfg.places.clear();
			cfg.catalogue = core.getPlaces();
		}

		// the saved document as an engine reads it, at its start and at every save
		void read(Engine &engine, Config &c)
		{
			c.setManaged(core.getControlPort(), core.getViewerPort(), core.getBindAddress());
			c.setFolder(Config::folderOf(core.getConfigFile()));
			// stdout is the journal or the container log here: silent unless "screen"
			// in the config says otherwise, where the command line prints NMEA by default
			engine.screen.setScreen("0");
			c.set(core.getConfig());
			c.finish();
			usePanelPlaces(c.site);
		}

		static void requireInput(const Engine &engine)
		{
			for (auto &r : engine.receivers)
				if (r->inputType() == Type::NONE && r->devices().serial().empty())
					throw std::runtime_error("no input device selected, configure one under Inputs");
		}

		// applies the site and mounts the viewer beside the panel once; a taken port leaves
		// the panel up without a viewer, and the next apply tries again
		void applySite(const SiteConfig &cfg)
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

		// A save: the site, the outputs and a running engine's receivers follow it.
		void apply(Engine *engine)
		{
			Engine next;
			Config c(next);
			try
			{
				read(next, c);
				applySite(c.site);
				if (engine)
					site.setOutputs(std::move(next.outputs));
			}
			catch (const std::exception &e)
			{
				Error() << "Control: configuration not applied: " << e.what();
				return;
			}
			if (!engine)
				return;
			requireInput(next);
			if (!engine->follow(next, c.stations, c.station_defaults))
				supervisor.restart();
		}

		// the panel's changes, whether an engine runs or not
		void sync(Engine *engine = nullptr)
		{
			auto places = core.getPlaces();
			if (places && places->consumeChanged())
				site.refreshPlaces();
			if (core.consumeConfigChanged())
				apply(engine);
		}

		// One engine session: read the config, start the receivers, run until stopped
		// or failed. The site and its viewer serve throughout.
		void session()
		{
			// before the engine, so it covers ~Engine's device close: no device rescans meanwhile
			struct Busy
			{
				ControlCore &core;
				Busy(ControlCore &c) : core(c) { core.beginEngineSession(); }
				~Busy() { core.endEngineSession(); }
			} busy(core);
			Engine engine;
			Config c(engine);

			try
			{
				engine.scanner().refreshDevices();

				core.consumeConfigChanged();
				read(engine, c);
				requireInput(engine);
				engine.loop_hook = [&] { sync(&engine); };
				engine.managed = true;
				engine.activity = &core.getChannelActivity();
				engine.on_started = [&] {
					supervisor.reportRunning();
					Info() << "Control: engine started";
				};

				engine.open(site, c.stations, c.station_defaults, c.site.tracks());
				applySite(c.site);
				engine.run();
			}
			catch (std::exception const &e)
			{
				Error() << e.what();

				// the throw skipped run()'s own detach() and ~Engine only runs at the end
				// of this call
				engine.detach();
				supervisor.failed();
			}
			supervisor.reportStopped();
			Info() << "Control: engine stopped";
		}

		// The back-off before an automatic restart. False when a command or the
		// shutdown cut it short.
		bool waitForRetry(int delay)
		{
			Warning() << "Control: engine stopped unexpectedly, restarting in " << delay
					  << " seconds";
			const int seq = supervisor.commandSequence();
			for (int i = 0;
				 i < delay * 4 && !stop_process && supervisor.commandSequence() == seq; i++)
			{
				// the engine is down for the whole wait, so the panel's changes apply here
				// as well as when idle
				sync();
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
			return !stop_process && supervisor.desired();
		}

	public:
		explicit ManagedInstallation(ControlCore &core)
			: core(core), supervisor(core.engineSwitch(), [&core](bool on) { core.keepEngineSwitch(on); }), server(core, *this)
		{
		}

		Supervisor &engine() override { return supervisor; }
		std::string stationsLive() override { return site.stationsLive(); }
		bool serverAddress(int port, std::string &host) override { return site.serverAddress(port, host); }
		std::vector<int> serverPorts() override { return site.serverPorts(); }
		std::vector<std::pair<int, long long>> serverTLS() override { return site.serverTLS(); }
		std::vector<std::pair<int, std::string>> receiverStates() override { return site.receiverStates(); }

		int run()
		{
			server.start();
			apply(nullptr);

			while (!stop_process)
			{
				if (!supervisor.desired())
				{
					// the panel's changes apply while the engine is stopped
					sync();
					supervisor.waitForCommand();
					continue;
				}

				const int delay = supervisor.consumeRetryDelay();
				if (delay > 0 && !waitForRetry(delay))
					continue;

				// the stop flags of the last session; a signal that came meanwhile still stops this one
				stop_failed = false;
				stop = stop_process.load();
				session();
			}

			server.close();
			site.stop();
			return 0;
		}
	};

	static int serve(const std::string &config_file, int port,
					 const std::string &bind, const std::string &tls_cert,
					 const std::string &tls_key)
	{
		Logger::getInstance().setMinLevel(LogLevel::DEBUG);

		ControlCore core(config_file, port, bind);
		if (!tls_cert.empty())
			core.setPanelTLSFromCommandLine(tls_cert, tls_key);

		if (core.authRequired())
			Info() << "Control: managed mode, config file \"" << config_file
				   << "\", bound to " << core.getBindAddress() << ", password required";
		else
			Info() << "Control: managed mode, config file \"" << config_file
				   << "\", local access only";

		return ManagedInstallation(core).run();
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
