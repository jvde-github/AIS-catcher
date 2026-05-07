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
#include "JSON/StringBuilder.h"

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
	void setHTTPfromJSON(const JSON::Member &m);
	void setUDPfromJSON(const JSON::Member &m);
	void setTCPfromJSON(const JSON::Member &m);
	void setMQTTfromJSON(const JSON::Member &m);
	void setTCPListenerfromJSON(const JSON::Member &m);
	void setModelfromJSON(const JSON::Member &m);
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
