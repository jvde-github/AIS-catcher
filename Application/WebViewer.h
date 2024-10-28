/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
#include "Prometheus.h"
#include "HTTPServer.h"
#include "DB.h"
#include "History.h"
#include "Receiver.h"

extern bool communityFeed;

class SSEStreamer : public StreamIn<JSON::JSON> {
	IO::HTTPServer* server = nullptr;

public:
	virtual ~SSEStreamer() {}
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

class WebViewer : public IO::HTTPServer, public Setting {
	uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;

	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	int backup_interval = -1;
	bool port_set = false;
	bool use_zlib = true;
	bool realtime = false;
	bool KML = false;
	bool GeoJSON = false;
	bool supportPrometheus = false;
	bool thread_running = false;
	bool aboutPresent = false;
	
	std::vector<char> binary;

	std::string params;
	std::string plugin_code;
	std::string plugins;
	std::string stylesheets;
	std::string cdn;
	std::string about = "This content can be set by the station owner";

	DB ships;

	// history of 180 minutes and 180 seconds
	History<60, 60> hist_minute;
	History<60, 1> hist_second;
	History<24, 3600> hist_hour;
	History<90, 86400> hist_day;

	Counter counter, counter_session;
	SSEStreamer sse_streamer;
	PromotheusCounter dataPrometheus;
	ByteCounter raw_counter;

	std::time_t time_start;
	std::string sample_rate, product, vendor, model, serial, station = "\"\"", station_link = "\"\"";
	std::string filename = "";
	std::string os, hardware;

	std::mutex m;
	std::condition_variable cv;
	std::thread backup_thread;

	void BackupService();
	void addPlugin(const std::string &str);

	bool Load();
	bool Save();
	void Clear();

	void stopThread();
	AIS::Filter filter;

public:
	WebViewer();

	~WebViewer() { stopThread(); }

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
	std::string Get() { return ""; }
};
