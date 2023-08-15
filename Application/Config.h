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

	std::vector<std::unique_ptr<Receiver>> & _receivers;
	OutputScreen& _screen;
	OutputHTTP& _http;
	OutputUDP& _udp;
	OutputTCP& _tcp;
	OutputTCPlistener& _tcp_server;
	std::vector<std::unique_ptr<WebClient>> & _server;

	int _nrec;

	bool isActiveObject(const JSON::Value& pd);
	void setSettingsFromJSON(const JSON::Value& pd, Setting& s);
	void setHTTPfromJSON(const JSON::Property& pd);
	void setUDPfromJSON(const JSON::Property& pd);
	void setTCPfromJSON(const JSON::Property& pd);
	void setTCPListenerfromJSON(const JSON::Property& pd);
	void setModelfromJSON(const JSON::Property& p);
	void setServerfromJSON(const JSON::Value& pd);
	void setReceiverfromJSON(const std::vector<JSON::Property>& pd, bool unspecAllowed);
	void setReceiverFromArray(const JSON::Property& pd);

public:
	Config(std::vector<std::unique_ptr<Receiver>> &r, int &nr, OutputScreen& s, OutputHTTP& h, OutputUDP& u, OutputTCP& t, OutputTCPlistener& tl, std::vector<std::unique_ptr<WebClient>> & v) : _receivers(r), _nrec(nr), _screen(s), _http(h), _server(v), _udp(u), _tcp(t), _tcp_server(tl) {};

	void read(std::string& file_config);
	void set(const std::string& str);
};