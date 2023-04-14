/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

class Config {

	Receiver& _receiver;
	OutputScreen& _screen;
	OutputHTTP& _http;
	OutputUDP& _udp;
	OutputTCP& _tcp;
	WebClient& _server;

	bool isActiveObject(const JSON::Value& pd);
	void setSettingsFromJSON(const JSON::Value& pd, Setting& s);
	void setHTTPfromJSON(const JSON::Property& pd);
	void setUDPfromJSON(const JSON::Property& pd);
	void setTCPfromJSON(const JSON::Property& pd);
	void setModelfromJSON(const JSON::Property& p);
	void setServerfromJSON(const JSON::Value& pd);

public:
	Config(Receiver& r, OutputScreen& s, OutputHTTP& h, OutputUDP& u, OutputTCP& t, WebClient& v) : _receiver(r), _screen(s), _http(h), _server(v), _udp(u), _tcp(t) {};

	void read(std::string& file_config);
	void set(const std::string& str);
};