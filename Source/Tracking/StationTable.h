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

#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "Common.h"
#include "StationRegistry.h"

// The receiving stations and where each is now: one table for the installation.
class StationTable
{
public:
	// as configured, from GPS on its input, or riding a vessel
	struct Position
	{
		enum Source : uint8_t { NONE, CONFIGURED, GPS, VESSEL };
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		Source source = NONE;
		std::time_t time = 0; // of the last fix; 0 when configured
	};
	static const char *sourceName(Position::Source s)
	{
		return s == Position::CONFIGURED ? "configured" : s == Position::GPS ? "gps" : s == Position::VESSEL ? "vessel" : nullptr;
	}

	// the stations as listed: those of the last engine to start, kept while none runs
	StationRegistry list()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return registry;
	}

	// the configuration's list; a fix outlives a new one while its source still applies
	void set(const StationRegistry &listed_now)
	{
		std::lock_guard<std::mutex> lock(mtx);
		registry = listed_now;
		std::map<int, Station> next;
		riders.clear();
		registry.forEach([&](const StationRegistry::Station &s) {
			Station &n = next[s.id];
			n.mmsi = s.mmsi;
			n.gps = s.gps;
			n.inputs = s.inputs;
			if (s.mmsi)
				riders[s.mmsi] = s.id;
			auto old = stations.find(s.id);
			if (old != stations.end() && ((old->second.now.source == Position::GPS && n.followsGPS()) ||
										  (old->second.now.source == Position::VESSEL && s.mmsi && s.mmsi == old->second.mmsi)))
				n.now = old->second.now;
			else if (isValidCoord(s.lat, s.lon))
			{
				n.now.lat = s.lat;
				n.now.lon = s.lon;
				n.now.source = Position::CONFIGURED;
			}
		});
		stations.swap(next);
		listed = true;
	}

	// a station a control feed describes; invalid coordinates take it away
	void place(int id, FLOAT32 lat, FLOAT32 lon)
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (!isValidCoord(lat, lon))
		{
			stations.erase(id);
			return;
		}
		Station &s = stations[id];
		s.now.lat = lat;
		s.now.lon = lon;
		s.now.source = Position::CONFIGURED;
		s.gps = false;
	}

	// A fix moves the stations that follow GPS: the one its input names, and any that no input names.
	void moveByGPS(int id, FLOAT32 lat, FLOAT32 lon, std::vector<int> &moved)
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (!isValidCoord(lat, lon))
			return;
		const std::time_t now = std::time(nullptr);
		for (auto &s : stations)
		{
			if (!s.second.followsGPS() || (s.first != id && s.second.inputs > 0))
				continue;
			Position &p = s.second.now;
			p.lat = lat;
			p.lon = lon;
			p.source = Position::GPS;
			p.time = now;
			moved.push_back(s.first);
		}
	}

	// a vessel's position report moves the station riding it; -1 when none does
	int follow(uint32_t mmsi, FLOAT32 lat, FLOAT32 lon)
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto r = riders.find(mmsi);
		if (r == riders.end())
			return -1;
		Position &p = stations[r->second].now;
		p.lat = lat;
		p.lon = lon;
		p.source = Position::VESSEL;
		p.time = std::time(nullptr);
		return r->second;
	}

	// The station a message is counted at (the named one if listed, else its input's), and where it is now.
	Position locate(int named, int &id)
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (named >= 0 && named != id && stations.count(named))
			id = named;
		auto it = stations.find(id);
		return it == stations.end() ? Position() : it->second.now;
	}

	Position at(int id)
	{
		int same = id;
		return locate(-1, same);
	}

	// the vessels stations ride: MMSI to station id
	std::map<uint32_t, int> riding()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return riders;
	}

	// every station that has a position, by number
	std::map<int, std::pair<FLOAT32, FLOAT32>> placed()
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::map<int, std::pair<FLOAT32, FLOAT32>> at;
		for (const auto &s : stations)
			if (isValidCoord(s.second.now.lat, s.second.now.lon))
				at[s.first] = std::make_pair(s.second.now.lat, s.second.now.lon);
		return at;
	}

	// the fixes, as a tracker's backup keeps them
	bool save(std::ostream &f)
	{
		std::lock_guard<std::mutex> lock(mtx);
		uint32_t fixes = 0;
		for (const auto &s : stations)
			fixes += s.second.fixed();
		bool ok = (bool)f.write((const char *)&fixes, sizeof(fixes));
		for (const auto &s : stations)
		{
			const Position &p = s.second.now;
			if (!ok || !s.second.fixed())
				continue;
			const int32_t id = s.first;
			const uint32_t mmsi = s.second.mmsi;
			const uint8_t source = p.source;
			const int64_t time = p.time;
			ok = f.write((const char *)&id, sizeof(id)) && f.write((const char *)&p.lat, sizeof(p.lat)) &&
				 f.write((const char *)&p.lon, sizeof(p.lon)) && f.write((const char *)&source, sizeof(source)) &&
				 f.write((const char *)&time, sizeof(time)) && f.write((const char *)&mmsi, sizeof(mmsi));
		}
		return ok;
	}

	// Before the stations are listed a fix waits in its entry.
	void load(std::istream &f)
	{
		uint32_t fixes = 0;
		if (!f.read((char *)&fixes, sizeof(fixes)))
			return;
		std::lock_guard<std::mutex> lock(mtx);
		for (uint32_t i = 0; i < fixes; i++)
		{
			int32_t id = 0;
			uint32_t mmsi = 0;
			uint8_t source = 0;
			int64_t time = 0;
			Position p;
			if (!f.read((char *)&id, sizeof(id)) || !f.read((char *)&p.lat, sizeof(p.lat)) || !f.read((char *)&p.lon, sizeof(p.lon)) ||
				!f.read((char *)&source, sizeof(source)) || !f.read((char *)&time, sizeof(time)) || !f.read((char *)&mmsi, sizeof(mmsi)))
				break;
			if ((source != Position::GPS && source != Position::VESSEL) || !isValidCoord(p.lat, p.lon))
				continue;
			p.source = (Position::Source)source;
			p.time = (std::time_t)time;
			auto it = stations.find(id);
			if (it == stations.end())
			{
				if (listed)
					continue;
				it = stations.emplace(id, Station()).first;
				it->second.mmsi = mmsi;
			}
			else if (!((source == Position::GPS && it->second.followsGPS()) || (source == Position::VESSEL && it->second.mmsi == mmsi)))
				continue;
			it->second.now = p;
		}
	}

private:
	struct Station
	{
		Position now;
		uint32_t mmsi = 0;
		bool gps = true;
		int inputs = 0;
		// one source at a time: the vessel it rides, else GPS when chosen, else where it was put
		bool followsGPS() const { return gps && !mmsi; }
		bool fixed() const { return now.source == Position::GPS || now.source == Position::VESSEL; }
	};
	std::mutex mtx;
	StationRegistry registry;
	std::map<int, Station> stations;
	std::map<uint32_t, int> riders;
	bool listed = false;
};
