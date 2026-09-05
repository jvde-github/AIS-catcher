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
#include <cstring>
#include <ctime>
#include <vector>

#include "JSON/Writer.h"

// What changed about a vessel rather than where it went, kept per ship: a
// ship slot refers to one block of its own, claimed on the first change and
// returned when the slot is recycled, so a vessel that never alters anything
// holds nothing and no vessel's history is pushed out by another's.
//
// A change is a before/after pair for draught and status, the new value for the
// text fields, the four figures of an ETA. A field that changes again inside its window is the same episode:
// the entry takes the new value and its time, its onset stands, and a numeric
// pair that lands back where it began is dropped as no change at all. A
// draught baseline is kept once so the chart has somewhere to start from.

class StaticStore
{
public:
	enum Field : uint8_t
	{
		DRAUGHT = 1,
		STATUS = 2,
		SHIPNAME = 3,
		CALLSIGN = 4,
		DESTINATION = 5,
		ETA = 6
	};

	enum : uint32_t { NIL = 0xFFFFFFFFu };
	enum : uint8_t { INITIAL = 1 };
	enum : std::size_t { SLOTS = 16 };

	void setup(int nships)
	{
		ref.assign(nships > 0 ? nships : 0, NIL);
		blocks.clear();
		free_list.clear();
	}

	void wipe(int ptr)
	{
		if (ptr < 0 || (std::size_t)ptr >= ref.size() || ref[ptr] == NIL)
			return;
		free_list.push_back(ref[ptr]);
		ref[ptr] = NIL;
	}

	void addNumeric(int ptr, Field field, uint8_t from, uint8_t to, std::time_t now, bool initial = false)
	{
		if (!slot(ptr) || (!initial && from == to))
			return;

		Entry e = {};
		e.time = e.first = (uint32_t)now;
		e.field = field;
		e.flags = initial ? INITIAL : 0;
		e.from = from;
		e.to = to;
		add(ptr, e);
	}

	void addText(int ptr, Field field, const char *value, std::time_t now)
	{
		if (!slot(ptr) || !value || !*value)
			return;

		Entry e = {};
		e.time = e.first = (uint32_t)now;
		e.field = field;
		std::strncpy(e.value, value, sizeof(e.value) - 1);
		add(ptr, e);
	}

	void addEta(int ptr, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, std::time_t now)
	{
		if (!slot(ptr))
			return;

		Entry e = {};
		e.time = e.first = (uint32_t)now;
		e.field = ETA;
		e.value[0] = (char)month;
		e.value[1] = (char)day;
		e.value[2] = (char)hour;
		e.value[3] = (char)minute;
		add(ptr, e);
	}

	// newest first; the card orders what it draws itself
	void writeJSON(JSON::Writer &w, int ptr) const
	{
		w.beginArray();
		if (slot(ptr) && ref[ptr] != NIL)
		{
			const Block &b = blocks[ref[ptr]];
			for (std::size_t i = 0; i < b.count; i++)
				writeEntry(w, b.at(b.count - 1 - i), 0);
		}
		w.endArray();
	}

private:
	struct Entry
	{
		uint32_t time, first;
		uint8_t field, flags, from, to;
		char value[24];
	};

	struct Block
	{
		Entry e[SLOTS];
		uint8_t head = 0, count = 0;

		// i-th oldest
		const Entry &at(std::size_t i) const { return e[(head + i) % SLOTS]; }
		Entry &at(std::size_t i) { return e[(head + i) % SLOTS]; }
	};

	// how long a field's change stays the same episode; the baseline flag keeps
	// a first value once for a chart to start from
	struct Policy
	{
		uint32_t window;
		bool baseline;
	};

	static const Policy &policy(uint8_t field)
	{
		static const Policy p[] = {
			{0, false},		   // unused
			{120, true},	   // DRAUGHT
			{120, false},	   // STATUS
			{600, false},	   // SHIPNAME
			{600, false},	   // CALLSIGN
			{120, false},	   // DESTINATION
			{0xFFFFFFFFu, false} // ETA: one entry, always the latest
		};
		return p[field <= ETA ? field : 0];
	}

	static bool numeric(uint8_t field) { return field == DRAUGHT || field == STATUS; }

	bool slot(int ptr) const { return ptr >= 0 && (std::size_t)ptr < ref.size(); }

	Block &block(int ptr)
	{
		if (ref[ptr] == NIL)
		{
			if (!free_list.empty())
			{
				ref[ptr] = free_list.back();
				free_list.pop_back();
				blocks[ref[ptr]] = Block();
			}
			else
			{
				ref[ptr] = (uint32_t)blocks.size();
				blocks.push_back(Block());
			}
		}
		return blocks[ref[ptr]];
	}

	static Entry *newest(Block &b, uint8_t field)
	{
		for (std::size_t i = 0; i < b.count; i++)
		{
			Entry &e = b.at(b.count - 1 - i);
			if (e.field == field)
				return &e;
		}
		return nullptr;
	}

	static void push(Block &b, const Entry &e)
	{
		if (b.count < SLOTS)
			b.at(b.count++) = e;
		else
		{
			b.e[b.head] = e;
			b.head = (b.head + 1) % SLOTS;
		}
	}

	static void erase(Block &b, Entry *victim)
	{
		std::size_t i = 0;
		while (i < b.count && &b.at(i) != victim)
			i++;
		for (; i + 1 < b.count; i++)
			b.at(i) = b.at(i + 1);
		if (b.count)
			b.count--;
	}

	void add(int ptr, const Entry &in)
	{
		const Policy &pol = policy(in.field);
		Block &b = block(ptr);
		Entry *prev = newest(b, in.field);

		if (in.flags & INITIAL)
		{
			if (!pol.baseline || prev)
				return;
			push(b, in);
			return;
		}

		const uint32_t age = prev ? (in.time > prev->first ? in.time - prev->first : prev->first - in.time) : 0;
		if (prev && !(prev->flags & INITIAL) && age < pol.window)
		{
			prev->to = in.to;
			std::memcpy(prev->value, in.value, sizeof(prev->value));
			if (in.time > prev->time)
				prev->time = in.time;
			if (numeric(in.field) && prev->from == prev->to)
				erase(b, prev);
			return;
		}

		push(b, in);
	}

	static void writeEntry(JSON::Writer &w, const Entry &e, uint32_t mmsi)
	{
		w.beginObject().kv("t", (int)e.time);
		if (mmsi)
			w.kv("mmsi", (int)mmsi);
		w.kv("f", (int)e.field);
		if (numeric(e.field))
			w.kv("from", (int)e.from).kv("to", (int)e.to);
		else if (e.field == ETA)
		{
			w.key("to").beginArray();
			for (int i = 0; i < 4; i++)
				w.val((int)(uint8_t)e.value[i]);
			w.endArray();
		}
		else
			w.kv("to", e.value);
		if (e.flags & INITIAL)
			w.kv("i", 1);
		w.endObject();
	}

	std::vector<Block> blocks;
	std::vector<uint32_t> ref;
	std::vector<uint32_t> free_list;
};
