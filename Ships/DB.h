/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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
#include "Ships.h"


struct PathPoint {
	float lat, lon;
	uint32_t mmsi = 0;
	int count = 0;
	int next = 0;
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
	int M = 4096 * 16;

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

	void getShipJSON(const Ship& ship, std::string& content, long int now);
	std::string getSinglePathJSON(int);
	bool isNextPathPoint(int idx, uint32_t mmsi, int count) { return idx != -1 && paths[idx].mmsi == mmsi && paths[idx].count < count; }

	AIS::Filter filter;

public:
	DB() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}

	std::mutex mtx;

	void setup();
	void setTimeHistory(int t) { TIME_HISTORY = t; }
	void setShareLatLon(bool b) { latlon_share = b; }
	bool getShareLatLon() { return latlon_share; }
	
	bool setUseGPS(bool b) { return use_GPS = b; }
	void setLatLon(float lat, float lon) {
		this->lat = lat;
		this->lon = lon;
	}
	void setLat(float lat) { this->lat = lat; }
	void setLon(float lon) { this->lon = lon; }

	float getLat() { return lat; }
	float getLon() { return lon; }

	void setOwnMMSI(uint32_t mmsi) { own_mmsi = mmsi; }

	void Receive(const JSON::JSON* data, int len, TAG& tag);
	void Receive(const AIS::GPS* data, int len, TAG& tag) {
		if (use_GPS) {
			lat = data[0].getLat();
			lon = data[0].getLon();
		}
	}

	void getBinary(std::vector<char>&);
	std::string getShipJSON(int mmsi);
	std::string getJSON(bool full = false);
	std::string getJSONcompact(bool full = false);
	std::string getPathJSON(uint32_t);
	std::string getAllPathJSON();
	std::string getMessage(uint32_t);
	std::string getKML();
	std::string getGeoJSON();

	int getCount() { return count; }
	int getMaxCount() { return N; }

	void setServerMode(bool b) { server_mode = b; }
	void setMsgSave(bool b) { msg_save = b; }
	void setFilterOption(std::string &opt, std::string &arg) { filter.SetOption(opt, arg); }
};
