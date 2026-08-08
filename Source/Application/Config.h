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

#pragma once
#include <iostream>
#include <string.h>
#include <memory>

#include "AIS-catcher.h"

#include "Engine.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/Writer.h"

#include "File.h"

#include "Device/FileRAW.h"
#include "Device/FileWAV.h"
#include "Device/RTLSDR.h"
#include "Device/AIRSPYHF.h"
#include "Device/HACKRF.h"
#include "Device/RTLTCP.h"
#include "Device/AIRSPY.h"
#include "Device/SDRPLAY.h"
#include "Device/SpyServer.h"
#include "Device/SoapySDR.h"
#include "Device/ZMQ.h"
#include "Device/HYDRASDR.h"

class Config
{
	Engine &_engine;

	bool isActiveObject(const JSON::Value &m);
	void setModelfromJSON(const JSON::Member &m);
	void setEnginefromJSON(const JSON::Value &v);
	static int engineType(const std::string &s);

	// Create one output of type T per active object in the array, with settings applied
	template <typename T>
	void addOutputsFromJSON(const JSON::Member &m, const std::string &label, const char *default_timeout = nullptr)
	{
		if (!m.Get().isArray())
			throw std::runtime_error(label + " settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : m.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			_engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(new T()));
			IO::OutputMessage &o = *_engine.msg.back();
			if (default_timeout)
				o.SetKey(AIS::KEY_SETTING_TIMEOUT, default_timeout);
			setSettingsFromJSON(v, o);
		}
	}
	// the "db" array names its backend with a "type" field
	void addDatabaseOutputsFromJSON(const JSON::Member &m);

public:
	// one name-to-backend map for config file and -D alike; throws on unknown type
	static IO::OutputMessage *newDatabaseOutput(const std::string &type);

private:

#ifdef HASWEBVIEWER
	void setServerfromJSON(const JSON::Value &m);
#endif
	bool dry_run = false;
	void setReceiverfromJSON(const std::vector<JSON::Member> &m, bool unspecAllowed);
	void setReceiverFromArray(const JSON::Member &m);
	void setSharing(const std::vector<JSON::Member> &members);

public:
	// dry: parse for validation only, without touching the managed viewer
	Config(Engine &engine, bool dry = false) : _engine(engine), dry_run(dry) {}

	void read(const std::string &file_config);
	void set(const std::string &str);

	// skip: one extra key the caller already consumed, e.g. the "type" selector
	static void setSettingsFromJSON(const JSON::Value &m, Setting &s, int skip = -1);
	static void setManagedViewerfromJSON(const JSON::Value &m);
	static void readManagedViewer(const std::string &file_config);
};
