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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <vector>

#include "History.h"
#include "Metrics.h"
#include "Statistics.h"
#include "Writer.h"

struct Coverage
{
	static const int DIRECTIONS = 24, RINGS = 19, CELLS = DIRECTIONS * RINGS;

	struct Cell
	{
		uint32_t n, leveled;
		float level;
	};

	struct Span
	{
		int interval, slots;
		std::vector<long int> slot;
		std::vector<Cell> cells;

		Span(int interval_, int slots_) : interval(interval_), slots(slots_), slot(slots_, 0), cells((std::size_t)slots_ * CELLS, Cell{0, 0, 0}) {}

		Cell *at(long int s)
		{
			const std::size_t i = (std::size_t)(s % slots);
			if (s < slot[i])
				return nullptr;
			if (s > slot[i])
			{
				slot[i] = s;
				std::fill(cells.begin() + i * CELLS, cells.begin() + (i + 1) * CELLS, Cell{0, 0, 0});
			}
			return &cells[i * CELLS];
		}

		static void add(Cell *to, const Cell *from)
		{
			for (int c = 0; c < CELLS; c++)
			{
				to[c].n += from[c].n;
				to[c].leveled += from[c].leveled;
				to[c].level += from[c].level;
			}
		}

		std::vector<Cell> sum(long int now) const
		{
			std::vector<Cell> out(CELLS, Cell{0, 0, 0});
			const long int s = now / interval;
			for (int i = 0; i < slots; i++)
				if (slot[i] > s - slots && slot[i] <= s)
					add(out.data(), &cells[(std::size_t)i * CELLS]);
			return out;
		}

		void add(const Span &o)
		{
			for (int i = 0; i < o.slots; i++)
				if (o.slot[i])
					if (Cell *to = at(o.slot[i]))
						add(to, &o.cells[(std::size_t)i * CELLS]);
		}

		bool save(std::ofstream &f) const
		{
			return f.write((const char *)slot.data(), slot.size() * sizeof(long int)) && f.write((const char *)cells.data(), cells.size() * sizeof(Cell));
		}
		bool load(std::ifstream &f)
		{
			return f.read((char *)slot.data(), slot.size() * sizeof(long int)) && f.read((char *)cells.data(), cells.size() * sizeof(Cell));
		}
	};

	Span hour{600, 6}, day{10800, 8}, week{86400, 7};

	static int ring(float nm)
	{
		return nm >= 0.5f ? MIN(RINGS - 1, 1 + (int)(2 * std::log2(nm / 0.5f))) : 0;
	}

	void add(long int t, int angle, float nm, float level)
	{
		const int c = angle / (360 / DIRECTIONS) * RINGS + ring(nm);
		for (Span *s : {&hour, &day, &week})
			if (Cell *cells = s->at(t / s->interval))
			{
				cells[c].n++;
				if (level != LEVEL_UNDEFINED)
				{
					cells[c].leveled++;
					cells[c].level += level;
				}
			}
	}

	void add(const Coverage &o)
	{
		hour.add(o.hour);
		day.add(o.day);
		week.add(o.week);
	}

	bool save(std::ofstream &f) const { return hour.save(f) && day.save(f) && week.save(f); }
	bool load(std::ifstream &f) { return hour.load(f) && day.load(f) && week.load(f); }
};

// What one stream of messages adds up to: totals kept across restarts and for
// this run, and the figures by second, minute, hour and day. The tracker keeps
// one for everything it hears and one for each station.
struct Figures : public StreamIn<JSON::JSON>
{
	Counter total, session;
	History<60, 1> second;
	History<60, 60> minute;
	History<168, 3600> hour;
	History<365, 86400> day;

	// The week by the UTC hour, Monday first: the vessels heard in each hour of
	// each weekday added up, and the hours a message came in, so that the one
	// over the other is the usual traffic of that hour.
	uint32_t week_vessels[7][24], week_hours[7][24];

	// The signal by the distance it came from, in bands of LEVEL_BAND_NM, the
	// last holding all beyond: the levels added up and their messages, since the
	// figures were first kept and since AIS-catcher started.
	static const int LEVEL_BANDS = 11, LEVEL_BAND_NM = 10;
	struct Levels
	{
		double sum[LEVEL_BANDS];
		uint32_t n[LEVEL_BANDS];
	};
	Levels levels_total, levels_session;

	std::unique_ptr<Coverage> coverage;

	std::unique_ptr<MetricHistory> reception;

	// A day of exceptional range, as tropospheric ducting brings
	struct Propagation
	{
		std::time_t day;
		float max_nm, usual_nm;
	};

	Figures() { clearProfile(); }

	void Receive(const JSON::JSON *data, int len, TAG &tag) override
	{
		total.Receive(data, len, tag);
		session.Receive(data, len, tag);
		second.Receive(data, len, tag);
		minute.Receive(data, len, tag);
		hour.Receive(data, len, tag);
		day.Receive(data, len, tag);
		for (int i = 0; i < len; i++)
			if (data[i].binary)
				profile(*(const AIS::Message *)data[i].binary, tag);
	}

	void setCutoff(int cutoff)
	{
		range_cutoff = cutoff;
		total.setCutOff(cutoff);
		session.setCutOff(cutoff);
		second.setCutoff(cutoff);
		minute.setCutoff(cutoff);
		hour.setCutoff(cutoff);
		day.setCutoff(cutoff);
	}

	// several streams' figures as one, in place of these: what they count adds
	// up, their extremes take the extreme. The parts must not change meanwhile.
	void combine(const std::vector<Figures *> &parts)
	{
		total.Clear();
		session.Clear();
		std::vector<decltype(second) *> seconds;
		std::vector<decltype(minute) *> minutes;
		std::vector<decltype(hour) *> hours;
		std::vector<decltype(day) *> days;
		for (Figures *f : parts)
		{
			total.add(f->total);
			session.add(f->session);
			seconds.push_back(&f->second);
			minutes.push_back(&f->minute);
			hours.push_back(&f->hour);
			days.push_back(&f->day);
		}
		second.combine(seconds);
		minute.combine(minutes);
		hour.combine(hours);
		day.combine(days);
		// the vessels add up; the hours heard in are those of the one that heard most
		clearProfile();
		for (Figures *f : parts)
		{
			for (int d = 0; d < 7; d++)
				for (int h = 0; h < 24; h++)
				{
					week_vessels[d][h] += f->week_vessels[d][h];
					week_hours[d][h] = MAX(week_hours[d][h], f->week_hours[d][h]);
				}
			for (int b = 0; b < LEVEL_BANDS; b++)
			{
				levels_total.sum[b] += f->levels_total.sum[b];
				levels_total.n[b] += f->levels_total.n[b];
				levels_session.sum[b] += f->levels_session.sum[b];
				levels_session.n[b] += f->levels_session.n[b];
			}
			if (f->coverage)
			{
				if (!coverage)
					coverage.reset(new Coverage());
				coverage->add(*f->coverage);
			}
		}
	}

	void clear()
	{
		total.Clear();
		session.Clear();
		second.Clear();
		minute.Clear();
		hour.Clear();
		day.Clear();
		clearProfile();
	}

	// the session counter starts again with every run, so it is not kept
	bool save(std::ofstream &f) { return total.Save(f) && second.Save(f) && minute.Save(f) && hour.Save(f) && day.Save(f); }
	bool load(std::ifstream &f) { return total.Load(f) && second.Load(f) && minute.Load(f) && hour.Load(f) && day.Load(f); }

	bool saveProfile(std::ofstream &f)
	{
		const int version = PROFILE_VERSION;
		const char covered = coverage ? 1 : 0, told = reception ? 1 : 0;
		return f.write((const char *)&version, sizeof(version)) && f.write((const char *)week_vessels, sizeof(week_vessels)) &&
			   f.write((const char *)week_hours, sizeof(week_hours)) && f.write((const char *)&levels_total, sizeof(levels_total)) &&
			   f.write(&covered, 1) && (!coverage || coverage->save(f)) && f.write(&told, 1) && (!reception || reception->save(f));
	}
	bool loadProfile(std::ifstream &f)
	{
		int version = 0;
		char covered = 0, told = 0;
		if (!f.read((char *)&version, sizeof(version)) || version < 1 || version > PROFILE_VERSION)
			return false;
		if (!f.read((char *)week_vessels, sizeof(week_vessels)) || !f.read((char *)week_hours, sizeof(week_hours)) ||
			!f.read((char *)&levels_total, sizeof(levels_total)) || (version >= 2 && !f.read(&covered, 1)))
			return false;
		coverage.reset(covered ? new Coverage() : nullptr);
		if ((coverage && !coverage->load(f)) || (version >= 3 && !f.read(&told, 1)))
			return false;
		reception.reset(told ? new MetricHistory() : nullptr);
		return !reception || reception->load(f, version >= 4);
	}

	// The days of exceptional range, oldest first, today's when it is one: the
	// day's farthest position at least twice the usual, the median of the 30
	// days before of which at least 7 heard something, and 20 NM beyond it; and
	// the third vessel beyond one and a half times the usual, so that one stray
	// position makes no such day.
	std::vector<Propagation> propagation()
	{
		struct Day
		{
			std::time_t start;
			float max, third;
		};
		std::vector<Day> days;
		day.forEachSlot([&](long int start, MessageStatistics &s, bool empty) {
			days.push_back(Day{(std::time_t)start, empty ? -1.0f : s.maxDistance(), empty ? -1.0f : s.farDistance(2)});
		});
		std::vector<Propagation> out;
		for (std::size_t i = 0; i < days.size(); i++)
		{
			if (days[i].max <= 0)
				continue;
			std::vector<float> before;
			for (std::size_t j = i >= 30 ? i - 30 : 0; j < i; j++)
				if (days[j].max > 0)
					before.push_back(days[j].max);
			if (before.size() < 7)
				continue;
			std::sort(before.begin(), before.end());
			const std::size_t n = before.size();
			const float usual = n % 2 ? before[n / 2] : (before[n / 2 - 1] + before[n / 2]) / 2;
			if (usual > 0 && days[i].max >= 2 * usual && days[i].max - usual >= 20 && days[i].third > 1.5f * usual)
				out.push_back(Propagation{days[i].start, days[i].max, usual});
		}
		return out;
	}

private:
	static const int PROFILE_VERSION = 4;
	int range_cutoff = LONG_RANGE_CUTOFF_DEFAULT;
	long int last_minute = 0, last_hour = 0;

	void clearProfile()
	{
		std::memset(week_vessels, 0, sizeof(week_vessels));
		std::memset(week_hours, 0, sizeof(week_hours));
		std::memset(&levels_total, 0, sizeof(levels_total));
		std::memset(&levels_session, 0, sizeof(levels_session));
		coverage.reset();
		reception.reset();
		last_minute = last_hour = 0;
	}

	void profile(const AIS::Message &m, const TAG &tag)
	{
		const long int t = (long int)m.getRxTimeUnix();
		if (t <= 0)
			return;
		// a minute heard in counts once, in each of the figures that count them
		if (t / 60 > last_minute)
		{
			last_minute = t / 60;
			total.addActiveMinute();
			session.addActiveMinute();
			minute.addActiveMinute();
			hour.addActiveMinute();
			day.addActiveMinute();
		}
		const long int h = t / 3600;
		const int weekday = (int)((h / 24 + 3) % 7), hr = (int)(h % 24);
		if (h > last_hour)
		{
			last_hour = h;
			week_hours[weekday][hr]++;
		}
		// a vessel counts in the hour it is first heard in, as the hourly figures count it
		if (h == last_hour && h != (long int)(tag.previous_signal / 3600))
			week_vessels[weekday][hr]++;

		// the positions the range figures take: validated, heard directly, no aid to navigation
		if (!tag.validated || tag.distance < 0 || tag.distance > range_cutoff ||
			m.type() == 21 || m.type() == 28 || (m.repeat() > 0 && m.type() != 27))
			return;
		if (tag.angle >= 0 && tag.angle < 360)
		{
			if (!coverage)
				coverage.reset(new Coverage());
			coverage->add(t, tag.angle, tag.distance, tag.level);
		}
		if (tag.level == LEVEL_UNDEFINED)
			return;
		const int band = MIN(LEVEL_BANDS - 1, (int)(tag.distance / LEVEL_BAND_NM));
		levels_total.sum[band] += tag.level;
		levels_total.n[band]++;
		levels_session.sum[band] += tag.level;
		levels_session.n[band]++;
	}
};
