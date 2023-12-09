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

#pragma once
#include <iostream>
#include <string.h>
#include <memory>

#include "AIS.h"
#include "JSONAIS.h"

#include "Keys.h"
#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

const float DISTANCE_UNDEFINED = -1;
const float LAT_UNDEFINED = 91;
const float LON_UNDEFINED = 181;
const float COG_UNDEFINED = 360;
const float SPEED_UNDEFINED = -1;
const float HEADING_UNDEFINED = 511;

const int STATUS_UNDEFINED = 15;
const int DIMENSION_UNDEFINED = -1;
const int ETA_DAY_UNDEFINED = 0;
const int ETA_MONTH_UNDEFINED = 0;
const int ETA_HOUR_UNDEFINED = 24;
const int ETA_MINUTE_UNDEFINED = 60;
const int IMO_UNDEFINED = 0;
const int ANGLE_UNDEFINED = -1;

struct PathPoint {
	float lat, lon;
	uint32_t mmsi = 0;
	std::time_t signal_time = (std::time_t)0;
	int next = 0;
};

struct Ship {
	int prev, next;
	uint32_t mmsi;
	int count, msg_type, channels, shiptype, heading, status, virtual_aid, path_ptr;
	int to_port, to_bow, to_starboard, to_stern, IMO, angle, validated;
	char month, day, hour, minute;
	float lat, lon, ppm, level, speed, cog, draught, distance;
	std::time_t last_signal;
	bool approximate;
	char shipname[21], destination[21], callsign[8], country_code[3];
	std::string msg;
	uint64_t last_group, group_mask;

	void reset() {
		mmsi = count = msg_type = channels = shiptype = validated = virtual_aid = group_mask = 0;
		path_ptr = -1;

		heading = HEADING_UNDEFINED;
		status = STATUS_UNDEFINED;
		to_port = to_bow = to_starboard = to_stern = DIMENSION_UNDEFINED;
		IMO = IMO_UNDEFINED;
		angle = ANGLE_UNDEFINED;
		month = ETA_MONTH_UNDEFINED;
		day = ETA_DAY_UNDEFINED;
		hour = ETA_HOUR_UNDEFINED;
		minute = ETA_MINUTE_UNDEFINED;
		lat = LAT_UNDEFINED;
		lon = LON_UNDEFINED;
		ppm = level = speed = draught = distance = 0;
		cog = COG_UNDEFINED;
		last_signal = {};
		approximate = false;

		memset(shipname, 0, sizeof(shipname));
		memset(destination, 0, sizeof(destination));
		memset(callsign, 0, sizeof(callsign));
		memset(country_code, 0, sizeof(country_code));
		last_group = GROUP_OUT_UNDEFINED;

		msg.clear();
	}
};

class DB : public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public StreamOut<JSON::JSON> {

	JSON::StringBuilder builder;

	int first, last, count, path_idx = 0;
	std::string content, delim;
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	int TIME_HISTORY = 30 * 60;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;
	bool use_GPS = true;
	uint32_t own_mmsi = 0;

	int N = 4096;
	int M = 4096;

	const int MSG_TYPE_OTHER = 0;
	const int MSG_TYPE_CLASSA = 1;
	const int MSG_TYPE_CLASSB = 2;
	const int MSG_TYPE_BASESTATION = 3;
	const int MSG_TYPE_ATON = 4;
	const int MSG_TYPE_SAR = 5;

	std::vector<Ship> ships;
	std::vector<PathPoint> paths;

	bool isValidCoord(float lat, float lon);
	const float EarthRadius = 6371.0f;			// Earth radius in kilometers
	const float NauticalMilePerKm = 0.5399568f; // Conversion factor

	float deg2rad(float deg) { return deg * PI / 180.0f; }
	int rad2deg(float rad) { return (int)(360 + rad * 180 / PI) % 360; }

	int findShip(uint32_t mmsi);
	int createShip();
	void moveShipToFront(int);
	bool updateFields(const JSON::Property& p, const AIS::Message* msg, Ship& v, bool allowApproximate);

	bool updateShip(const JSON::JSON&, TAG&, Ship&);
	void addToPath(int ptr);
	void addValidation(int, TAG&, float, float);

	void getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float& distance, int& bearing);

public:
	DB() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}

	void setup();
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void setShareLatLon(bool b) { latlon_share = b; }
	bool setUseGPS(bool b) { return use_GPS = b; }
	void setLatLon(float lat, float lon) {
		this->lat = lat;
		this->lon = lon;
	}
	void setLat(float lat) { this->lat = lat; }
	void setLon(float lon) { this->lon = lon; }
	void setOwnMMSI(uint32_t mmsi) { own_mmsi = mmsi; }

	void Receive(const JSON::JSON* data, int len, TAG& tag);
	void Receive(const AIS::GPS* data, int len, TAG& tag) {
		if (use_GPS) {
			lat = data[0].getLat();
			lon = data[0].getLon();
		}
	}

	void getShipJSON(const Ship& ship, std::string& content, long int now);
	std::string getShipJSON(int mmsi);
	std::string getJSON(bool full = false);
	std::string getJSONcompact(bool full = false);
	std::string getPathJSON(uint32_t);
	std::string getAllPathJSON();
	std::string getMessage(uint32_t);

	int getCount() { return count; }
	int getMaxCount() { return N; }

	void setServerMode(bool b) { server_mode = b; }
	void setMsgSave(bool b) { msg_save = b; }
};