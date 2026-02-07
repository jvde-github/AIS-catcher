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
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <unordered_map>

#include "AIS-catcher.h"

#include "Common.h"
#include "JSONAIS.h"
#include "AIS.h"
#include "Prometheus.h"
#include "HTTPServer.h"
#include "DB.h"
#include "PlaneDB.h"
#include "History.h"
#include "Receiver.h"
#include "MapTiles.h"

class SSEStreamer : public StreamIn<JSON::JSON>
{
	IO::HTTPServer *server = nullptr;

	unsigned idx = 0;
	bool obfuscate = true;

public:
	virtual ~SSEStreamer() = default;

	void Receive(const JSON::JSON *data, int len, TAG &tag);
	void setSSE(IO::HTTPServer *s) { server = s; }
	void setObfuscate(bool o) { obfuscate = o; }
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

class WebViewer : public Setting
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
	bool showdecoder = false;
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
	PlaneDB planes;

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
	std::string backup_filename = "";
	std::string os, hardware;

	std::mutex m;
	std::condition_variable cv;
	std::thread backup_thread;

	IO::HTTPServer server;

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
	void addFileSystemTilesSource(const std::string &directoryPath, bool overlay);

	// Router table for fast path lookup
	typedef std::function<void(IO::TCPServerConnection &, const std::string &, bool)> RouteHandler;
	std::unordered_map<std::string, RouteHandler> routeTable;
	void initializeRoutes();

	// HTTP request handler
	void handleRequest(IO::TCPServerConnection &c, const std::string &r, bool gzip);
	
	// Route handlers
	void handleCDNFile(IO::TCPServerConnection &c, const std::string &path, bool gzip);
	void handleAPIStats(IO::TCPServerConnection &c, bool gzip);
	void handleAPIShips(IO::TCPServerConnection &c, bool gzip);
	void handleAPIPath(IO::TCPServerConnection &c, const std::string &args, bool gzip);
	void handleAPIPathGeoJSON(IO::TCPServerConnection &c, const std::string &args, bool gzip);
	void handleAPIMessage(IO::TCPServerConnection &c, const std::string &args, bool gzip);
	void handleAPIDecode(IO::TCPServerConnection &c, const std::string &args, bool gzip);
	void handleAPIVessel(IO::TCPServerConnection &c, const std::string &args, bool gzip);
	void handleTiles(IO::TCPServerConnection &c, const std::string &path, bool gzip);
	void handleStaticFile(IO::TCPServerConnection &c, const std::string &filename, bool gzip);
	void handleKML(IO::TCPServerConnection &c, bool gzip);
	void handleMetrics(IO::TCPServerConnection &c, bool gzip);
	void handleAPIShipsArray(IO::TCPServerConnection &c, bool gzip);
	void handleAPIPlanesArray(IO::TCPServerConnection &c, bool gzip);
	void handleBinaryShips(IO::TCPServerConnection &c, bool gzip);
	void handleAPIShipsFull(IO::TCPServerConnection &c, bool gzip);
	void handleAPISSE(IO::TCPServerConnection &c, bool gzip);
	void handleAPISignal(IO::TCPServerConnection &c, bool gzip);
	void handleAPILog(IO::TCPServerConnection &c, bool gzip);
	void handleAPIBinaryMessages(IO::TCPServerConnection &c, bool gzip);
	void handlePluginsJS(IO::TCPServerConnection &c, bool gzip);
	void handleConfigCSS(IO::TCPServerConnection &c, bool gzip);
	void handleAboutMD(IO::TCPServerConnection &c, bool gzip);
	void handleAPIAllPath(IO::TCPServerConnection &c, bool gzip);
	void handleAPIAllPathGeoJSON(IO::TCPServerConnection &c, bool gzip);
	void handleGeoJSON(IO::TCPServerConnection &c, bool gzip);
	void handleAllPathGeoJSON(IO::TCPServerConnection &c, bool gzip);
	void handleHistoryFull(IO::TCPServerConnection &c, bool gzip);

	// NMEA decoder utility
	static std::string decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced = true);

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

	IO::HTTPServer& getServer() { return server; }

	bool isPortSet() { return port_set; }

	Setting &Set(std::string option, std::string arg);
	std::string Get() { return ""; }
};
