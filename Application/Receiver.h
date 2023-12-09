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
#include <thread>
#include <condition_variable>
#include <mutex>

#include "AIS-catcher.h"

#include "Signals.h"
#include "Common.h"
#include "Model.h"
#include "IO.h"
#include "Network.h"
#include "AIS.h"
#include "JSONAIS.h"
#include "DB.h"
#include "History.h"
#include "PostgreSQL.h"

#include "Keys.h"
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
#include "Device/Serial.h"
#include "Device/ZMQ.h"
#include "Device/UDP.h"

class Receiver;

//--------------------------------------------
class OutputHTTP {
	std::vector<std::unique_ptr<IO::HTTPStreamer>> _http;

public:
	std::unique_ptr<IO::HTTPStreamer>& add(const std::vector<std::vector<std::string>>& km, int dict);
	void connect(Receiver& r);
	void start();
};

//--------------------------------------------
class OutputTCPlistener {
	std::vector<std::unique_ptr<IO::TCPlistenerStreamer>> _listener;

public:
	void connect(Receiver& r);
	void start();
	IO::TCPlistenerStreamer& add(const std::string& port);
	IO::TCPlistenerStreamer& add() { return add("0"); }
};

//--------------------------------------------
class OutputUDP {
	std::vector<std::unique_ptr<IO::UDPStreamer>> _UDP;

public:
	void connect(Receiver& r);
	void start();
	IO::UDPStreamer& add();
	IO::UDPStreamer& add(const std::string& host, const std::string& port);
};

//--------------------------------------------
class OutputTCP {
	std::vector<std::unique_ptr<IO::TCPClientStreamer>> _TCP;

public:
	void connect(Receiver& r);
	void start();
	IO::TCPClientStreamer& add();
	IO::TCPClientStreamer& add(const std::string& host, const std::string& port);
};

//--------------------------------------------
class OutputDBMS {
	std::vector<std::unique_ptr<IO::PostgreSQL>> _PSQL;

public:
	void connect(Receiver& r);
	void start();
	IO::PostgreSQL& add();
};

//--------------------------------------------
class OutputScreen {
	OutputLevel level = OutputLevel::FULL;

public:
	IO::MessageToScreen msg2screen;
	IO::JSONtoScreen json2screen;

	int verboseUpdateTime = 3;

	OutputScreen() : json2screen(&AIS::KeyMap, JSON_DICT_FULL) {}

	void setScreen(const std::string& str);
	void setScreen(OutputLevel o);
	void connect(Receiver& r);
	void start();
};

//--------------------------------------------
class OutputStatistics {

public:
	std::vector<IO::StreamCounter<AIS::Message>> statistics;

	void connect(Receiver& r);
	void start();
};

// Hardware + Model with output connectors for messages and JSON
class Receiver {

	bool timing = false;

	Type type = Type::NONE;
	std::string serial;
	int sample_rate = 0, bandwidth = 0, ppm = 0;
	Device::Device* getDeviceByType(Type type);

	//  Models
	std::vector<std::unique_ptr<AIS::Model>> models;

	AIS::Mode ChannelMode = AIS::Mode::AB;
	std::string ChannelNMEA = "AB";

	// Output
	std::vector<AIS::JSONAIS> jsonais;

	// Devices
	Device::RAWFile _RAW;
	Device::WAVFile _WAV;
	Device::RTLSDR _RTLSDR;
	Device::RTLTCP _RTLTCP;
	Device::SpyServer _SpyServer;
	Device::AIRSPYHF _AIRSPYHF;
	Device::AIRSPY _AIRSPY;
	Device::SDRPLAY _SDRPLAY;
	Device::HACKRF _HACKRF;
	Device::SOAPYSDR _SOAPYSDR;
	Device::SerialPort _SerialPort;
	Device::ZMQ _ZMQ;
	Device::UDP _UDP;

	TAG tag;

public:
	~Receiver() {
		if (device) device->Close();
	}

	bool verbose = false;

	// Devices
	Device::RAWFile& RAW() { return _RAW; };
	Device::WAVFile& WAV() { return _WAV; };
	Device::RTLSDR& RTLSDR() { return _RTLSDR; };
	Device::RTLTCP& RTLTCP() { return _RTLTCP; };
	Device::SpyServer& SpyServer() { return _SpyServer; };
	Device::AIRSPYHF& AIRSPYHF() { return _AIRSPYHF; };
	Device::AIRSPY& AIRSPY() { return _AIRSPY; };
	Device::SDRPLAY& SDRPLAY() { return _SDRPLAY; };
	Device::HACKRF& HACKRF() { return _HACKRF; };
	Device::SerialPort& SerialPort() { return _SerialPort; };
	Device::SOAPYSDR& SOAPYSDR() { return _SOAPYSDR; };
	Device::ZMQ& ZMQ() { return _ZMQ; };
	Device::UDP& UDP() { return _UDP; };

	// available devices
	static std::vector<Device::Description> device_list;
	Device::Device* device = NULL;

	void refreshDevices(void);
	bool isTXTformatSet() { return getDeviceByType(type) ? (getDeviceByType(type)->getFormat() == Format::TXT) : false; }

	// Model
	void setChannel(std::string mode) { setChannel(mode, ""); }
	void setChannel(std::string mode, std::string NMEA);
	void setTags(const std::string& s);

	bool& Timing() { return timing; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message>& Output(int i) { return models[i]->Output().out; }
	Connection<AIS::GPS>& OutputGPS(int i) { return models[i]->OutputGPS().out; }
	Connection<JSON::JSON>& OutputJSON(int i) { return jsonais[i].out; }

	void setSampleRate(int s) { sample_rate = s; }
	void setBandwidth(int b) { bandwidth = b; }
	void setPPM(int p) { ppm = p; }

	Type& InputType() {
		return type;
	}

	std::string& Serial() {
		return serial;
	}

	std::unique_ptr<AIS::Model>& addModel(int m);
	std::unique_ptr<AIS::Model>& Model(int i) { return models[i]; }
	int Count() { return models.size(); }

	void setup(int& g) {
		setupDevice();
		setupModel(g);
	}
	void setupDevice();
	void setupModel(int& g);

	void play();
	void stop();
};
