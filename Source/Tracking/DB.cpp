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

#include "AIS-catcher.h"
#include "DB.h"
#include "Parse.h"
#include "Convert.h"

#include <fstream>
#include <sstream>
#include <cmath>

//-----------------------------------
// simple ship database

void DB::setup()
{

	if (server_mode)
	{
		Nships *= 32;
		HASH_SIZE = 262147;
		if (track_mem_mb < 0)
			track_mem_mb = 128;

		Info() << "DB: internal ship database extended to " << Nships << " ships";
	}

	if (track_mem_mb < 0)
		track_mem_mb = 8;

	parseTrackThin();

	const int chunk_bytes = (int)(sizeof(PathPoint) * PATH_CHUNK_POINTS + 4096);
	max_chunks = MAX(4, (int)(((int64_t)track_mem_mb << 20) / chunk_bytes));

	std::string tier_desc;
	for (size_t k = 1; k < tiers.size(); k++)
		tier_desc += (k > 1 ? "/" : "") + std::to_string(tiers[k].spacing / 60) + "min";
	Info() << "DB: track history " << track_mem_mb << " MB (" << max_chunks << " chunks of " << PATH_CHUNK_POINTS
		   << " points), tiers: raw" << (tier_desc.empty() ? "" : "/" + tier_desc);

	ships.resize(Nships);
	hash_table.resize(HASH_SIZE);

	first = Nships - 1;
	last = 0;
	count = 0;

	// set up linked list
	for (int i = 0; i < Nships; i++)
	{
		ships[i].incoming.next = i - 1;
		ships[i].incoming.prev = i + 1;

		ships[i].hash.next = -1;
		ships[i].hash.prev = -1;
	}
	ships[Nships - 1].incoming.prev = -1;
}

bool DB::isValidCoord(float lat, float lon)
{
	return !(lat == 0 && lon == 0) && lat != 91 && lon != 181;
}

// https://www.movable-type.co.uk/scripts/latlong.html
void DB::getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float &distance, int &bearing)
{
	const float EarthRadius = 6371.0f;			// Earth radius in kilometers
	const float NauticalMilePerKm = 0.5399568f; // Conversion factor

	// Convert the latitudes and longitudes from degrees to radians
	lat1 = deg2rad(lat1);
	lon1 = deg2rad(lon1);
	lat2 = deg2rad(lat2);
	lon2 = deg2rad(lon2);

	// Compute the distance using the haversine formula
	float dlat = lat2 - lat1, dlon = lon2 - lon1;
	float a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
	distance = 2 * EarthRadius * NauticalMilePerKm * asin(sqrt(a));

	float y = sin(dlon) * cos(lat2);
	float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
	bearing = rad2deg(atan2(y, x));
}

// add member to get JSON in form of array with values and keys separately
std::string DB::getJSONcompact(bool full, std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::time_t tm = time(nullptr);

	content.clear();
	{
		JSON::Writer w(content, 65536);

		w.beginObject().kv("count", count).kv("time", tm).kv("timeout", TIME_HISTORY);
		if (latlon_share && isValidCoord(lat, lon))
			w.key("station").beginObject().kv("lat", lat).kv("lon", lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();

		// --- Pass 1: dynamic array ---
		w.key("dynamic").beginArray();

		int ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (!full && delta_time > TIME_HISTORY)
					break;
				if (since > 0 && ship.last_signal < since)
					break;

				w.beginArray().val(ship.mmsi);
				if (isValidCoord(ship.lat, ship.lon))
				{
					w.val(ship.lat).val(ship.lon);
					if (ship.distance != DISTANCE_UNDEFINED && ship.angle != ANGLE_UNDEFINED)
						w.val(ship.distance).val(ship.angle);
					else
						w.val_null().val_null();
				}
				else
					w.val_null().val_null().val_null().val_null();

				w.val_unless(ship.heading, HEADING_UNDEFINED)
					.val_unless(ship.cog, COG_UNDEFINED)
					.val_unless(ship.speed, SPEED_UNDEFINED)
					.val(ship.status)
					.val_unless(ship.level, LEVEL_UNDEFINED)
					.val_unless(ship.ppm, PPM_UNDEFINED)
					.val(ship.count)
					.val(ship.msg_type)
					.val(ship.last_signal)
					.val(ship.last_group)
					.val(ship.group_mask)
					.val((unsigned long long)ship.flags.getPackedValue())
					.val_unless(ship.altitude, ALT_UNDEFINED)
					.val_unless(ship.received_stations, RECEIVED_STATIONS_UNDEFINED)
					.val(ship.mmsi_type)
					.val(ship.shipclass)
					.val(ship.country_code)
					.endArray();
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endArray(); // dynamic

		// --- Pass 2: static array ---
		w.key("static").beginArray();

		ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (!full && delta_time > TIME_HISTORY)
					break;
				if (since > 0 && ship.last_signal < since)
					break;

				if (since == 0 || ship.last_static_signal >= since)
				{
					w.beginArray().val(ship.mmsi);

					if (ship.getVirtualAid())
						w.val(ship.shipname, " [V]");
					else
						w.val(ship.shipname);

					w.val(ship.callsign)
						.val(ship.destination)
						.val(ship.shiptype)
						.val_unless(ship.IMO, IMO_UNDEFINED)
						.val_unless(ship.to_bow, DIMENSION_UNDEFINED)
						.val_unless(ship.to_stern, DIMENSION_UNDEFINED)
						.val_unless(ship.to_port, DIMENSION_UNDEFINED)
						.val_unless(ship.to_starboard, DIMENSION_UNDEFINED)
						.val_unless(ship.draught, DRAUGHT_UNDEFINED)
						.val_unless((int)ship.month, ETA_MONTH_UNDEFINED)
						.val_unless((int)ship.day, ETA_DAY_UNDEFINED)
						.val_unless((int)ship.hour, ETA_HOUR_UNDEFINED)
						.val_unless((int)ship.minute, ETA_MINUTE_UNDEFINED)
						.val(ship.vin)
						.val(ship.vendorid)
						.val_unless(ship.unit_model, -1)
						.val_unless(ship.unit_serial, -1)
						.endArray();
				}
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endArray(); // static

		w.endObject();
		w.raw("\n\n");
	}
	return content;
}

void DB::getShipJSON(const Ship &ship, JSON::Writer &w, long int delta_time)
{
	w.beginObject().kv("mmsi", ship.mmsi);

	if (isValidCoord(ship.lat, ship.lon))
	{
		w.kv("lat", ship.lat).kv("lon", ship.lon);
		if (isValidCoord(lat, lon))
			w.kv("distance", ship.distance).kv("bearing", ship.angle);
		else
			w.kv_null("distance").kv_null("bearing");
	}
	else
		w.kv_null("lat").kv_null("lon").kv_null("distance").kv_null("bearing");

	w.kv_unless("level", ship.level, LEVEL_UNDEFINED)
		.kv("count", ship.count)
		.kv_unless("ppm", ship.ppm, PPM_UNDEFINED)
		.kv("group_mask", ship.group_mask)
		.kv("approx", (bool)ship.getApproximate())
		.kv_unless("heading", ship.heading, HEADING_UNDEFINED)
		.kv_unless("cog", ship.cog, COG_UNDEFINED)
		.kv_unless("speed", ship.speed, SPEED_UNDEFINED)
		.kv_unless("to_bow", ship.to_bow, DIMENSION_UNDEFINED)
		.kv_unless("to_stern", ship.to_stern, DIMENSION_UNDEFINED)
		.kv_unless("to_starboard", ship.to_starboard, DIMENSION_UNDEFINED)
		.kv_unless("to_port", ship.to_port, DIMENSION_UNDEFINED)
		.kv("shiptype", ship.shiptype)
		.kv("mmsi_type", ship.mmsi_type)
		.kv("shipclass", ship.shipclass)
		.kv("validated", ship.getValidated())
		.kv("msg_type", ship.msg_type)
		.kv("channels", ship.getChannels())
		.kv("country", ship.country_code)
		.kv("status", ship.status)
		.kv_unless("draught", ship.draught, DRAUGHT_UNDEFINED)
		.kv_unless("eta_month", (int)ship.month, ETA_MONTH_UNDEFINED)
		.kv_unless("eta_day", (int)ship.day, ETA_DAY_UNDEFINED)
		.kv_unless("eta_hour", (int)ship.hour, ETA_HOUR_UNDEFINED)
		.kv_unless("eta_minute", (int)ship.minute, ETA_MINUTE_UNDEFINED)
		.kv_unless("imo", ship.IMO, IMO_UNDEFINED)
		.kv("callsign", ship.callsign);

	if (ship.getVirtualAid())
		w.kv("shipname", ship.shipname, " [V]");
	else
		w.kv("shipname", ship.shipname);

	w.kv("destination", ship.destination)
		.kv("eni", ship.vin)
		.kv("vendorid", ship.vendorid)
		.kv_unless("model", ship.unit_model, -1)
		.kv_unless("serial", ship.unit_serial, -1)
		.kv("repeat", ship.getRepeat())
		.kv("last_signal", delta_time)
		.endObject();
}

std::string DB::getJSON(bool full)
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject().kv("count", count);
		if (latlon_share)
			w.key("station").beginObject().kv("lat", lat).kv("lon", lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();
		w.key("ships").beginArray();

		std::time_t tm = time(nullptr);
		int ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (!full && delta_time > TIME_HISTORY)
					break;

				getShipJSON(ship, w, delta_time);
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endArray().kv("error", false).endObject().raw("\n\n");
	}
	return content;
}

std::string DB::getShipJSON(int mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);

	int ptr = findShip(mmsi);
	if (ptr == -1)
		return "{}";

	const Ship &ship = ships[ptr];
	long int delta_time = (long int)time(nullptr) - (long int)ship.last_signal;

	content.clear();
	{
		JSON::Writer w(content, 1024);
		getShipJSON(ship, w, delta_time);
	}
	return content;
}

std::string DB::getKML()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>");
	int ptr = first;
	std::time_t tm = time(nullptr);

	while (ptr != -1)
	{
		const Ship &ship = ships[ptr];
		if (ship.mmsi != 0)
		{
			long int delta_time = (long int)tm - (long int)ship.last_signal;
			if (delta_time > TIME_HISTORY)
				break;
			ship.getKML(content);
		}
		ptr = ships[ptr].incoming.next;
	}
	content += "</Document></kml>";
	return content;
}

std::string DB::getGeoJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject().kv("type", "FeatureCollection").kv("time_span", TIME_HISTORY).key("features").beginArray();

		int ptr = first;
		std::time_t tm = time(nullptr);
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (delta_time > TIME_HISTORY)
					break;

				ship.getGeoJSON(w);
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endArray().endObject();
	}
	return content;
}

std::string DB::getAllPathJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject();

		std::time_t tm = time(nullptr);
		int ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (delta_time > TIME_HISTORY)
					break;

				char keybuf[16];
				int n = snprintf(keybuf, sizeof(keybuf), "%u", ship.mmsi);
				w.key(keybuf, n);
				writeSinglePathJSONCompact(ptr, w);
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endObject().raw("\n\n");
	}
	return content;
}

// Path points are emitted as [lat, lon, ts_start, ts_end] like before; the
// storage keeps one timestamp per point, so ts_end (last time at that
// position) is derived as the next-newer point's ts — for the head, the
// ship's last_signal.

void DB::writeSinglePathJSON(int idx, JSON::Writer &w)
{
	PtRef ptr = ships[idx].path_ptr == -1 ? PATH_NONE : (PtRef)ships[idx].path_ptr;
	std::time_t newer_ts = ships[idx].last_signal;

	w.beginArray();
	while (ptr != PATH_NONE)
	{
		const PathPoint &p = deref(ptr);
		w.beginArray().val(p.lat / (float)PATH_LATLON_SCALE).val(p.lon / (float)PATH_LATLON_SCALE).val((std::time_t)p.ts).val(newer_ts).endArray();
		newer_ts = (std::time_t)p.ts;
		ptr = p.next_older;
	}
	w.endArray();
}

void DB::writeSinglePathJSONCompact(int idx, JSON::Writer &w)
{
	PtRef ptr = ships[idx].path_ptr == -1 ? PATH_NONE : (PtRef)ships[idx].path_ptr;
	std::time_t newer_ts = ships[idx].last_signal;
	int cnt = 0;

	w.beginArray();
	while (ptr != PATH_NONE && cnt < 250)
	{
		const PathPoint &p = deref(ptr);
		w.beginArray().val(p.lat / (float)PATH_LATLON_SCALE).val(p.lon / (float)PATH_LATLON_SCALE).val((std::time_t)p.ts).val(newer_ts).endArray();
		cnt++;
		newer_ts = (std::time_t)p.ts;
		ptr = p.next_older;
	}
	w.endArray();
}

bool DB::writeSinglePathJSONCompactSince(int idx, std::time_t since, JSON::Writer &w)
{
	PtRef ptr = ships[idx].path_ptr == -1 ? PATH_NONE : (PtRef)ships[idx].path_ptr;
	std::time_t newer_ts = ships[idx].last_signal;
	bool any = false;

	w.beginArray();
	while (ptr != PATH_NONE)
	{
		if ((long int)newer_ts < (long int)since)
			break;

		const PathPoint &p = deref(ptr);
		w.beginArray().val(p.lat / (float)PATH_LATLON_SCALE).val(p.lon / (float)PATH_LATLON_SCALE).val((std::time_t)p.ts).val(newer_ts).endArray();
		any = true;
		newer_ts = (std::time_t)p.ts;
		ptr = p.next_older;
	}
	w.endArray();
	return any;
}

bool DB::hasPathPointsSince(int idx, std::time_t since)
{
	// The chain is reverse-chronological; the head's derived ts_end is the
	// ship's last_signal. If that is older than `since`, nothing newer exists.
	if (ships[idx].path_ptr == -1)
		return false;
	return (long int)ships[idx].last_signal >= (long int)since;
}

std::string DB::getAllPathJSONSince(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject();

		int ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0 && hasPathPointsSince(ptr, since))
			{
				char keybuf[16];
				int n = snprintf(keybuf, sizeof(keybuf), "%u", ship.mmsi);
				w.key(keybuf, n);
				writeSinglePathJSONCompactSince(ptr, since, w);
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endObject().raw("\n\n");
	}
	return content;
}

void DB::writeSinglePathGeoJSON(int idx, JSON::Writer &w)
{
	uint32_t mmsi = ships[idx].mmsi;
	PtRef path_head = ships[idx].path_ptr == -1 ? PATH_NONE : (PtRef)ships[idx].path_ptr;

	w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
	{
		PtRef ptr = path_head;
		while (ptr != PATH_NONE)
		{
			const PathPoint &p = deref(ptr);
			w.beginArray().val(p.lon / (float)PATH_LATLON_SCALE).val(p.lat / (float)PATH_LATLON_SCALE).endArray();
			ptr = p.next_older;
		}
	}
	w.endArray().endObject().key("properties").beginObject().kv("mmsi", mmsi).key("timestamps_start").beginArray();
	{
		PtRef ptr = path_head;
		while (ptr != PATH_NONE)
		{
			const PathPoint &p = deref(ptr);
			w.val((std::time_t)p.ts);
			ptr = p.next_older;
		}
	}
	w.endArray().key("timestamps_end").beginArray();
	{
		PtRef ptr = path_head;
		std::time_t newer_ts = ships[idx].last_signal;
		while (ptr != PATH_NONE)
		{
			const PathPoint &p = deref(ptr);
			w.val(newer_ts);
			newer_ts = (std::time_t)p.ts;
			ptr = p.next_older;
		}
	}
	w.endArray().endObject().endObject();
}

std::string DB::getPathJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = findShip(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (idx != -1)
			writeSinglePathJSONCompact(idx, w);
		else
			w.beginArray().endArray();
	}
	return content;
}

std::string DB::getPathGeoJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = findShip(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (idx != -1)
			writeSinglePathGeoJSON(idx, w);
		else
			w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray().endArray().endObject().key("properties").beginObject().kv("mmsi", mmsi).endObject().endObject();
	}
	return content;
}

std::string DB::getAllPathGeoJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject().kv("type", "FeatureCollection").key("features").beginArray();

		std::time_t tm = time(nullptr);
		int ptr = first;
		while (ptr != -1)
		{
			const Ship &ship = ships[ptr];
			if (ship.mmsi != 0)
			{
				long int delta_time = (long int)tm - (long int)ship.last_signal;
				if (delta_time > TIME_HISTORY)
					break;

				writeSinglePathGeoJSON(ptr, w);
			}
			ptr = ships[ptr].incoming.next;
		}
		w.endArray().endObject().raw("\n\n");
	}
	return content;
}

std::string DB::getMessage(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int ptr = findShip(mmsi);
	if (ptr == -1)
		return "";
	return ships[ptr].msg;
}

int DB::findShip(uint32_t mmsi)
{
	int hash = Hash(mmsi);
	int ptr = hash_table[hash].first;

	while (ptr != -1)
	{
		if (ships[ptr].mmsi == mmsi)
			return ptr;
		ptr = ships[ptr].hash.next;
	}
	return -1;
}

int DB::createShip(int hash_new)
{
	int ptr = last;

	// remove the old vessel from the hash table, if any
	uint32_t old_mmsi = ships[ptr].mmsi;
	if (old_mmsi != 0)
	{
		int hash_old = Hash(old_mmsi);
		int hprev = ships[ptr].hash.prev;
		int hnext = ships[ptr].hash.next;

		if (hprev != -1)
			ships[hprev].hash.next = hnext;
		else
			hash_table[hash_old].first = hnext;

		if (hnext != -1)
			ships[hnext].hash.prev = hprev;
		else
			hash_table[hash_old].last = hprev;
	}

	count = MIN(count + 1, Nships);
	ships[ptr].reset();

	// insert into new hash bucket (after reset so hash pointers are clean)
	ships[ptr].hash.next = hash_table[hash_new].first;
	ships[ptr].hash.prev = -1;

	if (hash_table[hash_new].first != -1)
		ships[hash_table[hash_new].first].hash.prev = ptr;

	hash_table[hash_new].first = ptr;
	if (hash_table[hash_new].last == -1)
		hash_table[hash_new].last = ptr;

	return ptr;
}

void DB::moveShipToFront(int ptr)
{
	if (ptr == first)
		return;

	// remove ptr out of the linked list
	if (ships[ptr].incoming.next != -1)
		ships[ships[ptr].incoming.next].incoming.prev = ships[ptr].incoming.prev;
	else
		last = ships[ptr].incoming.prev;
	ships[ships[ptr].incoming.prev].incoming.next = ships[ptr].incoming.next;

	// new ship is first in list
	ships[ptr].incoming.next = first;
	ships[ptr].incoming.prev = -1;

	ships[first].incoming.prev = ptr;
	first = ptr;
}

// ------------------------------------------------------------------
// Track history storage: tiered chunk lists, bump inserts, age-triggered
// whole-chunk consolidation. See TRACK_STORAGE_DESIGN.md.

void DB::parseTrackThin()
{
	tiers.clear();

	std::vector<std::pair<int, int>> pairs; // boundary min, spacing min
	std::string s = track_thin;
	Util::Convert::toUpper(s);

	if (!s.empty() && s != "OFF")
	{
		std::stringstream ss(s);
		std::string item;
		int prev_b = 0, prev_s = 0;
		while (std::getline(ss, item, ','))
		{
			size_t c = item.find(':');
			if (c == std::string::npos)
				throw std::runtime_error("DB: TRACK_THIN entry \"" + item + "\" is not boundary:spacing");
			int b = (int)Util::Parse::Integer(item.substr(0, c), 1, 7 * 24 * 60, "TRACK_THIN");
			int sp = (int)Util::Parse::Integer(item.substr(c + 1), 1, 24 * 60, "TRACK_THIN");
			if (b <= prev_b || sp <= prev_s)
				throw std::runtime_error("DB: TRACK_THIN boundaries and spacings must be strictly increasing");
			prev_b = b;
			prev_s = sp;
			pairs.push_back(std::pair<int, int>(b, sp));
		}
	}

	PathTier t0;
	t0.spacing = 0;
	t0.exit_age = pairs.empty() ? -1 : pairs[0].first * 60;
	tiers.push_back(t0);

	for (size_t i = 0; i < pairs.size(); i++)
	{
		PathTier t;
		t.spacing = pairs[i].second * 60;
		t.exit_age = (i + 1 < pairs.size()) ? pairs[i + 1].first * 60 : -1;
		tiers.push_back(t);
	}
}

uint32_t DB::packSogCogHdg(float speed, float cog, int heading)
{
	uint32_t s = PATH_SOG_NA, c = PATH_COG_NA, h = PATH_HDG_NA;

	if (speed >= 0 && speed <= 102.2f)
		s = (uint32_t)(speed * 10 + 0.5f);
	if (cog >= 0 && cog < 360)
		c = (uint32_t)(cog * 10 + 0.5f) % 3600;
	if (heading >= 0 && heading < 360)
		h = (uint32_t)heading;

	return s | (c << 10) | (h << 22);
}

int DB::newChunk()
{
	chunks.push_back(std::unique_ptr<PathChunk>(new PathChunk()));
	return (int)chunks.size() - 1;
}

void DB::releaseChunk(int cidx)
{
	PathChunk &C = *chunks[cidx];
	PathTier &t = tiers[C.tier];

	if (C.prev != -1)
		chunks[C.prev]->next = C.next;
	else
		t.head = C.next;
	if (C.next != -1)
		chunks[C.next]->prev = C.prev;
	else
		t.tail = C.prev;
	t.nchunks--;

	C.tier = -1;
	C.prev = C.next = -1;
	C.bump = 0;
	C.sealed = false;
	C.latest_ts = 0;
	C.gen++; // invalidates all fix-up entries referencing points in this chunk
	C.fixups.clear();
	C.touched.clear();

	empty_chunks.push_back(cidx);
}

int DB::acquireChunk(int tier)
{
	int c;
	if (!empty_chunks.empty())
	{
		c = empty_chunks.back();
		empty_chunks.pop_back();
	}
	else if ((int)chunks.size() < max_chunks || consolidating)
	{
		// during consolidation the pool may transiently exceed the cap by a
		// chunk (the source is freed at the end and lands on the empty ring)
		c = newChunk();
	}
	else
		c = makeSpace();

	PathChunk &C = *chunks[c];
	PathTier &t = tiers[tier];

	C.tier = tier;
	C.prev = -1;
	C.next = t.head;
	if (t.head != -1)
		chunks[t.head]->prev = c;
	else
		t.tail = c;
	t.head = c;
	t.nchunks++;

	return c;
}

int DB::makeSpace()
{
	// walk back over the granularities: drop the oldest chunk of the
	// coarsest non-empty tier (keep-nothing consolidation — the splice is
	// what prevents dangling links; never a raw free)
	for (int k = (int)tiers.size() - 1; k >= 0; k--)
	{
		if (tiers[k].tail == -1)
			continue;
		consolidateChunk(tiers[k].tail, false);
		int c = empty_chunks.back();
		empty_chunks.pop_back();
		return c;
	}
	return newChunk(); // no chunks exist at all
}

PtRef DB::tierAppend(int tier, const PathPoint &p)
{
	PathTier &t = tiers[tier];
	int c = t.head;
	if (c == -1 || chunks[c]->bump == PATH_CHUNK_POINTS || chunks[c]->sealed)
		c = acquireChunk(tier);

	PathChunk &C = *chunks[c];
	int slot = C.bump++;
	C.pt[slot] = p;
	C.latest_ts = p.ts;
	return ((PtRef)c << PATH_CHUNK_BITS) | (PtRef)slot;
}

void DB::addToPath(int ptr)
{
	Ship &s = ships[ptr];

	if (!isValidCoord(s.lat, s.lon))
		return;

	int32_t la = (int32_t)std::lround(s.lat * PATH_LATLON_SCALE);
	int32_t lo = (int32_t)std::lround(s.lon * PATH_LATLON_SCALE);

	if (s.path_ptr != -1)
	{
		// stationary clustering: close to the head point -> do nothing;
		// dwell time is the gap to the next-newer point at read time.
		// threshold matches the old 1e-6 squared-degrees test (~100 m)
		const PathPoint &h = deref((PtRef)s.path_ptr);
		int64_t dla = (int64_t)h.lat - la, dlo = (int64_t)h.lon - lo;
		if (dla * dla + dlo * dlo < (int64_t)600 * 600)
			return;
	}

	PathPoint p;
	p.lat = la;
	p.lon = lo;
	p.ts = (uint32_t)s.last_signal;
	p.sog_cog_hdg = packSogCogHdg(s.speed, s.cog, s.heading);
	p.next_older = (s.path_ptr == -1) ? PATH_NONE : (PtRef)s.path_ptr;

	PtRef h = tierAppend(0, p);
	addCrossingBookkeeping(ptr, h);

	if (s.path_ptr == -1)
		s.path_oldest = (int)h;
	s.path_ptr = (int)h;
	s.path_count++;
}

// record fix-up/touched entries for a freshly written chain head link
void DB::addCrossingBookkeeping(int ship_idx, PtRef p)
{
	Ship &s = ships[ship_idx];
	PathChunk &pc = *chunks[chunkOf(p)];
	PtRef older = deref(p).next_older;

	if (older == PATH_NONE)
	{
		PathChunk::Touched t;
		t.ship = (uint32_t)ship_idx;
		t.ship_gen = s.generation;
		pc.touched.push_back(t);
	}
	else if (chunkOf(older) != chunkOf(p))
	{
		PathChunk::Fixup f;
		f.ship = (uint32_t)ship_idx;
		f.ship_gen = s.generation;
		f.chunk_gen = pc.gen;
		f.pred = p;
		chunks[chunkOf(older)]->fixups.push_back(f);

		PathChunk::Touched t;
		t.ship = (uint32_t)ship_idx;
		t.ship_gen = s.generation;
		pc.touched.push_back(t);
	}
}

void DB::ageCheck(std::time_t now)
{
	if (consolidating)
		return;

	for (size_t k = 0; k < tiers.size(); k++)
	{
		PathTier &t = tiers[k];
		if (t.exit_age < 0 || t.tail == -1)
			continue;

		// seal-by-age: a lingering bump chunk whose oldest point has aged
		// out stops receiving so it can drain (quiet stations)
		PathChunk &H = *chunks[t.head];
		if (!H.sealed && H.bump > 0 && (long int)H.pt[0].ts + t.exit_age < (long int)now)
			H.sealed = true;

		PathChunk &T = *chunks[t.tail];
		bool open_head = (t.tail == t.head && !T.sealed);
		if (!open_head && T.bump > 0 && (long int)T.latest_ts + t.exit_age < (long int)now)
		{
			consolidateChunk(t.tail, true);
			return; // at most one consolidation per call
		}
	}
}

// walk one ship's run inside chunk cidx (entered at `pred`, or the ship's
// head if pred == PATH_NONE), promote spacing/turn survivors into tier dst,
// splice the chain around the chunk
void DB::consolidateRun(int cidx, int dst, int ship_idx, PtRef pred)
{
	Ship &s = ships[ship_idx];
	int spacing = dst >= 0 ? tiers[dst].spacing : 0;

	run_buf.clear();
	PtRef cur = (pred == PATH_NONE) ? (PtRef)s.path_ptr : deref(pred).next_older;
	while (cur != PATH_NONE && chunkOf(cur) == cidx)
	{
		run_buf.push_back(cur);
		cur = deref(cur).next_older;
	}
	PtRef cont = cur; // continuation in coarser tiers, or PATH_NONE
	int removed = (int)run_buf.size();

	// process old -> new; the reference is the older kept neighbour
	PtRef link_older = cont;
	PtRef oldest_kept = PATH_NONE;
	long int ref_ts = 0;
	int ref_cog = -1; // 0.1 deg units, -1 = none/NA
	if (cont != PATH_NONE)
	{
		const PathPoint &cp = deref(cont);
		ref_ts = (long int)cp.ts;
		uint32_t cc = (cp.sog_cog_hdg >> 10) & 0xFFF;
		ref_cog = (cc == PATH_COG_NA) ? -1 : (int)cc;
	}

	for (int i = (int)run_buf.size() - 1; dst >= 0 && i >= 0; i--)
	{
		PathPoint p = deref(run_buf[i]); // copy before source is recycled

		uint32_t pc = (p.sog_cog_hdg >> 10) & 0xFFF;
		int p_cog = (pc == PATH_COG_NA) ? -1 : (int)pc;
		bool turn = false;
		if (p_cog >= 0 && ref_cog >= 0)
		{
			int d = p_cog > ref_cog ? p_cog - ref_cog : ref_cog - p_cog;
			turn = MIN(d, 3600 - d) > 120; // keep course changes > 12 deg
		}

		if (link_older != PATH_NONE && (long int)p.ts < ref_ts + spacing && !turn)
			continue; // dropped: slot is simply recycled with the chunk

		p.next_older = link_older;
		PtRef np = tierAppend(dst, p);
		if (link_older != PATH_NONE && chunkOf(link_older) != chunkOf(np))
		{
			PathChunk::Fixup f;
			f.ship = (uint32_t)ship_idx;
			f.ship_gen = s.generation;
			f.chunk_gen = chunks[chunkOf(np)]->gen;
			f.pred = np;
			chunks[chunkOf(link_older)]->fixups.push_back(f);
		}
		if (oldest_kept == PATH_NONE)
			oldest_kept = np;
		link_older = np;
		ref_ts = (long int)p.ts;
		ref_cog = p_cog;
		removed--;
	}

	PtRef new_target = link_older; // newest kept copy, or cont if none kept

	// splice the ship's chain around the chunk
	if (pred != PATH_NONE)
	{
		deref(pred).next_older = new_target;
		if (new_target != PATH_NONE && chunkOf(new_target) != chunkOf(pred))
		{
			PathChunk::Fixup f;
			f.ship = (uint32_t)ship_idx;
			f.ship_gen = s.generation;
			f.chunk_gen = chunks[chunkOf(pred)]->gen;
			f.pred = pred;
			chunks[chunkOf(new_target)]->fixups.push_back(f);
		}
	}
	else
	{
		s.path_ptr = (new_target == PATH_NONE) ? -1 : (int)new_target;
		if (new_target != PATH_NONE)
		{
			PathChunk::Touched t;
			t.ship = (uint32_t)ship_idx;
			t.ship_gen = s.generation;
			chunks[chunkOf(new_target)]->touched.push_back(t);
		}
	}

	if (cont == PATH_NONE)
	{
		// the run contained the ship's tail
		if (oldest_kept != PATH_NONE)
			s.path_oldest = (int)oldest_kept;
		else if (pred != PATH_NONE)
			s.path_oldest = (int)pred;
		else
			s.path_oldest = -1;
	}
	s.path_count -= removed;
}

void DB::consolidateChunk(int cidx, bool promote)
{
	consolidating = true;

	PathChunk &C = *chunks[cidx];
	int dst = (promote && C.tier + 1 < (int)tiers.size()) ? C.tier + 1 : -1;

	for (size_t i = 0; i < C.fixups.size(); i++)
	{
		const PathChunk::Fixup f = C.fixups[i];
		if (f.ship >= ships.size() || ships[f.ship].generation != f.ship_gen)
			continue;
		if (chunkOf(f.pred) >= (int)chunks.size() || chunks[chunkOf(f.pred)]->gen != f.chunk_gen)
			continue; // pred's chunk was recycled since the entry was recorded
		const PathPoint &pp = deref(f.pred);
		if (pp.next_older == PATH_NONE || chunkOf(pp.next_older) != cidx)
			continue; // pred no longer links into this chunk
		consolidateRun(cidx, dst, (int)f.ship, f.pred);
	}

	for (size_t i = 0; i < C.touched.size(); i++)
	{
		const PathChunk::Touched t = C.touched[i];
		if (t.ship >= ships.size() || ships[t.ship].generation != t.ship_gen)
			continue;
		if (ships[t.ship].path_ptr == -1 || chunkOf((PtRef)ships[t.ship].path_ptr) != cidx)
			continue; // ship's head moved on; a fix-up entry covers it
		consolidateRun(cidx, dst, (int)t.ship, PATH_NONE);
	}

	releaseChunk(cidx);
	consolidating = false;
}

bool DB::updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &v, bool allowApproximate, bool &staticUpdated)
{
	bool position_updated = false;
	switch (p.Key())
	{
	case AIS::KEY_LAT:
		if ((msg->type()) != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || v.getApproximate()))
		{
			v.lat = p.Get().getFloat();
			position_updated = true;
		}
		break;
	case AIS::KEY_LON:
		if ((msg->type()) != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || v.getApproximate()))
		{
			v.lon = p.Get().getFloat();
			position_updated = true;
		}
		break;
	case AIS::KEY_SHIPTYPE:
		if (p.Get().getInt())
		{
			v.shiptype = p.Get().getInt();
			staticUpdated = true;
		}
		break;
	case AIS::KEY_IMO:
		v.IMO = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_MONTH:
		if (msg->type() != 5)
			break;
		v.month = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_DAY:
		if (msg->type() != 5)
			break;
		v.day = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_MINUTE:
		if (msg->type() != 5)
			break;
		v.minute = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_HOUR:
		if (msg->type() != 5)
			break;
		v.hour = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_HEADING:
		v.heading = p.Get().getInt();
		break;
	case AIS::KEY_DRAUGHT:
		if (p.Get().getFloat() != 0)
		{
			v.draught = p.Get().getFloat();
			staticUpdated = true;
		}
		break;
	case AIS::KEY_COURSE:
		v.cog = p.Get().getFloat();
		break;
	case AIS::KEY_SPEED:
		if (msg->type() == 9 && p.Get().getInt() != 1023)
			v.speed = (float)p.Get().getInt();
		else if (p.Get().getFloat() != 102.3f)
			v.speed = p.Get().getFloat();
		break;
	case AIS::KEY_STATUS:
		v.status = p.Get().getInt();
		break;
	case AIS::KEY_TO_BOW:
		v.to_bow = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_STERN:
		v.to_stern = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_PORT:
		v.to_port = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_STARBOARD:
		v.to_starboard = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_RECEIVED_STATIONS:
		v.received_stations = p.Get().getInt();
		break;
	case AIS::KEY_ALT:
		v.altitude = p.Get().getInt();
		break;
	case AIS::KEY_VIRTUAL_AID:
		v.setVirtualAid(p.Get().getBool());
		staticUpdated = true;
		break;
	case AIS::KEY_CS:
		v.setCSUnit(p.Get().getBool() ? 2 : 1); // 1=SOTDMA (false), 2=Carrier Sense (true)
		break;
	case AIS::KEY_RAIM:
		v.setRAIM(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_DTE:
		v.setDTE(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=ready, 2=not ready
		break;
	case AIS::KEY_ASSIGNED:
		v.setAssigned(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=autonomous, 2=assigned
		break;
	case AIS::KEY_DISPLAY:
		v.setDisplay(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_DSC:
		v.setDSC(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_BAND:
		v.setBand(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_MSG22:
		v.setMsg22(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_OFF_POSITION:
		v.setOffPosition(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=on position, 2=off position
		break;
	case AIS::KEY_MANEUVER:
		v.setManeuver(p.Get().getInt()); // 0=not available, 1=no special, 2=special (direct value)
		break;
	case AIS::KEY_NAME:
	case AIS::KEY_SHIPNAME:
	{
		const std::string &s = p.Get().getString();
		size_t n = MIN(s.size(), sizeof(v.shipname) - 1);
		std::memcpy(v.shipname, s.data(), n);
		v.shipname[n] = '\0';
		staticUpdated = true;
		break;
	}
	case AIS::KEY_CALLSIGN:
	{
		const std::string &s = p.Get().getString();
		size_t n = MIN(s.size(), sizeof(v.callsign) - 1);
		std::memcpy(v.callsign, s.data(), n);
		v.callsign[n] = '\0';
		staticUpdated = true;
		break;
	}
	case AIS::KEY_VENDORID:
	{
		const std::string &s = p.Get().getString();
		size_t n = MIN(s.size(), sizeof(v.vendorid) - 1);
		std::memcpy(v.vendorid, s.data(), n);
		v.vendorid[n] = '\0';
		staticUpdated = true;
		break;
	}
	case AIS::KEY_MODEL:
		v.unit_model = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_SERIAL:
		v.unit_serial = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_COUNTRY_CODE:
	{
		const std::string &s = p.Get().getString();
		size_t n = MIN(s.size(), sizeof(v.country_code) - 1);
		std::memcpy(v.country_code, s.data(), n);
		v.country_code[n] = '\0';
		break;
	}
	case AIS::KEY_DESTINATION:
	{
		const std::string &s = p.Get().getString();
		size_t n = MIN(s.size(), sizeof(v.destination) - 1);
		std::memcpy(v.destination, s.data(), n);
		v.destination[n] = '\0';
		staticUpdated = true;
		break;
	}
	case AIS::KEY_VIN:
	{
		const std::string &s = p.Get().getString();
		if (s.size() < sizeof(v.vin)) // worst case (no spaces stripped) still fits
		{
			size_t n = 0;
			for (char c : s)
				if (c != ' ')
					v.vin[n++] = c;
			v.vin[n] = '\0';
		}
		staticUpdated = true;
		break;
	}
	}
	return position_updated;
}

bool DB::updateShip(const JSON::JSON &data, TAG &tag, Ship &ship)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;

	// determine whether we accept msg 27 to update lat/lon
	bool allowApproxLatLon = false, positionUpdated = false;

	int type = msg->type();
	int repeat = msg->repeat();

	if (type == 27)
	{
		int timeout = 10 * 60;
		repeat = 0;

		if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
			timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

		if (msg->getRxTimeUnix() - ship.last_signal > timeout)
			allowApproxLatLon = true;
	}

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.group_mask |= tag.group;
	ship.last_group = tag.group;

	std::time_t prev_signal = ship.last_signal;
	ship.last_signal = msg->getRxTimeUnix();

	if (repeat == 0)
	{
		ship.last_direct_signal = ship.last_signal;
		ship.setRepeat(0);
	}
	else
	{
		if (ship.last_signal - ship.last_direct_signal > 60)
		{
			ship.setRepeat(1);
		}
	}

	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.msg_type |= 1 << type;

	if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
		ship.orOpChannels(1 << (msg->getChannel() - 'A'));

	bool staticUpdated = false;
	for (const auto &p : data.getMembers())
		positionUpdated |= updateFields(p, msg, ship, allowApproxLatLon, staticUpdated);

	ship.setType();

	// Ship came back into dashboard scope after being gone long enough that
	// frontends will have dropped their cached entry. Replay static on the
	// next incremental poll by bumping last_static_signal.
	bool back_in_scope = prev_signal > 0 && ship.last_signal - prev_signal > TIME_HISTORY;

	if (staticUpdated || (back_in_scope && ship.last_static_signal > 0))
		ship.last_static_signal = ship.last_signal;

	if (positionUpdated)
	{
		ship.setApproximate(type == 27);

		if (ship.mmsi == own_mmsi)
		{
			lat = ship.lat;
			lon = ship.lon;
		}
	}

	if (msg_save)
	{
		ship.msg.clear();
		builder.stringify(data, ship.msg);
	}
	return positionUpdated;
}

static bool isBinaryContentKey(int key)
{
	switch (key)
	{
	case AIS::KEY_TEXT:
	case AIS::KEY_CREW_COUNT:
	case AIS::KEY_PASSENGER_COUNT:
	case AIS::KEY_SHIPBOARD_PERSONNEL_COUNT:
	case AIS::KEY_WSPEED:
	case AIS::KEY_WGUST:
	case AIS::KEY_WDIR:
	case AIS::KEY_WGUSTDIR:
	case AIS::KEY_AIRTEMP:
	case AIS::KEY_HUMIDITY:
	case AIS::KEY_DEWPOINT:
	case AIS::KEY_PRESSURE:
	case AIS::KEY_PRESSURETEND:
	case AIS::KEY_VISIBILITY:
	case AIS::KEY_WATERLEVEL:
	case AIS::KEY_LEVELTREND:
	case AIS::KEY_CSPEED:
	case AIS::KEY_CDIR:
	case AIS::KEY_CSPEED2:
	case AIS::KEY_CDIR2:
	case AIS::KEY_CDEPTH2:
	case AIS::KEY_CSPEED3:
	case AIS::KEY_CDIR3:
	case AIS::KEY_CDEPTH3:
	case AIS::KEY_WAVEHEIGHT:
	case AIS::KEY_WAVEPERIOD:
	case AIS::KEY_WAVEDIR:
	case AIS::KEY_SWELLHEIGHT:
	case AIS::KEY_SWELLPERIOD:
	case AIS::KEY_SWELLDIR:
	case AIS::KEY_SEASTATE:
	case AIS::KEY_WATERTEMP:
	case AIS::KEY_PRECIPTYPE:
	case AIS::KEY_SALINITY:
	case AIS::KEY_ICE:
		return true;
	default:
		return false;
	}
}

void DB::processBinaryMessage(const JSON::JSON &data, Ship &ship, bool &position_updated)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;
	int type = msg->type();
	FLOAT32 loc_lat = LAT_UNDEFINED, loc_lon = LON_UNDEFINED;
	bool has_content = false;

	// Only process binary message types 6 and 8
	if (type != 6 && type != 8)
		return;

	BinaryMessage &binmsg = binaryMessages[binaryMsgIndex];
	binmsg.Clear();

	binmsg.type = type;

	// Extract DAC and FI from message
	for (const auto &p : data.getMembers())
	{
		if (p.Key() == AIS::KEY_DAC)
		{
			binmsg.dac = p.Get().getInt();
		}
		else if (p.Key() == AIS::KEY_FID)
		{
			binmsg.fi = p.Get().getInt();
		}
		else if (p.Key() == AIS::KEY_LAT)
		{
			loc_lat = p.Get().getFloat();
		}
		else if (p.Key() == AIS::KEY_LON)
		{
			loc_lon = p.Get().getFloat();
		}
		else if (isBinaryContentKey(p.Key()))
		{
			has_content = true;
		}
	}

	const bool is_text = binmsg.dac == 1 && (binmsg.fi == 0 || binmsg.fi == 29 || binmsg.fi == 30);
	const bool is_stored_type = is_text || (binmsg.dac == 1 && binmsg.fi == 31) || (binmsg.dac == 200 && binmsg.fi == 55);
	if (is_stored_type && has_content)
	{
		binmsg.json.clear();
		builder.stringify(data, binmsg.json);
		binmsg.used = true;
		if (isValidCoord(loc_lat, loc_lon))
		{
			binmsg.lat = loc_lat;
			binmsg.lon = loc_lon;

			// switch off approximation of mmsi location
		}
		binmsg.timestamp = msg->getRxTimeUnix();
		binaryMsgIndex = (binaryMsgIndex + 1) % MAX_BINARY_MESSAGES;
	}
}

std::string DB::getBinaryMessagesJSON(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		std::time_t tm = time(nullptr);

		w.beginObject().kv("time", tm).kv("timeout", TIME_HISTORY).key("messages").beginArray();

		int startIndex = (binaryMsgIndex + MAX_BINARY_MESSAGES - 1) % MAX_BINARY_MESSAGES;

		for (int i = 0; i < MAX_BINARY_MESSAGES; i++)
		{
			int idx = (startIndex - i + MAX_BINARY_MESSAGES) % MAX_BINARY_MESSAGES;
			const BinaryMessage &msg = binaryMessages[idx];

			if (!msg.used)
				continue;

			if ((long int)tm - (long int)msg.timestamp > TIME_HISTORY)
				break;

			if (since > 0 && msg.timestamp < since)
				break;

			w.beginObject().kv("type", msg.type).kv("dac", msg.dac).kv("fi", msg.fi).kv("timestamp", msg.timestamp).kv_raw("message", msg.json).endObject();
		}
		w.endArray().endObject();
	}
	return content;
}

void DB::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	if (!filter.include(*(AIS::Message *)data[0].binary))
		return;

	std::unique_lock<std::mutex> lock(mtx);

	const AIS::Message *msg = (AIS::Message *)data[0].binary;
	int type = msg->type();

	if (type < 1 || type > 28 || msg->mmsi() == 0)
		return;

	if (lat == LAT_UNDEFINED && tag.station_lat != LAT_UNDEFINED && tag.station_lon != LON_UNDEFINED)
	{
		lat = tag.station_lat;
		lon = tag.station_lon;
	}

	// setup/find ship in database
	int hash = Hash(msg->mmsi());
	int ptr = findShip(msg->mmsi());

	if (ptr == -1)
		ptr = createShip(hash);

	moveShipToFront(ptr);

	// update ship and tag data
	Ship &ship = ships[ptr];

	// save some data for later on
	tag.previous_signal = ship.last_signal;

	float lat_old = ship.lat;
	float lon_old = ship.lon;

	bool position_updated = updateShip(data[0], tag, ship);
	position_updated &= isValidCoord(ship.lat, ship.lon);

	if (type == 1 || type == 2 || type == 3 || type == 18 || type == 19 || type == 9)
		addToPath(ptr);

	// consolidate at most one fully-aged chunk per message (data time as clock)
	ageCheck(ship.last_signal);

	if (type == 6 || type == 8)
		processBinaryMessage(data[0], ship, position_updated);

	// update ship with distance and bearing if position is updated with message
	if (position_updated && isValidCoord(lat, lon))
	{
		getDistanceAndBearing(lat, lon, ship.lat, ship.lon, ship.distance, ship.angle);

		tag.distance = ship.distance;
		tag.angle = ship.angle;
	}
	else
	{
		tag.distance = DISTANCE_UNDEFINED;
		tag.angle = ANGLE_UNDEFINED;
	}

	if (position_updated)
	{
		tag.lat = ship.lat;
		tag.lon = ship.lon;
	}
	else
	{
		if (isValidCoord(lat_old, lon_old))
		{
			tag.lat = lat_old;
			tag.lon = lon_old;
		}
		else
		{
			tag.lat = 0;
			tag.lon = 0;
		}
	}

	tag.shipclass = ship.shipclass;
	tag.speed = ship.speed;
	std::memcpy(tag.shipname, ship.shipname, sizeof(tag.shipname));

	if (position_updated && isValidCoord(lat_old, lon_old))
	{
		// flat earth approximation, roughly 10 nmi
		float d = (ship.lat - lat_old) * (ship.lat - lat_old) + (ship.lon - lon_old) * (ship.lon - lon_old);
		tag.validated = d < 0.1675;
		ships[ptr].setValidated(tag.validated ? 1 : 2);
	}
	else
		tag.validated = false;

#ifdef CHECK_DB_INTEGRITY
	if (++update_counter % 25 == 0)
		checkIntegrity();
#endif

	lock.unlock();
	Send(data, len, tag);
}

bool DB::Save(std::ofstream &file)
{
	std::lock_guard<std::mutex> lock(mtx);

	// Write magic number and version
	int magic = _DB_MAGIC;
	int version = _DB_VERSION;

	if (!file.write((const char *)&magic, sizeof(int)))
		return false;
	if (!file.write((const char *)&version, sizeof(int)))
		return false;

	// Write ship count first
	if (!file.write((const char *)&count, sizeof(int)))
		return false;

	// Find the last ship by going count steps from first
	int ptr = first;
	for (int i = 1; i < count; i++)
	{
		if (ptr == -1)
			break;
		ptr = ships[ptr].incoming.next;
	}

	// Write ships from last ship backwards to first
	int ships_written;
	for (ships_written = 0; ships_written < count; ships_written++)
	{
		if (ptr == -1)
			break;

		// Use Ship's Save method instead of direct binary write
		if (!ships[ptr].Save(file))
			return false;

		ptr = ships[ptr].incoming.prev;
	}

	Info() << "DB: Saved " << ships_written << " ships to backup";
	return true;
}

bool DB::Load(std::ifstream &file)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::cerr << "Loading ships from backup file." << std::endl;

	int magic = 0, version = 0;

	if (!file.read((char *)&magic, sizeof(int)))
		return false;
	if (!file.read((char *)&version, sizeof(int)))
		return false;

	if (magic != _DB_MAGIC || version != _DB_VERSION)
	{
		Warning() << "DB: Invalid backup file format. Magic: " << std::hex << magic
				  << ", Version: " << version;
		return false;
	}

	// Read number of ships in backup
	int ship_count = 0;
	if (!file.read((char *)&ship_count, sizeof(int)))
		return false;

	if (ship_count < 0 || ship_count > Nships)
	{
		Warning() << "DB: Invalid ship count in backup file: " << ship_count;
		return false;
	}

	// Read all ships into a temporary buffer first, validating before modifying DB state
	std::vector<Ship> temp_ships(ship_count);
	std::time_t previous_signal = 0;

	for (int i = 0; i < ship_count; i++)
	{
		if (!temp_ships[i].Load(file))
		{
			Error() << "DB: Failed to read ship " << i << " from backup file";
			return false;
		}

		// Not persisted; treat all loaded ships as having static data
		temp_ships[i].last_static_signal = temp_ships[i].last_signal;

		if (i > 0 && temp_ships[i].last_signal < previous_signal)
		{
			Error() << "DB: Ships not in chronological order at index " << i;
			return false;
		}

		previous_signal = temp_ships[i].last_signal;
	}

	// All validated, now apply to DB
	for (int i = 0; i < ship_count; i++)
	{
		int h = Hash(temp_ships[i].mmsi);
		int ptr = findShip(temp_ships[i].mmsi);
		if (ptr == -1)
			ptr = createShip(h);

		moveShipToFront(ptr);

		ShipLL saved_incoming = ships[ptr].incoming;
		ShipLL saved_hash = ships[ptr].hash;
		uint16_t saved_gen = ships[ptr].generation;

		ships[ptr] = temp_ships[i];

		ships[ptr].incoming = saved_incoming;
		ships[ptr].hash = saved_hash;

		// paths are not persisted; the slot keeps its live generation
		ships[ptr].generation = saved_gen;
		ships[ptr].path_ptr = -1;
		ships[ptr].path_oldest = -1;
		ships[ptr].path_count = 0;
	}

	Info() << "DB: Restored " << ship_count << " ships from backup";
	return true;
}

#ifdef CHECK_DB_INTEGRITY
void DB::checkIntegrity()
{
	int errors = 0;

	// 1. Walk the incoming linked list and verify structure
	int list_count = 0;
	std::vector<bool> in_list(Nships, false);

	int ptr = first;
	int prev_ptr = -1;
	while (ptr != -1 && list_count <= Nships)
	{
		if (ptr < 0 || ptr >= Nships)
		{
			Error() << "DB integrity: incoming list ptr " << ptr << " out of range";
			errors++;
			break;
		}
		if (in_list[ptr])
		{
			Error() << "DB integrity: incoming list has cycle at ptr " << ptr;
			errors++;
			break;
		}
		if (ships[ptr].incoming.prev != prev_ptr)
		{
			Error() << "DB integrity: ship " << ptr << " prev=" << ships[ptr].incoming.prev << " expected " << prev_ptr;
			errors++;
		}
		in_list[ptr] = true;
		prev_ptr = ptr;
		ptr = ships[ptr].incoming.next;
		list_count++;
	}

	if (list_count != Nships)
	{
		Error() << "DB integrity: incoming list has " << list_count << " nodes, expected " << Nships;
		errors++;
	}

	if (prev_ptr != last)
	{
		Error() << "DB integrity: last=" << last << " but tail of list is " << prev_ptr;
		errors++;
	}

	// 2. Verify count: walk from first, count ships with mmsi != 0 that are contiguous from head
	int active_count = 0;
	ptr = first;
	while (ptr != -1)
	{
		if (ships[ptr].mmsi != 0)
			active_count++;
		else
			break;
		ptr = ships[ptr].incoming.next;
	}
	if (active_count != count)
	{
		Error() << "DB integrity: active ship count " << active_count << " != stored count " << count;
		errors++;
	}

	// 3. Verify hash table: every ship with mmsi != 0 must be in the correct bucket
	int hash_total = 0;
	for (int h = 0; h < HASH_SIZE; h++)
	{
		int bucket_count = 0;
		int bptr = hash_table[h].first;
		int bprev = -1;
		int blast = -1;

		while (bptr != -1)
		{
			if (bptr < 0 || bptr >= Nships)
			{
				Error() << "DB integrity: hash bucket " << h << " ptr " << bptr << " out of range";
				errors++;
				break;
			}
			if (ships[bptr].hash.prev != bprev)
			{
				Error() << "DB integrity: hash bucket " << h << " ship " << bptr << " prev=" << ships[bptr].hash.prev << " expected " << bprev;
				errors++;
			}
			if (ships[bptr].mmsi == 0)
			{
				Error() << "DB integrity: hash bucket " << h << " contains ship " << bptr << " with mmsi=0";
				errors++;
			}
			else if (Hash(ships[bptr].mmsi) != h)
			{
				Error() << "DB integrity: ship " << bptr << " mmsi=" << ships[bptr].mmsi << " in bucket " << h << " but hash=" << Hash(ships[bptr].mmsi);
				errors++;
			}
			blast = bptr;
			bprev = bptr;
			bptr = ships[bptr].hash.next;
			bucket_count++;

			if (bucket_count > Nships)
			{
				Error() << "DB integrity: hash bucket " << h << " has cycle";
				errors++;
				break;
			}
		}

		if (hash_table[h].last != blast)
		{
			Error() << "DB integrity: hash bucket " << h << " last=" << hash_table[h].last << " but tail is " << blast;
			errors++;
		}

		hash_total += bucket_count;
	}

	if (hash_total != count)
	{
		Error() << "DB integrity: hash table contains " << hash_total << " ships, expected " << count;
		errors++;
	}

	// 4. Verify every active ship is findable via hash
	ptr = first;
	for (int i = 0; i < count; i++)
	{
		if (ptr == -1)
			break;
		if (ships[ptr].mmsi != 0 && findShip(ships[ptr].mmsi) != ptr)
		{
			Error() << "DB integrity: ship " << ptr << " mmsi=" << ships[ptr].mmsi << " not findable via hash";
			errors++;
		}
		ptr = ships[ptr].incoming.next;
	}

	// 5. Verify tier chunk lists: prev/next symmetry, nchunks, tier ids,
	//    time-sorted disjoint chunks (latest_ts non-increasing tail-ward)
	int chunks_in_tiers = 0;
	for (size_t k = 0; k < tiers.size(); k++)
	{
		const PathTier &t = tiers[k];
		int c = t.head, cprev = -1, n = 0;
		uint32_t newer_latest = 0xFFFFFFFF;
		while (c != -1 && n <= (int)chunks.size())
		{
			const PathChunk &C = *chunks[c];
			if (C.tier != (int)k)
			{
				Error() << "DB integrity: chunk " << c << " tier " << C.tier << " but in tier " << k << " list";
				errors++;
			}
			if (C.prev != cprev)
			{
				Error() << "DB integrity: chunk " << c << " prev=" << C.prev << " expected " << cprev;
				errors++;
			}
			if (C.latest_ts > newer_latest)
			{
				Error() << "DB integrity: tier " << k << " chunk " << c << " out of time order";
				errors++;
			}
			newer_latest = C.latest_ts;
			cprev = c;
			c = C.next;
			n++;
		}
		if (cprev != t.tail)
		{
			Error() << "DB integrity: tier " << k << " tail=" << t.tail << " but list ends at " << cprev;
			errors++;
		}
		if (n != t.nchunks)
		{
			Error() << "DB integrity: tier " << k << " nchunks=" << t.nchunks << " but list has " << n;
			errors++;
		}
		chunks_in_tiers += n;
	}
	if (chunks_in_tiers + (int)empty_chunks.size() != (int)chunks.size())
	{
		Error() << "DB integrity: " << chunks.size() << " chunks but " << chunks_in_tiers << " in tiers + " << empty_chunks.size() << " empty";
		errors++;
	}

	// 6. Verify path chains for active ships: handles valid, ts monotone
	//    non-increasing, chunk ranges older-or-equal tail-ward, length =
	//    path_count, tail = path_oldest
	ptr = first;
	for (int i = 0; i < count; i++)
	{
		if (ptr == -1)
			break;

		Ship &ship = ships[ptr];
		PtRef pidx = ship.path_ptr == -1 ? PATH_NONE : (PtRef)ship.path_ptr;
		PtRef tail = PATH_NONE;
		uint32_t newer_ts = 0xFFFFFFFF;
		int path_steps = 0;

		while (pidx != PATH_NONE)
		{
			int c = chunkOf(pidx);
			if (c < 0 || c >= (int)chunks.size() || chunks[c]->tier == -1 || (int)(pidx & (PATH_CHUNK_POINTS - 1)) >= chunks[c]->bump)
			{
				Error() << "DB integrity: ship mmsi=" << ship.mmsi << " path handle " << pidx << " invalid";
				errors++;
				break;
			}
			const PathPoint &p = deref(pidx);
			if (p.ts > newer_ts)
			{
				Error() << "DB integrity: ship mmsi=" << ship.mmsi << " path not time-ordered";
				errors++;
				break;
			}
			newer_ts = p.ts;
			tail = pidx;
			pidx = p.next_older;
			path_steps++;

			if (path_steps > ship.path_count)
				break;
		}
		if (path_steps != ship.path_count)
		{
			Error() << "DB integrity: ship mmsi=" << ship.mmsi << " path length " << path_steps << " != path_count " << ship.path_count;
			errors++;
		}
		if (tail != (ship.path_oldest == -1 ? PATH_NONE : (PtRef)ship.path_oldest))
		{
			Error() << "DB integrity: ship mmsi=" << ship.mmsi << " tail != path_oldest";
			errors++;
		}
		ptr = ships[ptr].incoming.next;
	}

	if (errors)
		Error() << "DB integrity: " << errors << " errors found";
}
#endif
