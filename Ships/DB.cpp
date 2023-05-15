/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include "DB.h"

//-----------------------------------
// simple ship database

void DB::setup(float lt, float ln) {

	if (server_mode) {
		N *= 32;
		M *= 32;

		std::cerr << "DB: internal ship database extended to " << N << " ships and " << M << " path points" << std::endl;
	}

	ships.resize(N);
	std::memset(ships.data(), 0, N * sizeof(ShipList));
	paths.resize(M);
	std::memset(paths.data(), 0, M * sizeof(PathList));

	lat = lt;
	lon = ln;

	first = N - 1;
	last = 0;
	count = 0;
	// set up linked list
	for (int i = 0; i < N; i++) {
		ships[i].next = i - 1;
		ships[i].prev = i + 1;
	}
	ships[N - 1].prev = -1;
}

bool DB::isValidCoord(float lat, float lon) {
	return !(lat == 0 && lon == 0) && lat != 91 && lon != 181;
}

// https://www.movable-type.co.uk/scripts/latlong.html
void DB::getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float& distance, int& bearing) {
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

// add member to get JSON in form of array with values and keys seperately
std::string DB::getJSONcompact(bool full) {
	const std::string null_str = "null";
	const std::string comma = ",";
	std::string str;

	content = "{\"count\":" + std::to_string(count) + comma;
	if (latlon_share)
		content += "\"station\":{\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) + "},";

	content += "\"values\":[";

	std::time_t tm = time(nullptr);
	int ptr = first;

	delim = "";
	while (ptr != -1) {
		const VesselDetail ship = ships[ptr].ship;
		if (ship.mmsi != 0) {
			long int delta_time = (long int)tm - (long int)ship.last_signal;
			if (!full && delta_time > TIME_HISTORY) break;

			content += delim + "[" + std::to_string(ship.mmsi) + comma;
			if (isValidCoord(ship.lat, ship.lon)) {
				content += std::to_string(ship.lat) + comma;
				content += std::to_string(ship.lon) + comma;

				if (isValidCoord(lat, lon)) {
					content += std::to_string(ship.distance) + comma;
					content += std::to_string(ship.angle) + comma;
				}
				else {
					content += null_str + comma;
					content += null_str + comma;
				}
			}
			else {
				content += null_str + comma;
				content += null_str + comma;
				content += null_str + comma;
				content += null_str + comma;
			}

			// content += "\"mmsi_type\":" + std::to_string(ship.mmsi_type) + ",";
			content += std::to_string(ship.level) + comma;
			content += std::to_string(ship.count) + comma;
			content += std::to_string(ship.ppm) + comma;
			content += std::string(ship.approximate ? "true" : "false") + comma;

			content += ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) + comma;
			content += ((ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ship.cog)) + comma;
			content += ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) + comma;

			content += ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) + comma;
			content += ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) + comma;
			content += ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) + comma;
			content += ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) + comma;

			content += std::to_string(ship.shiptype) + comma;
			content += std::to_string(ship.validated) + comma;
			content += std::to_string(ship.msg_type) + comma;
			content += std::to_string(ship.channels) + comma;
			content += "\"" + std::string(ship.country_code) + "\",";
			content += std::to_string(ship.status) + comma;

			content += std::to_string(ship.draught) + comma;

			content += ((ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ship.month)) + comma;
			content += ((ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ship.day)) + comma;
			content += ((ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ship.hour)) + comma;
			content += ((ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ship.minute)) + comma;

			content += ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) + comma;

			str = std::string(ship.callsign);
			JSON::StringBuilder::stringify(str, content);

			content += comma;
			str = std::string(ship.shipname) + (ship.virtual_aid ? std::string(" [V]") : std::string(""));
			JSON::StringBuilder::stringify(str, content);

			content += comma;
			str = std::string(ship.destination);
			JSON::StringBuilder::stringify(str, content);

			content += comma + std::to_string(delta_time) + "]";
			delim = comma;
		}
		ptr = ships[ptr].next;
	}
	content += "],\"error\":false}\n\n";
	return content;
}

void DB::getShipJSON(const VesselDetail &ship, std::string &content, long int delta_time) {
	const std::string null_str = "null";
	std::string str;

	content += "{\"mmsi\":" + std::to_string(ship.mmsi) + ",";
	if (isValidCoord(ship.lat, ship.lon)) {
		content += "\"lat\":" + std::to_string(ship.lat) + ",";
		content += "\"lon\":" + std::to_string(ship.lon) + ",";

		if (isValidCoord(lat, lon)) {
			content += "\"distance\":" + std::to_string(ship.distance) + ",";
			content += "\"bearing\":" + std::to_string(ship.angle) + ",";
		}
		else {
			content += "\"distance\":null,";
			content += "\"bearing\":null,";
		}
	}
	else {
		content += "\"lat\":null,";
		content += "\"lon\":null,";
		content += "\"distance\":null,";
		content += "\"bearing\":null,";
	}

	// content += "\"mmsi_type\":" + std::to_string(ship.mmsi_type) + ",";
	content += "\"level\":" + std::to_string(ship.level) + ",";
	content += "\"count\":" + std::to_string(ship.count) + ",";
	content += "\"ppm\":" + std::to_string(ship.ppm) + ",";
	content += "\"approx\":" + std::string(ship.approximate ? "true" : "false") + ",";

	content += "\"heading\":" + ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) + ",";
	content += "\"cog\":" + ((ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ship.cog)) + ",";
	content += "\"speed\":" + ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) + ",";

	content += "\"to_bow\":" + ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) + ",";
	content += "\"to_stern\":" + ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) + ",";
	content += "\"to_starboard\":" + ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) + ",";
	content += "\"to_port\":" + ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) + ",";

	content += "\"shiptype\":" + std::to_string(ship.shiptype) + ",";
	content += "\"validated\":" + std::to_string(ship.validated) + ",";
	content += "\"msg_type\":" + std::to_string(ship.msg_type) + ",";
	content += "\"channels\":" + std::to_string(ship.channels) + ",";
	content += "\"country\":\"" + std::string(ship.country_code) + "\",";
	content += "\"status\":" + std::to_string(ship.status) + ",";

	content += "\"draught\":" + std::to_string(ship.draught) + ",";

	content += "\"eta_month\":" + ((ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ship.month)) + ",";
	content += "\"eta_day\":" + ((ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ship.day)) + ",";
	content += "\"eta_hour\":" + ((ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ship.hour)) + ",";
	content += "\"eta_minute\":" + ((ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ship.minute)) + ",";

	content += "\"imo\":" + ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) + ",";

	content += "\"callsign\":";
	str = std::string(ship.callsign);
	JSON::StringBuilder::stringify(str, content);

	content += ",\"shipname\":";
	str = std::string(ship.shipname) + (ship.virtual_aid ? std::string(" [V]") : std::string(""));
	JSON::StringBuilder::stringify(str, content);

	content += ",\"destination\":";
	str = std::string(ship.destination);
	JSON::StringBuilder::stringify(str, content);

	content += ",\"last_signal\":" + std::to_string(delta_time) + "}";
}

std::string DB::getJSON(bool full) {
	const std::string null_str = "null";
	std::string str;

	content = "{\"count\":" + std::to_string(count);
	if (latlon_share)
		content += ",\"station\":{\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) + "}";
	content += ",\"ships\":[";

	std::time_t tm = time(nullptr);
	int ptr = first;

	delim = "";
	while (ptr != -1) {
		const VesselDetail ship = ships[ptr].ship;
		if (ship.mmsi != 0) {
			long int delta_time = (long int)tm - (long int)ship.last_signal;
			if (!full && delta_time > TIME_HISTORY) break;

			content += delim; 
			getShipJSON(ship, content, delta_time);
			delim = ",";
		}
		ptr = ships[ptr].next;
	}
	content += "],\"error\":false}\n\n";
	return content;
}
std::string DB::getShipJSON(int mmsi) {
	int ptr = findShip(mmsi);

	if (ptr == -1) return "{}";
	
	const VesselDetail ship = ships[ptr].ship;		
	long int delta_time = (long int)time(nullptr) - (long int)ship.last_signal;

	std::string content; 
	getShipJSON(ship, content, delta_time);
	return content;
}

std::string DB::getPathJSON(uint32_t mmsi) {
	const std::string null_str = "null";
	std::string str;

	int idx = -1;
	for (int i = 0; i < N && idx == -1; i++)
		if (ships[i].ship.mmsi == mmsi) idx = i;

	if (idx == -1) return "[]";

	content = "[";

	int ptr = ships[idx].ship.path_ptr;
	long int t0 = time(nullptr);

	long int t = t0;

	while (ptr != -1 && paths[ptr].mmsi == mmsi && (long int)paths[ptr].signal_time <= t) {
		t = (long int)paths[ptr].signal_time;

		if (isValidCoord(paths[ptr].lat, paths[ptr].lon)) {
			content += "{\"lat\":";
			content += std::to_string(paths[ptr].lat);
			content += ",\"lon\":";
			content += std::to_string(paths[ptr].lon);
			//content += ",\"received\":";
			//content += std::to_string(t0 - t);
			content += "},";
		}
		ptr = paths[ptr].next;
	}
	if (content != "[") content.pop_back();
	content += "]";
	return content;
}

std::string DB::getMessage(uint32_t mmsi) {
	int ptr = findShip(mmsi);
	if (ptr == -1 || !ships[ptr].ship.msg) return "";
	return *ships[ptr].ship.msg;
}

int DB::findShip(uint32_t mmsi) {
	int ptr = first, cnt = count;
	while (ptr != -1 && --cnt >= 0) {
		if (ships[ptr].ship.mmsi == mmsi) return ptr;
		ptr = ships[ptr].next;
	}
	return -1;
}

int DB::createShip() {
	int ptr = last;
	count = MIN(count + 1, N);
	if(ships[ptr].ship.msg)
		delete ships[ptr].ship.msg;

	ships[ptr].ship = VesselDetail();

	return ptr;
}

void DB::moveShipToFront(int ptr) {
	if (ptr == first) return;

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

void DB::addToPath(int ptr) {
	paths[path_idx].next = ships[ptr].ship.path_ptr;
	paths[path_idx].lat = ships[ptr].ship.lat;
	paths[path_idx].lon = ships[ptr].ship.lon;
	paths[path_idx].mmsi = ships[ptr].ship.mmsi;
	paths[path_idx].signal_time = ships[ptr].ship.last_signal;

	ships[ptr].ship.path_ptr = path_idx;
	path_idx = (path_idx + 1) % M;
}

bool DB::updateFields(const JSON::Property& p, const AIS::Message* msg, VesselDetail& v, bool allowApproximate) {
	bool position_updated = false;
	switch (p.Key()) {
	case AIS::KEY_LAT:
		if ((msg->type()) != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || v.approximate)) {
			v.lat = p.Get().getFloat();
			position_updated = true;
		}
		break;
	case AIS::KEY_LON:
		if ((msg->type()) != 8 && msg->type() != 17 && (msg->type() != 27 || allowApproximate || v.approximate)) {
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
		if (msg->type() != 5) break;
		v.month = p.Get().getInt();
		break;
	case AIS::KEY_DAY:
		if (msg->type() != 5) break;
		v.day = p.Get().getInt();
		break;
	case AIS::KEY_MINUTE:
		if (msg->type() != 5) break;
		v.minute = p.Get().getInt();
		break;
	case AIS::KEY_HOUR:
		if (msg->type() != 5) break;
		v.hour = p.Get().getInt();
		break;
	case AIS::KEY_HEADING:
		v.heading = p.Get().getInt();
		break;
	case AIS::KEY_DRAUGHT:
		if (p.Get().getFloat() != 0.0)
			v.draught = p.Get().getFloat();
		break;
	case AIS::KEY_COURSE:
		v.cog = p.Get().getFloat();
		break;
	case AIS::KEY_SPEED:
		if (msg->type() == 9 && p.Get().getInt() != 1023)
			v.speed = p.Get().getInt();
		else if (p.Get().getFloat() != 102.3)
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
	case AIS::KEY_VIRTUAL_AID:
		v.virtual_aid = p.Get().getBool();
		break;
	case AIS::KEY_NAME:
	case AIS::KEY_SHIPNAME:
		std::strncpy(v.shipname, p.Get().getString().c_str(), 20);
		break;
	case AIS::KEY_CALLSIGN:
		std::strncpy(v.callsign, p.Get().getString().c_str(), 7);
		break;
	case AIS::KEY_COUNTRY_CODE:
		std::strncpy(v.country_code, p.Get().getString().c_str(), 2);
		break;
	case AIS::KEY_DESTINATION:
		std::strncpy(v.destination, p.Get().getString().c_str(), 20);
		break;
	}
	return position_updated;
}

bool DB::updateShip(const JSON::JSON& data, TAG& tag, VesselDetail& ship) {
	const AIS::Message* msg = (AIS::Message*)data.binary;

	// determine whether we accept msg 27 to update lat/lon
	bool allowApproxLatLon = false, positionUpdated = false;

	if (msg->type() == 27) {
		int timeout = 10 * 60;

		if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
			timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

		if (msg->getRxTimeUnix() - ship.last_signal > timeout)
			allowApproxLatLon = true;
	}

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.last_signal = msg->getRxTimeUnix();
	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.msg_type |= 1 << msg->type();

	if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
		ship.channels |= 1 << (msg->getChannel() - 'A');

	for (const auto& p : data.getProperties())
		positionUpdated |= updateFields(p, msg, ship, allowApproxLatLon);

	if (positionUpdated)
		ship.approximate = msg->type() == 27;

	if(msg_save) {
		if(!ship.msg) ship.msg = new std::string();
		ship.msg->clear();
		builder.stringify(data, *ship.msg);
	}
	return positionUpdated;
}

void DB::addValidation(int ptr, TAG& tag, float lat, float lon) {
}

void DB::Receive(const JSON::JSON* data, int len, TAG& tag) {

	const AIS::Message* msg = (AIS::Message*)data[0].binary;
	int type = msg->type();

	if (type < 1 || type > 27 || msg->mmsi() == 0) return;

	int ptr = findShip(msg->mmsi());

	if (ptr == -1)
		ptr = createShip();

	moveShipToFront(ptr);

	VesselDetail& ship = ships[ptr].ship;
	float lat_old = ship.lat;
	float lon_old = ship.lon;

	bool position_updated = updateShip(data[0], tag, ship);
	position_updated &= isValidCoord(ship.lat, ship.lon);

	if (type == 1 || type == 2 || type == 3 || type == 18 || type == 19 || type == 9)
		addToPath(ptr);

	// add check to update only when new lat/lon
	if (position_updated && isValidCoord(lat, lon)) {
		getDistanceAndBearing(lat, lon, ship.lat, ship.lon, ship.distance, ship.angle);

		tag.distance = ship.distance;
		tag.angle = ship.angle;
	}
	else {
		tag.distance = DISTANCE_UNDEFINED;
		tag.angle = ANGLE_UNDEFINED;
	}

	if (position_updated && isValidCoord(lat_old, lon_old)) {
		// flat earth approximation, roughly 10 nmi
		float d = (ship.lat - lat_old) * (ship.lat - lat_old) + (ship.lon - lon_old) * (ship.lon - lon_old);
		tag.validated = d < 0.1675;
		ships[ptr].ship.validated = tag.validated ? 1 : -1;
	}
	else
		tag.validated = false;

	Send(data, len, tag);
}
