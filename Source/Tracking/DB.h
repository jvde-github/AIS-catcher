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

// validate ship and path structures once a minute
#define CHECK_DB_INTEGRITY

#include "Ships.h"
#include "SlotTable.h"
#include "PathStore.h"

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
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	int TIME_HISTORY = 30 * 60;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_GPS = true;
	bool gps_position = false;
	uint32_t own_mmsi = 0;

	int HASH_SIZE = 8209;
	// 0 = unset, resolved at setup: the server-mode default leaves room for the anchor slots
	int track_memory_kb = 0;

	bool expire_fields = false;
	std::time_t last_sweep = 0;

	static const int SHIP_NIL = SlotTable<Ship, uint32_t>::NIL;

	SlotTable<Ship, uint32_t> ships;
	PathStore paths;

	std::mutex mtx;

	struct PathPt
	{
		float lat, lon;
		uint32_t time, end;
	};
	std::vector<PathPt> path_scratch;

	bool updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &v, bool allowApproximate, bool &staticUpdated);

	bool updateShip(const JSON::JSON &, TAG &, Ship &);
	void addToPath(int ptr);
	int claimShip(uint32_t mmsi);

	template <typename F>
	void forEachRecent(std::time_t tm, bool full, std::time_t since, F f)
	{
		ships.forEach([&](int ptr) {
			const Ship &ship = ships[ptr];
			long int delta_time = (long int)tm - (long int)ship.last_signal;
			if (!full && delta_time > TIME_HISTORY)
				return false;
			if (since > 0 && ship.last_signal < since)
				return false;
			f(ptr, ship, delta_time);
			return true;
		});
	}

	void writeSinglePathJSONCompact(int idx, JSON::Writer &w, std::time_t since = 0);
	void writeSinglePathGeoJSON(int idx, JSON::Writer &w);

	AIS::Filter filter;

	static const int MAX_BINARY_MESSAGES = 10;
	BinaryMessage binaryMessages[MAX_BINARY_MESSAGES];
	int binaryMsgIndex = 0;

	void processBinaryMessage(const JSON::JSON &data);
#ifdef CHECK_DB_INTEGRITY
	void checkIntegrity();
	std::time_t last_check = 0;
#endif

public:
	void setup();
	void tick(std::time_t now);
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void setExpireFields(bool b) { expire_fields = b; }
	void setTrackMemory(int kb) { if (kb > 0) track_memory_kb = kb; }
	void setShareLatLon(bool b) { latlon_share = b; }
	bool getShareLatLon() { return latlon_share; }

	void setLatLon(float lat, float lon)
	{
		this->lat = lat;
		this->lon = lon;
		gps_position = false;
	}
	void setConfigPosition(float lat, float lon, bool use_gps)
	{
		use_GPS = use_gps;
		if (use_gps && gps_position)
			return;
		this->lat = lat;
		this->lon = lon;
		gps_position = false;
	}
	void setLat(float lat) { this->lat = lat; gps_position = false; }
	void setLon(float lon) { this->lon = lon; gps_position = false; }

	float getLat() { return lat; }
	float getLon() { return lon; }

	void setOwnMMSI(uint32_t mmsi) { own_mmsi = mmsi; }

	using StreamIn<JSON::JSON>::Receive;
	using StreamIn<AIS::GPS>::Receive;

	void Receive(const JSON::JSON *data, int len, TAG &tag);
	void Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (use_GPS)
		{
			std::lock_guard<std::mutex> lock(mtx);
			lat = data[0].getLat();
			lon = data[0].getLon();
			gps_position = true;
		}
	}

	std::string getShipJSON(int mmsi);
	std::string getJSON(bool full = false);
	std::string getJSONcompact(bool full = false, std::time_t since = 0);
	std::string getPathJSON(uint32_t);
	std::string getAllPathJSON();
	std::string getAllPathJSONSince(std::time_t since);
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
