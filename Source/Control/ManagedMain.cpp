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

#include "ManagedMain.h"
#include "ControlServer.h"
#include "Config.h"
#include "JSON.h"
#include "JSON/Parser.h"
#include "Logger.h"
#include "Helper.h"

#ifdef HASWEBVIEWER
#include "WebViewer.h"
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

#ifndef HASWEBVIEWER

	int run(const std::vector<std::string> &)
	{
		throw std::runtime_error("managed mode (-E) is not available in this build: it requires the web viewer (build with WEBVIEWER=ON)");
	}

#else

	static std::unique_ptr<WebViewer> makeViewer(ControlCore &core)
	{
		std::unique_ptr<WebViewer> viewer(new WebViewer());

		viewer->setActive(true);
		viewer->setManagedMode();
		if (core.getBindAddress() != "0.0.0.0")
			viewer->setIP(core.getBindAddress());
		viewer->setReusePort(false);

		viewer->setPort(core.getViewerPort());
		managed_viewer = viewer.get();

		return viewer;
	}

	// The one valid reconfigure order: stop, reset, apply, start. Its HTTP
	// server, ship database and statistics stay up throughout. If apply throws,
	// the viewer is left stopped and the caller decides how to bring it back.
	template <typename F>
	static void reconfigureViewer(WebViewer &viewer, ControlCore &core, F apply)
	{
		viewer.stopServing();
		viewer.resetSettings(core.getViewerPort());
		apply();
		viewer.startServing();
	}

	static void restartViewer(WebViewer &viewer, const std::string &config_file, ControlCore &core)
	{
		reconfigureViewer(viewer, core, [&]
						  { Config::readManagedViewer(config_file); });
	}

	// construct before the Engine so the session covers ~Engine's device close
	struct EngineSessionGuard
	{
		ControlCore &core;
		EngineSessionGuard(ControlCore &c) : core(c) { core.beginEngineSession(); }
		~EngineSessionGuard() { core.endEngineSession(); }
	};

	static int serve(const std::string &config_file, int port, const std::string &bind)
	{
		// the hub's log view is the only window into a managed station
		Logger::getInstance().setMinLevel(LogLevel::DEBUG);

		ControlCore core(config_file, port, bind);
		ControlServer server(core);

		if (core.authRequired())
			Info() << "Control: managed mode, config file \"" << config_file << "\", bound to " << core.getBindAddress() << ", password required";
		else
			Info() << "Control: managed mode, config file \"" << config_file << "\", local access only";

		server.start();

		std::unique_ptr<WebViewer> viewer = makeViewer(core);
		Config::readManagedViewer(config_file);
		viewer->startServing();

		while (!stop_process)
		{
			if (core.engineDesired())
			{
				int delay = core.consumeRetryDelay();
				if (delay > 0)
				{
					Warning() << "Control: engine stopped unexpectedly, restarting in " << delay << " seconds";
					int seq = core.commandSequence();
					for (int i = 0; i < delay * 4 && !stop_process && core.commandSequence() == seq; i++)
					{
						// the engine is down for the whole wait, so viewer-only
						// settings can be applied here as well as when idle
						if (core.consumeConfigChanged())
							restartViewer(*viewer, config_file, core);

						std::this_thread::sleep_for(std::chrono::milliseconds(250));
					}
					if (stop_process || !core.engineDesired())
						continue;
				}

				stop = false;
				if (!core.engineDesired())
					continue;

				EngineSessionGuard session(core);
				Engine engine;
				Config c(engine);

				try
				{
					engine.receivers.back()->getDeviceManager().refreshDevices();

					core.consumeConfigChanged();
					reconfigureViewer(*viewer, core, [&]
					{
						// c.read() applies "control"."viewer" to the stopped viewer
						c.read(config_file);

						for (auto &r : engine.receivers)
							if (r->getDeviceManager().InputType() == Type::NONE && r->getDeviceManager().SerialNumber().empty())
								throw std::runtime_error("no input device selected, configure one under Input");
					});

					engine.run(viewer.get(), &core);
				}
				catch (std::exception const &e)
				{
					Error() << e.what();

					// the throw skipped run()'s own detach() and ~Engine only runs
					// at the end of this iteration, after the viewer is back up
					engine.detach();

					viewer->startServing();
					core.engineFailed();
				}
				core.reportStopped();
				Info() << "Control: engine stopped";
			}
			else
			{
				// viewer-only settings apply while the engine is stopped; anything
				// else is picked up by the engine start above
				if (core.consumeConfigChanged())
					restartViewer(*viewer, config_file, core);

				core.waitForCommand();
			}
		}

		server.close();
		viewer->shutdown();
		return 0;
	}

	int run(const std::vector<std::string> &args)
	{
		const std::string usage = "AIS-catcher -E [config file] [bind address:port]";

		if (args.size() < 2 || args.size() > 4 || args[1] != "-E")
			throw std::runtime_error("in managed mode all settings live in the config file: " + usage);

		auto allDigits = [](const std::string &s)
		{ return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos; };

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
			std::string protocol, user, pass, host, portstr, path;
			Util::Parse::URL(addr, protocol, user, pass, host, portstr, path);

			if (allDigits(addr))
				port = Util::Parse::Integer(addr, 1, 65535);
			else if (!host.empty() && allDigits(portstr))
			{
				bind = host;
				port = Util::Parse::Integer(portstr, 1, 65535);
			}
			else
				throw std::runtime_error("Control: listen address must be [ip:]port, e.g. 127.0.0.1:8118: " + usage);
		}

		return serve(file, port, bind);
	}

#endif

}
