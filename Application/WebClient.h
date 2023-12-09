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
#include <thread>
#include <condition_variable>
#include <mutex>

#include "AIS-catcher.h"

#include "Common.h"
#include "JSONAIS.h"
#include "AIS.h"
#include "HTTPServer.h"
#include "DB.h"
#include "History.h"
#include "Receiver.h"

class Counter : public StreamIn<JSON::JSON> {
	MessageStatistics stat;

public:
	void setCutOff(int c) { stat.setCutoff(c); }
	void Clear() { stat.Clear(); }

	bool Load(std::ifstream& file) { return stat.Load(file); }
	bool Save(std::ofstream& file) { return stat.Save(file); }

	void Receive(const JSON::JSON* msg, int len, TAG& tag) { stat.Add(*((AIS::Message*)msg[0].binary), tag); }

	std::string toJSON(bool empty = false) { return stat.toJSON(empty); }
};

class SSEStreamer : public StreamIn<JSON::JSON> {
	IO::HTTPServer* server = NULL;

public:
	void Receive(const JSON::JSON* data, int len, TAG& tag) {
		if (server) {
			AIS::Message* m = (AIS::Message*)data[0].binary;
			for (const auto& s : m->NMEA)
				server->sendSSE(1, "nmea", s);

			if (tag.lat != 0 && tag.lon != 0) {
				std::string json = "{\"mmsi\":" + std::to_string(m->mmsi()) + ",\"channel\":\"" + m->getChannel() + "\",\"lat\":" + std::to_string(tag.lat) + ",\"lon\":" + std::to_string(tag.lon) + "}";
				server->sendSSE(2, "nmea", json);
			}
		}
	}
	void setSSE(IO::HTTPServer* s) { server = s; }
};

class PromotheusCounter : public StreamIn<AIS::Message> {
	std::mutex m;
	MessageStatistics stat;

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
};

struct RAWcounter : public StreamIn<RAW> {
	uint64_t received = 0;
	void Receive(const RAW* data, int len, TAG& tag) { received += data[0].size; }
	void Reset() { received = 0; }
};

class WebClient : public IO::HTTPServer, public Setting {
	uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;

	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	int backup_interval = -1;
	bool port_set = false;
	bool use_zlib = true;
	bool realtime = false;
	bool supportPrometheus = false;
	bool thread_running = false;

	std::string params = "build_string = '" + std::string(VERSION_DESCRIBE) + "';\ncontext='settings';\naboutMDpresent=false;\n\n";
	std::string plugins;
	std::string stylesheets;
	std::string cdn;
	std::string about = "This content can be set by the station owner";

	// history of 180 minutes and 180 seconds
	History<60, 60> hist_minute;
	History<60, 1> hist_second;
	History<24, 3600> hist_hour;
	History<90, 86400> hist_day;

	std::time_t time_start;
	std::string sample_rate, product, vendor, model, serial, station = "\"\"", station_link = "\"\"";
	std::string filename = "";
	std::string os, hardware;

	std::mutex m;
	std::condition_variable cv;
	std::thread backup_thread;

	Counter counter, counter_session;
	SSEStreamer sse_streamer;
	PromotheusCounter dataPrometheus;
	RAWcounter raw_counter;

	DB ships;

	void BackupService();

	bool Load();
	bool Save();
	void Clear();

	void stopThread();

public:
	WebClient() {
		os.clear();
		JSON::StringBuilder::stringify(Util::Helper::getOS(), os);
		hardware.clear();
		JSON::StringBuilder::stringify(Util::Helper::getHardware(), hardware);
	}

	~WebClient() { stopThread(); }

	bool& active() { return run; }
	void connect(Receiver& r);
	void connect(AIS::Model& model, Connection<JSON::JSON>& json, Device::Device& device);
	void start();
	void close();
	void Reset();

	void setDeviceDescription(std::string p, std::string v, std::string s) {
		product = p;
		vendor = v;
		serial = s;
	}

	bool isPortSet() { return port_set; }
	// HTTP callbacks
	void Request(TCP::ServerConnection& c, const std::string& r, bool gzip);

	Setting& Set(std::string option, std::string arg);
	std::string Get() { return ""; };
};
