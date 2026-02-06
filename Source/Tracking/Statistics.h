/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include "Stream.h"
#include "JSONAIS.h"
#include "Convert.h"
#include "Message.h"
#include "JSON/JSONBuilder.h"

// ----------------------------
// Class to log message count stat

class MessageStatistics {

	std::mutex mtx;

	static const int _MAGIC = 0x4f82b;
	static const int _VERSION = 2;
	static const int _RADAR_BUCKETS = 18;

	int _LONG_RANGE_CUTOFF = 2500;

	int _count, _exclude, _vessels;
	int _msg[27];
	int _channel[4];

	float _level_min, _level_max, _ppm, _distance;
	float _radarA[_RADAR_BUCKETS];
	float _radarB[_RADAR_BUCKETS];

public:
	MessageStatistics() { Clear(); }

	int getCount() { return _count; }
	void setCutoff(int cutoff) { _LONG_RANGE_CUTOFF = cutoff; }
	void clearVessels() { _vessels = 0; }

	void Clear() {
		std::lock_guard<std::mutex> l{ this->mtx };

		std::memset(_msg, 0, sizeof(_msg));
		std::memset(_channel, 0, sizeof(_channel));
		std::memset(_radarA, 0, sizeof(_radarA));
		std::memset(_radarB, 0, sizeof(_radarB));

		_count = _vessels = _exclude = 0;
		_distance = _ppm = 0;
		_level_min = 1e6;
		_level_max = -1e6;
	}

	void Add(const AIS::Message& m, const TAG& tag, bool new_vessel = false) {

		std::lock_guard<std::mutex> l{ this->mtx };

		if (m.type() > 27 || m.type() < 1) return;

		_count++;
		if (new_vessel) _vessels++;

		_msg[m.type() - 1]++;
		if (m.getChannel() >= 'A' && m.getChannel() <= 'D') _channel[m.getChannel() - 'A']++;

		if (tag.level == LEVEL_UNDEFINED || tag.ppm == PPM_UNDEFINED)
			_exclude++;
		else {
			_level_min = MIN(_level_min, tag.level);
			_level_max = MAX(_level_max, tag.level);
			_ppm += tag.ppm;
		}

		// for range we ignore atons
		if (m.type() == 21) return;

		if (!tag.validated || tag.distance > _LONG_RANGE_CUTOFF || m.repeat() > 0)
			return;

		if (tag.distance > _distance) {
			_distance = tag.distance;
		}

		if (m.type() == 18 || m.type() == 19 || m.type() == 24) {
			if (tag.angle >= 0 && tag.angle < 360) {
				int bucket = tag.angle / (360 / _RADAR_BUCKETS);
				if (tag.distance > _radarB[bucket])
					_radarB[bucket] = tag.distance;
			}
		}
		else if (m.type() <= 3 || m.type() == 5 || m.type() == 27) {
			if (tag.angle >= 0 && tag.angle < 360) {
				int bucket = tag.angle / (360 / _RADAR_BUCKETS);
				if (tag.distance > _radarA[bucket])
					_radarA[bucket] = tag.distance;
			}
		}
	}

	std::string toJSON(bool empty = false) {
		std::lock_guard<std::mutex> l{ this->mtx };

		JSON::JSONBuilder json;
		int c = _count - _exclude;

		json.start()
			.add("count", empty ? 0 : _count)
			.add("vessels", empty ? 0 : _vessels);

		if (empty || !c)
			json.addNull("level_min").addNull("level_max").addNull("ppm");
		else
			json.add("level_min", _level_min)
				.add("level_max", _level_max)
				.add("ppm", _ppm / c);

		if (empty)
			json.addNull("dist");
		else
			json.add("dist", _distance);

		json.key("channel").startArray();
		for (int i = 0; i < 4; i++)
			json.value(empty ? 0 : _channel[i]);

		json.endArray()
			.key("radar_a").startArray();
		for (int i = 0; i < _RADAR_BUCKETS; i++)
			json.value(empty ? 0 : _radarA[i]);

		json.endArray()
			.key("radar_b").startArray();
		for (int i = 0; i < _RADAR_BUCKETS; i++)
			json.value(empty ? 0 : _radarB[i]);

		json.endArray()
			.key("msg").startArray();
		for (int i = 0; i < 27; i++)
			json.value(empty ? 0 : _msg[i]);

		json.endArray().end();

		return json.str();
	}

	bool Save(std::ofstream& file) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int magic = _MAGIC;
		int version = _VERSION;

		if (!file.write((const char*)&magic, sizeof(int))) return false;			 // Check magic number
		if (!file.write((const char*)&version, sizeof(int))) return false;			 // Check version number
		if (!file.write((const char*)&_count, sizeof(int))) return false;			 // Check count
		if (!file.write((const char*)&_vessels, sizeof(int))) return false;			 // Check count
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
		std::lock_guard<std::mutex> l{ this->mtx };

		int magic = 0, version = 0;
		if (!file.read((char*)&magic, sizeof(int))) return false;	// Check count
		if (!file.read((char*)&version, sizeof(int))) return false; // Check count
		if (!file.read((char*)&_count, sizeof(int))) return false;	// Check count
		if (version == _VERSION) {
			if (!file.read((char*)&_vessels, sizeof(int))) return false; // Check count
		}
		if (!file.read((char*)&_msg, sizeof(_msg))) return false;			  // Check msg array
		if (!file.read((char*)&_channel, sizeof(_channel))) return false;	  // Check channel array
		if (!file.read((char*)&_level_min, sizeof(_level_min))) return false; // Check level
		if (!file.read((char*)&_level_max, sizeof(_level_max))) return false; // Check level
		if (!file.read((char*)&_ppm, sizeof(_ppm))) return false;			  // Check ppm
		if (!file.read((char*)&_distance, sizeof(_distance))) return false;	  // Check distance
		if (!file.read((char*)&_radarA, sizeof(_radarA))) return false;		  // Check radar array
		if (!file.read((char*)&_radarB, sizeof(_radarB))) return false;		  // Check radar array

		if (false && !file.eof()) {
			Warning() << "Statistics: error with incorrect file size.";
			return false;
		}
		if (magic != _MAGIC || (version != _VERSION && version != 1)) return false;

		return true;
	}
};

class Counter : public StreamIn<JSON::JSON> {
	MessageStatistics stat;

public:
	void setCutOff(int c) { stat.setCutoff(c); }
	void Clear() { stat.Clear(); }

	bool Load(std::ifstream& file) { return stat.Load(file); }
	bool Save(std::ofstream& file) { return stat.Save(file); }

	void Receive(const JSON::JSON* msg, int len, TAG& tag) { stat.Add(*((AIS::Message*)msg[0].binary), tag); }

	std::string toJSON(bool empty = false) { return stat.toJSON(empty); }
};

struct ByteCounter : public StreamIn<RAW> {
	AIS::Filter filter;
	virtual ~ByteCounter() {}
	uint64_t received = 0;
	void Receive(const RAW* data, int len, TAG& tag) { received += data[0].size; }
	void Reset() { received = 0; }
	void setFilterOption(std::string &arg, std::string &opt) { filter.SetOption(arg, opt); }
};
