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
#include "Geodesy.h"

#include <fstream>

//-----------------------------------
// simple ship database

void DB::setup()
{
	int Nships = 4096;

	if (server_mode)
	{
		Nships *= 32;
		HASH_SIZE = 262147;

		Info() << "DB: internal ship database extended to " << Nships << " ships";
	}

	ships.setup(Nships, HASH_SIZE);

	if (track_memory_kb == 0)
		track_memory_kb = server_mode ? 4096 : 1024;

	int path_blocks = paths.setup((long)track_memory_kb * 1024, Nships);
	Info() << "DB: track store " << track_memory_kb << " KB (" << path_blocks << " blocks)";
}

std::string DB::getJSONcompact(bool full, std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::time_t tm = time(nullptr);

	content.clear();
	{
		JSON::Writer w(content, 65536);

		w.beginObject().kv("count", ships.size()).kv("time", tm).kv("timeout", TIME_HISTORY);
		if (latlon_share && isValidCoord(lat, lon))
			w.key("station").beginObject().kv("lat", lat).kv("lon", lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();

		// --- Pass 1: dynamic array ---
		w.key("dynamic").beginArray();
		forEachRecent(tm, full, since, [&](int, const Ship &ship, long int) {
			ship.writeCompactDynamic(w);
		});
		w.endArray(); // dynamic

		// --- Pass 2: static array ---
		w.key("static").beginArray();
		forEachRecent(tm, full, since, [&](int, const Ship &ship, long int) {
			if (since == 0 || ship.last_static_signal >= since)
				ship.writeCompactStatic(w);
		});
		w.endArray(); // static

		w.endObject();
		w.raw("\n\n");
	}
	return content;
}

std::string DB::getJSON(bool full)
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject().kv("count", ships.size());
		if (latlon_share)
			w.key("station").beginObject().kv("lat", lat).kv("lon", lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();
		w.key("ships").beginArray();

		std::time_t tm = time(nullptr);
		forEachRecent(tm, full, 0, [&](int, const Ship &ship, long int delta_time) {
			ship.getJSON(w, delta_time, isValidCoord(lat, lon));
		});
		w.endArray().kv("error", false).endObject().raw("\n\n");
	}
	return content;
}

std::string DB::getShipJSON(int mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);

	int ptr = ships.find(mmsi);
	if (ptr == SHIP_NIL)
		return "{}";

	const Ship &ship = ships[ptr];
	long int delta_time = (long int)time(nullptr) - (long int)ship.last_signal;

	content.clear();
	{
		JSON::Writer w(content, 1024);
		ship.getJSON(w, delta_time, isValidCoord(lat, lon));
	}
	return content;
}

std::string DB::getKML()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>");
	std::time_t tm = time(nullptr);

	forEachRecent(tm, false, 0, [&](int, const Ship &ship, long int) {
		ship.getKML(content);
	});
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

		std::time_t tm = time(nullptr);
		forEachRecent(tm, false, 0, [&](int, const Ship &ship, long int) {
			ship.getGeoJSON(w, isValidCoord(lat, lon));
		});
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
		forEachRecent(tm, false, 0, [&](int ptr, const Ship &ship, long int) {
			w.key(ship.mmsi);
			writeSinglePathJSONCompact(ptr, w);
		});
		w.endObject().raw("\n\n");
	}
	return content;
}

void DB::writeSinglePathJSONCompact(int idx, JSON::Writer &w, std::time_t since)
{
	w.beginArray();
	for (uint32_t r = paths.tail(idx); PathStore::isPoint(r); r = paths.at(r).prev)
	{
		const PathStore::Point &p = paths.at(r);
		if ((std::time_t)p.end() < since)
			break;

		w.beginArray().val(p.lat).val(p.lon).val(p.time).val(p.end()).endArray();
	}
	w.endArray();
}

std::string DB::getAllPathJSONSince(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject();

		forEachRecent(time(nullptr), true, since, [&](int ptr, const Ship &ship, long int) {
			if (paths.hasSince(ptr, since))
			{
				w.key(ship.mmsi);
				writeSinglePathJSONCompact(ptr, w, since);
			}
		});
		w.endObject().raw("\n\n");
	}
	return content;
}

void DB::writeSinglePathGeoJSON(int idx, JSON::Writer &w)
{
	path_scratch.clear();
	for (uint32_t r = paths.tail(idx); PathStore::isPoint(r); r = paths.at(r).prev)
	{
		const PathStore::Point &p = paths.at(r);
		path_scratch.push_back(PathPt{p.lat, p.lon, p.time, p.end()});
	}

	w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
	for (const PathPt &p : path_scratch)
		w.beginArray().val(p.lon).val(p.lat).endArray();

	w.endArray().endObject().key("properties").beginObject().kv("mmsi", ships[idx].mmsi).key("timestamps_start").beginArray();
	for (const PathPt &p : path_scratch)
		w.val(p.time);

	w.endArray().key("timestamps_end").beginArray();
	for (const PathPt &p : path_scratch)
		w.val(p.end);

	w.endArray().endObject().endObject();
}

std::string DB::getPathJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = ships.find(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (idx != SHIP_NIL)
			writeSinglePathJSONCompact(idx, w);
		else
			w.beginArray().endArray();
	}
	return content;
}

std::string DB::getPathGeoJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = ships.find(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (idx != SHIP_NIL)
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
		forEachRecent(tm, false, 0, [&](int ptr, const Ship &, long int) {
			writeSinglePathGeoJSON(ptr, w);
		});
		w.endArray().endObject().raw("\n\n");
	}
	return content;
}

std::string DB::getMessage(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int ptr = ships.find(mmsi);
	if (ptr == SHIP_NIL)
		return "";
	return ships[ptr].msg;
}

// how far an idle ship may wander before a new track point is warranted:
// moored is fenders and GPS wander, anchored swings on the chain, otherwise
// status is unknown (Class B carries none)
static int idleBand(int status)
{
	const int NAV_ANCHORED = 1, NAV_MOORED = 5; // ITU-R M.1371
	if (status == NAV_MOORED)
		return 50;
	if (status == NAV_ANCHORED)
		return 200;
	return 100;
}

void DB::addToPath(int ptr)
{
	const Ship &ship = ships[ptr];

	if (isValidCoord(ship.lat, ship.lon))
		paths.add(ptr, ship.lat, ship.lon, ship.cog, ship.speed, idleBand(ship.status), ship.last_signal);
}

template <size_t N>
static void copyField(char (&dst)[N], const std::string &s)
{
	size_t n = MIN(s.size(), N - 1);
	std::memcpy(dst, s.data(), n);
	dst[n] = '\0';
}

bool DB::updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &v, bool allowApproximate, bool &staticUpdated)
{
	bool position_updated = false;
	switch (p.Key())
	{
	case AIS::KEY_LAT:
	case AIS::KEY_LON:
		if (msg->type() != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || v.getApproximate()))
		{
			(p.Key() == AIS::KEY_LAT ? v.lat : v.lon) = p.Get().getFloat();
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
		if (msg->type() == 9)
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
		copyField(v.shipname, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_CALLSIGN:
		copyField(v.callsign, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_VENDORID:
		copyField(v.vendorid, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_MODEL:
		v.unit_model = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_SERIAL:
		v.unit_serial = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_COUNTRY_CODE:
		copyField(v.country_code, p.Get().getString());
		break;
	case AIS::KEY_DESTINATION:
		copyField(v.destination, p.Get().getString());
		staticUpdated = true;
		break;
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
	else if (ship.last_signal - ship.last_direct_signal > 60)
		ship.setRepeat(1);

	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.markType(type);

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
		// raw sentences only; /api/message re-decodes on request
		ship.msg.clear();
		for (const auto &s : msg->sentences())
		{
			ship.msg += s;
			ship.msg += '\n';
		}
	}
	return positionUpdated;
}

static bool isBinaryContent(const JSON::Member &p)
{
	switch (p.Key())
	{
	case AIS::KEY_TEXT:
		// getText trims the '@'/space padding, so a pure-padding broadcast is empty
		return !p.Get().getString().empty();
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

void DB::processBinaryMessage(const JSON::JSON &data)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;
	int type = msg->type();
	FLOAT32 loc_lat = LAT_UNDEFINED, loc_lon = LON_UNDEFINED;
	bool has_content = false;

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
		else if (isBinaryContent(p))
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

// a recycled slot may still own the evicted ship's track, so every create
// is paired with a path wipe
int DB::claimShip(uint32_t mmsi)
{
	int ptr = ships.find(mmsi);
	if (ptr == SHIP_NIL)
	{
		ptr = ships.create(mmsi);
		paths.wipe(ptr);
		ships[ptr].reset();
	}
	else
		ships.touch(ptr);
	return ptr;
}

void DB::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	const AIS::Message *msg = (AIS::Message *)data[0].binary;
	int type = msg->type();

	if (!filter.include(*msg))
		return;
	if (type < 1 || type > 28 || msg->mmsi() == 0)
		return;

	std::unique_lock<std::mutex> lock(mtx);

	if (lat == LAT_UNDEFINED && tag.station_lat != LAT_UNDEFINED && tag.station_lon != LON_UNDEFINED)
	{
		lat = tag.station_lat;
		lon = tag.station_lon;
	}

	int ptr = claimShip(msg->mmsi());

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
		processBinaryMessage(data[0]);

	// update ship with distance and bearing if position is updated with message
	if (position_updated && isValidCoord(lat, lon))
	{
		Util::Geodesy::distanceBearing(lat, lon, ship.lat, ship.lon, ship.distance, ship.angle);

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
	else if (isValidCoord(lat_old, lon_old))
	{
		tag.lat = lat_old;
		tag.lon = lon_old;
	}
	else
	{
		tag.lat = 0;
		tag.lon = 0;
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

	lock.unlock();
	Send(data, len, tag);
}

void DB::tick(std::time_t now)
{
	// last_check/last_sweep are only written here, so they can be read before the lock
	bool check_due = false;
#ifdef CHECK_DB_INTEGRITY
	check_due = now - last_check >= 60;
#endif
	bool sweep_due = expire_fields && now - last_sweep >= TIME_HISTORY;

	if (!check_due && !sweep_due)
		return;

	std::lock_guard<std::mutex> lock(mtx);

#ifdef CHECK_DB_INTEGRITY
	if (check_due)
	{
		last_check = now;
		checkIntegrity();
	}
#endif

	if (!sweep_due)
		return;

	last_sweep = now;

	ships.forEach([&](int ptr) {
		ships[ptr].decayAndExpire();
		return true;
	});
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
	int count = ships.size();
	if (!file.write((const char *)&count, sizeof(int)))
		return false;

	// Find the last ship by going count steps from first
	int ptr = ships.front();
	for (int i = 1; i < count; i++)
	{
		if (ptr == SHIP_NIL)
			break;
		ptr = ships.next(ptr);
	}

	// Write ships from last ship backwards to first
	int ships_written;
	for (ships_written = 0; ships_written < count; ships_written++)
	{
		if (ptr == SHIP_NIL)
			break;

		if (!ships[ptr].Save(file))
			return false;

		ptr = ships.prev(ptr);
	}

	Debug() << "DB: Saved " << ships_written << " ships to backup";
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

	if (ship_count < 0 || ship_count > ships.capacity())
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

		// Not persisted; stale records are left to the sweep
		temp_ships[i].type_ttl = temp_ships[i].msg_type;

		if (temp_ships[i].mmsi == 0)
		{
			Error() << "DB: Ship with empty MMSI at index " << i;
			return false;
		}

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
		int ptr = claimShip(temp_ships[i].mmsi);
		ships[ptr] = temp_ships[i];
	}

	Info() << "DB: Restored " << ship_count << " ships from backup";
	return true;
}

#ifdef CHECK_DB_INTEGRITY
void DB::checkIntegrity()
{
	std::vector<std::string> errors;
	int n = ships.validate(errors);

	int live = 0;
	for (int ptr = ships.front(); ptr != SHIP_NIL; ptr = ships.next(ptr))
	{
		if (ships.key(ptr) != ships[ptr].mmsi)
		{
			errors.push_back("slot " + std::to_string(ptr) + " key " + std::to_string(ships.key(ptr)) + " but mmsi " + std::to_string(ships[ptr].mmsi));
			n++;
		}

		if (ships[ptr].mmsi != 0)
			live++;
	}

	if (live != ships.size())
	{
		errors.push_back("count " + std::to_string(ships.size()) + " but " + std::to_string(live) + " live records");
		n++;
	}

	n += paths.check(errors);

	for (std::size_t i = 0; i < errors.size(); i++)
		Error() << "DB integrity: " << errors[i];

	if (n == 0)
		Debug() << "DB integrity: OK (" << live << " ships)";
}
#endif
