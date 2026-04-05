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

// Tracking/DB config accumulated during Set(), applied to all ReceiverStates in connect().
struct TrackingConfig
{
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_GPS = true;
	uint32_t own_mmsi = 0;
	int time_history = 30 * 60;
	int cutoff = 0;
};

// Bundles all per-receiver (or aggregate) state: ship DB, counters, history.
// states[0] is always "All" (aggregate). states[1..N] are per-receiver when N > 1.
struct ReceiverState
{
private:
	DB ships;
	Counter counter, counter_session;
	History<60, 1> hist_second;
	History<60, 60> hist_minute;
	History<24, 3600> hist_hour;
	History<90, 86400> hist_day;

public:
	std::string label;
	std::string product, vendor, serial, model_name, sample_rate;

	// Device metadata
	void setDevice(Device::Device *device);
	void appendDevice(Device::Device *device, const std::string &newline);

	// Config
	void applyConfig(const TrackingConfig &cfg, const AIS::Filter &f);

	// Lifecycle
	void setup();
	void clear();
	void reset();
	bool save(std::ofstream &f);
	bool load(std::ifstream &f);

	// Wire internal streams (ships → hist_* → counters)
	void wireStreams();

	// Connect incoming data sources (ships as sink)
	void connectJSON(Connection<JSON::JSON> &c) { c.Connect((StreamIn<JSON::JSON> *)&ships); }
	void connectGPS(Connection<AIS::GPS> &c) { c.Connect((StreamIn<AIS::GPS> *)&ships); }

	// Connect outgoing sinks (ships as source)
	template <typename T>
	void connectSink(T &sink) { ships >> sink; }

	// JSON output
	std::string toHistoryJSON();
	std::string toCountersJSON();

	// Ship data queries
	int getCount() { return ships.getCount(); }
	int getMaxCount() { return ships.getMaxCount(); }
	float getMsgRate() { return hist_second.getAverage(); }

	std::string getShipsJSON(bool full = false) { return ships.getJSON(full); }
	std::string getShipsJSONcompact() { return ships.getJSONcompact(); }
	std::string getBinaryMessagesJSON() { return ships.getBinaryMessagesJSON(); }
	std::string getKML() { return ships.getKML(); }
	std::string getGeoJSON() { return ships.getGeoJSON(); }
	std::string getAllPathJSON() { return ships.getAllPathJSON(); }
	std::string getAllPathJSONSince(std::time_t since) { return ships.getAllPathJSONSince(since); }
	std::string getAllPathGeoJSON() { return ships.getAllPathGeoJSON(); }
	std::string getPathJSON(uint32_t mmsi) { return ships.getPathJSON(mmsi); }
	std::string getPathGeoJSON(uint32_t mmsi) { return ships.getPathGeoJSON(mmsi); }
	std::string getMessage(uint32_t mmsi) { return ships.getMessage(mmsi); }
	std::string getShipJSON(uint32_t mmsi) { return ships.getShipJSON(mmsi); }
};

// Manages JS/CSS plugin content and frontend configuration variables.
class PluginManager
{
	std::string params;
	std::string plugin_code;
	std::string plugin_preamble;
	std::string stylesheets;
	std::string about = "This content can be set by the station owner";
	bool aboutPresent = false;

public:
	PluginManager();

	void setContext(const std::string &ctx);
	void setWebControl(const std::string &url);
	void setShareLoc(bool b);
	void setMsgSave(bool b);
	void setRealtime(bool b);
	void setLog(bool b);
	void setDecoder(bool b);
	void setReceivers(const std::vector<std::unique_ptr<ReceiverState>> &states);
	void addPlugin(const std::string &path);
	void addPluginCode(const std::string &code);
	void addStyle(const std::string &path);
	void addPluginDir(const std::string &dir);
	void setAbout(const std::string &path);

	bool isAboutPresent() const { return aboutPresent; }
	const std::string &getAbout() const { return about; }
	const std::string &getStylesheets() const { return stylesheets; }

	std::string render(bool communityFeed) const;
};

class WebViewer : public IO::HTTPServer, public Setting
{
	uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;

public:
	std::vector<std::string> zones;

private:

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

	std::vector<std::shared_ptr<MapTiles>> mapSources;

	PluginManager pluginManager;
	TrackingConfig tracking;

	// All receiver states. Index 0 = aggregate "All", index 1..N = per-receiver (only when N > 1).
	std::vector<std::unique_ptr<ReceiverState>> states;

	PlaneDB planes;

	SSEStreamer sse_streamer;
	WebViewerLogger logger;
	PromotheusCounter dataPrometheus;
	ByteCounter raw_counter;

	std::time_t time_start;
	std::string station = "\"\"", station_link = "\"\"";
	std::string backup_filename = "";
	std::string os, hardware;

	std::mutex m;
	std::condition_variable cv;
	std::thread backup_thread;

	void BackupService();

	bool Load();
	bool Save();
	void Clear();

	void stopThread();
	AIS::Filter filter;

	std::vector<std::string> parsePath(const std::string &url);
	bool parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y);
	void addMBTilesSource(const std::string &filepath, bool overlay);
	void addFileSystemTilesSource(const std::string &directoryPath, bool overlay);

	// NMEA decoder utility
	static std::string decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced = true);

	const std::vector<std::unique_ptr<IO::OutputMessage>> *msg_channels = nullptr;

	// Parse ?receiver=N from query string; returns 0 on missing/invalid.
	int parseReceiver(const std::string &query);
	// Parse ?since=T from query string; returns 0 on missing/invalid.
	std::time_t parseSinceParam(const std::string &query);
	// Return state at idx, clamped to states[0] on out-of-range.
	ReceiverState *getState(int idx);

public:
	WebViewer();

	~WebViewer()
	{
		if (showlog)
			logger.Stop();

		stopThread();
	}

	bool &active() { return run; }
	void connect(const std::vector<std::unique_ptr<Receiver>> &receivers);
	void connect(AIS::Model &model, Connection<JSON::JSON> &json, Device::Device &device);
	void start();
	void close();
	void Reset();

	void setOutputChannels(const std::vector<std::unique_ptr<IO::OutputMessage>> &msg)
	{
		msg_channels = &msg;
	}

	bool isPortSet() { return port_set; }
	// HTTP callbacks
	void Request(IO::TCPServerConnection &c, const std::string &r, bool gzip);

	Setting &Set(std::string option, std::string arg);
	std::string Get() { return ""; }
};
