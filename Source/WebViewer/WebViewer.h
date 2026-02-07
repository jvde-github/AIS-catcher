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
#include "SSEStreamer.h"
#include "WebViewerLogger.h"

class WebViewer : public Setting
{
	uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;

	int port = 0;
	int firstport = 0;
	int lastport = 0;
	bool run = false;
	int backup_interval = -1;
	bool port_set = false;
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

	bool parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y);
	void addMBTilesSource(const std::string &filepath, bool overlay);
	void addFileSystemTilesSource(const std::string &directoryPath, bool overlay);

	// Router table for fast path lookup
	typedef std::function<void(IO::HTTPRequest &)> RouteHandler;
	std::unordered_map<std::string, RouteHandler> routeTable;
	void initializeRoutes();

	// HTTP request handler
	void handleRequest(IO::HTTPRequest &req);
	
	// Route handlers
	void handleCDNFile(IO::HTTPRequest &req);
	void handleAPIStats(IO::HTTPRequest &req);
	void handleAPIShips(IO::HTTPRequest &req);
	void handleAPIPath(IO::HTTPRequest &req);
	void handleAPIPathGeoJSON(IO::HTTPRequest &req);
	void handleAPIMessage(IO::HTTPRequest &req);
	void handleAPIDecode(IO::HTTPRequest &req);
	void handleAPIVessel(IO::HTTPRequest &req);
	void handleTiles(IO::HTTPRequest &req);
	void handleStaticFile(IO::HTTPRequest &req);
	void handleKML(IO::HTTPRequest &req);
	void handleMetrics(IO::HTTPRequest &req);
	void handleAPIShipsArray(IO::HTTPRequest &req);
	void handleAPIPlanesArray(IO::HTTPRequest &req);
	void handleBinaryShips(IO::HTTPRequest &req);
	void handleAPIShipsFull(IO::HTTPRequest &req);
	void handleAPISSE(IO::HTTPRequest &req);
	void handleAPISignal(IO::HTTPRequest &req);
	void handleAPILog(IO::HTTPRequest &req);
	void handleAPIBinaryMessages(IO::HTTPRequest &req);
	void handlePluginsJS(IO::HTTPRequest &req);
	void handleConfigCSS(IO::HTTPRequest &req);
	void handleAboutMD(IO::HTTPRequest &req);
	void handleAPIAllPath(IO::HTTPRequest &req);
	void handleAPIAllPathGeoJSON(IO::HTTPRequest &req);
	void handleGeoJSON(IO::HTTPRequest &req);
	void handleAllPathGeoJSON(IO::HTTPRequest &req);
	void handleHistoryFull(IO::HTTPRequest &req);

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
