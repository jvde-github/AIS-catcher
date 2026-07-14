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
#include <chrono>
#include <random>
#include <cstring>
#ifndef _WIN32
#include <climits>
#include <cstdlib>
#endif

#include "ControlCore.h"
#include "SHA256.h"
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

ControlCore::ControlCore(const std::string &file, int port_override, const std::string &bind) : config_file(file), bind_address(bind)
{
	if (!std::ifstream(config_file).good())
		createDefaultConfig();

	readManagedFields(port_override);

	auto_retry = desired;
}

void ControlCore::createDefaultConfig()
{
	const std::string content =
		"{\n"
		"  \"config\": \"aiscatcher\",\n"
		"  \"version\": 1,\n"
		"  \"engine\": \"off\",\n"
		"  \"sharing\": true,\n"
		"  \"control\": { \"wizard\": true }\n"
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

				applyAuthFields(m.Get());
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
	command_seq++;
	{
		std::lock_guard<std::mutex> lock(mtx);
		resetRetry();
		if (desired)
			return;
		desired = true;
		restart_pending = false;
		status_epoch++;
	}
	persistEngineField(true);
	cv.notify_all();
}

void ControlCore::stopEngine()
{
	command_seq++;
	{
		std::lock_guard<std::mutex> lock(mtx);
		desired = false;
		restart_pending = false;
		resetRetry();
		status_epoch++;
	}
	persistEngineField(false);
	StopRequest();
}

void ControlCore::restartEngine()
{
	command_seq++;
	bool running;
	{
		std::lock_guard<std::mutex> lock(mtx);
		desired = true;
		running = (state == EngineState::Running);
		restart_pending = running;
		resetRetry();
		auto_retry = running;
		status_epoch++;
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

long long ControlCore::getUptime()
{
	std::lock_guard<std::mutex> lock(mtx);

	if (state != EngineState::Running)
		return 0;

	return (long long)(std::time(nullptr) - engine_start_time);
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

	if (!writeFileAtomic(config_file, json, error))
		return false;

	refreshAuthFields(json);

	if (config_changed)
		config_changed();

	return true;
}

static bool isSameFile(const std::string &a, const std::string &b)
{
#ifdef _WIN32
	return a == b;
#else
	char ra[PATH_MAX], rb[PATH_MAX];
	if (!realpath(a.c_str(), ra) || !realpath(b.c_str(), rb))
		return a == b;
	return strcmp(ra, rb) == 0;
#endif
}

// Reads the pre-managed-mode config file recorded by the installer in
// control.legacy_config; only that one path is ever served, never the
// active config itself.
bool ControlCore::readLegacyConfig(std::string &content)
{
	std::string path;

	try
	{
		std::lock_guard<std::mutex> lock(file_mtx);

		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

		const JSON::Value *v = doc.root[AIS::KEY_SETTING_CONTROL];
		if (!v || !v->isObject())
			return false;

		for (const auto &c : v->getObject().getMembers())
			if (c.Key() == AIS::KEY_SETTING_LEGACY_CONFIG)
				path = c.Get().to_string();
	}
	catch (const std::exception &)
	{
		return false;
	}

	if (path.empty() || isSameFile(path, config_file))
		return false;

	try
	{
		content = Util::Helper::readFile(path);

		// syntactically valid JSON only; old configs may hold retired keys
		JSON::Parser parser(JSON_DICT_SETTING);
		parser.setSkipUnknown(true);
		parser.parse(content);
	}
	catch (const std::exception &)
	{
		return false;
	}

	return true;
}

void ControlCore::applyAuthFields(const JSON::Value &control)
{
	for (const auto &c : control.getObject().getMembers())
	{
		if (c.Key() == AIS::KEY_SETTING_PASSWORD)
			password_hash = c.Get().to_string();
		else if (c.Key() == AIS::KEY_SETTING_SALT)
			password_salt = c.Get().to_string();
	}
}

void ControlCore::refreshAuthFields(const std::string &json)
{
	try
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(json);

		const JSON::Value *v = doc.root[AIS::KEY_SETTING_CONTROL];
		if (v && v->isObject())
		{
			std::lock_guard<std::mutex> lock(mtx);
			applyAuthFields(*v);
		}
	}
	catch (const std::exception &)
	{
	}
}

bool ControlCore::hasPassword()
{
	std::lock_guard<std::mutex> lock(mtx);
	return !password_hash.empty();
}

bool ControlCore::verifyPassword(const std::string &password)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (password_hash.empty())
		return false;

	return Util::SHA256::hex(password_salt + password) == password_hash;
}

std::string ControlCore::randomHex(size_t length)
{
	static const char *digits = "0123456789abcdef";

	std::random_device rd;
	std::string s;
	while (s.size() < length)
	{
		uint32_t r = rd();
		for (int j = 28; j >= 0 && s.size() < length; j -= 4)
			s += digits[(r >> j) & 0xF];
	}
	return s;
}

void ControlCore::setPassword(const std::string &password)
{
	std::string salt = randomHex(32);
	std::string hash = Util::SHA256::hex(salt + password);

	{
		std::lock_guard<std::mutex> lock(mtx);
		password_salt = salt;
		password_hash = hash;
	}

	persistControlAuth(hash, salt);
}

void ControlCore::persistControlAuth(const std::string &hash, const std::string &salt)
{
	std::lock_guard<std::mutex> lock(file_mtx);

	try
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

		const JSON::Value *v = doc.root[AIS::KEY_SETTING_CONTROL];
		if (v && v->isObject())
		{
			JSON::Value control = *v;
			control.getObject().Set(AIS::KEY_SETTING_PASSWORD, hash, doc.pool);
			control.getObject().Set(AIS::KEY_SETTING_SALT, salt, doc.pool);
		}
		else
		{
			JSON::JSON *control = doc.pool.addObject();
			control->Add(AIS::KEY_SETTING_PASSWORD, hash, doc.pool);
			control->Add(AIS::KEY_SETTING_SALT, salt, doc.pool);

			JSON::Value nv;
			nv.setObject(control);
			doc.root.Set(AIS::KEY_SETTING_CONTROL, nv);
		}

		std::string out;
		JSON::Serializer serializer(JSON_DICT_SETTING);
		serializer.stringify(doc.root, out);

		std::string error;
		if (!writeFileAtomic(config_file, out, error))
			Error() << "Control: cannot persist password: " << error;
	}
	catch (const std::exception &e)
	{
		Error() << "Control: cannot persist password: " << e.what();
	}
}

std::string ControlCore::getDeviceListJSON()
{
	return DeviceManager::getDeviceListJSON();
}

std::string ControlCore::getSerialListJSON()
{
	std::string s;
	JSON::Writer w(s);
	w.beginArray();
	for (const auto &path : Device::SerialPort::getDevicePathsCopy())
		w.val(path);
	w.endArray().finish();
	return s;
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
	if (content.empty() || content.back() != '\n')
		return Util::Helper::writeFileAtomic(path, content + "\n", error);

	return Util::Helper::writeFileAtomic(path, content, error);
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

uint32_t ControlCore::statusStamp()
{
	std::lock_guard<std::mutex> lock(mtx);
	uint32_t bits = (state == EngineState::Running ? 1 : 0) | (desired ? 2 : 0) |
					((desired && auto_retry && state != EngineState::Running) ? 4 : 0);
	return (status_epoch << 3) | bits;
}

bool ControlCore::engineRetrying()
{
	std::lock_guard<std::mutex> lock(mtx);
	return desired && auto_retry && state != EngineState::Running;
}

void ControlCore::reportRunning()
{
	std::lock_guard<std::mutex> lock(mtx);
	state = EngineState::Running;
	engine_start_time = std::time(nullptr);
	auto_retry = false;
	status_epoch++;
}

void ControlCore::reportStopped()
{
	std::lock_guard<std::mutex> lock(mtx);
	bool was_running = state == EngineState::Running;
	bool healthy = was_running && std::time(nullptr) - engine_start_time >= RETRY_HEALTHY_UPTIME;
	state = EngineState::Stopped;
	status_epoch++;

	if (restart_pending)
		restart_pending = false;
	else if (desired)
	{
		if (was_running || auto_retry)
		{
			if (healthy)
				retry_delay = 0;
			retry_delay = retry_delay ? MIN(retry_delay * 2, RETRY_DELAY_MAX) : RETRY_DELAY_FIRST;
			auto_retry = true;
			retry_pending = true;
		}
		else
		{
			desired = false;
			resetRetry();
		}
	}
}

void ControlCore::engineFailed()
{
	std::lock_guard<std::mutex> lock(mtx);
	restart_pending = false;
}

void ControlCore::waitForCommand()
{
	std::unique_lock<std::mutex> lock(mtx);
	cv.wait_for(lock, std::chrono::milliseconds(250), [this]
				{ return desired; });
}

int ControlCore::consumeRetryDelay()
{
	std::lock_guard<std::mutex> lock(mtx);
	if (!retry_pending)
		return 0;
	retry_pending = false;
	return retry_delay;
}
