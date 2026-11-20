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
#include <cstring>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "Marine/Message.h"

class MetricTable
{
public:
	struct Row
	{
		int station = 0;
		char channel = 0;
		int key = 0;
		double value = 0;
		int window = 0;
		std::string unit;
		std::time_t at = 0;
	};

	bool set(int station, const AIS::Metric &m, Row &kept)
	{
		if (m.key < 0 || m.key >= AIS::KEY_COUNT)
			return false;
		const char channel = m.channel >= 'A' && m.channel <= 'D' ? m.channel : 0;
		const bool level = std::strcmp(AIS::KeyInfoMap[m.key].unit, "dB") == 0;
		std::lock_guard<std::mutex> lock(mtx);
		if (!table.count(station) && table.size() >= MAX_STATIONS)
			return false;
		std::vector<Row> &rows = table[station];
		auto it = std::find_if(rows.begin(), rows.end(), [&](const Row &r) { return r.key == m.key && r.channel == channel; });
		if (it == rows.end())
		{
			if (rows.size() >= MAX_ROWS)
				return false;
			it = rows.insert(rows.end(), Row());
			it->station = station;
			it->channel = channel;
			it->key = m.key;
		}
		it->value = m.value;
		it->window = std::max(0, std::min(m.window, 86400));
		it->unit.clear();
		for (const char *c = level ? m.unit : nullptr; c && *c && it->unit.size() < 8; c++)
			if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '%')
				it->unit += *c;
		it->at = std::time(nullptr);
		kept = *it;
		return true;
	}

	std::vector<Row> of(const std::vector<int> &stations)
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::vector<Row> out;
		for (const auto &s : table)
			if (stations.empty() || std::find(stations.begin(), stations.end(), s.first) != stations.end())
				out.insert(out.end(), s.second.begin(), s.second.end());
		return out;
	}

private:
	static const std::size_t MAX_STATIONS = 256, MAX_ROWS = 64;
	std::mutex mtx;
	std::map<int, std::vector<Row>> table;
};

struct MetricHistory
{
	static const int SPANS = 3;
	static int places(int span)
	{
		static const int n[SPANS] = {60, 24, 90};
		return n[span];
	}
	static long int interval(int span)
	{
		static const long int seconds[SPANS] = {60, 3600, 86400};
		return seconds[span];
	}
	static bool kept(int key) { return key == AIS::KEY_NOISE_FLOOR || key == AIS::KEY_CHANNEL_LOAD || key == AIS::KEY_CLIPPING; }

	struct Place
	{
		int32_t slot = 0;
		float sum = 0;
		uint16_t n = 0;
	};
	struct Series
	{
		int key = 0;
		char channel = 0;
		char unit[8] = {0};
		std::vector<Place> span[SPANS];
	};
	std::vector<Series> series;

	void add(const MetricTable::Row &r)
	{
		if (!kept(r.key))
			return;
		auto it = std::find_if(series.begin(), series.end(), [&](const Series &s) { return s.key == r.key && s.channel == r.channel; });
		if (it == series.end())
		{
			if (series.size() >= MAX_SERIES)
				return;
			it = series.insert(series.end(), Series());
			it->key = r.key;
			it->channel = r.channel;
			for (int s = 0; s < SPANS; s++)
				it->span[s].resize(places(s));
		}
		std::strncpy(it->unit, r.unit.c_str(), sizeof(it->unit) - 1);
		for (int s = 0; s < SPANS; s++)
		{
			const int32_t slot = (int32_t)(r.at / interval(s));
			Place &p = it->span[s][slot % places(s)];
			if (p.slot != slot)
				p = Place();
			p.slot = slot;
			if (p.n < UINT16_MAX)
			{
				p.sum += (float)r.value;
				p.n++;
			}
		}
	}

	static bool at(const Series &s, int span, long int start, float &value)
	{
		const int32_t slot = (int32_t)(start / interval(span));
		const Place &p = s.span[span][slot % places(span)];
		if (p.slot != slot || !p.n)
			return false;
		value = p.sum / p.n;
		return true;
	}

	bool save(std::ofstream &f) const
	{
		const uint8_t count = (uint8_t)series.size();
		bool ok = (bool)f.write((const char *)&count, 1);
		for (const Series &s : series)
		{
			char name[24] = {0};
			std::strncpy(name, AIS::KeyMap[s.key][JSON_DICT_FULL].p, sizeof(name) - 1);
			ok = ok && f.write(name, sizeof(name)) && f.write(&s.channel, 1) && f.write(s.unit, sizeof(s.unit));
			for (int i = 0; i < SPANS; i++)
				ok = ok && f.write((const char *)s.span[i].data(), places(i) * sizeof(Place));
		}
		return ok;
	}
	bool load(std::ifstream &f)
	{
		uint8_t count = 0;
		if (!f.read((char *)&count, 1) || count > MAX_SERIES)
			return false;
		series.clear();
		for (int k = 0; k < count; k++)
		{
			Series s;
			char name[24];
			if (!f.read(name, sizeof(name)) || !f.read(&s.channel, 1) || !f.read(s.unit, sizeof(s.unit)))
				return false;
			name[sizeof(name) - 1] = s.unit[sizeof(s.unit) - 1] = 0;
			for (int i = 0; i < SPANS; i++)
			{
				s.span[i].resize(places(i));
				if (!f.read((char *)s.span[i].data(), places(i) * sizeof(Place)))
					return false;
			}
			for (int key = 0; key < AIS::KEY_COUNT; key++)
				if (kept(key) && std::strcmp(name, AIS::KeyMap[key][JSON_DICT_FULL].p) == 0)
				{
					s.key = key;
					series.push_back(s);
				}
		}
		return true;
	}

private:
	static const std::size_t MAX_SERIES = 16;
};
