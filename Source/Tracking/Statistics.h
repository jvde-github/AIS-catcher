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
#include <atomic>

#include "Stream.h"
#include "JSONAIS.h"
#include "Convert.h"
#include "Message.h"
#include "StringBuilder.h"

// ----------------------------
// Class to log message count stat

class MessageStatistics {

	std::mutex mtx;

	static const int _MAGIC = 0x4f82b;
	static const int _VERSION = 3;
	static const int _RADAR_BUCKETS = 18;

	int _LONG_RANGE_CUTOFF = 2500;

	int _count, _exclude, _vessels;
	int _msg[28];
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

		if (m.type() > 28 || m.type() < 1) return;

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
		if (m.type() == 21 || m.type() == 28) return;

		if (!tag.validated || tag.distance > _LONG_RANGE_CUTOFF || (m.repeat() > 0 && m.type() != 27))
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

	void writeJSON(JSON::Writer &w, bool empty = false) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int c = _count - _exclude;
		bool has_level = c > 0 && _level_min <= _level_max;

		w.beginObject();
		w.kv("count", empty ? 0 : _count);
		w.kv("vessels", empty ? 0 : _vessels);
		if (empty || !has_level) {
			w.kv_null("level_min");
			w.kv_null("level_max");
			w.kv_null("ppm");
		} else {
			w.kv("level_min", _level_min);
			w.kv("level_max", _level_max);
			w.kv("ppm", _ppm / c);
		}
		if (empty)
			w.kv_null("dist");
		else
			w.kv("dist", _distance);

		w.key("channel").beginArray();
		for (int i = 0; i < 4; i++) w.val(empty ? 0 : _channel[i]);
		w.endArray();

		w.key("radar_a").beginArray();
		for (int i = 0; i < _RADAR_BUCKETS; i++) w.val(empty ? 0.0f : _radarA[i]);
		w.endArray();

		w.key("radar_b").beginArray();
		for (int i = 0; i < _RADAR_BUCKETS; i++) w.val(empty ? 0.0f : _radarB[i]);
		w.endArray();

		w.key("msg").beginArray();
		for (int i = 0; i < 28; i++) w.val(empty ? 0 : _msg[i]);
		w.endArray();

		w.endObject();
	}

#define W(x) file.write((const char*)&(x), sizeof(x))
#define R(x) file.read((char*)&(x), sizeof(x))

	bool Save(std::ofstream& file) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int magic = _MAGIC;
		int version = _VERSION;

		return (bool)(W(magic) && W(version) && W(_count) && W(_vessels)
			&& W(_msg) && W(_channel)
			&& W(_level_min) && W(_level_max) && W(_ppm) && W(_distance)
			&& W(_radarA) && W(_radarB));
	}

	bool Load(std::ifstream& file) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int magic = 0, version = 0;
		if (!(R(magic) && R(version) && R(_count))) return false;
		if (magic != _MAGIC) return false;
		if (version != 1 && version != 2 && version != _VERSION) return false;

		if (version >= 2 && !R(_vessels)) return false;
		if (version < 2) _vessels = 0;

		// v3 grew _msg from 27 to 28 entries to include type 28 (AtoN single-slot report).
		int n = (version >= 3) ? 28 : 27;
		if (!file.read((char *)_msg, n * sizeof(int))) return false;
		if (version < 3) _msg[27] = 0;

		return (bool)(R(_channel)
			&& R(_level_min) && R(_level_max) && R(_ppm) && R(_distance)
			&& R(_radarA) && R(_radarB));
	}

#undef W
#undef R
};

class Counter : public StreamIn<JSON::JSON> {
	MessageStatistics stat;

public:
	void setCutOff(int c) { stat.setCutoff(c); }
	void Clear() { stat.Clear(); }

	bool Load(std::ifstream& file) { return stat.Load(file); }
	bool Save(std::ofstream& file) { return stat.Save(file); }

	void Receive(const JSON::JSON* msg, int len, TAG& tag) { stat.Add(*((AIS::Message*)msg[0].binary), tag); }

	void writeJSON(JSON::Writer &w, bool empty = false) { stat.writeJSON(w, empty); }
};

struct ByteCounter : public StreamIn<RAW> {
	AIS::Filter filter;
	// Split into two 32-bit atomics so armv6/armel don't need libatomic.
	// received_hi counts 4 GB wraps of received_lo. Byte precision preserved.
	std::atomic<uint32_t> received_lo{0};
	std::atomic<uint32_t> received_hi{0};
	virtual ~ByteCounter() {}

	void Receive(const RAW* data, int len, TAG& tag) {
		uint32_t add = (uint32_t)data[0].size;
		uint32_t before = received_lo.fetch_add(add, std::memory_order_relaxed);
		if (before + add < before)
			received_hi.fetch_add(1, std::memory_order_relaxed);
	}

	uint64_t received() const {
		uint32_t h1 = received_hi.load(std::memory_order_relaxed);
		uint32_t lo = received_lo.load(std::memory_order_relaxed);
		uint32_t h2 = received_hi.load(std::memory_order_relaxed);
		if (h1 != h2) lo = received_lo.load(std::memory_order_relaxed);
		return ((uint64_t)h2 << 32) | lo;
	}

	void Reset() {
		received_lo.store(0, std::memory_order_relaxed);
		received_hi.store(0, std::memory_order_relaxed);
	}
	void setFilter(const AIS::Filter &f) { filter = f; }
};
