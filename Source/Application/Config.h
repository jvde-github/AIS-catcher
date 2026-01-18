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

#include "Receiver.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/StringBuilder.h"
#include "WebViewer.h"

#include "Screen.h"
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

class Config {

	std::vector<std::unique_ptr<Receiver>>& _receivers;
	int _nrec;
	std::vector<std::unique_ptr<IO::OutputMessage>>& _msg;
	std::vector<std::unique_ptr<IO::OutputJSON>>& _json;

	IO::ScreenOutput& _screen;
	std::vector<std::unique_ptr<WebViewer>>& _server;

	int &_own_mmsi;

	bool isActiveObject(const JSON::Value& pd);
	void setSettingsFromJSON(const JSON::Value& pd, Setting& s);
	void setHTTPfromJSON(const JSON::Property& pd);
	void setUDPfromJSON(const JSON::Property& pd);
	void setTCPfromJSON(const JSON::Property& pd);
	void setMQTTfromJSON(const JSON::Property& pd);
	void setTCPListenerfromJSON(const JSON::Property& pd);
	void setModelfromJSON(const JSON::Property& p);
	void setServerfromJSON(const JSON::Value& pd);
	void setReceiverfromJSON(const std::vector<JSON::Property>& pd, bool unspecAllowed);
	void setReceiverFromArray(const JSON::Property& pd);
	void setSharing(const std::vector<JSON::Property>& props);

public:
	Config(std::vector<std::unique_ptr<Receiver>>& r, int& nr, std::vector<std::unique_ptr<IO::OutputMessage>>& o, std::vector<std::unique_ptr<IO::OutputJSON>>& j, IO::ScreenOutput& s, std::vector<std::unique_ptr<WebViewer>>& v,int &own_mmsi) : _receivers(r), _nrec(nr), _msg(o), _json(j), _screen(s), _server(v), _own_mmsi(own_mmsi) {}

	void read(std::string& file_config);
	void set(const std::string& str);
};
