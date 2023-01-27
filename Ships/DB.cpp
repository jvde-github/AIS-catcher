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

std::string DB::getJSON(bool full) {
	const std::string null_str = "null";
	std::string str;

	content = "{\"count\":" + std::to_string(count);
	content += ",\"ships\":[";

	std::time_t tm = time(nullptr);
	int ptr = first;

	delim = "";
	while (ptr != -1) {
		if (ships[ptr].ship.mmsi != 0) {
			long int delta_time = (long int)tm - (long int)ships[ptr].ship.last_signal;
			if (!full && delta_time > TIME_HISTORY) break;

			content += delim + "{\"mmsi\":" + std::to_string(ships[ptr].ship.mmsi) + ",";
			if (isValidCoord(ships[ptr].ship.lat, ships[ptr].ship.lon)) {
				content += "\"lat\":" + std::to_string(ships[ptr].ship.lat) + ",";
				content += "\"lon\":" + std::to_string(ships[ptr].ship.lon) + ",";

				if (isValidCoord(lat, lon)) {
					content += "\"distance\":" + std::to_string(ships[ptr].ship.distance) + ",";
					content += "\"bearing\":" + std::to_string(ships[ptr].ship.angle) + ",";
				}
				else {
					content += "\"distance\":null,";
					content += "\"bearing\":null,";
				}
			}
			else {
				content += "\"lat\":" + null_str + ",";
				content += "\"lon\":" + null_str + ",";
				content += "\"distance\":null,";
				content += "\"bearing\":null,";
			}

			content += "\"mmsi_type\":" + std::to_string(ships[ptr].ship.mmsi_type) + ",";
			content += "\"level\":" + std::to_string(ships[ptr].ship.level) + ",";
			content += "\"count\":" + std::to_string(ships[ptr].ship.count) + ",";
			content += "\"ppm\":" + std::to_string(ships[ptr].ship.ppm) + ",";

			content += "\"heading\":" + ((ships[ptr].ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.heading)) + ",";
			content += "\"cog\":" + ((ships[ptr].ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.cog)) + ",";
			content += "\"speed\":" + ((ships[ptr].ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.speed)) + ",";

			content += "\"to_bow\":" + ((ships[ptr].ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.to_bow)) + ",";
			content += "\"to_stern\":" + ((ships[ptr].ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.to_stern)) + ",";
			content += "\"to_starboard\":" + ((ships[ptr].ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.to_starboard)) + ",";
			content += "\"to_port\":" + ((ships[ptr].ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.to_port)) + ",";

			content += "\"shiptype\":" + std::to_string(ships[ptr].ship.shiptype) + ",";
			content += "\"validated\":" + std::to_string(ships[ptr].ship.validated) + ",";
			content += "\"msg_type\":" + std::to_string(ships[ptr].ship.msg_type) + ",";
			content += "\"country\":\"" + std::string(ships[ptr].ship.country_code) + "\",";
			content += "\"status\":" + std::to_string(ships[ptr].ship.status) + ",";

			content += "\"draught\":" + std::to_string(ships[ptr].ship.draught) + ",";


			content += "\"eta_month\":" + ((ships[ptr].ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.month)) + ",";
			content += "\"eta_day\":" + ((ships[ptr].ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.day)) + ",";
			content += "\"eta_hour\":" + ((ships[ptr].ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.hour)) + ",";
			content += "\"eta_minute\":" + ((ships[ptr].ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.minute)) + ",";

			content += "\"imo\":" + ((ships[ptr].ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.IMO)) + ",";


			content += "\"callsign\":";
			str = std::string(ships[ptr].ship.callsign);
			JSON::StringBuilder::stringify(str, content);

			content += ",\"shipname\":";
			str = std::string(ships[ptr].ship.shipname) + (ships[ptr].ship.virtual_aid ? std::string(" [V]") : std::string(""));
			JSON::StringBuilder::stringify(str, content);

			content += ",\"destination\":";
			str = std::string(ships[ptr].ship.destination);
			JSON::StringBuilder::stringify(str, content);


			content += ",\"last_signal\":" + std::to_string(delta_time) + "}";
			delim = ",";
		}
		ptr = ships[ptr].next;
	}
	content += "],\"error\":false}";
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
			content += ",\"received\":";
			content += std::to_string(t0 - t);
			content += "},";
		}
		ptr = paths[ptr].next;
	}
	if (content != "[") content.pop_back();
	content += "]";
	return content;
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

int DB::setMmsiType(int mmsi, int type) {

	// type derived from MMSI
	if ((mmsi >= 200000000 && mmsi < 800000000) || (mmsi >= 20000000 && mmsi < 80000000) || ((mmsi >= 982000000 && mmsi < 988000000)))
		return (type == 18 || type == 19 || type == 24) ? MMSI_TYPE_CLASS_B : MMSI_TYPE_CLASS_A; // Class A & Class B
	else if ((mmsi >= 2000000 && mmsi < 8000000))
		return MMSI_TYPE_BASESTATION; // Base Station
	else if ((mmsi >= 111000000 && mmsi < 111999999) || (mmsi >= 11100000 && mmsi < 11199999))
		return MMSI_TYPE_SAR; // helicopter
	else if (mmsi >= 97000000 && mmsi < 98000000)
		return MMSI_TYPE_SARTEPIRB; //  AIS SART/EPIRB/man-overboard
	else if (mmsi >= 992000000 && mmsi < 998000000)
		return MMSI_TYPE_ATON; //  AtoN
	else
		return MMSI_TYPE_OTHER; // anything else
}

bool DB::updateFields(const JSON::Property& p, const AIS::Message* msg, VesselDetail& v) {
	bool position_updated = false;
	switch (p.Key()) {
	case AIS::KEY_LAT:
		v.lat = p.Get().getFloat();
		position_updated = true;
		break;
	case AIS::KEY_LON:
		v.lon = p.Get().getFloat();
		position_updated = true;
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

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.last_signal = msg->getRxTimeUnix();
	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.msg_type |= 1 << msg->type();
	ship.mmsi_type = setMmsiType(msg->mmsi(), msg->type());

	bool position_updated = false;

	for (const auto& p : data.getProperties())
		position_updated |= updateFields(p, msg, ship);

	return position_updated;
}

void DB::addValidation(int ptr, TAG& tag, float lat, float lon) {
}

void DB::Receive(const JSON::JSON* data, int len, TAG& tag) {

	const AIS::Message* msg = (AIS::Message*)data[0].binary;
	int type = msg->type();

	if (type < 1 || type > 27) return;

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
