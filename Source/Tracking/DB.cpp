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
	int nships = 4096;

	if (server_mode)
	{
		nships *= 32;
		nbuckets = 262147;

		Info() << "DB: internal ship database extended to " << nships << " ships";
	}

	ships.setup(nships, nbuckets);

	if (track_memory_kb == 0)
		track_memory_kb = server_mode ? 4096 : 1024;

	int path_blocks = paths.setup((long)track_memory_kb * 1024, nships);
	Debug() << "DB: track store " << track_memory_kb << " KB (" << path_blocks << " blocks)";
}

std::string DB::getJSONcompact(bool full, std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::time_t now = time(nullptr);

	content.clear();
	{
		JSON::Writer w(content, 65536);

		w.beginObject().kv("count", ships.size()).kv("time", now).kv("timeout", time_history);
		if (latlon_share && isValidCoord(station_lat, station_lon))
			w.key("station").beginObject().kv("lat", station_lat).kv("lon", station_lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();

		// --- Pass 1: dynamic array ---
		w.key("dynamic").beginArray();
		forEachRecent(now, full, since, [&](int, const Ship &ship, long int) {
			ship.writeCompactDynamic(w);
		});
		w.endArray(); // dynamic

		// --- Pass 2: static array ---
		w.key("static").beginArray();
		forEachRecent(now, full, since, [&](int, const Ship &ship, long int) {
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
			w.key("station").beginObject().kv("lat", station_lat).kv("lon", station_lon).kv("mmsi", own_mmsi).kv("gps", gps_position).endObject();
	
		w.key("ships").beginArray();

		std::time_t now = time(nullptr);
		forEachRecent(now, full, 0, [&](int, const Ship &ship, long int delta_time) {
			ship.getJSON(w, delta_time, isValidCoord(station_lat, station_lon));
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
		ship.getJSON(w, delta_time, isValidCoord(station_lat, station_lon));
	}
	return content;
}

std::string DB::getKML()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>");
	std::time_t now = time(nullptr);

	forEachRecent(now, false, 0, [&](int, const Ship &ship, long int) {
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
		w.beginObject().kv("type", "FeatureCollection").kv("time_span", time_history).key("features").beginArray();

		std::time_t now = time(nullptr);
		forEachRecent(now, false, 0, [&](int, const Ship &ship, long int) {
			ship.getGeoJSON(w, isValidCoord(station_lat, station_lon));
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

		std::time_t now = time(nullptr);
		forEachRecent(now, false, 0, [&](int ptr, const Ship &ship, long int) {
			w.key(ship.mmsi);
			writeSinglePathJSONCompact(ptr, w);
		});
		w.endObject().raw("\n\n");
	}
	return content;
}

void DB::writeSinglePathJSONCompact(int ptr, JSON::Writer &w, std::time_t since)
{
	w.beginArray();
	for (uint32_t r = paths.tail(ptr); PathStore::isPoint(r); r = paths.at(r).prev)
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

void DB::writeSinglePathGeoJSON(int ptr, JSON::Writer &w)
{
	path_scratch.clear();
	for (uint32_t r = paths.tail(ptr); PathStore::isPoint(r); r = paths.at(r).prev)
	{
		const PathStore::Point &p = paths.at(r);
		path_scratch.push_back(PathPt{p.lat, p.lon, p.time, p.end()});
	}

	w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
	for (const PathPt &p : path_scratch)
		w.beginArray().val(p.lon).val(p.lat).endArray();

	w.endArray().endObject().key("properties").beginObject().kv("mmsi", ships[ptr].mmsi).key("timestamps_start").beginArray();
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
	int ptr = ships.find(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (ptr != SHIP_NIL)
			writeSinglePathJSONCompact(ptr, w);
		else
			w.beginArray().endArray();
	}
	return content;
}

std::string DB::getPathGeoJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int ptr = ships.find(mmsi);

	content.clear();
	{
		JSON::Writer w(content, 1024);
		if (ptr != SHIP_NIL)
			writeSinglePathGeoJSON(ptr, w);
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

		std::time_t now = time(nullptr);
		forEachRecent(now, false, 0, [&](int ptr, const Ship &, long int) {
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

void DB::updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &ship, bool allowApproximate, bool &positionUpdated, bool &staticUpdated)
{
	switch (p.Key())
	{
	case AIS::KEY_LAT:
	case AIS::KEY_LON:
		if (msg->type() != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || ship.getApproximate()))
		{
			(p.Key() == AIS::KEY_LAT ? ship.lat : ship.lon) = p.Get().getFloat();
			positionUpdated = true;
		}
		break;
	case AIS::KEY_SHIPTYPE:
		if (p.Get().getInt())
		{
			ship.shiptype = p.Get().getInt();
			staticUpdated = true;
		}
		break;
	case AIS::KEY_IMO:
		ship.IMO = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_MONTH:
		if (msg->type() != 5)
			break;
		ship.month = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_DAY:
		if (msg->type() != 5)
			break;
		ship.day = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_MINUTE:
		if (msg->type() != 5)
			break;
		ship.minute = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_HOUR:
		if (msg->type() != 5)
			break;
		ship.hour = (char)p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_HEADING:
		ship.heading = p.Get().getInt();
		break;
	case AIS::KEY_DRAUGHT:
		if (p.Get().getFloat() != 0)
		{
			ship.draught = p.Get().getFloat();
			staticUpdated = true;
		}
		break;
	case AIS::KEY_COURSE:
		ship.cog = p.Get().getFloat();
		break;
	case AIS::KEY_SPEED:
		if (msg->type() == 9 && p.Get().getInt() != 1023)
			ship.speed = (float)p.Get().getInt();
		else if (p.Get().getFloat() != 102.3f)
			ship.speed = p.Get().getFloat();
		break;
	case AIS::KEY_STATUS:
		ship.status = p.Get().getInt();
		break;
	case AIS::KEY_TO_BOW:
		ship.to_bow = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_STERN:
		ship.to_stern = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_PORT:
		ship.to_port = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_TO_STARBOARD:
		ship.to_starboard = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_RECEIVED_STATIONS:
		ship.received_stations = p.Get().getInt();
		break;
	case AIS::KEY_ALT:
		if (msg->type() == 9)
			ship.altitude = p.Get().getInt();
		break;
	case AIS::KEY_VIRTUAL_AID:
		ship.setVirtualAid(p.Get().getBool());
		staticUpdated = true;
		break;
	case AIS::KEY_CS:
		ship.setCSUnit(p.Get().getBool() ? 2 : 1); // 1=SOTDMA (false), 2=Carrier Sense (true)
		break;
	case AIS::KEY_RAIM:
		ship.setRAIM(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_DTE:
		ship.setDTE(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=ready, 2=not ready
		break;
	case AIS::KEY_ASSIGNED:
		ship.setAssigned(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=autonomous, 2=assigned
		break;
	case AIS::KEY_DISPLAY:
		ship.setDisplay(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_DSC:
		ship.setDSC(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_BAND:
		ship.setBand(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_MSG22:
		ship.setMsg22(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
		break;
	case AIS::KEY_OFF_POSITION:
		ship.setOffPosition(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=on position, 2=off position
		break;
	case AIS::KEY_MANEUVER:
		ship.setManeuver(p.Get().getInt()); // 0=not available, 1=no special, 2=special (direct value)
		break;
	case AIS::KEY_NAME:
	case AIS::KEY_SHIPNAME:
		copyField(ship.shipname, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_CALLSIGN:
		copyField(ship.callsign, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_VENDORID:
		copyField(ship.vendorid, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_MODEL:
		ship.unit_model = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_SERIAL:
		ship.unit_serial = p.Get().getInt();
		staticUpdated = true;
		break;
	case AIS::KEY_COUNTRY_CODE:
		copyField(ship.country_code, p.Get().getString());
		break;
	case AIS::KEY_DESTINATION:
		copyField(ship.destination, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_VIN:
	{
		const std::string &s = p.Get().getString();
		if (s.size() < sizeof(ship.vin)) // worst case (no spaces stripped) still fits
		{
			size_t n = 0;
			for (char c : s)
				if (c != ' ')
					ship.vin[n++] = c;
			ship.vin[n] = '\0';
		}
		staticUpdated = true;
		break;
	}
	}
}

bool DB::updateShip(const JSON::JSON &data, TAG &tag, Ship &ship)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;

	bool positionUpdated = false, staticUpdated = false;

	int type = msg->type();
	int repeat = msg->repeat();

	// determine whether we accept msg 27 to update lat/lon
	bool allowApproxLatLon = false;
	if (type == 27)
	{
		int timeout = 10 * 60;
		repeat = 0;

		if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
			timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

		if (msg->getRxTimeUnix() - ship.last_signal > timeout)
			allowApproxLatLon = true;
	}

	// direct reception clears the repeated flag immediately; a relayed copy sets
	// it only when the ship has not been heard directly for over a minute
	if (repeat == 0)
	{
		ship.last_direct_signal = msg->getRxTimeUnix();
		ship.setRepeat(0);
	}
	else if (msg->getRxTimeUnix() - ship.last_direct_signal > 60)
		ship.setRepeat(1);

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.group_mask |= tag.group;
	ship.last_group = tag.group;

	std::time_t prev_signal = ship.last_signal;
	ship.last_signal = msg->getRxTimeUnix();

	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.markType(type);

	if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
		ship.orOpChannels(1 << (msg->getChannel() - 'A'));

	for (const auto &p : data.getMembers())
		updateFields(p, msg, ship, allowApproxLatLon, positionUpdated, staticUpdated);

	ship.setType();

	// Ship came back into dashboard scope after being gone long enough that
	// frontends will have dropped their cached entry. Replay static on the
	// next incremental poll by bumping last_static_signal.
	bool back_in_scope = prev_signal > 0 && ship.last_signal - prev_signal > time_history;

	if (staticUpdated || (back_in_scope && ship.last_static_signal > 0))
		ship.last_static_signal = ship.last_signal;

	if (positionUpdated)
	{
		ship.setApproximate(type == 27);

		if (ship.mmsi == own_mmsi)
		{
			station_lat = ship.lat;
			station_lon = ship.lon;
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

	BinaryMessage &binmsg = binary_messages[binary_msg_index];
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
		binary_msg_index = (binary_msg_index + 1) % MAX_BINARY_MESSAGES;
	}
}

std::string DB::getBinaryMessagesJSON(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		std::time_t now = time(nullptr);

		w.beginObject().kv("time", now).kv("timeout", time_history).key("messages").beginArray();

		int startIndex = (binary_msg_index + MAX_BINARY_MESSAGES - 1) % MAX_BINARY_MESSAGES;

		for (int i = 0; i < MAX_BINARY_MESSAGES; i++)
		{
			int idx = (startIndex - i + MAX_BINARY_MESSAGES) % MAX_BINARY_MESSAGES;
			const BinaryMessage &msg = binary_messages[idx];

			if (!msg.used)
				continue;

			if ((long int)now - (long int)msg.timestamp > time_history)
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

	if (type < 1 || type > 28 || msg->mmsi() == 0)
		return;

	if (!filter.include(*msg))
		return;
	
	std::unique_lock<std::mutex> lock(mtx);

	if (!isValidCoord(station_lat, station_lon) && isValidCoord(tag.station_lat, tag.station_lon))
	{
		station_lat = tag.station_lat;
		station_lon = tag.station_lon;
	}

	int ptr = claimShip(msg->mmsi());

	// update ship and tag data
	Ship &ship = ships[ptr];

	// save some data for later on
	tag.previous_signal = ship.last_signal;

	float lat_old = ship.lat;
	float lon_old = ship.lon;

	bool newValidPosition = updateShip(data[0], tag, ship) && isValidCoord(ship.lat, ship.lon);

	if (type == 6 || type == 8)
		processBinaryMessage(data[0]);

	tag.shipclass = ship.shipclass;
	tag.speed = ship.speed;
	std::memcpy(tag.shipname, ship.shipname, sizeof(tag.shipname));

	tag.distance = DISTANCE_UNDEFINED;
	tag.angle = ANGLE_UNDEFINED;
	tag.validated = false;

	if (newValidPosition)
	{
		if (type == 1 || type == 2 || type == 3 || type == 18 || type == 19 || type == 9)
			addToPath(ptr);

		if (isValidCoord(station_lat, station_lon))
		{
			Util::Geodesy::distanceBearing(station_lat, station_lon, ship.lat, ship.lon, ship.distance, ship.angle);

			tag.distance = ship.distance;
			tag.angle = ship.angle;
		}

		tag.lat = ship.lat;
		tag.lon = ship.lon;

		if (isValidCoord(lat_old, lon_old))
		{
			// flat earth approximation, roughly 10 nmi
			float d = (ship.lat - lat_old) * (ship.lat - lat_old) + (ship.lon - lon_old) * (ship.lon - lon_old);
			tag.validated = d < 0.1675;
			ship.setValidated(tag.validated ? 1 : 2);
		}
	}
	else if (isValidCoord(lat_old, lon_old))
	{
		tag.lat = lat_old;
		tag.lon = lon_old;
	}
	else
	{
		tag.lat = LAT_UNDEFINED;
		tag.lon = LON_UNDEFINED;
	}

	lock.unlock();
	Send(data, len, tag);
}

void DB::tick(std::time_t now)
{
	// last_check/last_sweep are only written here, so they can be read before the lock
#ifdef CHECK_DB_INTEGRITY
	bool check_due = now - last_check >= 60;
#else
	bool check_due = false;
#endif
	bool sweep_due = expire_fields && now - last_sweep >= time_history;

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
