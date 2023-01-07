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

class DB : public SimpleStreamInOut<JSON::JSON, JSON::JSON> {
	int first, last, count, path_idx = 0;
	std::string content, delim;
	float lat, lon;
	int TIME_HISTORY = 30 * 60;

	const int N = 4096;
	const int M = 4096;

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

	const int MSG_TYPE_OTHER = 0;
	const int MSG_TYPE_CLASSA = 1;
	const int MSG_TYPE_CLASSB = 2;
	const int MSG_TYPE_BASESTATION = 3;
	const int MSG_TYPE_ATON = 4;
	const int MSG_TYPE_SAR = 5;

	const int MMSI_TYPE_OTHER = 0;
	const int MMSI_TYPE_CLASS_A = 1;
	const int MMSI_TYPE_CLASS_B = 2;
	const int MMSI_TYPE_BASESTATION = 3;
	const int MMSI_TYPE_SAR = 4;
	const int MMSI_TYPE_SARTEPIRB = 5;
	const int MMSI_TYPE_ATON = 6;

	struct VesselDetail {

		uint32_t mmsi;
		int count, mmsi_type, msg_type, shiptype, heading, status, virtual_aid, path_ptr;
		int to_port, to_bow, to_starboard, to_stern, IMO, angle;
		char month, day, hour, minute;
		float lat, lon, ppm, level, speed, cog, draught, distance;
		std::time_t last_signal;
		char shipname[21], destination[21], callsign[8], country_code[3];
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

	void getDistanceAndAngle(float lat1, float lon1, float lat2, float lon2, float& distance, int& bearing);

public:
	void setup(float lat = 0.0f, float lon = 0.0f);
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void Receive(const JSON::JSON* data, int len, TAG& tag);
	std::string getJSON(bool full = false);
	std::string getPathJSON(uint32_t);

	int getCount() { return count; }
	int getMaxCount() { return N; }
};
