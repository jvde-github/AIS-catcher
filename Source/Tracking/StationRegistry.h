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
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "Common.h"
#include "Keys.h"
#include "JSON.h"
#include "Writer.h"

// Receiving stations told to the tracker over the control channel: one record
// per station id, sequenced on the object feed's counter so the feed lists a
// station only when it changed. A withdrawn station leaves a tombstone.
class StationRegistry
{
public:
	// a station stays until withdrawn; a row's ttl only keeps a client from ageing it out
	enum { KEEP_S = 315360000 };

	struct Station
	{
		int id = 0;
		std::string name, country; // country as a two-letter code
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		uint32_t mmsi = 0;
		bool online = false;
		std::time_t last = 0;
		float rate = 0;
		uint64_t seq = 0;
	};

	void apply(const JSON::JSON &o, uint64_t &next_seq)
	{
		Station s;
		bool gone = false;
		for (const auto &p : o.getMembers())
		{
			const JSON::Value &v = p.Get();
			switch (p.Key())
			{
			case AIS::KEY_ID: s.id = (int)v.getInt(); break;
			case AIS::KEY_NAME: s.name = v.getString(); break;
			case AIS::KEY_COUNTRY: s.country = v.getString(); break;
			case AIS::KEY_LAT: s.lat = (FLOAT32)v.getFloat(LAT_UNDEFINED); break;
			case AIS::KEY_LON: s.lon = (FLOAT32)v.getFloat(LON_UNDEFINED); break;
			case AIS::KEY_MMSI: s.mmsi = (uint32_t)v.getInt(); break;
			case AIS::KEY_ONLINE: s.online = v.getBool(); break;
			case AIS::KEY_LAST_SEEN: s.last = (std::time_t)v.getInt(); break;
			case AIS::KEY_RATE: s.rate = (float)v.getFloat(); break;
			case AIS::KEY_GONE: gone = v.getBool(); break;
			default: break;
			}
		}
		if (s.id <= 0)
			return;
		auto it = stations.find(s.id);
		if (gone)
		{
			if (it == stations.end())
				return;
			unmap(it->second);
			stations.erase(it);
			tombstones.push_back(std::make_pair(s.id, ++next_seq));
			return;
		}
		if (it != stations.end())
		{
			Station &cur = it->second;
			// what only says the station is alive rides along without a new sequence number
			if (cur.lat == s.lat && cur.lon == s.lon && cur.name == s.name && cur.country == s.country && cur.mmsi == s.mmsi && cur.online == s.online)
			{
				cur.last = s.last;
				cur.rate = s.rate;
				return;
			}
			unmap(cur);
		}
		s.seq = ++next_seq;
		if (s.mmsi)
			by_mmsi[s.mmsi] = s.id;
		stations[s.id] = s;
		tombstones.erase(std::remove_if(tombstones.begin(), tombstones.end(), [&](const std::pair<int, uint64_t> &t) { return t.first == s.id; }), tombstones.end());
	}

	// the rows that changed after `since`, in the object feed's row shape; kind 8 is a station
	void writeRows(JSON::Writer &w, uint64_t since) const
	{
		char id[24];
		for (const auto &kv : stations)
		{
			const Station &s = kv.second;
			if (s.seq <= since)
				continue;
			snprintf(id, sizeof(id), "s%d", s.id);
			w.beginObject().kv("id", id).kv("kind", 8).kv("seq", (long long)s.seq)
				.kv("lat", s.lat).kv("lon", s.lon).kv("label", s.name).kv("count", 0)
				.kv("t", (long long)s.last).kv("first", (long long)s.last);
			writeFields(w, s.mmsi, s.online, s.rate, s.country);
			w.endObject();
		}
	}

	// the station part of a row, after the fields every object row has
	static void writeFields(JSON::Writer &w, uint32_t mmsi, bool online, float rate, const std::string &country)
	{
		w.kv("ttl", KEEP_S).kv("mmsi", mmsi).kv("online", online).kv("rate", rate).kv("country", country);
	}

	void writeRemoved(JSON::Writer &w, uint64_t since) const
	{
		char id[24];
		for (const auto &ts : tombstones)
			if (ts.second > since)
			{
				snprintf(id, sizeof(id), "s%d", ts.first);
				w.val(id);
			}
	}

	template <typename F>
	void forEach(F f) const
	{
		for (const auto &kv : stations)
			f(kv.second);
	}

	int idFor(uint32_t mmsi) const
	{
		auto it = by_mmsi.find(mmsi);
		return it == by_mmsi.end() ? 0 : it->second;
	}

	bool writeOne(JSON::Writer &w, int id) const
	{
		auto it = stations.find(id);
		if (it == stations.end())
			return false;
		const Station &s = it->second;
		w.key("station").beginObject().kv("id", s.id).kv("name", s.name).kv("lat", s.lat).kv("lon", s.lon)
			.kv("mmsi", s.mmsi).kv("online", s.online).kv("last", (long long)s.last).kv("rate", s.rate).kv("country", s.country).endObject();
		return true;
	}

private:
	void unmap(const Station &cur)
	{
		auto m = by_mmsi.find(cur.mmsi);
		if (m != by_mmsi.end() && m->second == cur.id)
			by_mmsi.erase(m);
	}

	std::map<int, Station> stations;
	std::map<uint32_t, int> by_mmsi;
	std::vector<std::pair<int, uint64_t>> tombstones;
};
