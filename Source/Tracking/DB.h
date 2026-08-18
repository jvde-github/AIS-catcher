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
#include "PathStore.h"
#include "StaticHistory.h"

class DB : public StreamIn<JSON::JSON>,
		   public StreamIn<AIS::GPS>,
		   public StreamOut<JSON::JSON>
{
	struct BinaryMessage
	{
		std::string json;
		int type;
		int dac;
		int fi;
		FLOAT32 lat, lon;
		time_t timestamp = 0;
		bool used;

		BinaryMessage() { Clear(); }

		void Clear()
		{
			used = false;
			type = dac = fi = -1;
			lat = LAT_UNDEFINED;
			lon = LON_UNDEFINED;
		};
	};

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

	bool expire_fields = false;
	std::time_t last_sweep = 0;

	static const int SHIP_NIL = SlotTable<Ship, uint32_t>::NIL;

	SlotTable<Ship, uint32_t> ships;
	PathStore paths;
	StaticHistory changes;

	std::mutex mtx;

	void updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &ship, bool allowApproximate, bool &positionUpdated, bool &staticUpdated);

	bool updateShip(const JSON::JSON &, TAG &, Ship &);
	void addToPath(int ptr);
	int claimShip(uint32_t mmsi);

	// the two scope rules fold into one cutoff: 0 passes everything, since
	// last_signal is never negative
	template <typename F>
	void forEachRecent(std::time_t now, bool full, std::time_t since, F f)
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

				forEachRecent(now, true, from, [&](int ptr, const Ship &ship, long int) {
					if (paths.hasSince(ptr, from))
						emit(w, ptr, ship, since);
				});
			}
			w.endObject().raw("\n\n");
		}
		return content;
	}

	AIS::Filter filter;

	static const int MAX_BINARY_MESSAGES = 10;
	BinaryMessage binary_messages[MAX_BINARY_MESSAGES];
	int binary_msg_index = 0;

	void processBinaryMessage(const JSON::JSON &data);
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
	void logTextChange(const Ship &ship, int field, const char *old_value, const std::string &value);
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

	std::string getBinaryMessagesJSON(std::time_t since = 0);

	// Persistence functions for ship database
	bool Save(std::ofstream &file);
	bool Load(std::ifstream &file);

private:
	static const int _DB_MAGIC = 0x41495346;
	static const int _DB_VERSION = 1;
};
