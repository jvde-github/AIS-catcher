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
#include "JSON/JSONBuilder.h"

#include <fstream>

//-----------------------------------
// simple ship database

void DB::setup()
{

	if (server_mode)
	{
		Nships *= 32;
		Npaths *= 32;

		Info() << "DB: internal ship database extended to " << Nships << " ships and " << Npaths << " path points";
	}

	ships.resize(Nships);
	paths.resize(Npaths);

	first = Nships - 1;
	last = 0;
	count = 0;

	// set up linked list
	for (int i = 0; i < Nships; i++)
	{
		ships[i].next = i - 1;
		ships[i].prev = i + 1;
	}
	ships[Nships - 1].prev = -1;
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

void DB::getBinary(std::vector<char> &v)
{
	std::lock_guard<std::mutex> lock(mtx);

	Util::Serialize::Uint64(time(nullptr), v);
	Util::Serialize::Int32(count, v);

	if (latlon_share && isValidCoord(lat, lon))
	{
		Util::Serialize::Int8(1, v);
		Util::Serialize::LatLon(lat, lon, v);
		Util::Serialize::Uint32(own_mmsi, v);
	}
	else
	{
		Util::Serialize::Int8(0, v);
	}

	int ptr = first;

	delim = "";
	while (ptr != -1)
	{
		const Ship &ship = ships[ptr];
		if (ship.mmsi != 0)
		{
			ship.Serialize(v);
		}
		ptr = ships[ptr].next;
	}
}

// add member to get JSON in form of array with values and keys separately
std::string DB::getJSONcompact(bool full)
{
	std::lock_guard<std::mutex> lock(mtx);

	JSON::JSONBuilder json;
	json.start()
		.add("count", count);

	if (latlon_share && isValidCoord(lat, lon))
	{
		json.key("station")
			.start()
			.add("lat", lat)
			.add("lon", lon)
			.add("mmsi", own_mmsi)
			.end();
	}

	json.key("values")
		.startArray();

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

			ship.toJSONArray(json, delta_time, lat, lon);
		}
		ptr = ships[ptr].next;
	}

	json.endArray()
		.add("error", false)
		.end()
		.nl()
		.nl();

	return json.str();
}

std::string DB::getJSON(bool full)
{
	std::lock_guard<std::mutex> lock(mtx);

	JSON::JSONBuilder json;
	json.start()
		.add("count", count);

	if (latlon_share)
	{
		json.key("station")
			.start()
			.add("lat", lat)
			.add("lon", lon)
			.add("mmsi", own_mmsi)
			.end();
	}

	json.key("ships")
		.startArray();

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

			json.start();
			ship.toJSON(json, delta_time, lat, lon);
			json.end();
		}
		ptr = ships[ptr].next;
	}

	json.endArray()
		.add("error", false)
		.end()
		.nl()
		.nl();

	return json.str();
}

std::string DB::getShipJSON(int mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);

	int ptr = findShip(mmsi);
	if (ptr == -1)
		return "{}";

	const Ship &ship = ships[ptr];
	long int delta_time = (long int)time(nullptr) - (long int)ship.last_signal;

	JSON::JSONBuilder json;
	json.start();
	ship.toJSON(json, delta_time, lat, lon);
	json.end();
	return json.str();
}

std::string DB::getKML()
{
	std::lock_guard<std::mutex> lock(mtx);

	std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>";
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
			ship.getKML(s);
		}
		ptr = ships[ptr].next;
	}
	s += "</Document></kml>";
	return s;
}

std::string DB::getGeoJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	JSON::JSONBuilder json;
	json.start()
		.add("type", "FeatureCollection")
		.add("time_span", TIME_HISTORY)
		.key("features")
		.startArray();

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

			if (ship.lat != LAT_UNDEFINED && ship.lon != LON_UNDEFINED && !(ship.lat == 0 && ship.lon == 0))
			{
				ship.getGeoJSON(json);
			}
		}
		ptr = ships[ptr].next;
	}

	json.endArray()
		.end();

	return json.str();
}

std::string DB::getAllPathJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	JSON::JSONBuilder json;
	json.start();

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

			std::string path = getSinglePathJSON(ptr);
			json.key(std::to_string(ship.mmsi))
				.valueRaw(path);
		}
		ptr = ships[ptr].next;
	}

	json.end()
		.nl()
		.nl();

	return json.str();
}

std::string DB::getSinglePathJSON(int idx)
{
	uint32_t mmsi = ships[idx].mmsi;
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;

	JSON::JSONBuilder json;
	json.startArray();

	while (isNextPathPoint(ptr, mmsi, t))
	{
		if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
		{
			json.startArray()
				.value(paths[ptr].lat)
				.value(paths[ptr].lon)
				.value(paths[ptr].timestamp_start)
				.value(paths[ptr].timestamp_end)
				.endArray();
		}
		t = paths[ptr].count;
		ptr = paths[ptr].next;
	}

	json.endArray();
	return json.str();
}

std::string DB::getSinglePathGeoJSON(int idx)
{
	uint32_t mmsi = ships[idx].mmsi;
	int ptr = ships[idx].path_ptr;
	int t = ships[idx].count + 1;

	JSON::JSONBuilder json;
	json.start()
		.add("type", "Feature")
		.key("geometry")
		.start()
		.add("type", "LineString")
		.key("coordinates")
		.startArray();

	std::vector<std::time_t> ts_start, ts_end;

	while (isNextPathPoint(ptr, mmsi, t))
	{
		if (isValidCoord(paths[ptr].lat, paths[ptr].lon))
		{
			// GeoJSON uses [longitude, latitude] format (note the order!)
			json.startArray()
				.value(paths[ptr].lon)
				.value(paths[ptr].lat)
				.endArray();

			ts_start.push_back(paths[ptr].timestamp_start);
			ts_end.push_back(paths[ptr].timestamp_end);
		}
		t = paths[ptr].count;
		ptr = paths[ptr].next;
	}

	json.endArray()
		.end()
		.key("properties")
		.start()
		.add("mmsi", mmsi)
		.key("timestamps_start")
		.startArray();

	for (auto ts : ts_start)
		json.value(ts);

	json.endArray()
		.key("timestamps_end")
		.startArray();

	for (auto ts : ts_end)
		json.value(ts);

	json.endArray()
		.end()
		.end();

	return json.str();
}

std::string DB::getPathJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = findShip(mmsi);
	if (idx == -1)
		return "[]";
	return getSinglePathJSON(idx);
}

std::string DB::getPathGeoJSON(uint32_t mmsi)
{
	std::lock_guard<std::mutex> lock(mtx);
	int idx = findShip(mmsi);
	if (idx == -1)
		return "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[]},\"properties\":{\"mmsi\":" + std::to_string(mmsi) + "}}";
	return getSinglePathGeoJSON(idx);
}

std::string DB::getAllPathGeoJSON()
{
	std::lock_guard<std::mutex> lock(mtx);

	JSON::JSONBuilder json;
	json.start()
		.add("type", "FeatureCollection")
		.key("features")
		.startArray();

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

			std::string feature = getSinglePathGeoJSON(ptr);
			json.valueRaw(feature);
		}
		ptr = ships[ptr].next;
	}

	json.endArray()
		.end()
		.nl()
		.nl();

	return json.str();
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
	int ptr = first, cnt = count;
	while (ptr != -1 && --cnt >= 0)
	{
		if (ships[ptr].mmsi == mmsi)
			return ptr;
		ptr = ships[ptr].next;
	}
	return -1;
}

int DB::createShip()
{
	int ptr = last;
	count = MIN(count + 1, Nships);
	ships[ptr].reset();

	return ptr;
}

void DB::moveShipToFront(int ptr)
{
	if (ptr == first)
		return;

	// remove ptr out of the linked list
	if (ships[ptr].next != -1)
		ships[ships[ptr].next].prev = ships[ptr].prev;
	else
		last = ships[ptr].prev;
	ships[ships[ptr].prev].next = ships[ptr].next;

	// new ship is first in list
	ships[ptr].next = first;
	ships[ptr].prev = -1;

	ships[first].prev = ptr;
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

bool DB::updateFields(const JSON::Property &p, const AIS::Message *msg, Ship &v, bool allowApproximate)
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
			v.shiptype = p.Get().getInt();
		break;
	case AIS::KEY_IMO:
		v.IMO = p.Get().getInt();
		break;
	case AIS::KEY_MONTH:
		if (msg->type() != 5)
			break;
		v.month = (char)p.Get().getInt();
		break;
	case AIS::KEY_DAY:
		if (msg->type() != 5)
			break;
		v.day = (char)p.Get().getInt();
		break;
	case AIS::KEY_MINUTE:
		if (msg->type() != 5)
			break;
		v.minute = (char)p.Get().getInt();
		break;
	case AIS::KEY_HOUR:
		if (msg->type() != 5)
			break;
		v.hour = (char)p.Get().getInt();
		break;
	case AIS::KEY_HEADING:
		v.heading = p.Get().getInt();
		break;
	case AIS::KEY_DRAUGHT:
		if (p.Get().getFloat() != 0)
			v.draught = p.Get().getFloat();
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
		break;
	case AIS::KEY_TO_STERN:
		v.to_stern = p.Get().getInt();
		break;
	case AIS::KEY_TO_PORT:
		v.to_port = p.Get().getInt();
		break;
	case AIS::KEY_TO_STARBOARD:
		v.to_starboard = p.Get().getInt();
		break;
	case AIS::KEY_RECEIVED_STATIONS:
		v.received_stations = p.Get().getInt();
		break;
	case AIS::KEY_ALT:
		v.altitude = p.Get().getInt();
		break;
	case AIS::KEY_VIRTUAL_AID:
		v.setVirtualAid(p.Get().getBool());
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
#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
	case AIS::KEY_NAME:
	case AIS::KEY_SHIPNAME:
		std::strncpy(v.shipname, p.Get().getString().c_str(), sizeof(v.shipname) - 1);
		v.shipname[sizeof(v.shipname) - 1] = '\0';
		break;
	case AIS::KEY_CALLSIGN:
		std::strncpy(v.callsign, p.Get().getString().c_str(), sizeof(v.callsign) - 1);
		v.callsign[sizeof(v.callsign) - 1] = '\0';
		break;
	case AIS::KEY_COUNTRY_CODE:
		std::strncpy(v.country_code, p.Get().getString().c_str(), sizeof(v.country_code) - 1);
		v.country_code[sizeof(v.country_code) - 1] = '\0';
		break;
	case AIS::KEY_DESTINATION:
		std::strncpy(v.destination, p.Get().getString().c_str(), sizeof(v.destination) - 1);
		v.destination[sizeof(v.destination) - 1] = '\0';
		break;
#pragma GCC diagnostic pop
	}
	return position_updated;
}

bool DB::updateShip(const JSON::JSON &data, TAG &tag, Ship &ship)
{
	const AIS::Message *msg = (AIS::Message *)data.binary;

	// determine whether we accept msg 27 to update lat/lon
	bool allowApproxLatLon = false, positionUpdated = false;

	if (msg->type() == 27)
	{
		int timeout = 10 * 60;

		if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
			timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

		if (msg->getRxTimeUnix() - ship.last_signal > timeout)
			allowApproxLatLon = true;
	}

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.group_mask |= tag.group;
	ship.last_group = tag.group;

	ship.last_signal = msg->getRxTimeUnix();

	if (msg->repeat() == 0)
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
	ship.msg_type |= 1 << msg->type();

	if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
		ship.orOpChannels(1 << (msg->getChannel() - 'A'));

	for (const auto &p : data.getProperties())
		positionUpdated |= updateFields(p, msg, ship, allowApproxLatLon);

	ship.setType();

	if (positionUpdated)
	{
		ship.setApproximate(msg->type() == 27);

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
	for (const auto &p : data.getProperties())
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

	// if (binmsg.dac != -1 && binmsg.fi != -1)
	if (binmsg.dac == 1 && binmsg.fi == 31)
	{
		binmsg.json.clear();
		builder.stringify(data, binmsg.json);
		binmsg.used = true;
		if (isValidCoord(loc_lat, loc_lon))
		{
			binmsg.lat = loc_lat;
			binmsg.lon = loc_lon;

			// switch off approximation of mmsi location
			if (false && !isValidCoord(ship.lat, ship.lon))
			{
				position_updated = true;
				ship.lat = loc_lat;
				ship.lon = loc_lon;
			}
		}
		binmsg.timestamp = msg->getRxTimeUnix();
		binaryMsgIndex = (binaryMsgIndex + 1) % MAX_BINARY_MESSAGES;
	}
}

std::string DB::getBinaryMessagesJSON() const
{
	JSON::JSONBuilder json;
	json.startArray();

	// Start from newest message and go backward
	int startIndex = (binaryMsgIndex + MAX_BINARY_MESSAGES - 1) % MAX_BINARY_MESSAGES;
	std::time_t tm = time(nullptr);

	for (int i = 0; i < MAX_BINARY_MESSAGES; i++)
	{
		int idx = (startIndex - i + MAX_BINARY_MESSAGES) % MAX_BINARY_MESSAGES;
		const BinaryMessage &msg = binaryMessages[idx];

		if (!msg.used || (long int)tm - (long int)msg.timestamp > TIME_HISTORY)
			continue;

		json.start()
			.add("type", msg.type)
			.add("dac", msg.dac)
			.add("fi", msg.fi)
			.add("timestamp", msg.timestamp)
			.addRaw("message", msg.json)
			.end();
	}

	json.endArray();
	return json.str();
}

void DB::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	if (!filter.include(*(AIS::Message *)data[0].binary))
		return;

	std::lock_guard<std::mutex> lock(mtx);

	const AIS::Message *msg = (AIS::Message *)data[0].binary;
	int type = msg->type();

	if (type < 1 || type > 27 || msg->mmsi() == 0)
		return;

	// setup/find ship in database
	int ptr = findShip(msg->mmsi());

	if (ptr == -1)
		ptr = createShip();

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
	std::strcpy(tag.shipname, ship.shipname);

	if (position_updated && isValidCoord(lat_old, lon_old))
	{
		// flat earth approximation, roughly 10 nmi
		float d = (ship.lat - lat_old) * (ship.lat - lat_old) + (ship.lon - lon_old) * (ship.lon - lon_old);
		tag.validated = d < 0.1675;
		ships[ptr].setValidated(tag.validated ? 1 : 2);
	}
	else
		tag.validated = false;

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
		ptr = ships[ptr].next;
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

		ptr = ships[ptr].prev;
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

	// Load ships and validate chronological order (oldest first, then newer)
	std::time_t previous_signal = 0;
	for (int i = 0; i < ship_count; i++)
	{
		Ship temp_ship;

		if (!temp_ship.Load(file))
		{
			std::cout << "DB: Failed to read ship " << i << " from backup file." << std::endl;
			return false;
		}

		// Validate chronological order - ships should be oldest first
		if (i > 0 && temp_ship.last_signal < previous_signal)
		{
			Error() << "DB: Ships not in chronological order at index " << i;
			return false;
		}

		previous_signal = temp_ship.last_signal;

		// Find or create ship entry using existing mechanisms
		int ptr = findShip(temp_ship.mmsi);
		if (ptr == -1)
			ptr = createShip();

		moveShipToFront(ptr);

		// Copy ship data while preserving linked list pointers
		int next_ptr = ships[ptr].next;
		int prev_ptr = ships[ptr].prev;

		ships[ptr] = temp_ship;

		ships[ptr].next = next_ptr;
		ships[ptr].prev = prev_ptr;
	}

	Info() << "DB: Restored " << ship_count << " ships from backup";
	return true;
}
