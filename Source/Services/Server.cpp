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


#include "Server.h"
#include <algorithm>
#include <ctime>
#include "ApiV1.h"
#include "Convert.h"
#include "SHA256.h"
#include "Logger.h"
#include "Mcp.h"
#include "Prometheus.h"
#ifdef HASWEBVIEWER
#include "Streams.h"
#include "Ui.h"
#endif

namespace Services
{
	namespace
	{
		// The ships as GeoJSON (/geojson, their tracks at /allpath.geojson) and the
		// message counts for Prometheus (/metrics), as other tools read them, through
		// the server's filter
		class Feeds : public Service
		{
		public:
			Feeds(Tracker &tracker, const ServerSpec &spec, PrometheusCounter *metrics, bool geojson)
				: tracker(tracker), filter(spec.filter), metrics(metrics), geojson(geojson) {}

			int scope(const Request &request) const override
			{
				return request.path == "/metrics" || request.path == "/geojson" || request.path == "/allpath.geojson" ? (int)ApiKey::DATA : NOT_MINE;
			}

			bool handle(const Request &request, Response &response) override
			{
				if (request.method != "GET")
					return false;
				if (metrics && request.path == "/metrics")
				{
					response.type = "text/plain";
					response.body = metrics->toPrometheus();
					return true;
				}
				if (!geojson || (request.path != "/geojson" && request.path != "/allpath.geojson"))
					return false;
				DB::Keep keep;
				if (filter.any())
				{
					const ViewFilter within = filter;
					keep = [within](const Ship &s) { return within.passes(s, std::time(nullptr)); };
				}
				response.body = request.path == "/geojson" ? tracker.ships().getGeoJSON(keep) : tracker.ships().getAllPathGeoJSON(keep);
				return true;
			}

		private:
			Tracker &tracker;
			ViewFilter filter;
			PrometheusCounter *metrics;
			bool geojson;
		};
	}

	Server::Server(const ServerSpec &spec, Tracker &tracker, std::function<StationRegistry()> stations,
				   EngineWriter engine, std::function<SystemFacts()> system)
		: spec_(spec), tracker(tracker), stations(std::move(stations)), engine(std::move(engine)), system(std::move(system))
	{
		build();
	}

	Server::~Server()
	{
		{
			std::lock_guard<std::mutex> lock(services_mtx);
			drop();
		}
		stopThread();
	}

	void Server::reconfigure(const ServerSpec &spec)
	{
		std::lock_guard<std::mutex> lock(services_mtx);
		drop();
		spec_ = spec;
		build();
	}

	// under services_mtx, or before the server runs
	void Server::build()
	{
		// the pages that may frame this server's: any, unless the viewer's settings say
		setFrameAncestors(spec_.viewer.frame_ancestors.empty() ? "*" : spec_.viewer.frame_ancestors);
		PrometheusCounter *counter = nullptr;
		if (spec_.metrics)
		{
			counter = new PrometheusCounter();
			metrics.reset(counter);
			tracker.addTap(counter);
		}
		if (spec_.geojson || spec_.metrics)
			services.push_back(std::unique_ptr<Service>(new Feeds(tracker, spec_, counter, spec_.geojson)));
		if (spec_.api)
			services.push_back(std::unique_ptr<Service>(new ApiV1(tracker, stations, spec_)));
		if (spec_.mcp)
		{
			// the installation's tools when the server offers the system
			std::function<SystemFacts()> facts;
			if (spec_.system)
				facts = system ? system : [] { return SystemFacts(); };
			services.push_back(std::unique_ptr<Service>(new Mcp(tracker, stations, spec_, facts, engine)));
		}
		if (spec_.system)
			services.push_back(std::unique_ptr<Service>(new System(tracker, system ? system : [] { return SystemFacts(); }, engine)));
#ifdef HASWEBVIEWER
		if (spec_.ui)
		{
			// the page shows the GeoJSON link when the server offers it
			ServerSpec page = spec_;
			page.viewer.geojson = spec_.geojson;
			page.viewer.prome = spec_.metrics;
			services.push_back(std::unique_ptr<Service>(new Ui(tracker, stations, page, engine, [this] { return numberOfClients(); })));
			if (spec_.viewer.realtime)
			{
				messages.reset(new MessageStream(*this, !spec_.viewer.decoder));
				tracker.addTap(messages.get());
			}
			if (spec_.viewer.log)
				log = std::make_shared<LogStream>(*this);
		}
#endif
	}

	// the streams stop before what they write to goes
	void Server::drop()
	{
		if (messages)
			tracker.removeTap(messages.get());
		if (metrics)
			tracker.removeTap(metrics.get());
		log.reset();
		services.clear();
		messages.reset();
		metrics.reset();
	}

	bool Server::open()
	{
		setIP(spec_.bind);
		if (!start(spec_.port))
		{
			Error() << "Server: cannot listen on " << spec_.bind << ":" << spec_.port;
			return false;
		}
		std::string list;
		for (const auto &s : spec_.served())
			list += (list.empty() ? "" : ", ") + s;
		Info() << "Server: " << spec_.bind << ":" << spec_.port << " offers " << list;
		return true;
	}

	// The key a request names in its Authorization header, found by its hash
	// and compared in constant time; null when it names none of this server's.
	const ApiKey *Server::keyOf(const std::string &authorization) const
	{
		static const std::string bearer = "bearer ";
		if (authorization.size() <= bearer.size())
			return nullptr;
		std::string scheme = authorization.substr(0, bearer.size());
		Util::Convert::toLower(scheme);
		if (scheme != bearer)
			return nullptr;
		const std::string hash = Util::SHA256::hex(authorization.substr(bearer.size()));
		const ApiKey *found = nullptr;
		for (const auto &k : spec_.keys)
		{
			unsigned char diff = hash.size() != k.sha256.size();
			for (std::size_t i = 0; i < hash.size() && i < k.sha256.size(); i++)
				diff |= (unsigned char)(hash[i] ^ k.sha256[i]);
			if (!diff)
				found = &k;
		}
		return found;
	}

	// The strikes of an address in the current minute; null when it has none
	// and `add` is false. A new address takes the place of the least recent.
	Server::Strikes *Server::strikesOf(const std::string &peer, std::time_t now, bool add)
	{
		Strikes *found = nullptr;
		for (auto &s : strikes)
			if (s.peer == peer)
				found = &s;
		if (found && now - found->since >= STRIKE_S)
			*found = Strikes(peer, now);
		if (found || !add)
			return found;
		if (strikes.size() < PEERS)
		{
			strikes.push_back(Strikes(peer, now));
			return &strikes.back();
		}
		Strikes *oldest = &strikes[0];
		for (auto &s : strikes)
			if (s.last < oldest->last)
				oldest = &s;
		*oldest = Strikes(peer, now);
		return oldest;
	}

	// Whether a request to a service of `scope` may go on with `key`, the key
	// it names; when not, the answer that says why. A server without keys is
	// this machine's own or declared public; one with keys lets its data out to
	// anyone only when public, and nothing else without a key that has the
	// scope. Many wrong keys in a minute from one address close the server to
	// that address's keys for the rest of the minute.
	bool Server::admit(int scope, const Services::Request &request, const ApiKey *key, Services::Response &response)
	{
		if (!scope || spec_.keys.empty() || (scope == ApiKey::DATA && spec_.open))
			return true;
		const std::time_t now = std::time(nullptr);
		Strikes *s = strikesOf(request.peer, now, false);
		if (s && s->count >= STRIKES)
		{
			response = Services::Response::problem(429, "Too many requests", "Too many wrong keys; try again in a minute.");
			response.header = "Retry-After: " + std::to_string((int)STRIKE_S);
			return false;
		}
		if (!key)
		{
			s = strikesOf(request.peer, now, true);
			s->count++;
			s->last = now;
			response = Services::Response::problem(401, "Unauthorized", "This needs a key: Authorization: Bearer <key>.");
			response.header = "WWW-Authenticate: Bearer realm=\"AIS-catcher\"";
			return false;
		}
		if (!(key->scopes & scope))
		{
			response = Services::Response::problem(403, "Forbidden", "The key \"" + key->name + "\" may not " +
				(scope == ApiKey::SYSTEM ? "read the system." : "read data."));
			return false;
		}
		return true;
	}

	void Server::Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip)
	{
		if (extra && extra(*this, c, r, accept_gzip))
			return;
		Services::Request request;
		request.method = r.method;
		request.path = r.path();
		request.query = r.query();
		request.body = r.body;
		request.authorization = r.authorization;
		request.origin = r.origin;
		request.host = r.host;
		request.peer = r.peer;

		std::lock_guard<std::mutex> lock(services_mtx);
		Services::Response response;
		bool handled = false;
		try
		{
			// what the caller may read, for a service that serves both: everything from a
			// server without keys, its key's scopes on one with keys, the data alone when public
			const ApiKey *key = spec_.keys.empty() ? nullptr : keyOf(request.authorization);
			request.scopes = spec_.keys.empty() ? ApiKey::DATA | ApiKey::SYSTEM : key ? key->scopes : spec_.open ? (int)ApiKey::DATA : 0;
			// the first service that takes the path and answers it, each asking of the key what it serves
			for (auto &s : services)
			{
				const int scope = s->scope(request);
				if (scope == Service::NOT_MINE)
					continue;
				if (!admit(scope, request, key, response) || s->handle(request, response))
				{
					handled = true;
					break;
				}
			}
		}
		catch (const std::exception &e)
		{
			response = Services::Response::problem(500, "Internal error", e.what());
			handled = true;
		}
		if (!handled)
			response = Services::Response::problem(404, "Not found", "No such resource: " + request.path);
		if (!response.header.empty())
			setExtraHeader(response.header);
		if (response.stream)
			upgradeSSE(c, response.stream, response.stream_topic, response.stream_backlog);
		else if (response.raw)
			ResponseRaw(c, response.type, response.raw, (int)response.raw_size, response.gzipped, response.cache, false, response.status);
		else
			Response(c, response.type, response.body, accept_gzip, response.cache, false, response.status);
	}
}
