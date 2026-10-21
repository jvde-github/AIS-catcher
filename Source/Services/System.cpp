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
#include <limits>
#include "JournalJSON.h"
#include "Helper.h"
#include "Logger.h"
#include "Writer.h"
#include "AIS-catcher.h"

namespace Services
{
	namespace
	{
		const long long LIST_MAX = 1000;

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
			w.endObject();
		}
		endList(w, f.receivers.size(), true);
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
			w.key("data").beginArray().endArray().key("meta").beginObject().kv("count", 0).kv("complete", true).endObject();
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

	// the log as the logger keeps it: its last lines, from a sequence number on
	Response System::log(const Request &request)
	{
		long long after = 0, limit = 0;
		if (!request.integer("after", after, 0, 0, 4294967295LL))
			return Response::problem(400, "Bad request", "after must be a line number.");
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		uint32_t last = (uint32_t)after;
		const std::vector<std::string> lines = Logger::getInstance().getBacklogJSON((int)limit, (uint32_t)after, &last);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &l : lines)
			w.raw_val(l);
		// the next page starts after the last line given
		endList(w, lines.size(), (long long)lines.size() < limit, "/api/v1/system/log?after=" + std::to_string((unsigned long long)last));
		w.finish();
		return Response::json(out);
	}

	// the installation's own events from the journal, on the journal's numbers
	Response System::events(const Request &request)
	{
		long long after = 0, limit = 0;
		if (!request.integer("after", after, -1, 0, 9223372036854775807LL))
			return Response::problem(400, "Bad request", "after must be an event number.");
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		Journal &journal = tracker.journal();
		if (after < 0)
			after = 0;
		std::vector<Journal::Entry> entries;
		if (!journal.since((uint64_t)after, std::numeric_limits<std::size_t>::max(), entries))
			return Response::problem(410, "Gone", "Events after " + std::to_string(after) + " are no longer kept; start again without after.");
		std::string out;
		JSON::Writer w(out);
		w.beginObject().key("data").beginArray();
		std::size_t count = 0;
		uint64_t last = (uint64_t)after;
		for (const auto &e : entries)
		{
			if (count >= (std::size_t)limit)
				break;
			last = e.seq;
			if (e.event.kind != Tracking::Kind::SYSTEM)
				continue;
			writeEvent(w, e);
			count++;
		}
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", last >= journal.newest())
			.kv("next", "/api/v1/system/events?after=" + std::to_string(last)).kv("epoch", (long long)journal.epoch()).endObject().endObject();
		w.finish();
		return Response::json(out);
	}
}
