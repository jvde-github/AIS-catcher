/*
	Copyright(c) 2026 jvde.github@gmail.com

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
#include <fstream>
#include <map>
#include <vector>

#include "Marine/Message.h"
#include "Writer.h"

// What a station's GPS did over the last day and week. Per slot: the worst
// fix, the satellites used and seen, the highest HDOP, the average signal per
// system and band, and the position. Per satellite and band its signal, for
// the day only. A slot that heard nothing stays empty: the GPS was silent.
// Positions leave only as metres from the median of the span, never as such.
class GnssHistory
{
public:
	static const int SYSTEMS = 4; // GPS, GLONASS, Galileo, BeiDou; the others have rows, no average
	static const int GROUPS = SYSTEMS * AIS::Sky::BANDS;
	static const int DAY_SLOTS = 720; // of two minutes

	// what the day held that a person should know of, by slot of the day
	struct Event
	{
		char kind;			  // 'j' jamming suspected, 'g' nothing heard, 's' a position jump
		int from, to;		  // slots, both included
		int l1 = 0, l5 = 0;	  // the drop in dB-Hz; l5 counts with has_l5 only
		bool has_l5 = false;
		int used_before = -1, used_min = 0, fix = 0, satellites = 0;
		double off = 0; // metres from the median position, the most
	};

	void add(std::time_t now, const AIS::GPS &gps);
	// last: only that many of the newest slots, for whoever has the rest already; 0 for the whole span
	void json(JSON::Writer &w, std::time_t now, bool week, int last = 0) const;
	// what the health asks: how long the history runs, whether jamming shows now, and how far the
	// latest fix lies from where the station is: the place given, or the day's median without one
	struct Now
	{
		double hours = 0;
		bool signals = false, jam = false;
		int drop = 0; // dB, of the L1 signals
		bool placed = false, has_off = false;
		double off = 0; // metres
		std::time_t heard = 0, fixed = 0;
	};
	Now read(std::time_t now, bool has_place, double lat, double lon) const;
	std::time_t lastHeard() const { return heard; }
	std::time_t lastFix() const { return fixed; }
	bool save(std::ofstream &f) const;
	// 0 of none
	static double median(std::vector<double> v);
	bool load(std::ifstream &f);

private:
	struct Slot
	{
		int32_t t = -1; // the slot's number: the time over the interval
		uint16_t reports = 0, fixes = 0;
		uint8_t fix = 0;			 // the worst reported: 1 none, 2 2D, 3 3D
		uint8_t used = 0, seen = 0;	 // averages over the reports
		uint8_t hdop = 0;			 // the highest, in tenths, 255 for 25.5 and up
		uint8_t signal[GROUPS] = {}; // dB-Hz, 0 for not heard
		double lat = 0, lon = 0;	 // the average position
		double at_lat = 0, at_lon = 0; // the slot's last fix: the scatter
	};

	struct Sums
	{
		int32_t t = -1;
		uint32_t reports = 0, fixes = 0, used = 0, seen = 0;
		int fix = 0;
		float hdop = 0;
		float signal[GROUPS] = {};
		uint32_t heard[GROUPS] = {};
		double lat = 0, lon = 0, at_lat = 0, at_lon = 0;
		Slot slot() const;
	};

	struct Span
	{
		int interval, places;
		std::vector<Slot> ring;
		Sums open;
		Span(int i, int p) : interval(i), places(p), ring(p) {}
		// the slot numbered t as kept, or nullptr when nothing was heard then
		bool at(int32_t t, Slot &out) const;
	};

	// one satellite on one band over the day: its signal per slot, 0 for not heard
	struct Row
	{
		uint8_t v[DAY_SLOTS] = {};
		float sum = 0;
		uint32_t n = 0;
		int32_t last = 0;
	};

	Span day{120, DAY_SLOTS}, week{600, 1008};
	std::map<uint32_t, Row> rows; // system << 16 | id << 2 | band

	// a day slot judged once, when it closes: jammed or not, by how much; made again on loading, not kept
	struct Mark
	{
		int32_t t = -1;
		bool jam = false, has5 = false;
		int8_t drop1 = 0, drop5 = 0; // the median drop of the L1 and L5 signals, each against its own half hour before
		uint8_t sats = 0;			 // the L1 satellites it went by
		uint8_t level = 0;			 // the median L1 signal of the slot, 0 without one
	};
	std::vector<Mark> marks = std::vector<Mark>(DAY_SLOTS);
	// when the GPS last reported, and last had a position; after a restart from the newest slot
	std::time_t heard = 0, fixed = 0;
	void mark(int32_t t);

	// Jamming: most L1 signals fall at once, each against its own level of the half hour before.
	// A gap is a stretch the GPS sent nothing; a jump a position far from the rest, outside a jam.
	// Of the day's slots from `first`, as json() gathered them, around their median position.
	std::vector<Event> events(int32_t first, const std::vector<Slot> &slots, const std::vector<char> &held, double ref_lat, double ref_lon) const;

	uint8_t value(const Row &r, int32_t t) const;
	void roll(Span &s, int32_t t);
	void closeRows(int32_t closed, int32_t next);
};
