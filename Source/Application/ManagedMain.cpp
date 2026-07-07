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
#include <mutex>
#include <thread>

#include "AIS-catcher.h"

#include "ManagedMain.h"
#include "ControlServer.h"
#include "Config.h"
#include "JSON.h"
#include "JSON/Parser.h"
#include "Logger.h"
#include "Helper.h"

namespace Managed
{

#ifdef HASWEBVIEWER
	static std::unique_ptr<WebViewer> makeViewer(const std::string &config_file, ControlCore &core)
	{
		std::unique_ptr<WebViewer> viewer(new WebViewer());

		viewer->active() = true;
		viewer->setManagedMode();
		if (core.getBindAddress() != "0.0.0.0")
			viewer->setIP(core.getBindAddress());
		viewer->setReusePort(false);

		viewer->SetKey(AIS::KEY_SETTING_SHARE_LOC, "on");
		viewer->SetKey(AIS::KEY_SETTING_REALTIME, "on");
		viewer->SetKey(AIS::KEY_SETTING_DECODER, "on");
		viewer->SetKey(AIS::KEY_SETTING_LOG, "off");

		try
		{
			JSON::Parser parser(JSON_DICT_SETTING);
			JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

			const JSON::Value *ctrl = doc.root[AIS::KEY_SETTING_CONTROL];
			if (ctrl && ctrl->isObject())
			{
				const JSON::Value *v = ctrl->getObject()[AIS::KEY_SETTING_VIEWER];
				if (v && v->isObject())
					Config::setSettingsFromJSON(*v, *viewer);
			}
		}
		catch (std::exception const &e)
		{
			Error() << "Control: viewer configuration failed: " << e.what();
		}

		if (core.getControlPort() < 65535)
			viewer->SetKey(AIS::KEY_SETTING_PORT, std::to_string(core.getControlPort() + 1));

		return viewer;
	}

	static std::unique_ptr<WebViewer> startManagedViewer(const std::string &config_file, ControlCore &core)
	{
		std::unique_ptr<WebViewer> viewer = makeViewer(config_file, core);
		viewer->start();
		return viewer;
	}

	static void reapplyViewerSettings(WebViewer &viewer, const std::string &config_file)
	{
		static const AIS::Keys live_keys[] = {
			AIS::KEY_SETTING_LAT, AIS::KEY_SETTING_LON, AIS::KEY_SETTING_USE_GPS};

		try
		{
			JSON::Parser parser(JSON_DICT_SETTING);
			JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

			const JSON::Value *ctrl = doc.root[AIS::KEY_SETTING_CONTROL];
			if (!ctrl || !ctrl->isObject())
				return;

			const JSON::Value *v = ctrl->getObject()[AIS::KEY_SETTING_VIEWER];
			if (!v || !v->isObject())
				return;

			for (const auto &m : v->getObject().getMembers())
				for (const auto k : live_keys)
					if (m.Key() == k)
						viewer.SetKey(k, m.Get().to_string());
		}
		catch (std::exception const &e)
		{
			Error() << "Control: viewer settings not re-applied: " << e.what();
		}
	}
#endif

	static int runManaged(const std::string &config_file, int port, const std::string &bind)
	{
		ControlCore core(config_file, port, bind);
		ControlServer server(core);

		if (core.authRequired())
			Info() << "Control: managed mode, config file \"" << config_file << "\", bound to " << core.getBindAddress() << ", password required";
		else
			Info() << "Control: managed mode, config file \"" << config_file << "\", local access only";

		server.start();

#ifdef HASWEBVIEWER
		std::unique_ptr<WebViewer> viewer = startManagedViewer(config_file, core);

		std::mutex viewer_mtx;

		core.setConfigChangedCallback([&viewer, &viewer_mtx, &config_file]()
									  {
									std::lock_guard<std::mutex> lock(viewer_mtx);
									if (viewer) reapplyViewerSettings(*viewer, config_file); });
#endif

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
						std::this_thread::sleep_for(std::chrono::milliseconds(250));
					if (stop_process || !core.engineDesired())
						continue;
				}

				stop = false;
				if (!core.engineDesired())
					continue;

				RunState state;
				Config c(state);

				try
				{
					state.receivers.back()->getDeviceManager().refreshDevices();
					c.read(config_file);

					for (auto &r : state.receivers)
						if (r->getDeviceManager().InputType() == Type::NONE && r->getDeviceManager().SerialNumber().empty())
							throw std::runtime_error("no input device selected, configure one under Input");

					if (!state.xshare_defined && !state.comm_feed)
						state.createCommunityFeed();
#ifdef HASWEBVIEWER
					{
						std::lock_guard<std::mutex> lock(viewer_mtx);
						if (!viewer)
							viewer = startManagedViewer(config_file, core);
						else
							reapplyViewerSettings(*viewer, config_file);
					}
#endif
#ifdef HASWEBVIEWER
					run(state, viewer.get(), &core);
#else
					run(state, &core);
#endif
				}
				catch (std::exception const &e)
				{
					Error() << e.what();
					for (auto &r : state.receivers)
						r->stop();
#ifdef HASWEBVIEWER
					if (viewer)
						viewer->disconnectEngine();
#endif
					core.engineFailed();
				}
				core.reportStopped();
				Info() << "Control: engine stopped";
			}
			else
			{
				core.waitForCommand();
			}
		}

		server.close();

#ifdef HASWEBVIEWER
		{
			std::lock_guard<std::mutex> lock(viewer_mtx);
			if (viewer)
				viewer->close();
		}
#endif
		return 0;
	}

	bool isInvocation(const std::vector<std::string> &args)
	{
		for (const auto &a : args)
			if (a == "-E")
				return true;
		return false;
	}

	int main(const std::vector<std::string> &args)
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

		return runManaged(file, port, bind);
	}

}
