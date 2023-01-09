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

	static const int _MAGIC = 0x4f81b;
	static const int _VERSION = 1;
	static const int _RADAR_BUCKETS = 36;
	static const int _LONG_RANGE_CUTOFF = 50;

	int _count;
	int _msg[27];
	int _channel[4];

	float _level, _ppm, _distance;
	float _radar[_RADAR_BUCKETS];

public:
	int getCount() { return _count; }
	void Clear() {
		std::lock_guard<std::mutex> l{ this->m };

		std::memset(_msg, 0, sizeof(_msg));
		std::memset(_channel, 0, sizeof(_channel));
		std::memset(_radar, 0, sizeof(_radar));

		_count = 0;
		_distance = _level = _ppm = 0;
	}

	void Add(const AIS::Message& m, const TAG& tag) {

		std::lock_guard<std::mutex> l{ this->m };

		if (m.type() > 27 || m.type() < 1) return;

		_count++;
		_msg[m.type() - 1]++;
		if (m.getChannel() >= 'A' || m.getChannel() <= 'D') _channel[m.getChannel() - 'A']++;

		_level += tag.level;
		_ppm += tag.ppm;

		// for range we ignore atons for now
		if (m.type() == 21) return;

		if (tag.distance > _distance && (tag.distance < _LONG_RANGE_CUTOFF || tag.validated)) {
			_distance = tag.distance;
		}

		if (tag.angle >= 0 && tag.angle < 360) {
			int bucket = tag.angle / (360 / _RADAR_BUCKETS);
			if (tag.distance > _radar[bucket] && (tag.distance < _LONG_RANGE_CUTOFF || tag.validated))
				_radar[bucket] = tag.distance;
		}
	}

	std::string toJSON(bool empty = false) {
		std::lock_guard<std::mutex> l{ this->m };

		std::string element;

		element += "{\"count\":" + std::to_string(empty ? 0 : _count) +
				   ",\"level\":" + std::to_string(empty || !_count ? 0 : _level / _count) +
				   ",\"ppm\":" + std::to_string(empty || !_count ? 0 : _ppm / _count) +
				   ",\"dist\":" + std::to_string(empty ? 0 : _distance) +
				   ",\"channel\":[";

		for (int i = 0; i < 4; i++) element += std::to_string(empty ? 0 : _channel[i]) + ",";
		element.pop_back();
		element += "],\"radar\":[";
		for (int i = 0; i < _RADAR_BUCKETS; i++) element += std::to_string(empty ? 0 : _radar[i]) + ",";
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

		if (!file.write((const char*)&magic, sizeof(int))) return false;		   // Check magic number
		if (!file.write((const char*)&version, sizeof(int))) return false;		   // Check version number
		if (!file.write((const char*)&_count, sizeof(int))) return false;		   // Check count
		if (!file.write((const char*)&_msg, sizeof(_msg))) return false;		   // Check msg array
		if (!file.write((const char*)&_channel, sizeof(_channel))) return false;   // Check channel array
		if (!file.write((const char*)&_level, sizeof(_level))) return false;	   // Check level
		if (!file.write((const char*)&_ppm, sizeof(_ppm))) return false;		   // Check ppm
		if (!file.write((const char*)&_distance, sizeof(_distance))) return false; // Check distance
		if (!file.write((const char*)&_radar, sizeof(_radar))) return false;	   // Check radar array

		return true;
	}

	bool Load(std::ifstream& file) {
		std::lock_guard<std::mutex> l{ this->m };

		int magic = 0, version = 0, sum = 0;

		if (!file.read((char*)&magic, sizeof(int))) return false;			// Check count
		if (!file.read((char*)&version, sizeof(int))) return false;			// Check count
		if (!file.read((char*)&_count, sizeof(int))) return false;			// Check count
		if (!file.read((char*)&_msg, sizeof(_msg))) return false;			// Check msg array
		if (!file.read((char*)&_channel, sizeof(_channel))) return false;	// Check channel array
		if (!file.read((char*)&_level, sizeof(_level))) return false;		// Check level
		if (!file.read((char*)&_ppm, sizeof(_ppm))) return false;			// Check ppm
		if (!file.read((char*)&_distance, sizeof(_distance))) return false; // Check distance
		if (!file.read((char*)&_radar, sizeof(_radar))) return false;		// Check radar array

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
