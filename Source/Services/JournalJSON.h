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
#include <functional>

#include "Http.h"
#include "Journal.h"
#include "Writer.h"

namespace Services
{
	// An event's id, the same in every reader and as a webhook's webhook-id: the
	// journal's epoch and the event's number in it. A list's cursor as it stands.
	inline std::string eventId(uint64_t epoch, uint64_t seq)
	{
		return cursor("evt", {(long long)epoch, (long long)seq});
	}

	// One event of the journal as every reader gets it: the API's lists, the
	// system's, a webhook's body, the MQTT event. A ship's event names the ship;
	// an event of the installation says what happened in `event` and `text`.
	// `lead` writes a reader's own fields first.
	inline void writeEvent(JSON::Writer &w, const Journal::Entry &e, uint64_t epoch, const std::function<void(JSON::Writer &)> &lead = nullptr)
	{
		const EventRing::Entry &v = e.event;
		w.beginObject();
		if (lead)
			lead(w);
		w.kv("id", eventId(epoch, e.seq)).kv("time", isoTime(v.time)).kv("kind", Tracking::journalName(v.kind))
			.kv("level", v.level == Tracking::Level::URGENT ? "urgent" : v.level == Tracking::Level::NOTICE ? "notice" : "routine");
		// the installation's: what happened, to which receiver and station
		if (v.kind == Tracking::Kind::SYSTEM)
		{
			w.kv("event", v.label).kv("text", v.text);
			if (v.receiver >= 0)
				w.kv("receiver", v.receiver);
			if (v.station >= 0)
				w.kv("station", v.station);
			w.endObject();
			return;
		}
		w.kv("mmsi", (long long)v.from);
		textOrNull(w, "name", v.from_name);
		if (v.to)
		{
			w.key("addressee").beginObject().kv("mmsi", (long long)v.to);
			textOrNull(w, "name", v.to_name);
			w.endObject();
		}
		if (!e.place.empty())
			w.kv("place", e.place);
		if (!v.label.empty())
			w.kv("field", v.label).kv("from", v.was).kv("to", v.text);
		else
			w.kv("text", v.text);
		if (isValidCoord(v.lat, v.lon))
			w.kv("lat", v.lat).kv("lon", v.lon);
		w.endObject();
	}

	// A page of the journal's events that `keep` passes, at `path`: without a
	// cursor the newest, with one the oldest after it. The journal is read a
	// chunk at a time until the page is full, and an event passed over still
	// moves the cursor.
	inline Response pageEvents(const Request &request, const Journal &journal, const std::string &path,
							   const std::function<bool(const Journal::Entry &)> &keep)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		const uint64_t epoch = journal.epoch();
		const std::string from = request.param("cursor");
		const std::size_t want = (std::size_t)limit, chunk = std::max<std::size_t>(want, 256);
		std::vector<Journal::Entry> page, read;
		uint64_t last = journal.newest();
		if (from.empty())
		{
			// back from the newest until the page is full or nothing older is kept
			for (uint64_t hi = last, lo = journal.beforeOldest(); page.size() < want && hi > lo;)
			{
				const uint64_t start = hi - lo > chunk ? hi - chunk : lo;
				if (!journal.since(start, (std::size_t)(hi - start), read))
					break;
				for (auto e = read.rbegin(); e != read.rend() && page.size() < want; ++e)
					if (keep(*e))
						page.push_back(*e);
				hi = start;
			}
			std::reverse(page.begin(), page.end());
		}
		else
		{
			std::vector<long long> c;
			if (!parseCursor(from, "evt", 2, c))
				return badCursor();
			if ((uint64_t)c[0] != epoch || (uint64_t)c[1] > last)
				return gone("The journal started again since this cursor was given.");
			last = (uint64_t)c[1];
			while (page.size() < want)
			{
				if (!journal.since(last, chunk, read))
					return gone("The events after this cursor are no longer kept.");
				if (read.empty())
					break;
				for (const auto &e : read)
				{
					last = e.seq;
					if (keep(e))
					{
						page.push_back(e);
						if (page.size() == want)
							break;
					}
				}
			}
		}
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &e : page)
			writeEvent(w, e, epoch);
		const std::string poll = path + "?cursor=" + eventId(epoch, last) + "&limit=" + std::to_string(limit);
		beginMeta(w, page.size(), last < journal.newest() ? poll : std::string());
		w.kv("poll", poll).endObject().endObject();
		w.finish();
		return Response::json(out);
	}
}
