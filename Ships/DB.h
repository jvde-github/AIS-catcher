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
#include "Server.h"

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

class DB : public StreamIn<JSON::JSON>, public StreamIn<AIS::GPS>, public StreamOut<JSON::JSON> {

	JSON::StringBuilder builder;

	int first, last, count, path_idx = 0;
	std::string content, delim;
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	int TIME_HISTORY = 30 * 60;
	bool latlon_share = false;
	bool server_mode = false;
	bool msg_save = false;

	int N = 4096;
	int M = 4096;

	const int MSG_TYPE_OTHER = 0;
	const int MSG_TYPE_CLASSA = 1;
	const int MSG_TYPE_CLASSB = 2;
	const int MSG_TYPE_BASESTATION = 3;
	const int MSG_TYPE_ATON = 4;
	const int MSG_TYPE_SAR = 5;

	/*
		const int MMSI_TYPE_OTHER = 0;
		const int MMSI_TYPE_CLASS_A = 1;
		const int MMSI_TYPE_CLASS_B = 2;
		const int MMSI_TYPE_BASESTATION = 3;
		const int MMSI_TYPE_SAR = 4;
		const int MMSI_TYPE_SARTEPIRB = 5;
		const int MMSI_TYPE_ATON = 6;
	*/

	struct VesselDetail {

		uint32_t mmsi = 0;
		int count = 0, msg_type = 0, channels = 0, shiptype = 0, heading = HEADING_UNDEFINED;
		int status = STATUS_UNDEFINED, virtual_aid = 0, path_ptr = -1;
		int to_port = DIMENSION_UNDEFINED, to_bow = DIMENSION_UNDEFINED, to_starboard = DIMENSION_UNDEFINED, to_stern = DIMENSION_UNDEFINED;
		int IMO = IMO_UNDEFINED, angle = ANGLE_UNDEFINED, validated = 0;
		char month = ETA_MONTH_UNDEFINED, day = ETA_DAY_UNDEFINED, hour = ETA_HOUR_UNDEFINED, minute = ETA_MINUTE_UNDEFINED;
		float lat = LAT_UNDEFINED, lon = LON_UNDEFINED, ppm = 0, level = 0, speed = SPEED_UNDEFINED;
		float cog = COG_UNDEFINED, draught = 0, distance = DISTANCE_UNDEFINED;
		std::time_t last_signal;
		bool approximate = false;
		char shipname[21] = { 0 }, destination[21] = { 0 }, callsign[8] = { 0 }, country_code[3] = { 0 };
		std::string *msg = NULL;
	};

	struct PathList {
		float lat, lon;
		uint32_t mmsi;
		std::time_t signal_time;
		int prev, next;
	};

	struct ShipList {
		int prev, next;
		VesselDetail ship;
	};

	std::vector<ShipList> ships;
	std::vector<PathList> paths;

	bool isValidCoord(float lat, float lon);
	const float EarthRadius = 6371.0f;			// Earth radius in kilometers
	const float NauticalMilePerKm = 0.5399568f; // Conversion factor

	// Converts degrees to radians
	float deg2rad(float deg) {
		return deg * PI / 180.0f;
	}

	// Converts radians to degrees
	int rad2deg(float rad) {
		// return (360 + rad * 180 / PI) % 360;
		return (int)(360 + rad * 180 / PI) % 360;
	}

	// int setMmsiType(int, int);
	int findShip(uint32_t mmsi);
	int createShip();
	void moveShipToFront(int);
	bool updateFields(const JSON::Property& p, const AIS::Message* msg, VesselDetail& v, bool allowApproximate);

	bool updateShip(const JSON::JSON&, TAG&, VesselDetail&);
	void addToPath(int ptr);
	void addValidation(int, TAG&, float, float);

	void getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float& distance, int& bearing);

public:

	DB() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}

	void setup(float lat = 0.0f, float lon = 0.0f);
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void setShareLatLon(bool b) { latlon_share = b; }

	void Receive(const JSON::JSON* data, int len, TAG& tag);
	void Receive(const AIS::GPS* data, int len, TAG& tag) {
		lat = data[0].lat;
		lon = data[0].lon;
	}

	void getShipJSON(const VesselDetail& ship, std::string& content, long int now);
	std::string getShipJSON(int mmsi);
	std::string getJSON(bool full = false);
	std::string getJSONcompact(bool full = false);
	std::string getPathJSON(uint32_t);
	std::string getMessage(uint32_t);

	int getCount() { return count; }
	int getMaxCount() { return N; }

	void setServerMode(bool b) { server_mode = b; }
	void setMsgSave(bool b) { msg_save = b; }
};
