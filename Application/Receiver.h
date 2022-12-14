/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include "Signals.h"
#include "Common.h"
#include "Model.h"
#include "IO.h"
#include "Network.h"
#include "AIS.h"
#include "JSONAIS.h"
#include "Server.h"

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

//--------------------------------------------

// Hardware + Model with output connectors for messages and JSON
class Receiver {

	bool timing = false;

	Type type = Type::NONE;
	std::string serial;

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

	// available devices
	std::vector<Device::Description> device_list;
	Device::Device* device = NULL;

	void refreshDevices(void);

	// Model
	void setChannel(std::string mode) { setChannel(mode, ""); }
	void setChannel(std::string mode, std::string NMEA);
	void setTags(const std::string& s);

	bool& Timing() { return timing; }

	// Receiver output are Messages or JSON
	Connection<AIS::Message>& Output(int i) { return models[i]->Output().out; }
	Connection<JSON::JSON>& OutputJSON(int i) { return jsonais[i].out; }

	void setDeviceSampleRate(uint32_t rate) {
		if (rate && device) device->setSampleRate(rate);
	}

	void setDeviceFreqCorrection(int ppm) {
		if (ppm && device) device->Set("FREQOFFSET", std::to_string(ppm));
	}

	void setDeviceBandwidth(int bandwidth) {
		if (bandwidth && device) device->Set("BW", std::to_string(bandwidth));
	}

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
	void setup(Receiver& r);
};

//--------------------------------------------
class OutputUDP {
	std::vector<IO::UDP> _UDP;

public:
	void setup(Receiver& r);
	IO::UDP& add();
	IO::UDP& add(const std::string& host, const std::string& port);
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
	void setup(Receiver& r);
};

//--------------------------------------------
class OutputStatistics {

public:
	std::vector<IO::StreamCounter<AIS::Message>> statistics;

	void setup(Receiver& r);
};

// ----------------------------
// Class to log historic message count
template <int N, int INTERVAL>
struct History : public StreamIn<AIS::Message> {

	struct {
		long int time;
		int count;
	} history[N];

	int start, end;

	History() : start(0), end(0) { history[end] = { ((long int)time(nullptr)) / INTERVAL, 0 }; }

	void Receive(const AIS::Message* msg, int len, TAG& tag) {
		for (int i = 0; i < len; i++) {
			long int tm = ((long int)msg[i].getRxTimeUnix()) / INTERVAL;

			if (history[end].time != tm) {
				end = (end + 1) % N;
				if (start == end) start = (start + 1) % N;
				history[end] = { tm, 1 };
			}
			else
				history[end].count++;
		}
	}

	float average() {
		float avg = 0.0f;

		for (int idx = start; idx != end; idx = (idx + 1) % N) {
			avg += history[idx].count;
		}

		int delta_time = 1 + (int)((long int)history[end].time - (long int)history[start].time);
		return avg / delta_time;
	}

	float last() {
		if (start == end) return 0.0f;
		return history[(end + N - 1) % N].count;
	}

	void addJSONarray(std::string& content) {
		std::string delim = "";
		content += "[";

		long int tm = ((long int)time(nullptr)) / INTERVAL;
		bool done = false;
		int idx = end;

		for (int i = N; i > 0 && !done; i--) {
			int v = 0;
			if (!done && history[idx].time >= tm) {
				v = history[idx].count;
				if (idx == start)
					done = true;
				else
					idx = (idx + N - 1) % N;
			}

			content += delim + "{\"x\":" + std::to_string(i - N) + ",\"y\":" + std::to_string(v) + "}";
			delim = ",";
			tm--;
		}

		content += "]";
	}
};

class Ships : public StreamIn<JSON::JSON> {
	int first, last, count;
	std::string content, delim;
	float lat, lon;
	int TIME_HISTORY = 30 * 60;

	const int N = 4096; // recalibrate
	struct Detail {

		uint32_t mmsi;
		float lat, lon, ppm, level;
		int count;
		int type;
		std::time_t last_signal;
		char shipname[21];
		char callsign[8];
	};

	struct List {
		int prev, next;
		Detail ship;
	};
	std::vector<List> ships;

	bool isValidCoord(float lat, float lon);
	float distance(float lat_a, float lon_a, float lat_b, float lon_b);

public:
	void setup(float lat = 0.0f, float lon = 0.0f);
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void Receive(const JSON::JSON* data, int len, TAG& tag);
	std::string getJSON();

	int getCount() { return count; }
	int getMaxCount() { return N; }
};

class OutputServer : public IO::Server, public Setting {
	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	float lat = 0, lon = 0;

	// history of 180 minutes and 180 seconds
	History<180, 60> hist_minute;
	History<180, 1> hist_second;
	History<168, 3600> hist_hour;


	std::time_t time_start;
	std::string sample_rate, product, vendor, model, serial, station = "\"\"", station_link = "\"\"";

	struct Counter : public StreamIn<AIS::Message> {
		int count[28] = { 0 };
		int ChA = 0, ChB = 0;
		void Receive(const AIS::Message* msg, int len, TAG& tag);
	} counter;

	struct RAWcounter : public StreamIn<RAW> {
		uint64_t received = 0;
		void Receive(const RAW* data, int len, TAG& tag) {
			received += data[0].size;
		}
	} raw_counter;

	Ships ships;

public:
	bool& active() { return run; }
	void setup(Receiver& r);

	// HTTP callbacks
	void Request(SOCKET s, const std::string& r);

	Setting& Set(std::string option, std::string arg);
	std::string Get() { return ""; };
};
