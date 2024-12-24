/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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
#include "MapTiles.h"

extern bool communityFeed;

class SSEStreamer : public StreamIn<JSON::JSON>
{
	IO::HTTPServer *server = nullptr;

	unsigned idx = 0;

public:
	virtual ~SSEStreamer() = default;

	void Receive(const JSON::JSON *data, int len, TAG &tag);
	void setSSE(IO::HTTPServer *s) { server = s; }
};

class WebViewerLogger
{
protected:
	IO::HTTPServer *server = nullptr;
	Logger::LogCallback logCallback;
	int cb = -1;

public:
	void Start()
	{
		logCallback = [this](const LogMessage &msg)
		{
			if (server)
			{
				server->sendSSE(3, "log", msg.toJSON());
			}
		};

		cb = Logger::getInstance().addLogListener(logCallback);
	}

	void Stop()
	{
		if (cb != -1)
		{
			Logger::getInstance().removeLogListener(cb);
			cb = -1;
		}
	}

	virtual ~WebViewerLogger() = default;

	void setSSE(IO::HTTPServer *s)
	{
		server = s;
	}
};

class WebViewer : public IO::HTTPServer, public Setting
{
	uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;

	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	int backup_interval = -1;
	bool port_set = false;
	bool use_zlib = true;
	bool realtime = false;
	bool showlog = false;
	bool KML = false;
	bool GeoJSON = false;
	bool supportPrometheus = false;
	bool thread_running = false;
	bool aboutPresent = false;

	std::vector<char> binary;
	std::vector<std::shared_ptr<MapTiles>> mapSources;

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
	WebViewerLogger logger;
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

	std::vector<std::string> parsePath(const std::string &url);
	bool parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y);
	void addMBTilesSource(const std::string &filepath, bool overlay);

public:
	WebViewer();

	~WebViewer()
	{
		if (showlog)
			logger.Stop();

		stopThread();
	}

	bool &active() { return run; }
	void connect(Receiver &r);
	void connect(AIS::Model &model, Connection<JSON::JSON> &json, Device::Device &device);
	void start();
	void close();
	void Reset();

	void setDeviceDescription(std::string p, std::string v, std::string s)
	{
		product = p;
		vendor = v;
		serial = s;
	}

	bool isPortSet() { return port_set; }
	// HTTP callbacks
	void Request(TCP::ServerConnection &c, const std::string &r, bool gzip);

	Setting &Set(std::string option, std::string arg);
	std::string Get() { return ""; }
};
