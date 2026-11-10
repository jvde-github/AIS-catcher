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

#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Located.h"
#include "StationRegistry.h"
#include "SystemFacts.h"
#include "Writer.h"

class Tracker;
struct ServerSpec;

namespace Services
{
	// `path` is below the server, with `query` split off
	struct Request
	{
		std::string method, path, query, body;
		// the Authorization, Origin and Host headers, and the client's numeric address
		std::string authorization, origin, host, peer;
		// what it may read, as the server admitted it: ApiKey's DATA and SYSTEM
		int scopes = 0;
		// the owner of this machine's stations, who may read the range figures of one that keeps its position home
		bool owner = false;
		// the station the key names, -1 for none: its holder sees where that station is
		int station = -1;
		// from this machine itself
		bool local = false;
		// the calls its quota has left this hour, 0 when nothing counts it: a page is cut to them
		int calls_left = 0;

		std::string param(const std::string &name) const;
		// every name and value of the query, decoded, in order
		std::vector<std::pair<std::string, std::string>> params() const;
		// a whole number from the query; `fallback` when absent, false when malformed
		bool integer(const std::string &name, long long &out, long long fallback, long long min, long long max) const;
	};

	// the server writes it out: handlers never touch a socket
	struct Response
	{
		int status = 200;
		std::string type = "application/json";
		std::string body;
		// a body kept elsewhere and shared, sent in place of body: only for what goes
		// straight to the client, as a caller reading body finds it empty
		std::shared_ptr<const std::string> shared;
		// one more header line, such as WWW-Authenticate
		std::string header;
		// the connection afterwards: kept, closed after this answer, or closed with no answer at all
		enum After
		{
			KEEP,
			CLOSE,
			DROP
		} after = KEEP;
		// a body that lives elsewhere, as stored: compressed or not, and whether it may be cached
		const char *raw = nullptr;
		std::size_t raw_size = 0;
		bool gzipped = false, cache = false;
		// readable by a page of any origin
		bool cors = false;
		// instead of a body: keep the connection as a stream of these events, the backlog first
		uint32_t stream = 0;
		std::string stream_topic;
		std::function<std::vector<std::string>()> stream_backlog;

		// RFC 9457 problem details, with a title that stays the same for a kind of problem
		static Response problem(int status, const std::string &title, const std::string &detail);
		static Response json(std::string body, int status = 200);
		static Response json(std::shared_ptr<const std::string> body, int status = 200);
	};

	// UTC, to the second: 2026-10-19T14:02:11Z
	std::string isoTime(std::time_t t);
	struct IsoTime
	{
		char text[24];
	};
	IsoTime isoTimeText(std::time_t t);
	// a time as a query gives it: that form, or seconds since 1970; false when neither
	bool parseTime(const std::string &s, std::time_t &out);

	// The envelope of every list: the rows under `data`, then under `meta` how
	// many, whether that is all, and the request for the next page, null on
	// the last. beginMeta leaves `meta` open for a list's own; endList closes it.
	void beginList(JSON::Writer &w);
	void beginMeta(JSON::Writer &w, std::size_t count, const std::string &next = std::string());
	void endList(JSON::Writer &w, std::size_t count, const std::string &next = std::string());

	// A cursor as the lists hand them out: a tag and whole numbers joined by
	// underscores, such as evt_1792762066_1532. Clients take it as it comes.
	std::string cursor(const std::string &tag, const std::vector<long long> &parts);
	// the numbers of a cursor with this tag and `n` of them; false for anything else
	bool parseCursor(const std::string &s, const std::string &tag, std::size_t n, std::vector<long long> &out);

	// The longest page of a list, and what a page that cannot be given answers:
	// a limit out of range, a cursor no page gave, one pointing at what is gone
	const long long LIST_MAX = 1000;
	Response badLimit(long long max = LIST_MAX);
	Response badCursor();
	Response gone(const std::string &why);

	void textOrNull(JSON::Writer &w, const char *key, const std::string &v);

	std::string urlEncode(const std::string &v);

	// what writes the running engine's part of a status page
	typedef std::function<void(JSON::Writer &, EnginePart)> EngineWriter;

	// which stations' locations a server shows: its share_loc, or each station's own where it does not say
	Located located(Tracker &tracker, const ServerSpec &spec, const StationRegistry &stations);

	struct ShipSource;

	// the server outlives its services and rebuilds them when its settings change
	struct Context
	{
		Tracker &tracker;
		const ServerSpec &spec;
		std::function<StationRegistry()> stations;
		EngineWriter engine;
		std::function<SystemFacts()> facts;
		// the API's walk over the ships, null for the ship table itself
		ShipSource *ships;
	};

	class Service
	{
	public:
		// what a request asks of a key: nothing, or ApiKey's DATA or SYSTEM
		enum { NOT_MINE = -1, OPEN = 0 };

		virtual ~Service() {}
		// what a key must allow for this service to answer the request, NOT_MINE when it
		// does not take the path; asked before handle
		virtual int scope(const Request &request) const = 0;
		// false leaves the request to the next service
		virtual bool handle(const Request &request, Response &response) = 0;
		// the methods the path takes, as an Allow header lists them
		virtual std::string methods(const Request &) const { return "GET, HEAD"; }
		virtual bool metered(const Request &) const { return false; }
		// what an answered metered request counts for against a quota: one call, or more for a large page
		virtual int cost(const Request &) const { return 1; }
	};
}
