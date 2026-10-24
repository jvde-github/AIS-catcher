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
}
