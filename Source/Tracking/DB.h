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
#include <memory>
#include <mutex>
#include <vector>

#include "AIS.h"
#include "JSONAIS.h"

#include "Keys.h"
#include "JSON.h"
#include "Writer.h"


// define CHECK_DB_INTEGRITY to validate ship and path structures once a minute

#include "Ships.h"
#include "SlotTable.h"
#include "StationRegistry.h"
#include "PathStore.h"
#include "BinaryStore.h"
#include "StaticHistory.h"
#include "EventRing.h"

class DB : public StreamIn<JSON::JSON>,
		   public StreamIn<AIS::GPS>,
		   public StreamIn<AIS::Control>,
		   public StreamOut<JSON::JSON>
{

	JSON::Serializer builder{JSON_DICT_FULL};

	std::string content;
	float station_lat = LAT_UNDEFINED, station_lon = LON_UNDEFINED;
	int time_history = 30 * 60;
	// oldest track and replay data served, 0 is unlimited
	int track_time = 3600;
	// newest last_signal among recycled ships: older scenes are missing vessels
	std::time_t evict_horizon = 0;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_gps = true;
	bool gps_position = false;
	uint32_t own_mmsi = 0;

	int nbuckets = 8209;
	// 0 = unset, resolved at setup: the server-mode default leaves room for the anchor slots
	int track_memory_kb = 0;
	// 0 = default sizing
	int max_ships = 0;

	bool expire_fields = false;
	std::time_t last_sweep = 0;
	uint16_t quality_mask = 0;
	uint64_t copies_dropped = 0;

	static const int SHIP_NIL = SlotTable<Ship, uint32_t>::NIL;

	SlotTable<Ship, uint32_t> ships;

public:
	// Locked reads for code that lives outside the DB: one ship by MMSI, or all of them.
	template <typename F>
	bool withShip(uint32_t mmsi, F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		int ptr = ships.find(mmsi);
		if (ptr == SHIP_NIL)
			return false;
		f(ships[ptr], ptr);
		return true;
	}

	// A batch of ships under one lock; keep f short - copy fields out, work later.
	template <typename F>
	void withShips(const uint32_t *mmsi, size_t n, F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for (size_t i = 0; i < n; i++)
		{
			int ptr = ships.find(mmsi[i]);
			f(i, ptr == SHIP_NIL ? nullptr : &ships[ptr]);
		}
	}
	// Locked write for an operator correction: the record under `mmsi`, mutable.
	template <typename F>
	bool withShipMutable(uint32_t mmsi, F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		int ptr = ships.find(mmsi);
		if (ptr == SHIP_NIL)
			return false;
		f(ships[ptr]);
		return true;
	}
	// Drops the record; its slot is the next recycled. Path and history untouched.
	bool deleteShip(uint32_t mmsi)
	{
		std::lock_guard<std::mutex> lock(mtx);
		int ptr = ships.find(mmsi);
		if (ptr == SHIP_NIL)
			return false;
		ships[ptr].reset();
		return ships.remove(mmsi);
	}
	// most recently heard first; f returns false to stop the walk
	template <typename F>
	void forEachShip(F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		ships.forEach([&](int ptr) { return f(ships[ptr], ptr); });
	}
	// The ships heard since `since`, newest first, under the table's lock. The
	// window is the caller's own - no time_history clamp - and outside callers
	// must come through here: the walk follows LRU links the decode path relinks.
	template <typename F>
	void forEachRecent(std::time_t now, std::time_t since, F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		forEachRecentUnlocked(now, true, since, f);
	}
	int capacity() const { return ships.capacity(); }
	// Quality bits that mark a message as a copy: delivered, but it does not
	// move a ship, count, or add a path point. Zero (default) takes everything.
	void setQualityMask(uint16_t m) { quality_mask = m; }
	uint16_t getQualityMask() const { return quality_mask; }
	// messages the mask turned away
	uint64_t getCopiesDropped() const { return copies_dropped; }
	// Puts a record in the table under its MMSI, replacing what is there; for
	// seeding from another source. The region follows from the position.
	void putShip(const Ship &s);

private:
	PathStore paths;
	StaticHistory changes;

	std::mutex mtx;

	void updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &ship, bool allowApproximate, bool &positionUpdated, bool &staticUpdated);

	bool updateShip(const JSON::JSON &, TAG &, Ship &);
	void addToPath(int ptr);
	int claimShip(uint32_t mmsi);

	// the two scope rules fold into one cutoff; 0 passes everything. Caller holds mtx.
	template <typename F>
	void forEachRecentUnlocked(std::time_t now, bool full, std::time_t since, F f)
	{
		std::time_t cutoff = full ? since : MAX(since, now - time_history);
		ships.forEach([&](int ptr) {
			const Ship &ship = ships[ptr];
			if (ship.last_signal < cutoff)
				return false;

			f(ptr, ship, (long int)now - (long int)ship.last_signal);
			return true;
		});
	}


	void writeSinglePathJSONCompact(int ptr, JSON::Writer &w, std::time_t since = 0, std::time_t until = 0);
	void writeSinglePathGeoJSON(int ptr, JSON::Writer &w, std::time_t floor);

	// Oldest time any path data is served: the track_time cap, never reaching
	// past the eviction horizon into scenes that are missing recycled vessels.
	std::time_t pathFloor(std::time_t now) const
	{
		std::time_t cutoff = track_time > 0 && now > track_time ? now - track_time : 0;
		return MAX(cutoff, evict_horizon);
	}

	// Shared scaffolding for the replay endpoints: eligibility reaches back by
	// `lookback` past the window start, so a vessel silent since before the
	// window still shows for as long as the viewer keeps it on the map, and
	// `emit` writes the per-ship value. The window start is clamped here, under
	// the lock that guards the horizon it is clamped to, and handed to `emit`;
	// `until` is the window end, 0 for an unbounded one.
	template <typename F>
	std::string getReplayObjectJSON(std::time_t since, std::time_t lookback, std::time_t until, F emit)
	{
		std::lock_guard<std::mutex> lock(mtx);

		const std::time_t now = time(nullptr);
		const std::time_t floor = pathFloor(now);

		content.clear();
		{
			JSON::Writer w(content, 65536);
			w.beginObject();

			// a window that ends before the cutoff serves nothing
			if (until <= 0 || until >= floor)
			{
				since = MAX(since, floor);
				const std::time_t from = since > lookback ? since - lookback : 0;

				forEachRecentUnlocked(now, true, from, [&](int ptr, const Ship &ship, long int) {
					if (paths.hasSince(ptr, from))
						emit(w, ptr, ship, since);
				});
			}
			w.endObject().raw("\n\n");
		}
		return content;
	}

	AIS::Filter filter;

	BinaryStore binary;
	StationRegistry stations;
	EventRing events;
#ifdef CHECK_DB_INTEGRITY
	void checkIntegrity();
	std::time_t last_check = 0;
#endif

public:
	void setup();
	void tick(std::time_t now);
	void setTimeHistory(int t) { time_history = t; }
	void setTrackTime(int t) { track_time = t; }
	void setExpireFields(bool b) { expire_fields = b; }
	void setTrackMemory(int kb) { if (kb > 0) track_memory_kb = kb; }
	void setMaxShips(int n) { max_ships = n; }
	void setShareLatLon(bool b) { latlon_share = b; }
	bool getShareLatLon() { return latlon_share; }

	void setLatLon(float lat, float lon)
	{
		station_lat = lat;
		station_lon = lon;
		gps_position = false;
	}
	void setConfigPosition(float lat, float lon, bool gps)
	{
		use_gps = gps;
		if (gps && gps_position)
			return;
		station_lat = lat;
		station_lon = lon;
		gps_position = false;
	}
	void setLat(float lat) { station_lat = lat; gps_position = false; }
	void setLon(float lon) { station_lon = lon; gps_position = false; }

	float getLat() { return station_lat; }
	float getLon() { return station_lon; }

	void setOwnMMSI(uint32_t mmsi) { own_mmsi = mmsi; }

	using StreamIn<JSON::JSON>::Receive;
	using StreamIn<AIS::GPS>::Receive;
	using StreamIn<AIS::Control>::Receive;

	void Receive(const AIS::Control *data, int len, TAG &)
	{
		std::lock_guard<std::mutex> lock(mtx);
		for (int i = 0; i < len; i++)
			if (data[i].topic == AIS::KEY_STATION && data[i].payload)
				stations.apply(*data[i].payload, binary.sequence());
	}

	void Receive(const JSON::JSON *data, int len, TAG &tag);
	void Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (use_gps)
		{
			std::lock_guard<std::mutex> lock(mtx);
			station_lat = data[0].getLat();
			station_lon = data[0].getLon();
			gps_position = true;
		}
	}

	std::string getShipJSON(int mmsi);
	std::string getChangesJSON(int mmsi);
	std::string getRecentChangesJSON(uint32_t since, std::size_t max);
	void logTextChange(const Ship &ship, int field, const char *old_value, const std::string &value);
	void note(const Ship &ship, EventRing::Kind kind, EventRing::Level level, std::time_t now, const std::string &text,
			  const std::string &label = std::string(), uint32_t to = 0, const std::string &was = std::string());
	void noteSafety(Ship &ship, const JSON::JSON &data);
	void noteDestination(Ship &ship, const std::string &v);
	void noteStatus(Ship &ship, int status);
	std::string getJSON(bool full = false);
	std::string getJSONcompact(bool full = false, std::time_t since = 0);
	std::string getPathJSON(uint32_t);
	std::string getAllPathJSON();
	std::string getAllPathJSONSince(std::time_t since);
	std::string getReplayInfoJSON(std::time_t block);
	std::string getReplayShipsJSON(std::time_t since, std::time_t lookback);
	std::string getReplayJSON(std::time_t since, std::time_t until, std::time_t lookback);
	std::string getPathGeoJSON(uint32_t);
	std::string getAllPathGeoJSON();
	std::string getMessage(uint32_t);
	std::string getKML();
	std::string getGeoJSON();

	int getCount() { return ships.size(); }
	int getMaxCount() { return ships.capacity(); }

	void setServerMode(bool b) { server_mode = b; }
	void setMsgSave(bool b) { msg_save = b; }
	void setOptionKey(AIS::Keys key, const std::string &arg) { filter.SetOptionKey(key, arg); }
	void setFilter(const AIS::Filter &f) { filter = f; }

	std::string getBinaryMessagesJSON(std::time_t since = 0, uint64_t marker = 0, uint32_t owner = 0);
	std::string getMapObjectsJSON(uint64_t since = 0);
	// what stands behind an object key: a marker's members, or a station's record
	std::string getObjectJSON(const std::string &key);
	std::string getEventsJSON(uint64_t since, int level);
	// the ship row's packed badge, for a caller composing its own ship record
	uint16_t getBinaryBadge(uint32_t mmsi)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return binary.badge(mmsi, time(nullptr));
	}
	// the same from inside a withShip/forEach callback, where the lock is already held
	uint16_t binaryBadgeHeld(uint32_t mmsi, std::time_t now) const { return binary.badge(mmsi, now); }
	// the station riding a vessel, for callers already inside the locked walk
	int stationHeld(uint32_t mmsi) const { return stations.idFor(mmsi); }
	int getStation(uint32_t mmsi)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return stations.idFor(mmsi);
	}
	// the map markers under the lock, for a caller cutting its own tiles; keep f short
	template <typename F>
	void withMarkers(std::time_t now, F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		binary.forEachMarker(now, f);
	}
	int getBinaryTTL() const { return binary.ttl; }
	// every station on record; `riding` says its vessel is on the map, where the
	// ship row carries it and no object of its own is due
	template <typename F>
	void withStations(F f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		stations.forEach([&](const StationRegistry::Station &s) {
			int ptr = s.mmsi ? ships.find(s.mmsi) : SHIP_NIL;
			bool riding = ptr != SHIP_NIL && isValidCoord(ships[ptr].lat, ships[ptr].lon);
			f(s, riding);
		});
	}

	// Persistence functions for ship database
	bool Save(std::ofstream &file);
	bool Load(std::ifstream &file);

private:
	static const int _DB_MAGIC = 0x41495346;
	static const int _DB_VERSION = 1;
};
