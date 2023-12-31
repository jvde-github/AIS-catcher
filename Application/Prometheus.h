/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
#include <mutex>

#include "AIS-catcher.h"

#include "JSONAIS.h"

class PromotheusCounter : public StreamIn<JSON::JSON> {
	std::mutex m;

	static const int _RADAR_BUCKETS = 18;

	int _LONG_RANGE_CUTOFF = 2500;

	int _count;
	int _msg[27];
	int _channel[4];
	int _shipclass[14];
	int _ship_stationary;
	int _ship_moving;

	float _distance;
	// float _level_min, _level_max, _ppm, _distance;
	// float _radarA[_RADAR_BUCKETS];
	// float _radarB[_RADAR_BUCKETS];

	std::string ppm;
	std::string level;

public:
	void setCutOff(int c) { _LONG_RANGE_CUTOFF = c; }

	PromotheusCounter() {
		reset();
		Clear();
	}

	virtual ~PromotheusCounter() {}

	int getCount() { return _count; }
	void setCutoff(int cutoff) { _LONG_RANGE_CUTOFF = cutoff; }

	void Clear() {

		m.lock();
		std::memset(_msg, 0, sizeof(_msg));
		std::memset(_channel, 0, sizeof(_channel));
		std::memset(_shipclass, 0, sizeof(_shipclass));

		_count = _ship_moving = _ship_stationary = 0;
		_distance = 0;

		m.unlock();
	}

	void Add(const AIS::Message& m, const TAG& tag, bool new_vessel = false) {

		if (m.type() > 27 || m.type() < 1) return;
		if (tag.ppm < 1000)
			ppm += "ais_msg_ppm{type=\"" + std::to_string(m.type()) + "\",mmsi=\"" + std::to_string(m.mmsi()) + "\",channel=\"" + std::string(1, m.getChannel()) + "\"} " + std::to_string(tag.ppm) + "\n";

		if (tag.level < 1000)
			level += "ais_msg_level{type=\"" + std::to_string(m.type()) + "\",mmsi=\"" + std::to_string(m.mmsi()) + "\",channel=\"" + std::string(1, m.getChannel()) + "\"} " + std::to_string(tag.level) + "\n";

		_count++;
		_msg[m.type() - 1]++;

		if (tag.shipclass < 14 && tag.shipclass >= 0)
			_shipclass[tag.shipclass]++;

		if (m.getChannel() >= 'A' || m.getChannel() <= 'D')
			_channel[m.getChannel() - 'A']++;

		if (tag.distance > _distance) {
			_distance = tag.distance;
		}

		if (tag.speed != SPEED_UNDEFINED && tag.shipclass != CLASS_ATON && tag.shipclass != CLASS_STATION && tag.shipclass != CLASS_HELICOPTER && tag.shipclass != CLASS_PLANE && tag.shipclass != CLASS_SARTEPIRB) {
			if (tag.speed > 0.5)
				_ship_moving++;
			else
				_ship_stationary++;
		}
	}

	void Receive(const JSON::JSON* json, int len, TAG& tag) {
		AIS::Message& data = *((AIS::Message*)json[0].binary);

		if (ppm.size() > 32768 || level.size() > 32768) {
			return;
		}

		m.lock();

		Add(data, tag);

		m.unlock();
	}

	void reset() {
		m.lock();
		ppm = "# HELP ais_msg_ppm\n# TYPE ais_msg_ppm gauge\n";
		level = "# HELP ais_msg_level\n# TYPE ais_msg_level gauge\n";
		m.unlock();
	}

	std::string toPrometheus() {
		m.lock();
		std::string element;

		element += "# HELP ais_stat_count Total number of messages\n";
		element += "# TYPE ais_stat_count counter\n";
		element += "ais_stat_count " + std::to_string(_count) + "\n";

		element += "# HELP ais_stat_ship_moving Total number of ships moving\n";
		element += "# TYPE ais_stat_ship_moving counter\n";
		element += "ais_stat_ship_moving " + std::to_string(_ship_moving) + "\n";

		element += "# HELP ais_stat_ship_stationary Total number of ships stationary\n";
		element += "# TYPE ais_stat_ship_stationary counter\n";
		element += "ais_stat_ship_stationary " + std::to_string(_ship_stationary) + "\n";


		element += "# HELP ais_stat_distance Longest distance\n";
		element += "# TYPE ais_stat_distance gauge\n";
		element += "ais_stat_distance " + std::to_string(_distance) + "\n";

		for (int i = 0; i < 4; i++) {
			std::string ch(1, i + 'A');
			element += "# HELP ais_stat_count_channel_" + ch + " Total number of messages on channel " + ch + "\n";
			element += "# TYPE ais_stat_count_channel_" + ch + " counter\n";
			element += "ais_stat_count_channel_" + ch + " " + std::to_string(_channel[i]) + "\n";
		}

		for (int i = 0; i < 27; i++) {
			std::string type = std::to_string(i + 1);
			element += "# HELP ais_stat_count_type_" + type + " Total number of messages of type " + type + "\n";
			element += "# TYPE ais_stat_count_type_" + type + " counter\n";
			element += "ais_stat_count_type_" + type + " " + std::to_string(_msg[i]) + "\n";
		}

		for (int i = 0; i < 14; i++) {
			std::string type = std::to_string(i);
			element += "# HELP ais_stat_count_shipclass_" + type + " Total number of messages of shipclass " + type + "\n";
			element += "# TYPE ais_stat_count_shipclass_" + type + " counter\n";
			element += "ais_stat_count_shipclass_" + type + " " + std::to_string(_shipclass[i]) + "\n";
		}
		element += ppm + level;
		m.unlock();
		return element;
	}
};
