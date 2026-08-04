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
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <memory>
#include <fstream>

#include "AIS-catcher.h"

#include "Common.h"
#include "JSONAIS.h"
#include "AIS.h"
#include "Prometheus.h"
#include "HTTPServer.h"
#include "MsgOut.h"
#include "PlaneDB.h"
#include "ReceiverTracker.h"
#include "FrontendConfig.h"
#include "BackupManager.h"
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

// Forwards log messages to the viewer's SSE clients. The Logger singleton
// outlives this object, so the listener must be removed in the destructor or it
// keeps invoking a callback into freed memory.
class WebViewerLogger
{
	IO::HTTPServer *server = nullptr;
	int cb = -1;

public:
	~WebViewerLogger() { Stop(); }

	void setSSE(IO::HTTPServer *s) { server = s; }

	void Start()
	{
		Stop();
		cb = Logger::getInstance().addLogListener([this](const LogMessage &msg)
												  { if (server) server->sendSSE(3, "log", msg.toJSON()); });
	}

	void Stop()
	{
		if (cb != -1)
		{
			Logger::getInstance().removeLogListener(cb);
			cb = -1;
		}
	}
};

class WebViewer : public IO::HTTPServer, public Setting
{
public:
	// Every plain value SetKey() writes lives here, so resetSettings() can put
	// the viewer back to its defaults by assigning a fresh instance. Anything
	// that is not a setting (bound socket, ship database, serving state) must
	// stay outside, or a reconfigure would throw away the running viewer.
	struct Settings
	{
		uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;
		std::vector<std::string> zones;

		int firstport = 0;
		int lastport = 0;
		bool port_set = false;

		bool use_zlib = true;
		bool realtime = false;
		bool showlog = false;
		bool showdecoder = false;
		bool KML = false;
		bool GeoJSON = false;
		bool supportPrometheus = false;
		bool stats_on_close = false;

		std::string station, station_link;

		TrackingConfig tracking;
	};

private:
	Settings settings;

	int bound_port = 0;
	bool is_active = false;
	bool serving = false;
	bool initialized = false;
	// the statistics file last read, so a change of name triggers a fresh read
	std::string stats_file;

	std::vector<std::shared_ptr<MapTiles>> mapSources;

	PluginStore plugins;
	FrontendConfig frontend;

	void applyStationPosition();

	// All receiver states. Index 0 = aggregate "All", index 1..N = per receiver/model pair (only when there is more than one).
	std::vector<std::unique_ptr<ReceiverTracker>> states;
	mutable std::recursive_mutex state_mtx;

	PlaneDB planes;

	SSEStreamer sse_streamer;
	WebViewerLogger logger;
	PrometheusCounter dataPrometheus;
	ByteCounter raw_counter;

	std::time_t time_start = 0;
	std::string os, hardware;

	BackupManager backup;

	AIS::Filter filter;

	std::string pending_product, pending_vendor, pending_serial;

	bool parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y);
	void addMBTilesSource(const std::string &filepath, bool overlay);
	void addFileSystemTilesSource(const std::string &directoryPath, bool overlay);

	// Route table
	typedef std::string (*RouteHandler)(WebViewer *, ReceiverTracker *, const std::string &);

	struct Route {
		const char *path;
		bool Settings::*flag;
		const char *content_type;
		RouteHandler handler;
		bool cors;
		// Per-request cacheability; null for routes that never are.
		bool (*cacheable)(const std::string &query);
	};

	static const Route routes[];
	static int parseMMSI(const std::string &query);

	// JSON builders for complex endpoints
	std::string buildStatJSON(ReceiverTracker *s);
	std::string buildOutputStatsJSON();
	std::string buildMultiPathJSON(ReceiverTracker *s, const std::string &query);
	void writeOutputsJSON(JSON::Writer &w);

	// NMEA decoder utility
	static std::string decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced = true);

	const std::vector<std::unique_ptr<IO::OutputMessage>> *msg_channels = nullptr;

	// Community feed output (not owned), used to report sharing status
	IO::OutputMessage *comm_feed = nullptr;

	// read from the HTTP thread while attachEngine()/detachEngine() write it
	std::atomic<bool> engine_attached{false};

	// attachEngine() in pieces. beginAttach()/endAttach() are shared with the
	// Android overload, so that path cannot drift from the receiver-list one;
	// beginAttach() hands back the previous run's trackers, ignore them to
	// start over.
	void resolveZoneMask(const std::vector<std::unique_ptr<Receiver>> &receivers);
	void wireAggregate(const std::vector<std::unique_ptr<Receiver>> &receivers, const std::string &newline);
	void attachTrackers(const std::vector<std::unique_ptr<Receiver>> &receivers, std::vector<std::unique_ptr<ReceiverTracker>> &previous, const std::string &newline);
	std::vector<std::unique_ptr<ReceiverTracker>> beginAttach();
	void endAttach();

	// Named integer from a query string; 0 when missing or malformed.
	static long long queryInt(const std::string &query, const char *name);
	// Return state at idx, clamped to states[0] on out-of-range.
	ReceiverTracker *getState(int idx);

public:
	WebViewer();

	~WebViewer()
	{
		// ~TCPServer joins the server thread only after members are destroyed
		stopThread();
	}

	void setActive(bool b) { is_active = b; }
	bool isActive() const { return is_active; }
	void setManagedMode() { frontend.setManaged(true); }
	void attachEngine(const std::vector<std::unique_ptr<Receiver>> &receivers);

	// Called only from the Android JNI glue, which lives outside this repository:
	// attachEngine(model, json, device), setDeviceDescription(), setEphemeralPort(),
	// resetStatistics(), setActive(), startServing()/stopServing()/shutdown() and
	// SetKey(). Renaming any of those breaks that build, so keep this list current.
	void attachEngine(AIS::Model &model, Connection<JSON::JSON> &json, Device::Device &device);
	void detachEngine();
	void setDeviceDescription(const std::string &product, const std::string &vendor, const std::string &serial);
	// stopServing, resetSettings, the settings themselves, then startServing: the
	// HTTP socket and the ship database stay up, so the viewer stays reachable.
	// While stopped the server answers 503 rather than half-applied settings.
	void startServing();
	void stopServing();
	// for good: unbinds, writes the statistics file and joins the server thread
	void shutdown();
	// periodic maintenance; handlers gate on the timestamp and must not block
	void tick(std::time_t now);
	// drop the accumulated statistics, keeping the configuration (Android only)
	void resetStatistics();
	// Back to defaults, then the one setting the config file does not own. Taking
	// the port here keeps it impossible to reset and forget to restore it.
	void resetSettings(int port);
	// settings that need more than a stored value: sinks, log listener, backup
	void applySettings();

	void setOutputChannels(const std::vector<std::unique_ptr<IO::OutputMessage>> &msg)
	{
		msg_channels = &msg;
	}

	void setCommFeed(IO::OutputMessage *f)
	{
		comm_feed = f;
		frontend.setSharing(f != nullptr, f && f->hasUUID());
	}

	bool isPortSet() { return settings.port_set; }
	int getBoundPort() { return bound_port; }

	// 0 leaves the port unset, so startServing() reports it rather than binding at random
	void setPort(int port)
	{
		if (!port)
			return;

		settings.port_set = true;
		settings.firstport = settings.lastport = port;
	}

	void setEphemeralPort()
	{
		settings.port_set = true;
		settings.firstport = settings.lastport = 0;
	}
	// HTTP callbacks
	void Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool gzip) override;

	Setting &SetKey(AIS::Keys key, const std::string &arg) override;
	std::string Get() override { return ""; }

};
