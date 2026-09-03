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
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "AIS-catcher.h"

#include "AIS.h"
#include "DB.h"
#include "History.h"
#include "Message.h"
#include "Statistics.h"
#include "Writer.h"

#include "Device/Device.h"

// Tracking/DB config accumulated during Set(), applied to all ReceiverTrackers in attachEngine().
struct TrackingConfig
{
	// keep every member resettable by assigning a default-constructed copy
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_gps = true;
	uint32_t own_mmsi = 0;
	int time_history = 30 * 60;
	// oldest track and replay data served, 0 is unlimited
	int track_time = 3600;
	bool expire_fields = false;
	// 0 means "not configured": LONG_RANGE_CUTOFF_DEFAULT is used instead
	int cutoff = 0;
	// kilobytes of track storage; 0 keeps the default
	int track_memory = 0;
	// ship table size; 0 keeps the default (4096, or 32 times that in server mode)
	int max_ships = 0;
};

// Bundles all per-receiver (or aggregate) state: ship DB, counters, history.
// states[0] is always "All" (aggregate). states[1..N] are per receiver/model pair when there is more than one.
struct ReceiverTracker
{
private:
	DB ships;
	Counter counter, counter_session;
	History<60, 1> hist_second;
	History<60, 60> hist_minute;
	History<24, 3600> hist_hour;
	History<90, 86400> hist_day;

	void clearHistories();

public:
	std::string label;
	// Device::getIdentity() plus the model, so attachEngine() can hand a tracker back to
	// the same input after a restart. Never shown: label is what the user sees.
	std::string key;
	// Parallel, one entry per device; model_name lists that device's engines, newline-separated.
	std::vector<std::string> product, vendor, serial, model_name, sample_rate, device_label;

	ReceiverTracker(const std::string &label = "") : label(label) {}

	// Device metadata
	void setDevice(Device::Device *device);
	void appendDevice(Device::Device *device);
	void appendModel(const std::string &name, bool same_device = false);

	// Config
	void applyConfig(const TrackingConfig &cfg, const AIS::Filter &f);
	void setStationPosition(float lat, float lon, bool use_gps) { ships.setConfigPosition(lat, lon, use_gps); }

	// Lifecycle
	void setup();
	void clear();
	void reset();
	bool save(std::ofstream &f, bool include_ships = true);
	bool load(std::ifstream &f);

	// Wire internal streams (ships → hist_* → counters)
	void wireStreams();
	// Drop every sink, so wireStreams() can be run again without duplicating.
	// Only safe while the engine is stopped and nothing is delivering.
	void clearSinks() { ships.out.clear(); }

	// Connect incoming data sources (ships as sink)
	void connectJSON(Connection<JSON::JSON> &c) { c.Connect((StreamIn<JSON::JSON> *)&ships); }
	void connectGPS(Connection<AIS::GPS> &c) { c.Connect((StreamIn<AIS::GPS> *)&ships); }

	// Connect outgoing sinks (ships as source)
	template <typename T>
	void connectSink(T &sink) { ships >> sink; }

	DB &database() { return ships; }

	// JSON output
	void writeHistoryJSON(JSON::Writer &w);
	void writeCountersJSON(JSON::Writer &w);
	std::string toHistoryJSON();

	// Ship data queries
	void tick(std::time_t now) { ships.tick(now); }

	int getCount() { return ships.getCount(); }
	int getMaxCount() { return ships.getMaxCount(); }
	float getMsgRate() { return hist_second.getAverage(); }

	std::string getShipsJSON(bool full = false) { return ships.getJSON(full); }
	std::string getShipsJSONcompact(std::time_t since = 0) { return ships.getJSONcompact(false, since); }
	std::string getBinaryMessagesJSON(std::time_t since = 0, uint64_t marker = 0, uint32_t owner = 0) { return ships.getBinaryMessagesJSON(since, marker, owner); }
	std::string getMapObjectsJSON(uint64_t since = 0) { return ships.getMapObjectsJSON(since); }
	std::string getKML() { return ships.getKML(); }
	std::string getGeoJSON() { return ships.getGeoJSON(); }
	std::string getAllPathJSON() { return ships.getAllPathJSON(); }
	std::string getAllPathJSONSince(std::time_t since) { return ships.getAllPathJSONSince(since); }
	std::string getReplayInfoJSON(std::time_t block) { return ships.getReplayInfoJSON(block); }
	std::string getReplayShipsJSON(std::time_t since, std::time_t lookback) { return ships.getReplayShipsJSON(since, lookback); }
	std::string getReplayJSON(std::time_t since, std::time_t until, std::time_t lookback) { return ships.getReplayJSON(since, until, lookback); }
	std::string getAllPathGeoJSON() { return ships.getAllPathGeoJSON(); }
	std::string getPathJSON(uint32_t mmsi) { return ships.getPathJSON(mmsi); }
	std::string getPathGeoJSON(uint32_t mmsi) { return ships.getPathGeoJSON(mmsi); }
	std::string getMessage(uint32_t mmsi) { return ships.getMessage(mmsi); }
	std::string getShipJSON(uint32_t mmsi) { return ships.getShipJSON(mmsi); }
	std::string getChangesJSON(uint32_t mmsi) { return ships.getChangesJSON(mmsi); }
	std::string getRecentChangesJSON(uint32_t since, std::size_t max) { return ships.getRecentChangesJSON(since, max); }
};

// How a device names itself. attachEngine() needs this before it has a tracker
// to put it in, because the name is what identifies a tracker across runs.
std::string deviceLabel(Device::Device *device);
