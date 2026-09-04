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

// What the receiver found worth telling: a safety message, a vessel bound
// somewhere new. Every event has a place, stamped when it is made, so a reader
// can go there. One ring per level: a burst of routine events fills its own
// ring and never pushes out a distress call. The same words from the same sender, heard again while the
// earlier event is still live, count on that event instead of becoming another
// one: a device left transmitting for days is one event that began days ago.
class EventRing
{
public:
	enum Kind : uint8_t { SAFETY = 1, DESTINATION = 2 };
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
		std::string text, was; // what the value was, where an event is a change
	};

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
				.kv("kind", (int)e->kind).kv("level", (int)e->level).kv("from", (long long)e->from);
			if (e->to)
				w.kv("to", (long long)e->to);
			if (isValidCoord(e->lat, e->lon))
				w.kv("lat", e->lat).kv("lon", e->lon);
			if (e->count > 1)
				w.kv("count", e->count);
			if (!e->was.empty())
				w.kv("was", e->was);
			w.kv("text", e->text).endObject();
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
