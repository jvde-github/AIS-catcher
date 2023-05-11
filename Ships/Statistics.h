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
#include <fstream>
#include <memory>
#include <mutex>

// ----------------------------
// Class to log message count stat

class Statistics {

	std::mutex m;

	bool log = false;

	static const int _MAGIC = 0x4f82b;
	static const int _VERSION = 1;
	static const int _RADAR_BUCKETS = 18;

	int _LONG_RANGE_CUTOFF = 2500;

	int _count;
	int _msg[27];
	int _channel[4];

	float _level_min, _level_max, _ppm, _distance;
	float _radarA[_RADAR_BUCKETS];
	float _radarB[_RADAR_BUCKETS];

public:
	void setLog(bool b) { log = b; }
	int getCount() { return _count; }
	void setCutoff(int cutoff) { _LONG_RANGE_CUTOFF = cutoff; }

	void Clear() {
		std::lock_guard<std::mutex> l{ this->m };

		std::memset(_msg, 0, sizeof(_msg));
		std::memset(_channel, 0, sizeof(_channel));
		std::memset(_radarA, 0, sizeof(_radarA));
		std::memset(_radarB, 0, sizeof(_radarB));

		_count = 0;
		_distance = _ppm = 0;
		_level_min = 1e6;
		_level_max = -1e6;
	}

	void Add(const AIS::Message& m, const TAG& tag) {

		std::lock_guard<std::mutex> l{ this->m };

		if (m.type() > 27 || m.type() < 1) return;

		_count++;
		_msg[m.type() - 1]++;
		if (m.getChannel() >= 'A' || m.getChannel() <= 'D') _channel[m.getChannel() - 'A']++;

		_level_min = MIN(_level_min, tag.level);
		_level_max = MAX(_level_max, tag.level);
		_ppm += tag.ppm;

		// for range we ignore atons for now
		if (m.type() == 21) return;
		
		if( !tag.validated || tag.distance > _LONG_RANGE_CUTOFF)
			return;
			 
		if(tag.distance > _distance) {
			_distance = tag.distance;
			
			if (log) {
				std::cerr << "stat log: distance " << _distance << " ";
				for(auto n : m.NMEA) std::cerr << n << " ";
				std::cerr << std::endl;
			}
		}

		if (m.type() == 18 || m.type() == 19 || m.type() == 24) {
			if (tag.angle >= 0 && tag.angle < 360) {
				int bucket = tag.angle / (360 / _RADAR_BUCKETS);
				if (tag.distance > _radarB[bucket])
					_radarB[bucket] = tag.distance;
			}
		}
		else if (m.type() <= 3 || m.type() == 5) {
			if (tag.angle >= 0 && tag.angle < 360) {
				int bucket = tag.angle / (360 / _RADAR_BUCKETS);
				if (tag.distance > _radarA[bucket])
					_radarA[bucket] = tag.distance;
			}
		}
	}

	std::string toPrometheus() {
		std::lock_guard<std::mutex> l{ this->m };

		std::string element;

		element += "# HELP ais_stat_count Total number of messages\n";
		element += "# TYPE ais_stat_count counter\n";
		element += "ais_stat_count " + std::to_string(_count) + "\n";
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

		return element;
	}

	std::string toJSON(bool empty = false) {
		std::lock_guard<std::mutex> l{ this->m };

		std::string element;

		element += "{\"count\":" + std::to_string(empty ? 0 : _count) +
				   ",\"level_min\":" + std::to_string(empty || !_count ? 0 : _level_min) +
				   ",\"level_max\":" + std::to_string(empty || !_count ? 0 : _level_max) +
				   ",\"ppm\":" + std::to_string(empty || !_count ? 0 : _ppm / _count) +
				   ",\"dist\":" + std::to_string(empty ? 0 : _distance) +
				   ",\"channel\":[";

		for (int i = 0; i < 4; i++) element += std::to_string(empty ? 0 : _channel[i]) + ",";
		element.pop_back();
		element += "],\"radar_a\":[";
		for (int i = 0; i < _RADAR_BUCKETS; i++) element += std::to_string(empty ? 0 : _radarA[i]) + ",";
		element.pop_back();
		element += "],\"radar_b\":[";
		for (int i = 0; i < _RADAR_BUCKETS; i++) element += std::to_string(empty ? 0 : _radarB[i]) + ",";
		element.pop_back();
		element += "],\"msg\":[";
		for (int i = 0; i < 27; i++) element += std::to_string(empty ? 0 : _msg[i]) + ",";
		element.pop_back();
		element += "]}";
		return element;
	}

	bool Save(std::ofstream& file) {
		std::lock_guard<std::mutex> l{ this->m };

		int magic = _MAGIC;
		int version = _VERSION;

		if (!file.write((const char*)&magic, sizeof(int))) return false;			 // Check magic number
		if (!file.write((const char*)&version, sizeof(int))) return false;			 // Check version number
		if (!file.write((const char*)&_count, sizeof(int))) return false;			 // Check count
		if (!file.write((const char*)&_msg, sizeof(_msg))) return false;			 // Check msg array
		if (!file.write((const char*)&_channel, sizeof(_channel))) return false;	 // Check channel array
		if (!file.write((const char*)&_level_min, sizeof(_level_min))) return false; // Check level
		if (!file.write((const char*)&_level_max, sizeof(_level_max))) return false; // Check level
		if (!file.write((const char*)&_ppm, sizeof(_ppm))) return false;			 // Check ppm
		if (!file.write((const char*)&_distance, sizeof(_distance))) return false;	 // Check distance
		if (!file.write((const char*)&_radarA, sizeof(_radarA))) return false;		 // Check radar array
		if (!file.write((const char*)&_radarB, sizeof(_radarB))) return false;		 // Check radar array

		return true;
	}

	bool Load(std::ifstream& file) {
		std::lock_guard<std::mutex> l{ this->m };

		int magic = 0, version = 0, sum = 0;

		if (!file.read((char*)&magic, sizeof(int))) return false;			  // Check count
		if (!file.read((char*)&version, sizeof(int))) return false;			  // Check count
		if (!file.read((char*)&_count, sizeof(int))) return false;			  // Check count
		if (!file.read((char*)&_msg, sizeof(_msg))) return false;			  // Check msg array
		if (!file.read((char*)&_channel, sizeof(_channel))) return false;	  // Check channel array
		if (!file.read((char*)&_level_min, sizeof(_level_min))) return false; // Check level
		if (!file.read((char*)&_level_max, sizeof(_level_max))) return false; // Check level
		if (!file.read((char*)&_ppm, sizeof(_ppm))) return false;			  // Check ppm
		if (!file.read((char*)&_distance, sizeof(_distance))) return false;	  // Check distance
		if (!file.read((char*)&_radarA, sizeof(_radarA))) return false;		  // Check radar array
		if (!file.read((char*)&_radarB, sizeof(_radarB))) return false;		  // Check radar array

		if (false && !file.eof()) {
			std::cerr << "Statistics: error with incorrect file size." << std::endl;
			return false;
		}

		if (magic != _MAGIC || version != _VERSION) return false;

		/*
		sum = 0;
		for (int i = 0; i < 27; i++) sum += _msg[i];
		if (_count != sum) {
			std::cerr << "Statistics: error with consistency [sum of msg count != total msg count]." << std::endl;
			return false;
		}
		*/
		return true;
	}
};
