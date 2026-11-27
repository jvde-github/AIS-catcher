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

#include <cmath>

#include "System.h"
#include <algorithm>
#include <climits>
#include <limits>
#include "JournalJSON.h"
#include "Routes.h"
#include "SiteConfig.h"
#include "Helper.h"
#include "Logger.h"
#include "Writer.h"
#include "AIS-catcher.h"

namespace Services
{
	namespace
	{
		long long age(std::time_t now, std::time_t then) { return then && now > then ? (long long)(now - then) : 0; }
	}

	int System::scope(const Request &request) const
	{
		const std::string base = "/api/v1/system";
		const std::string &p = request.path;
		const bool mine = p.compare(0, base.size(), base) == 0 && (p.size() == base.size() || p[base.size()] == '/');
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
		const std::string rest = request.path.substr(base.size());
		if (rest.empty() || rest == "/")
			response = overview();
		else if (rest == "/receivers")
			response = receivers();
		else if (rest == "/outputs")
			response = outputs();
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
		const Util::Helper::SystemInfo sys = Util::Helper::getSystemInfo();
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		w.beginObject().kv("name", "AIS-catcher").kv("version", VERSION).kv("describe", VERSION_DESCRIBE).kv("build_date", __DATE__)
			.kv("os", sys.os).kv("hardware", sys.hardware).kv("processor", sys.processor)
			.kv("uptime_s", age(now, f.born)).kv("memory_bytes", (long long)sys.memory).kv("cpu_percent", std::round(sys.cpu * 10) / 10.0)
			.kv("received_bytes", (unsigned long long)f.received);
		if (sys.wifi)
			w.key("wifi").beginObject().kv("signal_dbm", sys.wifi_dbm).kv("quality", sys.wifi_quality).endObject();
		w.key("engine").beginObject().kv("running", f.running);
		if (f.running)
			w.kv("uptime_s", age(now, f.session));
		w.kv("receivers", (long long)f.receivers.size()).kv("overruns", (long long)f.overruns).endObject()
			.key("resources").beginArray();
		for (const char *r : {"/api/v1/system/receivers", "/api/v1/system/outputs", "/api/v1/system/log", "/api/v1/system/events"})
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
			const std::time_t heard = r.last_message;
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
		const SystemFacts f = facts();
		w.beginObject().key("data").beginArray();
		writeOutputs(w, f);
		w.endArray().key("meta").beginObject().kv("count", (long long)f.outputs.size()).kv("complete", true).kv_null("next").endObject();
		w.endObject();
		w.finish();
		return Response::json(out);
	}

	HealthAsk healthAsk(const Request &request)
	{
		HealthAsk ask;
		for (const auto &p : request.params())
		{
			if (p.first == "strict")
				ask.strict = p.second != "0" && p.second != "false";
			else if (p.first == "view")
				ask.brief = p.second == "brief";
			else if (p.first == "station" && !p.second.empty() && p.second.find_first_not_of("0123456789") == std::string::npos && p.second.size() < 10)
				ask.station = std::atoi(p.second.c_str());
		}
		return ask;
	}

	Response healthResponse(Tracker &tracker, const ServerSpec &spec, const std::function<StationRegistry()> &stations, const std::function<SystemFacts()> &facts, const HealthAsk &ask)
	{
		const auto shown = [&](int id) { return spec.showsStation(id) && (ask.station < 0 || id == ask.station); };
		const Health h = tracker.healthNow();
		const std::time_t now = std::time(nullptr);
		// before the first verdict nothing is known to be wrong
		const bool running = h.running || !h.at;
		Health::Status ais = running ? Health::OK : Health::DOWN, gps = Health::OK;
		bool has_gps = false, station_down = false;
		std::size_t receivers = 0, down = 0;
		for (const auto &s : h.stations)
		{
			if (!shown(s.id))
				continue;
			ais = std::max(ais, s.ais);
			station_down = station_down || s.ais == Health::DOWN;
			std::size_t out;
			receivers += s.listening(out);
			down += out;
			if (s.has_gps)
			{
				has_gps = true;
				gps = std::max(gps, s.gps.status);
			}
		}
		std::string out;
		JSON::Writer w(out);
		const auto ageOf = [&](const char *key, std::time_t then) { then ? (void)w.kv(key, age(now, then)) : (void)w.kv_null(key); };
		w.beginObject().kv("version", 1).kv("time", isoTime(now)).kv("uptime_s", age(now, h.born)).kv("ais", Health::name(ais));
		has_gps ? (void)w.kv("gps", Health::name(gps)) : (void)w.kv_null("gps");
		// a brief answer names no station and lists no output: it needs neither
		const StationRegistry registry = ask.brief ? StationRegistry() : stations();
		const SystemFacts live = ask.brief ? SystemFacts() : facts();
		if (!ask.brief)
			w.key("stations").beginObject();
		for (const auto &s : h.stations)
		{
			if (ask.brief || !shown(s.id))
				continue;
			const StationRegistry::Station *known = registry.find(s.id);
			w.key(std::to_string(s.id)).beginObject();
			if (known && !known->name.empty())
				w.kv("name", known->name);
			w.key("ais").beginObject().kv("status", Health::name(s.ais));
			ageOf("last_msg_age_s", s.heard);
			w.kv("msgs_1m", s.msgs).kv("ch_a_1m", s.a).kv("ch_b_1m", s.b);
			const Health::Station::Checks &c = s.checks;
			w.key("checks").beginObject().kv("input", c.input).kv("reception", c.reception).kv("rate", c.rate).kv("channels", c.channels).kv("gain", c.gain).kv("frequency", c.frequency).endObject();
			if (s.rf)
			{
				w.key("rf").beginObject();
				if (!s.noise_unit.empty() || s.noise != 0)
					w.kv("noise", s.noise).kv("noise_unit", s.noise_unit).kv_or_null("noise_delta_db", s.noise_delta, s.has_delta);
				if (s.has_clip)
					w.kv("clip_pct", s.clip);
				if (s.has_ppm)
					w.kv("freq_offset_ppm", s.ppm);
				w.endObject();
			}
			else
				w.kv_null("rf");
			w.key("receivers").beginObject();
			for (const auto &r : s.receivers)
			{
				w.key(std::to_string(r.index)).beginObject().kv("label", r.label).kv("status", Health::name(r.status)).kv("input", r.input);
				std::time_t heard = r.heard;
				unsigned long long bytes = r.bytes;
				for (const auto &now_r : live.receivers)
					if (now_r.index == r.index)
					{
						heard = now_r.last_message;
						bytes = now_r.bytes;
					}
				ageOf("last_msg_age_s", heard);
				w.kv("msgs_1m", r.per_minute).kv("bytes", bytes).kv("carries", r.carries()).endObject();
			}
			w.endObject().endObject();
			if (s.has_gps)
			{
				const Health::GPS &p = s.gps;
				w.key("gps").beginObject().kv("status", Health::name(p.status)).kv("fix", p.fix == 3 ? "3d" : p.fix == 2 ? "2d" : p.fix == 1 ? "none" : "unknown");
				ageOf("last_fix_age_s", p.fixed);
				ageOf("last_data_age_s", p.heard);
				w.kv("sats", p.sats).kv_or_null("hdop", std::round(p.hdop * 10) / 10, p.hdop > 0).kv("jamming", p.jamming).kv("spoofing", p.spoofing);
				w.key("checks").beginObject().kv("data", p.checks.data).kv("fix", p.checks.fix).kv("jamming", p.checks.jamming).kv("position", p.checks.position).endObject();
				w.kv_or_null("pos_ok", p.pos_ok == 1, p.pos_ok >= 0).kv("pos_ref", p.pos_ref).endObject();
			}
			else
				w.kv_null("gps");
			w.endObject();
		}
		if (!ask.brief)
			w.endObject();
		// The outputs are the installation's, not a station's: what each is called, never where it sends.
		// Its servers follow them; a server tells of the others, and of their trouble, only where it offers
		// the system service.
		const auto told = [&](const Health::Output &o) { return o.kind != Health::Output::SERVICE || o.port == spec.port || spec.system; };
		bool servers = false, shut = false;
		for (const auto &o : h.outputs)
			if (o.kind == Health::Output::SERVICE && told(o))
			{
				servers = true;
				shut = shut || !o.up;
			}
		const Health::Status outputs = shut ? std::max(h.out, Health::DEGRADED) : h.out;
		if (std::none_of(h.outputs.begin(), h.outputs.end(), told))
			w.kv_null("outputs");
		else if (ask.brief)
			w.key("outputs").beginObject().kv("status", Health::name(outputs)).endObject();
		else
		{
			const Health::OutputChecks &c = h.out_checks;
			w.key("outputs").beginObject().kv("status", Health::name(outputs));
			w.key("checks").beginObject().kv("connected", c.connected).kv("sending", c.sending).kv("dropped", c.dropped).kv("reconnects", c.reconnects).kv("feed", c.feed);
			w.kv("services", !servers ? "unknown" : shut ? "degraded" : "ok").endObject();
			w.key("items").beginObject();
			for (const auto &o : h.outputs)
			{
				if (!told(o))
					continue;
				w.key(std::to_string(o.index)).beginObject().kv("kind", o.kindName()).kv("type", o.type).kv("label", o.label).kv("status", Health::name(o.status));
				w.kv("state", o.state()).kv("bytes_out", o.bytes_out).kv("dropped", o.dropped);
				if (o.kind != Health::Output::SERVICE)
					w.kv("messages", o.messages);
				if (o.kind == Health::Output::LISTENER || o.kind == Health::Output::SERVICE)
					w.kv("clients", o.clients);
				if (o.kind == Health::Output::CONNECTION)
					w.kv("reconnects", o.reconnects);
				if (o.kind == Health::Output::SERVICE)
				{
					w.key("offers").beginArray();
					for (const auto &what : o.offers)
						w.val(what);
					w.endArray();
				}
				w.endObject();
			}
			w.endObject().endObject();
		}
		w.key("issues").beginArray();
		for (const auto &i : h.issues)
		{
			if (!i.active || (i.station >= 0 && !shown(i.station)))
				continue;
			// what is wrong with a server this one does not tell of is not told either
			if (i.output >= 0 && std::none_of(h.outputs.begin(), h.outputs.end(), [&](const Health::Output &o) { return o.index == i.output && told(o); }))
				continue;
			w.beginObject().kv("level", Health::name(i.level)).kv("type", i.type());
			if (i.station >= 0)
				w.kv("station", std::to_string(i.station));
			if (i.receiver >= 0)
				w.kv("receiver", std::to_string(i.receiver));
			if (i.output >= 0)
				w.kv("output", std::to_string(i.output));
			w.kv("code", i.code).kv("since", isoTime(i.since));
			if (i.valued)
				w.kv("value", i.value).kv("unit", i.unit).kv("method", i.method);
			w.endObject();
		}
		w.endArray().endObject();
		w.finish();
		const bool nothing = !running || (receivers && down == receivers);
		return Response::json(out, nothing || (ask.strict && station_down) ? 503 : 200);
	}

	// The log as the logger keeps it, oldest first; without a cursor the newest
	// page. A line's number holds as long as the process: the cursor carries
	// when the process began.
	Response System::log(const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
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
			return badLimit();
		return pageEvents(request, tracker.journal(), "/api/v1/system/events",
						  [](const Journal::Entry &e) { return e.event.kind == Tracking::Kind::SYSTEM; }, limit);
	}
}
