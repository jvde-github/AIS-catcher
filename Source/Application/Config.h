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

#include "RunState.h"

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
	RunState &_state;

	bool isActiveObject(const JSON::Value &m);
	void setSettingsFromJSON(const JSON::Value &m, Setting &s);
	void setModelfromJSON(const JSON::Member &m);

	// Device settings (RTLSDR, AIRSPY, ...) of the current receiver, nullptr if key is not a device
	Setting *getDeviceSetting(AIS::Keys key);

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

			_state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new T()));
			IO::OutputMessage &o = *_state.msg.back();
			if (default_timeout)
				o.SetKey(AIS::KEY_SETTING_TIMEOUT, default_timeout);
			setSettingsFromJSON(v, o);
		}
	}
#ifdef HASWEBVIEWER
	void setServerfromJSON(const JSON::Value &m);
#endif
	void setReceiverfromJSON(const std::vector<JSON::Member> &m, bool unspecAllowed);
	void setReceiverFromArray(const JSON::Member &m);
	void setSharing(const std::vector<JSON::Member> &members);

public:
	Config(RunState &state) : _state(state) {}

	void read(const std::string &file_config);
	void set(const std::string &str);

	bool isSharingDefined() const { return _state.xshare_defined; }
};
