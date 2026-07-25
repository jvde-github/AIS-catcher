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

#include "AIS.h"
#include "JSONAIS.h"

#include "Keys.h"
#include "JSON.h"
#include "Writer.h"
#include "Ships.h"

// Track history storage — tiered chunk lists with age-triggered whole-chunk
// consolidation; see TRACK_STORAGE_DESIGN.md for the full design.

typedef uint32_t PtRef; // (chunk << PATH_CHUNK_BITS) | slot

const PtRef PATH_NONE = 0xFFFFFFFF;
const int PATH_CHUNK_BITS = 12;
const int PATH_CHUNK_POINTS = 1 << PATH_CHUNK_BITS;
const int PATH_LATLON_SCALE = 600000; // AIS wire unit: 1/600000 deg

// packed sog_cog_hdg: bits 0-9 sog (0.1 kn, 1023 n/a), 10-21 cog (0.1 deg,
// 3600 n/a), 22-30 heading (deg, 511 n/a), bit 31 spare
const uint32_t PATH_SOG_NA = 1023;
const uint32_t PATH_COG_NA = 3600;
const uint32_t PATH_HDG_NA = 511;

struct PathPoint // 20 bytes, AIS wire units
{
	int32_t lat, lon;
	uint32_t ts;
	uint32_t sog_cog_hdg;
	PtRef next_older;
};

struct PathChunk
{
	struct Fixup
	{
		uint32_t ship;
		uint16_t ship_gen;
		uint16_t chunk_gen; // generation of pred's chunk at record time
		PtRef pred;			// point in a newer chunk whose next_older crosses into this chunk
	};
	struct Touched
	{
		uint32_t ship;
		uint16_t ship_gen;
	};

	std::vector<PathPoint> pt;
	std::vector<Fixup> fixups;
	std::vector<Touched> touched;
	uint32_t latest_ts = 0;
	int prev = -1, next = -1; // tier chunk list: prev = newer, next = older
	int tier = -1;
	int bump = 0;
	bool sealed = false; // bump chunk closed early (seal-by-age); no longer receives
	uint16_t gen = 0;	 // bumped when the chunk is emptied; invalidates fix-ups into it

	PathChunk() : pt(PATH_CHUNK_POINTS) {}
};

struct PathTier
{
	int spacing = 0;   // seconds between stored points (0 = raw)
	int exit_age = -1; // age in seconds at which chunks consolidate into the next tier (-1 = last tier)
	int head = -1;	   // newest chunk (bump target)
	int tail = -1;	   // oldest chunk (consolidation candidate)
	int nchunks = 0;
};

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

class DB : public StreamIn<JSON::JSON>,
		   public StreamIn<AIS::GPS>,
		   public StreamOut<JSON::JSON>
{

	JSON::Serializer builder{JSON_DICT_FULL};

	int first = 0, last = 0, count = 0;
	std::string content, delim;
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	int TIME_HISTORY = 30 * 60;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_GPS = true;
	bool gps_position = false;
	uint32_t own_mmsi = 0;

	int Nships = 4096;
	int HASH_SIZE = 8209;

	// track history storage
	int track_mem_mb = 8; // TRACK_MEM; server_mode scales in setup()
	std::string track_thin = "60:5,120:10,1440:30";
	std::vector<std::unique_ptr<PathChunk>> chunks;
	std::vector<int> empty_chunks;
	std::vector<PathTier> tiers;
	std::vector<PtRef> run_buf; // scratch for consolidateChunk
	int max_chunks = 0;
	bool consolidating = false;

	struct HashBucket
	{
		int first = -1;
		int last = -1;
	};

	std::vector<Ship> ships;
	std::vector<HashBucket> hash_table;

	bool isValidCoord(float lat, float lon);

	static float deg2rad(float deg) { return deg * PI / 180.0f; }
	static int rad2deg(float rad) { return (int)(360 + rad * 180 / PI) % 360; }
	int Hash(uint32_t mmsi) { return mmsi % HASH_SIZE; }

	int findShip(uint32_t mmsi);
	int createShip(int hash);
	void moveShipToFront(int);
	bool updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &v, bool allowApproximate, bool &staticUpdated);

	bool updateShip(const JSON::JSON &, TAG &, Ship &);
	void addToPath(int ptr);

	static void getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float &distance, int &bearing);

	void getShipJSON(const Ship &ship, JSON::Writer &w, long int now);
	void writeSinglePathJSON(int idx, JSON::Writer &w);
	void writeSinglePathJSONCompact(int idx, JSON::Writer &w);
	bool writeSinglePathJSONCompactSince(int idx, std::time_t since, JSON::Writer &w);
	bool hasPathPointsSince(int idx, std::time_t since);
	void writeSinglePathGeoJSON(int idx, JSON::Writer &w);

	// track history storage internals
	PathPoint &deref(PtRef h) { return chunks[h >> PATH_CHUNK_BITS]->pt[h & (PATH_CHUNK_POINTS - 1)]; }
	const PathPoint &deref(PtRef h) const { return chunks[h >> PATH_CHUNK_BITS]->pt[h & (PATH_CHUNK_POINTS - 1)]; }
	int chunkOf(PtRef h) const { return (int)(h >> PATH_CHUNK_BITS); }
	static uint32_t packSogCogHdg(float speed, float cog, int heading);
	PtRef tierAppend(int tier, const PathPoint &p);
	int acquireChunk(int tier);
	int newChunk();
	int makeSpace();
	void releaseChunk(int cidx);
	void ageCheck(std::time_t now);
	void consolidateChunk(int cidx, bool promote);
	void consolidateRun(int cidx, int dst, int ship_idx, PtRef pred);
	void parseTrackThin();
	void addCrossingBookkeeping(int ship_idx, PtRef p); // fix-up/touched entries for a new head link

	AIS::Filter filter;

	static const int MAX_BINARY_MESSAGES = 10;
	BinaryMessage binaryMessages[MAX_BINARY_MESSAGES];
	int binaryMsgIndex = 0;

	void processBinaryMessage(const JSON::JSON &data, Ship &ship, bool &position_updated);
#ifdef CHECK_DB_INTEGRITY
	void checkIntegrity();
	int update_counter = 0;
#endif

public:
	DB() : builder(JSON_DICT_FULL) {}

	std::mutex mtx;

	void setup();
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void setTrackMem(int mb) { track_mem_mb = mb; }
	void setTrackThin(const std::string &s) { track_thin = s; }
	void setShareLatLon(bool b) { latlon_share = b; }
	bool getShareLatLon() { return latlon_share; }

	bool setUseGPS(bool b) { return use_GPS = b; }
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

	int getCount() { return count; }
	int getMaxCount() { return Nships; }

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
