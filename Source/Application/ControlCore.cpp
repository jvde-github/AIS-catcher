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

#include <fstream>
#include <cstdio>
#include <chrono>

#include "ControlCore.h"
#include "Common.h"
#include "Logger.h"
#include "Helper.h"
#include "Parse.h"
#include "Keys.h"
#include "Parser.h"
#include "Writer.h"
#include "RunState.h"
#include "Config.h"
#include "DeviceManager.h"

ControlCore::ControlCore(const std::string &file, int port_override) : config_file(file)
{
	if (!std::ifstream(config_file).good())
		createDefaultConfig();

	readManagedFields(port_override);
}

void ControlCore::createDefaultConfig()
{
	const std::string content =
		"{\n"
		"  \"config\": \"aiscatcher\",\n"
		"  \"version\": 1,\n"
		"  \"engine\": \"off\",\n"
		"  \"control\": { \"port\": 8110 },\n"
		"  \"server\": { \"active\": true, \"port\": 8100 }\n"
		"}\n";

	std::string error;
	if (!writeFileAtomic(config_file, content, error))
		throw std::runtime_error("Control: cannot create config file \"" + config_file + "\": " + error);

	Info() << "Control: created default config file " << config_file;
}

void ControlCore::readManagedFields(int port_override)
{
	try
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

		for (const auto &m : doc.getMembers())
		{
			switch (m.Key())
			{
			case AIS::KEY_SETTING_ENGINE:
				desired = Util::Parse::Switch(m.Get().to_string());
				break;
			case AIS::KEY_SETTING_CONTROL:
				if (!m.Get().isObject())
					throw std::runtime_error("control section needs to be an object");

				for (const auto &c : m.Get().getObject().getMembers())
				{
					switch (c.Key())
					{
					case AIS::KEY_SETTING_PORT:
						control_port = Util::Parse::Integer(c.Get().to_string(), 1, 65535);
						break;
					case AIS::KEY_SETTING_PASSWORD:
						password_hash = c.Get().to_string();
						break;
					case AIS::KEY_SETTING_SALT:
						password_salt = c.Get().to_string();
						break;
					}
				}
				break;
			}
		}
	}
	catch (const std::exception &e)
	{
		Error() << "Control: config file invalid, engine not started: " << e.what();
		desired = false;
	}

	if (port_override > 0)
		control_port = port_override;
}

void ControlCore::startEngine()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (desired)
			return;
		desired = true;
		restart_pending = false;
	}
	persistEngineField(true);
	cv.notify_all();
}

void ControlCore::stopEngine()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		desired = false;
		restart_pending = false;
	}
	persistEngineField(false);
	StopRequest();
}

void ControlCore::restartEngine()
{
	bool running;
	{
		std::lock_guard<std::mutex> lock(mtx);
		desired = true;
		running = (state == EngineState::Running);
		restart_pending = running;
	}
	persistEngineField(true);

	if (running)
		StopRequest();
	else
		cv.notify_all();
}

ControlCore::EngineState ControlCore::getEngineState()
{
	std::lock_guard<std::mutex> lock(mtx);
	return state;
}

std::string ControlCore::getStatusJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	bool running = (state == EngineState::Running);
	std::string s;
	JSON::Writer w(s);
	w.beginObject()
		.kv("engine", running ? "running" : "stopped")
		.kv("uptime", running ? (long long)(std::time(nullptr) - engine_start_time) : 0LL)
		.endObject()
		.finish();
	return s;
}

std::string ControlCore::getConfig()
{
	std::lock_guard<std::mutex> lock(file_mtx);
	return Util::Helper::readFile(config_file);
}

bool ControlCore::setConfig(const std::string &json, std::string &error)
{
	if (!validate(json, error))
		return false;

	std::lock_guard<std::mutex> lock(file_mtx);

	try
	{
		std::string ignore;
		writeFileAtomic(config_file + ".bak", Util::Helper::readFile(config_file), ignore);
	}
	catch (const std::exception &)
	{
	}

	return writeFileAtomic(config_file, json, error);
}

std::string ControlCore::getDeviceListJSON()
{
	return DeviceManager::getDeviceListJSON();
}

bool ControlCore::validate(const std::string &json, std::string &error)
{
	try
	{
		RunState scratch;
		Config c(scratch);
		c.set(json);
	}
	catch (const std::exception &e)
	{
		error = e.what();
		return false;
	}
	return true;
}

bool ControlCore::writeFileAtomic(const std::string &path, const std::string &content, std::string &error)
{
	const std::string tmp = path + ".tmp";

	std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		error = "cannot open \"" + tmp + "\" for writing";
		return false;
	}

	file.write(content.data(), content.size());
	file.close();

	if (file.fail())
	{
		std::remove(tmp.c_str());
		error = "cannot write \"" + tmp + "\"";
		return false;
	}

#ifdef _WIN32
	std::remove(path.c_str());
#endif
	if (std::rename(tmp.c_str(), path.c_str()) != 0)
	{
		std::remove(tmp.c_str());
		error = "cannot rename \"" + tmp + "\" to \"" + path + "\"";
		return false;
	}
	return true;
}

void ControlCore::persistEngineField(bool on)
{
	std::lock_guard<std::mutex> lock(file_mtx);

	try
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));
		doc.root.Set(AIS::KEY_SETTING_ENGINE, on ? "on" : "off", doc.pool);

		std::string out;
		JSON::Serializer serializer(JSON_DICT_SETTING);
		serializer.stringify(doc.root, out);

		std::string error;
		if (!writeFileAtomic(config_file, out, error))
			Error() << "Control: cannot persist engine state: " << error;
	}
	catch (const std::exception &e)
	{
		Error() << "Control: cannot persist engine state: " << e.what();
	}
}

bool ControlCore::engineDesired()
{
	std::lock_guard<std::mutex> lock(mtx);
	return desired;
}

void ControlCore::reportRunning()
{
	std::lock_guard<std::mutex> lock(mtx);
	state = EngineState::Running;
	engine_start_time = std::time(nullptr);
}

void ControlCore::reportStopped()
{
	std::lock_guard<std::mutex> lock(mtx);
	state = EngineState::Stopped;

	if (restart_pending)
		restart_pending = false;
	else
		desired = false;
}

void ControlCore::engineFailed()
{
	std::lock_guard<std::mutex> lock(mtx);
	desired = false;
	restart_pending = false;
}

void ControlCore::waitForCommand()
{
	std::unique_lock<std::mutex> lock(mtx);
	cv.wait_for(lock, std::chrono::milliseconds(250), [this]
				{ return desired; });
}
