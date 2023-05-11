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
#include "Server.h"
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
#include "Device/ZMQ.h"
#include "Device/UDP.h"

//--------------------------------------------

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

	void setup() {
		setupDevice();
		setupModel();
	}
	void setupDevice();
	void setupModel();

	void play();
	void stop();
};

//--------------------------------------------
class OutputHTTP {
	std::vector<std::unique_ptr<IO::HTTP>> _http;

public:
	std::unique_ptr<IO::HTTP>& add(const std::vector<std::vector<std::string>>& km, int dict);
	void connect(Receiver& r);
	void start();
};

//--------------------------------------------
class OutputUDP {
	std::vector<std::unique_ptr<IO::UDP>> _UDP;

public:
	void connect(Receiver& r);
	void start();
	IO::UDP& add();
	IO::UDP& add(const std::string& host, const std::string& port);
};

//--------------------------------------------
class OutputTCP {
	std::vector<std::unique_ptr<IO::TCP>> _TCP;

public:
	void connect(Receiver& r);
	void start();
	IO::TCP& add();
	IO::TCP& add(const std::string& host, const std::string& port);
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

//--------------------------------------------
class WebClient : public IO::Server, public Setting {
	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	float lat = 0, lon = 0;
	int backup_interval = -1;
	bool port_set = false;
	bool use_zlib = true;
	bool supportPrometheus = false;

	std::string params = "build_string = '" + std::string(VERSION_DESCRIBE) + "';\naboutMDpresent=false;\n\n";
	std::string plugins;
	std::string stylesheets;
	std::string about = "This content can be set by the station owner";

	// history of 180 minutes and 180 seconds
	History<60, 60> hist_minute;
	History<60, 1> hist_second;
	History<24, 3600> hist_hour;
	History<90, 86400> hist_day;

	std::time_t time_start;
	std::string sample_rate, product, vendor, model, serial, station = "\"\"", station_link = "\"\"";
	std::string filename = "";

	std::mutex m;
	std::condition_variable cv;
	std::thread backup_thread;

	void BackupService();
	class Counter : public StreamIn<AIS::Message> {
		Statistics stat;

	public:
		void setCutOff(int c) { stat.setCutoff(c); }
		void Clear() { stat.Clear(); }

		bool Load(std::ifstream& file) { return stat.Load(file); }
		bool Save(std::ofstream& file) { return stat.Save(file); }

		void Receive(const AIS::Message* msg, int len, TAG& tag);

		std::string toJSON(bool empty = false) { return stat.toJSON(empty); }
	} counter;

	class PromotheusCounter : public StreamIn<AIS::Message> {
		std::mutex m;
		Statistics stat;

	public:
		void setCutOff(int c) { stat.setCutoff(c); }

		std::string ppm;
		std::string level;

		void Receive(const AIS::Message* data, int len, TAG& tag) {
			if (ppm.size() > 32768) return;
			if (level.size() > 32768) return;

			m.lock();
			stat.Add(data[0], tag);
			ppm += "ais_msg_ppm{type=\"" + std::to_string(data[0].type()) + "\",mmsi=\"" + std::to_string(data[0].mmsi()) + "\",channel=\"" + std::string(1, data[0].getChannel()) + "\"} " + std::to_string(tag.ppm) + "\n";
			level += "ais_msg_level{type=\"" + std::to_string(data[0].type()) + "\",mmsi=\"" + std::to_string(data[0].mmsi()) + "\",channel=\"" + std::string(1, data[0].getChannel()) + "\"} " + std::to_string(tag.level) + "\n";
			m.unlock();
		}

		void reset() {
			m.lock();
			ppm = "# HELP ais_msg_ppm\n# TYPE ais_msg_ppm gauge\n";
			level = "# HELP ais_msg_level\n# TYPE ais_msg_level gauge\n";
			m.unlock();
		}

		std::string toPrometheus() { return stat.toPrometheus(); }
		PromotheusCounter() { reset(); }

	} dataPrometheus;

	struct RAWcounter : public StreamIn<RAW> {
		uint64_t received = 0;
		void Receive(const RAW* data, int len, TAG& tag) {
			received += data[0].size;
		}
	} raw_counter;

	DB ships;

	bool Load();
	bool Save();
	void Clear();

public:
	bool& active() { return run; }
	void connect(Receiver& r);
	void start();
	void close();

	bool isPortSet() { return port_set; }
	// HTTP callbacks
	void Request(IO::Client& c, const std::string& r, bool gzip);

	Setting& Set(std::string option, std::string arg);
	std::string Get() { return ""; };
};
