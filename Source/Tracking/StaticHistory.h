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

// A log of what changed about a vessel rather than where it went: draught and
// navigation status as before/after pairs, and the free-text fields as the new
// value. Two rings rather than one because a status flip must not carry the 21
// bytes a destination needs.
//
// Deliberately not filtered for plausibility: a callsign that flips between two
// values, or a status oscillating once a second, means several transmitters share
// one MMSI or a decode is going wrong, and that is worth seeing.
class StaticHistory
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

	// A vessel flapping cannot crowd out every other vessel's history
	static const uint32_t MIN_INTERVAL = 10;

	// flags bit: this entry is the first value seen, not a change
	static const uint8_t INITIAL = 1;

	void setup(int numeric_entries, int text_entries)
	{
		num.assign(numeric_entries > 0 ? numeric_entries : 0, NumChange());
		txt.assign(text_entries > 0 ? text_entries : 0, TextChange());
		num_next = txt_next = 0;
	}

	// `initial` marks the first value ever seen rather than a change, so a vessel
	// that never alters course still has a baseline to draw from
	void addNumeric(uint32_t mmsi, Field field, uint8_t from, uint8_t to, std::time_t now, bool initial = false)
	{
		if (num.empty() || !valid(mmsi) || (!initial && from == to) || recent(num, num_next, mmsi, field, now))
			return;

		NumChange &e = num[num_next];
		e.mmsi = mmsi;
		e.time = (uint32_t)now;
		e.field = field;
		e.from = from;
		e.to = to;
		e.flags = initial ? INITIAL : 0;
		num_next = next(num_next, num.size());
	}

	void addText(uint32_t mmsi, Field field, const char *value, std::time_t now, bool initial = false)
	{
		if (txt.empty() || !valid(mmsi) || !value || !*value || recent(txt, txt_next, mmsi, field, now))
			return;

		TextChange &e = txt[txt_next];
		e.mmsi = mmsi;
		e.time = (uint32_t)now;
		e.field = field;
		e.flags = initial ? INITIAL : 0;
		std::strncpy(e.value, value, sizeof(e.value) - 1);
		e.value[sizeof(e.value) - 1] = '\0';
		txt_next = next(txt_next, txt.size());
	}

	// Newest first, so the reader can stop early; matches the path endpoint.
	void writeJSON(JSON::Writer &w, uint32_t mmsi) const
	{
		w.beginArray();
		emit(w, mmsi, num, num_next, writeNum);
		emit(w, mmsi, txt, txt_next, writeText);
		w.endArray();
	}

	// The whole fleet rather than one vessel, for a caller that polls: entries
	// at or before `since` are ones it already has, so each ring is walked from
	// the newest backwards and abandoned at the first stale entry.
	void writeRecentJSON(JSON::Writer &w, uint32_t since, std::size_t max) const
	{
		// a budget per ring, not one shared: a fleet reporting draughts would
		// otherwise spend the whole allowance before a single name is reached
		w.beginArray();
		emitRecent(w, since, max, num, num_next, writeNum);
		emitRecent(w, since, max, txt, txt_next, writeText);
		w.endArray();
	}

private:
	struct NumChange
	{
		uint32_t mmsi = 0;
		uint32_t time = 0;
		uint8_t field = 0, from = 0, to = 0, flags = 0;
	};

	struct TextChange
	{
		uint32_t mmsi = 0;
		uint32_t time = 0;
		uint8_t field = 0;
		uint8_t flags = 0;
		char value[21] = {0};
	};

	// MMSI 0 is not an identity: many transmitters share it, and letting it in
	// evicts real history several times faster
	static bool valid(uint32_t mmsi) { return mmsi != 0; }

	static std::size_t next(std::size_t i, std::size_t n) { return i + 1 == n ? 0 : i + 1; }
	static std::size_t prev(std::size_t i, std::size_t n) { return (i == 0 ? n : i) - 1; }

	// Flapping writes land next to each other, so the recent tail is the only
	// part worth checking and no per-ship timestamps are needed
	static const std::size_t LOOKBACK = 64;

	static void writeNum(JSON::Writer &o, const NumChange &e) { o.kv("from", (int)e.from).kv("to", (int)e.to); }
	static void writeText(JSON::Writer &o, const TextChange &e) { o.kv("to", e.value); }

	// one record shape for both endpoints
	template <typename E, typename F>
	static void writeEntry(JSON::Writer &w, const E &e, bool with_mmsi, F value)
	{
		w.beginObject().kv("t", (int)e.time);
		if (with_mmsi)
			w.kv("mmsi", (int)e.mmsi);
		w.kv("f", (int)e.field);
		value(w, e);
		if (e.flags & INITIAL)
			w.kv("i", 1);
		w.endObject();
	}

	// The two rings differ only in what a value looks like, so the walk, the
	// ownership check and the shared keys live here once.
	template <typename V, typename F>
	static void emit(JSON::Writer &w, uint32_t mmsi, const V &ring, std::size_t next_idx, F value)
	{
		for (std::size_t i = 0, r = prev(next_idx, ring.size()); i < ring.size(); i++, r = prev(r, ring.size()))
		{
			const auto &e = ring[r];
			if (e.mmsi != mmsi || !e.time)
				continue;

			writeEntry(w, e, false, value);
		}
	}

	template <typename V, typename F>
	static void emitRecent(JSON::Writer &w, uint32_t since, std::size_t left, const V &ring, std::size_t next_idx, F value)
	{
		for (std::size_t i = 0, r = prev(next_idx, ring.size()); i < ring.size() && left; i++, r = prev(r, ring.size()))
		{
			const auto &e = ring[r];

			// newest-first: an unwritten slot means the ring never wrapped
			if (!e.time)
				break;

			// the ring is in write order, so the first stale entry ends the walk
			if (e.time <= since)
				break;

			writeEntry(w, e, true, value);
			left--;
		}
	}

	template <typename V>
	static bool recent(const V &ring, std::size_t next_idx, uint32_t mmsi, uint8_t field, std::time_t now)
	{
		std::size_t n = ring.size() < LOOKBACK ? ring.size() : LOOKBACK;
		for (std::size_t i = 0, r = prev(next_idx, ring.size()); i < n; i++, r = prev(r, ring.size()))
			if (ring[r].mmsi == mmsi && ring[r].field == field)
				return (uint32_t)now - ring[r].time < MIN_INTERVAL;
		return false;
	}

	std::vector<NumChange> num;
	std::vector<TextChange> txt;
	std::size_t num_next = 0, txt_next = 0;
};
