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

#include "AIS-catcher.h"

#include "Signals.h"
#include "Common.h"
#include "Model.h"
#include "IO.h"
#include "Network.h"
#include "AIS.h"
#include "JSONAIS.h"
#include "Server.h"

#include "Keys.h"
#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/StringBuilder.h"


// ----------------------------
// Class to log message count stat

class Statistics {
	int count;
	int msg[27];
	int channel[4];

	float level, ppm, distance;
	float radar[36];

public:
	int getCount() { return count; }
	void clear() {
		std::memset(msg, 0, sizeof(msg));
		std::memset(channel, 0, sizeof(channel));
		std::memset(radar, 0, sizeof(radar));

		count = 0;
		distance = level = ppm = 0;
	}

	void Add(const AIS::Message& m, const TAG& tag) {
		if (m.type() > 27 || m.type() < 1) return;

		count++;
		msg[m.type() - 1]++;
		if (m.getChannel() >= 'A' || m.getChannel() <= 'D') channel[m.getChannel() - 'A']++;

		level += tag.level;
		ppm += tag.ppm;

		// for range we ignore atons for now
		if (m.type() == 21) return;

		if (tag.distance > distance && (tag.distance < 50 || tag.validated)) {
			distance = tag.distance;
		}

		if (tag.angle >= 0 && tag.angle < 360) {
			int bucket = tag.angle / 10;
			if (tag.distance > radar[bucket] && (tag.distance < 50 || tag.validated))
				radar[bucket] = tag.distance;
		}
	}

	std::string toJSON(bool empty = false) {
		std::string element;

		element += "{\"count\":" + std::to_string(empty ? 0 : count) +
				   ",\"level\":" + std::to_string(empty || !count ? 0 : level / count) +
				   ",\"ppm\":" + std::to_string(empty || !count ? 0 : ppm / count) +
				   ",\"dist\":" + std::to_string(empty ? 0 : distance) +
				   ",\"channel\":[";

		for (int i = 0; i < 4; i++) element += std::to_string(empty ? 0 : channel[i]) + ",";
		element.pop_back();
		element += "],\"radar\":[";
		for (int i = 0; i < 36; i++) element += std::to_string(empty ? 0 : radar[i]) + ",";
		element.pop_back();
		element += "],\"msg\":[";
		for (int i = 0; i < 27; i++) element += std::to_string(empty ? 0 : msg[i]) + ",";
		element.pop_back();
		element += "]}";
		return element;
	}
};

// adds bearing and distance to tags
/*
class DistanceCalc : public SimpleStreamInOut<JSON::JSON, JSON::JSON> {

	float lat = 0, lon = 0;

	const int LAT_UNDEFINED = 91;
	const int LON_UNDEFINED = 181;

	const float EarthRadius = 6371.0f;  // Earth radius in kilometers
	const float NauticalMilePerKm = 0.5399568f;  // Conversion factor

	// Converts degrees to radians
	float deg2rad(float deg) {
		return deg * PI / 180.0f;
	}

	// Converts radians to degrees
	int rad2deg(float rad) {
		//return (360 + rad * 180 / PI) % 360;
		return (int)(360 + rad * 180 / PI) % 360;
	}

	void getDistanceAndAngle(float lat1, float lon1, float lat2, float lon2,
		float& distance, int& bearing) {
		// Convert the latitudes and longitudes from degrees to radians
		lat1 = deg2rad(lat1);
		lon1 = deg2rad(lon1);
		lat2 = deg2rad(lat2);
		lon2 = deg2rad(lon2);

		// Compute the distance using the haversine formula
		float dlat = lat2 - lat1, dlon = lon2 - lon1;
		float a = sin(dlat / 2) * sin(dlat / 2) +
			cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
		distance = 2 * EarthRadius * NauticalMilePerKm * asin(sqrt(a));

		float y = sin(dlon) * cos(lat2);
		float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
		bearing = rad2deg(atan2(y, x));
	}

	bool isValidCoord(float lat, float lon) {
		return !(lat == 0 && lon == 0) && lat != LAT_UNDEFINED && lon != LON_UNDEFINED;
	}

public:

	void setLat(float a) { lat =a; }
	void setLon(float a) { lon = a;}

	void Receive(const JSON::JSON* data, int len, TAG& tag) {

		for(int i = 0; i < len; i++) {
			float lat2 = LAT_UNDEFINED, lon2 = LON_UNDEFINED;

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_LAT:
					lat2 = p.Get().getFloat();
					break;
				case AIS::KEY_LON:
					lon2 = p.Get().getFloat();
					break;
				}
			}

			if (isValidCoord(lat,lon) && isValidCoord(lat2, lon2)) {
				getDistanceAndAngle(lat,lon,lat2, lon2, tag.distance, tag.angle);
			}
			else
			{
				tag.distance = -1;
				tag.angle = -1;
			}
			Send(data, len, tag);
		}
	}
};

template <int N, int INTERVAL>
struct History : public StreamIn<JSON::JSON> {

	struct {
		long int time;
		Statistics stat;
	} history[N];

	int start, end;

	void create(long int t) {
		history[end].time = t;
		history[end].stat.clear();
	}

	History() : start(0), end(0) {
		create((long int)time(NULL) / (long int)INTERVAL);
	}

	void Receive(const JSON::JSON* j, int len, TAG& tag) {

		for (int i = 0; i < len; i++) {
			if (!j[i].binary) return;

			const AIS::Message *msg = (AIS::Message*) j[i].binary;
			long int tm = ((long int)msg->getRxTimeUnix()) / (long int)INTERVAL;

			if (history[end].time != tm) {
				end = (end + 1) % N;
				create(tm);
				if (start == end) start = (start + 1) % N;
			}

			history[end].stat.Add(*msg,tag);
		}
	}

	float getAverage() {
		float avg = 0.0f;

		for (int idx = start; idx != end; idx = (idx + 1) % N)
			avg += history[idx].stat.getCount();

		int delta_time = 1 + (int)((long int)history[end].time - (long int)history[start].time);
		return avg / delta_time;
	}

	Statistics *last() {
		if (start == end) return nullptr;
		return &history[(end + N - 1) % N].stat;
	}

	std::string toJSON() {
		std::string content;
		long int tm, tm_now = ((long int)time(nullptr)) / (long int)INTERVAL;
		int idx;

		content += "{\"time\":[";
		for (int i = N, tm = tm_now, idx = end; i > 0; i--) {
			bool new_tm = history[idx].time >= tm;

			content += std::to_string(i - N) + ",";

			if (new_tm) {
				if (idx == start) break;
				idx = (idx + N - 1) % N;
			}
			tm--;
		}
		if (content[content.size() - 1] == ',') content.pop_back();
		content += "],\"stat\":[";

		for (int i = N, tm = tm_now, idx = end; i > 0; i--) {
			bool empty = history[idx].time < tm;
			content += history[idx].stat.toJSON(empty)+",";

			if (!empty) {
				if (idx == start) break;
				idx = (idx + N - 1) % N;
			}
			tm--;
		}
		if (content[content.size() - 1] == ',') content.pop_back();
		content += "]}";
		return content;
	}
};

*/
