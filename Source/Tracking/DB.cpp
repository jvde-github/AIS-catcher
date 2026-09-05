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

#include <cstdio>
#include "AIS-catcher.h"
#include <algorithm>
#include <cctype>
#include "DB.h"
#include "Geodesy.h"
#include "Region.h"
#include "Logger.h"

#include <fstream>

//-----------------------------------
// simple ship database

void DB::setup()
{
	std::lock_guard<std::mutex> lock(mtx);

	int nships = 4096;

	if (server_mode)
	{
		nships *= 32;
		nbuckets = 262147;
	}
	if (max_ships > 0)
	{
		nships = max_ships;
		nbuckets = 2 * nships + 1;
	}
	if (nships != 4096)
		Info() << "DB: internal ship database extended to " << nships << " ships";

	ships.setup(nships, nbuckets);

	if (track_memory_kb == 0)
		track_memory_kb = server_mode ? 4096 : 1024;

	int path_blocks = paths.setup((long)track_memory_kb * 1024, nships);
	Debug() << "DB: track store " << track_memory_kb << " KB (" << path_blocks << " blocks)";

	changes.setup(nships);

	binary.setup(server_mode ? 65536 : 256);
	evict_horizon = 0;
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
		forEachRecentUnlocked(now, full, since, [&](int, const Ship &ship, long int) {
			ship.writeCompactDynamic(w, now, binary.badge(ship.mmsi, now), stations.idFor(ship.mmsi));
		});
		w.endArray(); // dynamic

		// --- Pass 2: static array ---
		w.key("static").beginArray();
		forEachRecentUnlocked(now, full, since, [&](int, const Ship &ship, long int) {
			if (since == 0 || ship.last_static_signal >= since)
				ship.writeCompactStatic(w);
		});
		w.endArray(); // static

		w.endObject();
		w.raw("\n\n");
	}
	return content;
}

std::string DB::getJSONtable(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::time_t now = time(nullptr);

	content.clear();
	{
		JSON::Writer w(content, 16384);
		w.beginObject().kv("time", now).key("rows").beginArray();
		forEachRecentUnlocked(now, false, since, [&](int, const Ship &ship, long int) { ship.writeCompactTable(w); });
		w.endArray().endObject();
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
		forEachRecentUnlocked(now, full, 0, [&](int, const Ship &ship, long int delta_time) {
			ship.writeJSON(w, delta_time, isValidCoord(station_lat, station_lon));
		});

		w.endArray().kv("error", false).endObject().raw("\n\n");
	}
	return content;
}

std::string DB::getChangesJSON(int mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::string content;
	JSON::Writer w(content);
	changes.writeJSON(w, ships.find((uint32_t)mmsi));
	w.finish();
	return content;
}

std::string DB::getShipJSON(int mmsi)
{
	return vesselJSON((uint32_t)mmsi, [](JSON::Writer &, const Ship &, int) {});
}

std::string DB::getKML()
{
	std::lock_guard<std::mutex> lock(mtx);

	content.assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>");
	std::time_t now = time(nullptr);

	forEachRecentUnlocked(now, false, 0, [&](int, const Ship &ship, long int) {
		ship.writeKML(content);
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
		forEachRecentUnlocked(now, false, 0, [&](int, const Ship &ship, long int) {
			ship.writeGeoJSON(w, isValidCoord(station_lat, station_lon));
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
		std::time_t floor = pathFloor(now);
		forEachRecentUnlocked(now, false, 0, [&](int ptr, const Ship &ship, long int) {
			w.key(ship.mmsi);
			writeSinglePathJSONCompact(ptr, w, floor);
		});
		w.endObject().raw("\n\n");
	}
	return content;
}

// Points are newest first, so the walk skips past the window, emits while it
// overlaps and stops once clear of it. A dwell point is emitted by every window
// its [time, end] span touches. A windowed walk (until > 0) also emits the
// first point wholly before the window — where the vessel was when it opened —
// without which a chunk cannot draw a ship that last reported before it began.
void DB::writeSinglePathJSONCompact(int ptr, JSON::Writer &w, std::time_t since, std::time_t until)
{
	auto emit = [&](const PathStore::Point &p) {
		w.beginArray().val(p.lat).val(p.lon).val(p.time).val(p.end())
			.val_unless(p.sog, PathStore::NA)
			.val_unless(p.cog, PathStore::NA)
			.val_unless(p.hdg, PathStore::NA)
			.endArray();
	};

	w.beginArray();
	for (uint32_t r = paths.tail(ptr); PathStore::isPoint(r); r = paths.at(r).prev)
	{
		const PathStore::Point &p = paths.at(r);
		if ((std::time_t)p.end() < since)
		{
			if (until > 0)
				emit(p);
			break;
		}

		if (until <= 0 || (std::time_t)p.time <= until)
			emit(p);
	}
	w.endArray();
}

std::string DB::getAllPathJSONSince(std::time_t since)
{
	std::lock_guard<std::mutex> lock(mtx);

	since = MAX(since, pathFloor(time(nullptr)));

	content.clear();
	{
		JSON::Writer w(content, 65536);
		w.beginObject();

		forEachRecentUnlocked(time(nullptr), true, since, [&](int ptr, const Ship &ship, long int) {
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

std::string DB::getReplayInfoJSON(std::time_t block)
{
	std::lock_guard<std::mutex> lock(mtx);

	content.clear();
	{
		JSON::Writer w(content, 256);

		std::time_t now = time(nullptr);

		// bounds of the replayable timeline, 0 when empty; `newest` trails
		// `now` while the feed is quiet
		uint32_t oldest = 0, newest = 0;
		ships.forEach([&](int ptr) {
			uint32_t h = paths.head(ptr);
			if (PathStore::isPoint(h))
			{
				uint32_t t = paths.at(h).time;
				uint32_t e = paths.at(paths.tail(ptr)).end();
				if (oldest == 0 || t < oldest)
					oldest = t;
				if (e > newest)
					newest = e;
			}
			return true;
		});

		// the client only asks for blocks within the bounds it is given
		std::time_t cutoff = pathFloor(now);
		if (oldest && (std::time_t)oldest < cutoff)
			oldest = (uint32_t)cutoff;

		w.beginObject()
			.kv("now", now)
			.kv("oldest", oldest)
			.kv("newest", newest)
			.kv("block", block)
			.kv("granularity", (int)PathStore::GRANULARITY)
			.kv("dwell_gap", (int)PathStore::DWELL_GAP)
			.kv("point_format", 2)
			.endObject();
		w.raw("\n\n");
	}
	return content;
}

// Per-ship styling, sent once so the chunks stay geometry. Read straight off
// the record rather than through the static-signal gate: a name captured hours
// ago is what a replay of that period wants.
std::string DB::getReplayShipsJSON(std::time_t since, std::time_t lookback)
{
	return getReplayObjectJSON(since, lookback, 0, [](JSON::Writer &w, int, const Ship &ship, std::time_t) {
		w.key(ship.mmsi).beginObject()
			.kv("c", ship.shipclass)
			.kv("n", ship.shipname)
			.kv("f", ship.country_code)
			.kv_unless("t", ship.shiptype, 0);

		// all-zero is how AtoN and many Class B units say "not applicable"
		if (ship.to_bow != DIMENSION_UNDEFINED && ship.to_stern != DIMENSION_UNDEFINED &&
			ship.to_port != DIMENSION_UNDEFINED && ship.to_starboard != DIMENSION_UNDEFINED &&
			ship.to_bow + ship.to_stern > 0 && ship.to_port + ship.to_starboard > 0)
			w.key("d").beginArray().val(ship.to_bow).val(ship.to_stern).val(ship.to_port).val(ship.to_starboard).endArray();

		w.endObject();
	});
}

std::string DB::getReplayJSON(std::time_t since, std::time_t until, std::time_t lookback)
{
	return getReplayObjectJSON(since, lookback, until, [this, until](JSON::Writer &w, int ptr, const Ship &ship, std::time_t from) {
		w.key(ship.mmsi);
		writeSinglePathJSONCompact(ptr, w, from, until);
	});
}

template <typename F>
static void walkPath(const PathStore &paths, int ptr, std::time_t floor, F emit)
{
	for (uint32_t r = paths.tail(ptr); PathStore::isPoint(r) && (std::time_t)paths.at(r).end() >= floor; r = paths.at(r).prev)
		emit(paths.at(r));
}

void DB::writeSinglePathGeoJSON(int ptr, JSON::Writer &w, std::time_t floor)
{
	w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
	walkPath(paths, ptr, floor, [&](const PathStore::Point &p) { w.beginArray().val(p.lon).val(p.lat).endArray(); });

	w.endArray().endObject().key("properties").beginObject().kv("mmsi", ships[ptr].mmsi).key("timestamps_start").beginArray();
	walkPath(paths, ptr, floor, [&](const PathStore::Point &p) { w.val(p.time); });

	w.endArray().key("timestamps_end").beginArray();
	walkPath(paths, ptr, floor, [&](const PathStore::Point &p) { w.val(p.end()); });

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
			writeSinglePathJSONCompact(ptr, w, pathFloor(time(nullptr)));
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
			writeSinglePathGeoJSON(ptr, w, pathFloor(time(nullptr)));
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
		std::time_t floor = pathFloor(now);
		forEachRecentUnlocked(now, false, 0, [&](int ptr, const Ship &, long int) {
			writeSinglePathGeoJSON(ptr, w, floor);
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
		paths.add(ptr, ship.lat, ship.lon, ship.cog, ship.heading, ship.speed, idleBand(ship.status), ship.last_signal);
}

template <size_t N>
static void copyField(char (&dst)[N], const std::string &s)
{
	size_t n = MIN(s.size(), N - 1);
	std::memcpy(dst, s.data(), n);
	dst[n] = '\0';
}

// A binary payload carries the position of whatever it describes - a buoy, a
// sensor station, a tidal window - never the sender's, so 6/25/26 are excluded
// alongside 8; 17 reports the DGNSS reference station.
static bool carriesOwnPosition(int type)
{
	return type != 6 && type != 8 && type != 17 && type != 25 && type != 26;
}

// A first value is initialisation, not a change: the ship record is where the
// current value lives.
void DB::logTextChange(const Ship &ship, int field, const char *old_value, const std::string &value)
{
	if (old_value[0] && value != old_value)
		changes.addText(ships.find(ship.mmsi), (StaticStore::Field)field, value.c_str(), ship.last_signal);
}

void DB::updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &ship, bool allowApproximate, bool &positionUpdated, bool &staticUpdated)
{
	switch (p.Key())
	{
	case AIS::KEY_LAT:
	case AIS::KEY_LON:
		if (carriesOwnPosition(msg->type()) && (msg->type() != 27 || allowApproximate || ship.getApproximate()))
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
	case AIS::KEY_DAY:
	case AIS::KEY_HOUR:
	case AIS::KEY_MINUTE:
		if (msg->type() == 5)
		{
			(p.Key() == AIS::KEY_MONTH ? ship.month : p.Key() == AIS::KEY_DAY ? ship.day
										  : p.Key() == AIS::KEY_HOUR		 ? ship.hour
																			 : ship.minute) = (char)p.Get().getInt();
			staticUpdated = true;
		}
		break;
	case AIS::KEY_HEADING:
		ship.heading = p.Get().getInt();
		break;
	case AIS::KEY_DRAUGHT:
	{
		// an inland vessel reports draught twice and the two disagree; the DAC 200
		// FID 10 value is the finer one, so the type 5 field is dropped once it is heard
		const float d = p.Get().getFloat();
		const bool inland = msg->type() == 6 || msg->type() == 8;

		if (d != 0 && (inland || !ship.getInlandDraught()))
		{
			const bool first = ship.draught == DRAUGHT_UNDEFINED || ship.draught <= 0;
			changes.addNumeric(ships.find(ship.mmsi), StaticStore::DRAUGHT,
							   first ? (uint8_t)(d * 10 + 0.5f) : (uint8_t)(ship.draught * 10 + 0.5f),
							   (uint8_t)(d * 10 + 0.5f), ship.last_signal, first);
			if (!first)
				noteDraught(ship, d);
			ship.draught = d;
			ship.setInlandDraught(inland);
			staticUpdated = true;
		}
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
	{
		const int st = p.Get().getInt();
		if (ship.status != STATUS_UNDEFINED)
			changes.addNumeric(ships.find(ship.mmsi), StaticStore::STATUS, (uint8_t)ship.status, (uint8_t)st, ship.last_signal);
		noteStatus(ship, st);
		ship.status = st;
	}
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
		logTextChange(ship, StaticStore::SHIPNAME, ship.shipname, p.Get().getString());
		copyField(ship.shipname, p.Get().getString());
		staticUpdated = true;
		break;
	case AIS::KEY_CALLSIGN:
		logTextChange(ship, StaticStore::CALLSIGN, ship.callsign, p.Get().getString());
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
	{
		const std::string &d = p.Get().getString();
		logTextChange(ship, StaticStore::DESTINATION, ship.destination, d);
		noteDestination(ship, d);
		copyField(ship.destination, d);
		staticUpdated = true;
	}
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

	// ETA arrives as four separate keys, so it is compared around the whole
	// message rather than at an assignment site
	const char eta_before[4] = {ship.month, ship.day, ship.hour, ship.minute};
	const bool eta_was_set = ship.month != ETA_MONTH_UNDEFINED || ship.day != ETA_DAY_UNDEFINED ||
							 ship.hour != ETA_HOUR_UNDEFINED || ship.minute != ETA_MINUTE_UNDEFINED;

	for (const auto &p : data.getMembers())
		updateFields(p, msg, ship, allowApproxLatLon, positionUpdated, staticUpdated);

	const bool eta_now_set = ship.month != ETA_MONTH_UNDEFINED || ship.day != ETA_DAY_UNDEFINED ||
							 ship.hour != ETA_HOUR_UNDEFINED || ship.minute != ETA_MINUTE_UNDEFINED;

	if (eta_was_set && eta_now_set && (eta_before[0] != ship.month || eta_before[1] != ship.day ||
									   eta_before[2] != ship.hour || eta_before[3] != ship.minute))
	{
		changes.addEta(ships.find(ship.mmsi), (uint8_t)ship.month, (uint8_t)ship.day, (uint8_t)ship.hour, (uint8_t)ship.minute, ship.last_signal);
	}

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
		ship.region = Region::find(ship.lat, ship.lon);

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

std::string DB::getBinaryMessagesJSON(std::time_t since, uint64_t marker, uint32_t owner)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		binary.writeJSON(w, time(nullptr), since, marker, owner);
	}
	return content;
}

// a vessel makes no events for this long after a destination or status change,
// and for this long after saying TEST
static const int CHANGE_SETTLE_S = 30, TEST_MUTE_S = 300;

static std::string words(const std::string &text)
{
	std::string s = " ";
	for (char c : text)
		s += std::isalnum((unsigned char)c) ? (char)std::toupper((unsigned char)c) : ' ';
	return s + ' ';
}

// nothing a decode gone wrong leaves behind
static bool plausibleText(const std::string &text)
{
	int letters = 0;
	for (char c : text)
	{
		unsigned char u = (unsigned char)c;
		if (u < 32 || u > 126)
			return false;
		if (std::isalpha(u))
			letters++;
	}
	return text.size() >= 3 && letters >= 2;
}

// A safety message has no priority field: a distress device sending it (AIS-SART
// 970, man-overboard 972, EPIRB 974) or the words used for something serious say
// it matters; `s` is the text as words() gives it
static EventRing::Level safetyLevel(uint32_t from, const std::string &s)
{
	auto has = [&](const char *w) { return s.find(std::string(" ") + w + " ") != std::string::npos; };
	auto wordStarts = [&](const char *w) { return s.find(std::string(" ") + w) != std::string::npos; };
	uint32_t prefix = from / 1000000;
	if (prefix == 970 || prefix == 972 || prefix == 974)
		return has("ACTIVE") || has("MAYDAY") || has("SART") || has("MOB") || has("EPIRB") || has("DISTRESS") || has("HELP") ? EventRing::URGENT : EventRing::ROUTINE;
	if (has("MAYDAY") || has("SOS") || has("DISTRESS") || has("MOB") || has("OVERBOARD") || has("MAN OVER BOARD") || has("MAN OVERBOARD") ||
		has("SINKING") || wordStarts("CAPSIZ") || has("FIRE") || has("EMERGENCY"))
		return EventRing::URGENT;
	if (has("PAN PAN") || has("PANPAN") || has("SECURITE") || has("ACCIDENT") || has("COLLISION") || has("AGROUND") || has("GROUND") ||
		has("GROUNDING") || has("DANGER") || has("WARNING") || has("KEEP AWAY") || has("STAY AWAY") || has("KEEP CLEAR") ||
		has("NOT UNDER COMMAND") || has("NUC"))
		return EventRing::NOTICE;
	return EventRing::ROUTINE;
}

// a change quiets the vessel for a while; true when it already was, so a value
// flapping between two decodes never gets out
static bool settle(Ship &ship, std::time_t now)
{
	bool quiet = ship.quiet_until > now;
	ship.quiet_until = MAX(ship.quiet_until, now + CHANGE_SETTLE_S);
	return quiet;
}

void DB::note(const Ship &ship, EventRing::Kind kind, EventRing::Level level, std::time_t now, const std::string &text,
			  const std::string &label, uint32_t to, const std::string &was)
{
	EventRing::Event e;
	e.kind = kind;
	e.level = level;
	e.from = ship.mmsi;
	e.to = to;
	e.time = now;
	e.lat = ship.lat;
	e.lon = ship.lon;
	e.text = text;
	e.was = was;
	e.label = label;
	events.push(e);
}

// a test silences its sender for a while: what follows an exercise is the exercise;
// the quiet after a test or a change holds a notice back, never a distress call
void DB::noteSafety(Ship &ship, const JSON::JSON &data)
{
	std::string text;
	uint32_t to = 0;
	for (const auto &p : data.getMembers())
	{
		if (p.Key() == AIS::KEY_TEXT)
			text = p.Get().getString();
		else if (p.Key() == AIS::KEY_DEST_MMSI)
			to = (uint32_t)p.Get().getInt();
	}
	if (!plausibleText(text))
		return;
	std::time_t now = std::time(nullptr);
	std::string w = words(text);
	if (w.find(" TEST ") != std::string::npos)
	{
		ship.quiet_until = now + TEST_MUTE_S;
		return;
	}
	EventRing::Level level = safetyLevel(ship.mmsi, w);
	if (level == EventRing::ROUTINE || (level == EventRing::NOTICE && ship.quiet_until > now))
		return;
	note(ship, EventRing::SAFETY, level, now, text, std::string(), to);
}

// a value that names no place: a vessel leaving or arriving at one is no news
static bool namesAPlace(const std::string &text)
{
	static const char *NOTHING[] = {"UNKNOWN", "UNK", "NA", "NONE", "NIL", "NOTAVAILABLE", "NODESTINATION", "NODEST", "TBA", "TBD", "TBN", "UNSPECIFIED"};
	if (!plausibleText(text))
		return false;
	std::string s;
	for (char c : text)
		if (std::isalnum((unsigned char)c))
			s += (char)std::toupper((unsigned char)c);
	for (const char *w : NOTHING)
		if (s == w)
			return false;
	return true;
}

// the event is the change, so the first destination a vessel reports only
// starts the clock, and one that stood for nothing knowable is left alone
void DB::noteDestination(Ship &ship, const std::string &v)
{
	if (v.empty() || v == ship.destination)
		return;
	std::time_t now = std::time(nullptr);
	std::string was = ship.destination;
	if (settle(ship, now) || !namesAPlace(v) || !namesAPlace(was))
		return;
	note(ship, EventRing::DESTINATION, EventRing::ROUTINE, now, v, "destination", 0, was);
}

// a vessel's draught, in metres as the message gives it: a change is news, the
// first reading only a baseline
void DB::noteDraught(Ship &ship, float d)
{
	if (ship.draught == DRAUGHT_UNDEFINED || ship.draught <= 0 || d <= 0 || d == ship.draught)
		return;
	std::time_t now = std::time(nullptr);
	if (settle(ship, now))
		return;
	char was[16], to[16];
	std::snprintf(was, sizeof(was), "%.1f m", ship.draught);
	std::snprintf(to, sizeof(to), "%.1f m", d);
	note(ship, EventRing::DRAUGHT, EventRing::ROUTINE, now, to, "draught", 0, was);
}

// a vessel's navigation status, named the way the message names it; the status
// it starts out with, and one going undefined, only start the clock
void DB::noteStatus(Ship &ship, int status)
{
	const std::vector<std::string> &names = AIS::LookupTable_nav_status;
	if (status == ship.status || status < 0 || status >= (int)names.size())
		return;
	std::time_t now = std::time(nullptr);
	int had = ship.status;
	if (settle(ship, now) || had < 0 || had >= (int)names.size() || had == STATUS_UNDEFINED || status == STATUS_UNDEFINED)
		return;
	note(ship, EventRing::STATUS, EventRing::ROUTINE, now, names[status], "status", 0, names[had]);
}

std::string DB::getEventsJSON(uint64_t since, int level)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		std::time_t now = time(nullptr);
		w.beginObject().kv("time", now).kv("seq", (long long)events.sequence());
		events.writeSince(w, since, level, now);
		w.endObject();
	}
	return content;
}

std::string DB::getObjectJSON(const std::string &key)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		std::time_t now = time(nullptr);
		if (!key.empty() && key[0] == 's')
		{
			w.beginObject().kv("time", now);
			stations.writeOne(w, std::atoi(key.c_str() + 1));
			w.endObject();
		}
		else
			binary.writeJSON(w, now, 0, std::strtoull(key.c_str(), nullptr, 16), 0);
	}
	return content;
}

std::string DB::getMapObjectsJSON(uint64_t since)
{
	std::lock_guard<std::mutex> lock(mtx);
	content.clear();
	{
		JSON::Writer w(content, 4096);
		std::time_t now = time(nullptr);
		binary.refresh(now);
		w.beginObject().kv("time", now).kv("seq", (long long)binary.sequence()).key("objects").beginArray();
		binary.writeMarkerRows(w, since);
		stations.writeRows(w, since);
		w.endArray().key("removed").beginArray();
		binary.writeRemoved(w, since);
		stations.writeRemoved(w, since);
		w.endArray().endObject();
	}
	return content;
}

// a recycled slot may still own the evicted ship's track, so every create
// is paired with a path wipe
void DB::putShip(const Ship &s)
{
	if (s.mmsi == 0)
		return;
	std::lock_guard<std::mutex> lock(mtx);
	int ptr = claimShip(s.mmsi);
	ships[ptr] = s;
	ships[ptr].region = isValidCoord(s.lat, s.lon) ? Region::find(s.lat, s.lon) : Region::NONE;
}

int DB::claimShip(uint32_t mmsi)
{
	int ptr = ships.find(mmsi);
	if (ptr == SHIP_NIL)
	{
		ptr = ships.create(mmsi);
		// the recycled record still holds the evicted ship
		evict_horizon = MAX(evict_horizon, ships[ptr].last_signal);
		paths.wipe(ptr);
		changes.wipe(ptr);
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

	// A copy of a transmission the record already took: it must not count, move
	// the ship, or reorder the table - find() instead of claimShip(), because a
	// touch without a fresh stamp breaks the newest-first walk. An unknown ship
	// is not created here either (a reset record at the head does the same
	// damage), but the message still travels with its cleared tag.
	bool copy = quality_mask && (tag.quality & quality_mask);

	int ptr;
	if (copy)
	{
		copies_dropped++;
		ptr = ships.find(msg->mmsi());
		if (ptr == SHIP_NIL)
		{
			lock.unlock();
			Send(data, len, tag);
			return;
		}
	}
	else
		ptr = claimShip(msg->mmsi());

	// update ship and tag data
	Ship &ship = ships[ptr];

	// save some data for later on
	tag.previous_signal = ship.last_signal;

	float lat_old = ship.lat;
	float lon_old = ship.lon;

	bool newValidPosition = !copy && updateShip(data[0], tag, ship) && isValidCoord(ship.lat, ship.lon);

	if (!copy && (type == 6 || type == 8 || type == 12 || type == 14 || type == 23))
	{
		int h = binary.process(data[0], ship.lat, ship.lon);
		if (h >= 0)
			binary.settle(h, [&](uint32_t m) { return ships.find(m) != SHIP_NIL; });
		if (type == 12 || type == 14)
			noteSafety(ship, data[0]);
	}

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
