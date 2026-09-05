/* Copyright(c) 2021-2026 jvde.github@gmail.com

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
#include <ctime>
#include <string>
#include <vector>

#include "Common.h"
#include "Writer.h"

// What the receiver found worth telling: a safety message, or a field of a
// vessel's record changing. Structured fields stay inside the ring for repeat
// detection; ticker-v1 JSON carries fully worded, escaped presentation text.
// Every event has a place, stamped when it is made, so a reader can go there. One ring per level: a burst of routine events fills its own
// ring and never pushes out a distress call. The same words from the same sender, heard again while the
// earlier event is still live, count on that event instead of becoming another
// one: a device left transmitting for days is one event that began days ago.
class EventRing
{
public:
	enum Kind : uint8_t { SAFETY = 1, DESTINATION = 2, STATUS = 3, DRAUGHT = 4 };
	enum Level : uint8_t { ROUTINE = 0, NOTICE = 1, URGENT = 2, LEVELS = 3 };

	struct Event
	{
		uint64_t seq = 0;
		std::time_t time = 0, first = 0;
		Kind kind = SAFETY;
		Level level = ROUTINE;
		uint32_t from = 0, to = 0;
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		int count = 1;
		// a change reads as `label`: `was` -> `text`; a safety message is `text` alone
		std::string text, was, label, from_name, to_name;
	};

	// Escape received text before adding the three ticker formatting markers.
	static std::string escapeText(const std::string &text)
	{
		std::string out;
		for (char c : text)
		{
			if (c == '\\' || c == '*' || c == ':' || c == '[' || c == ']') out += '\\';
			out += c;
		}
		return out;
	}

	static std::string displayName(uint32_t mmsi, const std::string &name)
	{
		const std::string id = std::to_string(mmsi);
		if (id.compare(0, 3, "972") == 0) return "MOB device";
		if (id.compare(0, 3, "974") == 0) return "EPIRB";
		if (id.compare(0, 3, "970") == 0) return "AIS-SART";
		if (mmsi >= 2000000 && mmsi <= 9999999) return "VTS " + (name.empty() ? id : name);
		return name.empty() ? "MMSI " + id : name;
	}

	static std::string formatText(const Event &e)
	{
		std::string out = e.label.empty() && e.level >= NOTICE ? "⚠ " : "";
		out += "**" + escapeText(displayName(e.from, e.from_name)) + "**";
		if (!e.label.empty())
			return out + " · ::" + escapeText(e.label + (e.was.empty() ? "" : " " + e.was)) + ":: → [[" + escapeText(e.text) + "]]";
		if (e.to) out += " → **" + escapeText(displayName(e.to, e.to_name)) + "**";
		out += " · [[" + escapeText(e.text) + "]]";
		if (e.count > 1) out += " (×" + std::to_string(e.count) + ")";
		return out;
	}

	EventRing()
	{
		for (int l = 0; l < LEVELS; l++)
			rings[l].reserve(capacity(l));
	}

	void push(Event e)
	{
		std::vector<Event> &r = rings[e.level];
		for (Event &have : r)
			if (have.kind == e.kind && have.from == e.from && have.to == e.to && have.text == e.text && e.time - have.time <= COLLAPSE_S)
			{
				have.count++;
				have.time = e.time;
				return;
			}
		e.seq = ++next_seq;
		e.first = e.time;
		if (r.size() < capacity(e.level))
			r.push_back(e);
		else
		{
			r[head[e.level]] = e;
			head[e.level] = (head[e.level] + 1) % capacity(e.level);
		}
	}

	uint64_t sequence() const { return next_seq; }

	// the events after `since` of at least `level`, newest last; the horizon runs
	// from an event's onset, longer per level: a repeat counts on an event, it
	// does not make old news current
	void writeSince(JSON::Writer &w, uint64_t since, int level, std::time_t now) const
	{
		std::vector<const Event *> picked;
		for (int l = MAX(0, MIN(level, LEVELS - 1)); l < LEVELS; l++)
			for (const Event &e : rings[l])
				if (e.seq > since && now - e.first <= HORIZON_S * (l + 1))
					picked.push_back(&e);
		std::sort(picked.begin(), picked.end(), [](const Event *a, const Event *b) { return a->seq < b->seq; });
		if ((int)picked.size() > LIMIT)
			picked.erase(picked.begin(), picked.end() - LIMIT);
		w.key("events").beginArray();
		for (const Event *e : picked)
		{
			w.beginObject().kv("seq", (long long)e->seq).kv("t", (long long)e->time).kv("first", (long long)e->first)
				.kv("format", "ticker-v1").kv("level", (int)e->level).kv("mmsi", (long long)e->from);
			if (isValidCoord(e->lat, e->lon))
				w.kv("lat", e->lat).kv("lon", e->lon);
			if (e->count > 1)
				w.kv("count", e->count);
			w.kv("text", formatText(*e)).endObject();
		}
		w.endArray();
	}

private:
	enum { COLLAPSE_S = 4 * 3600, HORIZON_S = 600, LIMIT = 50 };
	static size_t capacity(int level)
	{
		static const size_t c[LEVELS] = {128, 64, 64};
		return c[level];
	}
	std::vector<Event> rings[LEVELS];
	size_t head[LEVELS] = {0, 0, 0};
	uint64_t next_seq = 0;
};
