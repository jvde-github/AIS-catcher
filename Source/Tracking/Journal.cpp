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


#include "Journal.h"

namespace
{
	const uint32_t JOURNAL_MAGIC = 0x4a524e4c; // "JRNL"
	// 3: an installation's event names its station and receiver, -1 for none; 2: kinds as the
	// event stream numbers them, the station and receiver in `from` and `to`, 0 for none;
	// 1 numbered the kinds as the ticker did
	const uint32_t JOURNAL_VERSION = 3;

	Tracking::Kind kindOfVersion1(uint8_t k)
	{
		switch (k)
		{
		case 2: return Tracking::Kind::DESTINATION;
		case 3: return Tracking::Kind::STATUS;
		case 4: return Tracking::Kind::DRAUGHT;
		case 5: return Tracking::Kind::ENTER;
		case 6: return Tracking::Kind::LEAVE;
		case 7: return Tracking::Kind::SYSTEM;
		default: return Tracking::Kind::SAFETY;
		}
	}

	template <typename T>
	bool put(std::ofstream &f, const T &v) { return (bool)f.write((const char *)&v, sizeof(v)); }
	template <typename T>
	bool get(std::ifstream &f, T &v) { return (bool)f.read((char *)&v, sizeof(v)); }

	bool putString(std::ofstream &f, const std::string &s)
	{
		const uint32_t n = (uint32_t)s.size();
		return put(f, n) && (bool)f.write(s.data(), n);
	}
	bool getString(std::ifstream &f, std::string &s)
	{
		uint32_t n = 0;
		if (!get(f, n) || n > 65536)
			return false;
		s.resize(n);
		return n == 0 || (bool)f.read(&s[0], n);
	}
}

bool Journal::save(std::ofstream &f) const
{
	std::lock_guard<std::mutex> lock(mtx);
	const uint32_t n = (uint32_t)entries.size();
	if (!put(f, JOURNAL_MAGIC) || !put(f, JOURNAL_VERSION) || !put(f, epoch_) || !put(f, last) || !put(f, n))
		return false;
	for (const auto &e : entries)
	{
		const EventRing::Entry &v = e.event;
		const int64_t time = (int64_t)v.time, first = (int64_t)v.first;
		const uint8_t kind = (uint8_t)v.kind, level = (uint8_t)v.level;
		const int32_t station = v.station, receiver = v.receiver;
		if (!put(f, e.seq) || !put(f, time) || !put(f, first) || !put(f, kind) || !put(f, level) ||
			!put(f, v.from) || !put(f, v.to) || !put(f, v.lat) || !put(f, v.lon) || !put(f, v.count) ||
			!put(f, station) || !put(f, receiver) ||
			!putString(f, v.text) || !putString(f, v.was) || !putString(f, v.label) ||
			!putString(f, v.from_name) || !putString(f, v.to_name) || !putString(f, e.place))
			return false;
	}
	return true;
}

bool Journal::load(std::ifstream &f)
{
	uint32_t magic = 0, version = 0, n = 0;
	uint64_t epoch = 0, newest = 0;
	if (!get(f, magic) || magic != JOURNAL_MAGIC || !get(f, version) || version < 1 || version > JOURNAL_VERSION ||
		!get(f, epoch) || !get(f, newest) || !get(f, n))
		return false;
	std::deque<Entry> loaded;
	for (uint32_t i = 0; i < n; i++)
	{
		Entry e;
		EventRing::Entry &v = e.event;
		int64_t time = 0, first = 0;
		uint8_t kind = 0, level = 0;
		int32_t station = -1, receiver = -1;
		if (!get(f, e.seq) || !get(f, time) || !get(f, first) || !get(f, kind) || !get(f, level) ||
			!get(f, v.from) || !get(f, v.to) || !get(f, v.lat) || !get(f, v.lon) || !get(f, v.count) ||
			(version >= 3 && (!get(f, station) || !get(f, receiver))) ||
			!getString(f, v.text) || !getString(f, v.was) || !getString(f, v.label) ||
			!getString(f, v.from_name) || !getString(f, v.to_name) || !getString(f, e.place))
			return false;
		v.time = (std::time_t)time;
		v.first = (std::time_t)first;
		v.kind = version == 1 ? kindOfVersion1(kind) : (Tracking::Kind)kind;
		if (version < 3 && v.kind == Tracking::Kind::SYSTEM)
		{
			station = v.from ? (int32_t)v.from : -1;
			receiver = v.to ? (int32_t)v.to : -1;
			v.from = v.to = 0;
		}
		v.station = station;
		v.receiver = receiver;
		v.level = level < (uint8_t)Tracking::Level::LEVELS ? (Tracking::Level)level : Tracking::Level::ROUTINE;
		v.seq = e.seq;
		loaded.push_back(std::move(e));
	}
	std::lock_guard<std::mutex> lock(mtx);
	while (loaded.size() > capacity)
		loaded.pop_front();
	entries = std::move(loaded);
	epoch_ = epoch;
	last = newest;
	return true;
}
