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

#include "System.h"
#include <algorithm>
#include <climits>
#include <limits>
#include "JournalJSON.h"
#include "Routes.h"
#include "Helper.h"
#include "Logger.h"
#include "Writer.h"
#include "AIS-catcher.h"

namespace Services
{
	namespace
	{
		const long long LIST_MAX = 1000;

		Response badCursor()
		{
			return Response::problem(400, "Bad request", "cursor must be what a previous page gave in meta.next or meta.poll.");
		}

		Response gone(const std::string &why)
		{
			return Response::problem(410, "Gone", why + " Start again without cursor.");
		}

		long long age(std::time_t now, std::time_t then) { return then && now > then ? (long long)(now - then) : 0; }
	}

	int System::scope(const Request &request) const
	{
		const std::string base = "/api/v1/system";
		const std::string &p = request.path;
		const bool mine = p == "/api/v1/health" || (p.compare(0, base.size(), base) == 0 && (p.size() == base.size() || p[base.size()] == '/'));
		return mine ? (int)ApiKey::SYSTEM : NOT_MINE;
	}

	bool System::handle(const Request &request, Response &response)
	{
		const std::string base = "/api/v1/system";
		if (scope(request) == NOT_MINE)
			return false;
		if (request.method != "GET")
		{
			response = Response::problem(405, "Method not allowed", "The system is read here, not changed.");
			response.header = "Allow: GET, HEAD";
			return true;
		}
		if (const Route *r = route(request.path))
			for (const auto &p : request.params())
				if (!r->takes(p.first))
				{
					response = Response::problem(400, "Bad request", "\"" + p.first + "\" is not a parameter of " + r->path + ".");
					return true;
				}
		const std::string rest = request.path == "/api/v1/health" ? "/health" : request.path.substr(base.size());
		if (rest.empty() || rest == "/")
			response = overview();
		else if (rest == "/receivers")
			response = receivers();
		else if (rest == "/outputs")
			response = outputs();
		else if (rest == "/health")
			response = health();
		else if (rest == "/log")
			response = log(request);
		else if (rest == "/events")
			response = events(request);
		else
			response = Response::problem(404, "Not found", "No such resource: " + request.path);
		return true;
	}

	Response System::overview()
	{
		const SystemFacts f = facts();
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		w.beginObject().kv("name", "AIS-catcher").kv("version", VERSION).kv("describe", VERSION_DESCRIBE).kv("build_date", __DATE__)
			.kv("os", Util::Helper::getOS()).kv("hardware", Util::Helper::getHardware())
			.kv("uptime_s", age(now, f.born)).kv("memory_bytes", (long long)Util::Helper::getMemoryConsumption())
			.kv("received_bytes", (unsigned long long)f.received)
			.key("engine").beginObject().kv("running", f.running);
		if (f.running)
			w.kv("uptime_s", age(now, f.session));
		w.kv("receivers", (long long)f.receivers.size()).endObject()
			.key("resources").beginArray();
		for (const char *r : {"/api/v1/system/receivers", "/api/v1/system/outputs", "/api/v1/system/log", "/api/v1/system/events",
							  "/api/v1/health"})
			w.val(r);
		w.endArray().endObject();
		w.finish();
		return Response::json(out);
	}

	Response System::receivers()
	{
		const SystemFacts f = facts();
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : f.receivers)
		{
			const std::time_t heard = tracker.lastHeard(r.station);
			w.beginObject().kv("index", r.index).kv("station", r.station).kv("input", r.input).kv("label", r.label)
				.kv("state", r.status())
				.kv("messages", (unsigned long long)r.messages).kv("messages_last_minute", (unsigned long long)r.per_minute);
			if (heard)
				w.kv("last_heard", isoTime(heard)).kv("last_heard_age_s", age(now, heard));
			else
				w.kv_null("last_heard").kv_null("last_heard_age_s");
			w.endObject();
		}
		endList(w, f.receivers.size());
		w.finish();
		return Response::json(out);
	}

	Response System::outputs()
	{
		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		if (engine)
			engine(w, ENGINE_OUTPUT_LIST);
		else
			w.key("data").beginArray().endArray().key("meta").beginObject().kv("count", 0).kv("complete", true).kv_null("next").endObject();
		w.endObject();
		w.finish();
		return Response::json(out);
	}

	// pass, warn or fail, as health checks say it (draft-inadarei-api-health-check):
	// fail, and 503, when the engine is not running or a receiver lost its
	// device; warn when a station has been quiet for a quarter of an hour
	Response System::health()
	{
		const SystemFacts f = facts();
		const std::time_t now = std::time(nullptr);
		int worst = f.running ? 0 : 2;
		std::string out;
		JSON::Writer w(out);
		w.beginObject().key("checks").beginObject();
		w.key("engine").beginArray().beginObject().kv("status", f.running ? "pass" : "fail").endObject().endArray();
		for (const auto &r : f.receivers)
		{
			const std::time_t heard = tracker.lastHeard(r.station);
			const bool quiet = f.running && age(now, f.session) > QUIET_S && age(now, heard ? heard : f.session) > QUIET_S;
			const int level = r.lost || !r.streaming ? 2 : quiet ? 1 : 0;
			worst = std::max(worst, level);
			w.key("receiver:" + std::to_string(r.index)).beginArray().beginObject()
				.kv("status", level == 2 ? "fail" : level == 1 ? "warn" : "pass").kv("componentId", std::to_string(r.station));
			if (level == 2)
				w.kv("output", r.lost ? "the device stopped" : "not streaming");
			else if (level == 1)
				w.kv("output", "nothing heard for " + std::to_string(age(now, heard ? heard : f.session) / 60) + " minutes");
			w.endObject().endArray();
		}
		w.endObject().kv("status", worst == 2 ? "fail" : worst == 1 ? "warn" : "pass").endObject();
		w.finish();
		Response r = Response::json(out, worst == 2 ? 503 : 200);
		r.type = "application/health+json";
		return r;
	}

	// The log as the logger keeps it, oldest first; without a cursor the newest
	// page. A line's number holds as long as the process: the cursor carries
	// when the process began.
	Response System::log(const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		Logger &logger = Logger::getInstance();
		const long long epoch = (long long)logger.epoch();
		std::vector<LogMessage> lines = logger.getLastMessages(INT_MAX);
		std::size_t first = lines.size() > (std::size_t)limit ? lines.size() - (std::size_t)limit : 0;
		const std::string from = request.param("cursor");
		std::vector<long long> c;
		if (!from.empty())
		{
			if (!parseCursor(from, "log", 2, c) || c[1] > (long long)UINT32_MAX)
				return badCursor();
			const uint32_t after = (uint32_t)c[1];
			if (c[0] != epoch || (!lines.empty() && after > lines.back().seq))
				return gone("The log started again since this cursor was given.");
			if (!lines.empty() && after + 1 < lines.front().seq)
				return gone("The lines after this cursor are no longer kept.");
			first = 0;
			while (first < lines.size() && lines[first].seq <= after)
				first++;
		}
		const std::size_t end = std::min(lines.size(), first + (std::size_t)limit);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (std::size_t i = first; i < end; i++)
			w.beginObject().kv("id", cursor("log", {epoch, (long long)lines[i].seq})).kv("time", isoTime(lines[i].utc))
				.kv("level", lines[i].levelToString()).kv("message", lines[i].message).endObject();
		const std::string last = end > first ? cursor("log", {epoch, (long long)lines[end - 1].seq})
											 : !from.empty() ? from
											 : cursor("log", {epoch, lines.empty() ? 0LL : (long long)lines.back().seq});
		const std::string tail = "&limit=" + std::to_string(limit);
		beginMeta(w, end - first, end < lines.size() ? "/api/v1/system/log?cursor=" + last + tail : std::string());
		w.kv("poll", "/api/v1/system/log?cursor=" + last + tail).endObject().endObject();
		w.finish();
		return Response::json(out);
	}

	// The installation's own events from the journal, under the ids every
	// reader of the journal gives them; without a cursor the newest page.
	Response System::events(const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		Journal &journal = tracker.journal();
		const uint64_t epoch = journal.epoch();
		uint64_t after = journal.beforeOldest();
		const std::string from = request.param("cursor");
		std::vector<long long> c;
		if (!from.empty())
		{
			if (!parseCursor(from, "evt", 2, c))
				return badCursor();
			if ((uint64_t)c[0] != epoch || (uint64_t)c[1] > journal.newest())
				return gone("The journal started again since this cursor was given.");
			after = (uint64_t)c[1];
		}
		std::vector<Journal::Entry> entries;
		if (!journal.since(after, std::numeric_limits<std::size_t>::max(), entries))
			return gone("The events after this cursor are no longer kept.");
		std::vector<const Journal::Entry *> system;
		for (const auto &e : entries)
			if (e.event.kind == Tracking::Kind::SYSTEM)
				system.push_back(&e);
		// without a cursor the newest page, with one the oldest after it
		const std::size_t first = from.empty() && system.size() > (std::size_t)limit ? system.size() - (std::size_t)limit : 0;
		const std::size_t end = std::min(system.size(), first + (std::size_t)limit);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (std::size_t i = first; i < end; i++)
			writeEvent(w, *system[i], epoch);
		// past the last given, or past every event read when that was all
		const uint64_t last = end < system.size() ? system[end - 1]->seq : entries.empty() ? after : entries.back().seq;
		const std::string tail = "&limit=" + std::to_string(limit);
		const std::string poll = "/api/v1/system/events?cursor=" + eventId(epoch, last) + tail;
		beginMeta(w, end - first, end < system.size() ? poll : std::string());
		w.kv("poll", poll).endObject().endObject();
		w.finish();
		return Response::json(out);
	}
}
