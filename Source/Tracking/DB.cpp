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

#include <fstream>

//-----------------------------------
// simple ship database

void DB::setup()
{

	if (server_mode)
	{
		Nships *= 32;
		Npaths *= 32;
		HASH_SIZE = 262147;

		Info() << "DB: internal ship database extended to " << Nships << " ships and " << Npaths << " path points";
	}

	ships.resize(Nships);
	paths.resize(Npaths);
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

void DB::writeSinglePathJSON(int idx, JSON::Writer &w)
{
	uint32_t mmsi = ships[idx].mmsi;
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;

	w.beginArray();
	while (isNextPathPoint(ptr, mmsi, t))
	{
		if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
			w.beginArray().val(paths[ptr].lat).val(paths[ptr].lon).val(paths[ptr].timestamp_start).val(paths[ptr].timestamp_end).endArray();
		t = paths[ptr].count;
		ptr = paths[ptr].next;
	}
	w.endArray();
}

void DB::writeSinglePathJSONCompact(int idx, JSON::Writer &w)
{
	uint32_t mmsi = ships[idx].mmsi;
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;
	int cnt = 0;

	w.beginArray();
	while (isNextPathPoint(ptr, mmsi, t))
	{
		if (cnt >= 250)
			break;

		if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
		{
			w.beginArray().val(paths[ptr].lat).val(paths[ptr].lon).val(paths[ptr].timestamp_start).val(paths[ptr].timestamp_end).endArray();
			cnt++;
		}
		t = paths[ptr].count;
		ptr = paths[ptr].next;
	}
	w.endArray();
}

bool DB::writeSinglePathJSONCompactSince(int idx, std::time_t since, JSON::Writer &w)
{
	uint32_t mmsi = ships[idx].mmsi;
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;
	bool any = false;

	w.beginArray();
	while (isNextPathPoint(ptr, mmsi, t))
	{
		if ((long int)paths[ptr].timestamp_end < (long int)since)
			break;

		if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
		{
			w.beginArray().val(paths[ptr].lat).val(paths[ptr].lon).val(paths[ptr].timestamp_start).val(paths[ptr].timestamp_end).endArray();
			any = true;
		}
		t = paths[ptr].count;
		ptr = paths[ptr].next;
	}
	w.endArray();
	return any;
}

bool DB::hasPathPointsSince(int idx, std::time_t since)
{
	// Path list is reverse-chronological — the head is the most recent
	// point. If the head is older than `since`, nothing newer exists;
	// otherwise at least one point is in the window. Coordinate validity
	// is filtered at write time by writeSinglePathJSONCompactSince.
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;
	if (!isNextPathPoint(ptr, ships[idx].mmsi, t))
		return false;
	return (long int)paths[ptr].timestamp_end >= (long int)since;
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
	int path_head = ships[idx].path_ptr;
	int count_head = ships[idx].count + 1;

	w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
	{
		int ptr = path_head;
		int t = count_head;
		while (isNextPathPoint(ptr, mmsi, t))
		{
			if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
				w.beginArray().val(paths[ptr].lon).val(paths[ptr].lat).endArray();
			t = paths[ptr].count;
			ptr = paths[ptr].next;
		}
	}
	w.endArray().endObject().key("properties").beginObject().kv("mmsi", mmsi).key("timestamps_start").beginArray();
	{
		int ptr = path_head;
		int t = count_head;
		while (isNextPathPoint(ptr, mmsi, t))
		{
			if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
				w.val(paths[ptr].timestamp_start);
			t = paths[ptr].count;
			ptr = paths[ptr].next;
		}
	}
	w.endArray().key("timestamps_end").beginArray();
	{
		int ptr = path_head;
		int t = count_head;
		while (isNextPathPoint(ptr, mmsi, t))
		{
			if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
				w.val(paths[ptr].timestamp_end);
			t = paths[ptr].count;
			ptr = paths[ptr].next;
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

void DB::addToPath(int ptr)
{

	int idx = ships[ptr].path_ptr;
	float lat = ships[ptr].lat;
	float lon = ships[ptr].lon;
	int count = ships[ptr].count;
	uint32_t mmsi = ships[ptr].mmsi;
	std::time_t timestamp = ships[ptr].last_signal;

	if (isNextPathPoint(idx, mmsi, count))
	{
		// path exists and ship did not move
		if (paths[idx].lat == lat && paths[idx].lon == lon)
		{
			paths[idx].count = ships[ptr].count;
			paths[idx].timestamp_end = timestamp; // Update end time, keep start time
			return;
		}
		// if there exist a previous path point, check if ship moved more than 100 meters and, if not, update clustered path point
		int next = paths[idx].next;
		if (isNextPathPoint(next, mmsi, paths[idx].count))
		{
			float lat_prev = paths[next].lat;
			float lon_prev = paths[next].lon;

			float d = (lat_prev - lat) * (lat_prev - lat) + (lon_prev - lon) * (lon_prev - lon);

			if (d < 0.000001)
			{
				// Update clustered point: keep start time, update end time and position
				paths[idx].lat = lat;
				paths[idx].lon = lon;
				paths[idx].count = ships[ptr].count;
				paths[idx].timestamp_end = timestamp; // Keep timestamp_start unchanged
				return;
			}
		}
	}

	// create new path point
	paths[path_idx].next = idx;
	paths[path_idx].lat = lat;
	paths[path_idx].lon = lon;
	paths[path_idx].mmsi = ships[ptr].mmsi;
	paths[path_idx].count = ships[ptr].count;
	paths[path_idx].timestamp_start = timestamp; // New point: start and end are the same initially
	paths[path_idx].timestamp_end = timestamp;

	ships[ptr].path_ptr = path_idx;
	path_idx = (path_idx + 1) % Npaths;
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

void DB::processBinaryMessage(const JSON::JSON &data, Ship &ship, bool &position_updated)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;
	int type = msg->type();
	FLOAT32 loc_lat = LAT_UNDEFINED, loc_lon = LON_UNDEFINED;

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
	}

	const bool is_text = binmsg.dac == 1 && (binmsg.fi == 0 || binmsg.fi == 29 || binmsg.fi == 30);
	if (is_text || (binmsg.dac == 1 && binmsg.fi == 31) || (binmsg.dac == 200 && binmsg.fi == 55))
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

		ships[ptr] = temp_ships[i];

		ships[ptr].incoming = saved_incoming;
		ships[ptr].hash = saved_hash;
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

	// 5. Verify path_idx is in range
	if (path_idx < 0 || path_idx >= Npaths)
	{
		Error() << "DB integrity: path_idx " << path_idx << " out of range [0," << Npaths << ")";
		errors++;
	}

	// 6. Verify path chains for active ships
	ptr = first;
	for (int i = 0; i < count; i++)
	{
		if (ptr == -1)
			break;

		Ship &ship = ships[ptr];
		int pidx = ship.path_ptr;
		int pcount = ship.count + 1;
		int path_steps = 0;

		while (isNextPathPoint(pidx, ship.mmsi, pcount))
		{
			if (pidx < 0 || pidx >= Npaths)
			{
				Error() << "DB integrity: ship mmsi=" << ship.mmsi << " path ptr " << pidx << " out of range";
				errors++;
				break;
			}
			pcount = paths[pidx].count;
			pidx = paths[pidx].next;
			path_steps++;

			if (path_steps > Npaths)
			{
				Error() << "DB integrity: ship mmsi=" << ship.mmsi << " path chain has cycle";
				errors++;
				break;
			}
		}
		ptr = ships[ptr].incoming.next;
	}

	if (errors)
		Error() << "DB integrity: " << errors << " errors found";
}
#endif
