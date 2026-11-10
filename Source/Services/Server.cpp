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
#include "GeoJSON.h"
#include "Kml.h"
#include "Prometheus.h"
#ifdef HASWEBVIEWER
#include "Streams.h"
#include "Ui.h"
#endif

namespace Services
{
	Server::Server(const ServerSpec &spec, Tracker &tracker, std::function<StationRegistry(const ServerSpec &)> stations,
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
		// another certificate, or HTTPS on or off: what is connected spoke the old way
		if (spec.tls_cert != spec_.tls_cert || spec.tls_key != spec_.tls_key)
		{
			std::string error;
			if (setTLS(spec.tls_cert, spec.tls_key, error))
				closeConnections();
			else
				Error() << "Server: port " << spec.port << " keeps how it serves: " << error;
		}
		spec_ = spec;
		allowances.clear();
		build();
	}

	void Server::setKeys(const std::vector<ApiKey> &keys)
	{
		std::lock_guard<std::mutex> lock(services_mtx);
		external_keys = true;
		indexKeys(keys);
	}

	void Server::setKey(const ApiKey &key, bool gone)
	{
		std::lock_guard<std::mutex> lock(services_mtx);
		external_keys = true;
		if (gone)
			keys_.erase(key.sha256);
		else
			keys_[key.sha256] = key;
	}

	void Server::indexKeys(const std::vector<ApiKey> &keys)
	{
		keys_.clear();
		for (const auto &k : keys)
			keys_[k.sha256] = k;
	}

	void Server::setShips(ShipSource *source)
	{
		std::lock_guard<std::mutex> lock(services_mtx);
		ships_ = source;
		drop();
		build();
	}

	// under services_mtx, or before the server runs
	void Server::build()
	{
		setFrameAncestors(spec_.viewer.frame_ancestors.empty() ? "*" : spec_.viewer.frame_ancestors);
		if (!external_keys)
			indexKeys(spec_.keys);
		const Context context = {tracker, spec_, [this] { return stations(spec_); }, engine, system ? system : [] { return SystemFacts(); }, ships_};
		if (spec_.metrics)
		{
			PrometheusCounter *counter = new PrometheusCounter();
			metrics.reset(counter);
			tap(counter);
			services.push_back(std::unique_ptr<Service>(new Metrics(*counter, context)));
		}
		if (spec_.geojson)
			services.push_back(std::unique_ptr<Service>(new GeoJSON(context)));
		if (spec_.kml)
			services.push_back(std::unique_ptr<Service>(new Kml(context)));
		if (spec_.api)
			services.push_back(std::unique_ptr<Service>(new ApiV1(context)));
		if (spec_.mcp)
			services.push_back(std::unique_ptr<Service>(new Mcp(context)));
		if (spec_.system)
			services.push_back(std::unique_ptr<Service>(new System(context)));
#ifdef HASWEBVIEWER
		if (spec_.ui)
		{
			services.push_back(std::unique_ptr<Service>(new Ui(context, [this] { return numberOfClients(); })));
			if (spec_.viewer.realtime)
			{
				messages.reset(new MessageStream(*this, !spec_.viewer.decoder));
				tap(messages.get());
			}
			if (spec_.viewer.log)
				log = std::make_shared<LogStream>(*this);
		}
#endif
	}

	void Server::tap(StreamIn<JSON::JSON> *reader)
	{
		tapped.push_back(reader);
		tracker.addTap(reader);
	}

	// undoes build(): the listeners come off the tracker first, so the decoding
	// thread no longer calls them, then the services and streams are destroyed
	void Server::drop()
	{
		for (auto *t : tapped)
			tracker.removeTap(t);
		tapped.clear();
		log.reset();
		services.clear();
		messages.reset();
		metrics.reset();
	}

	bool Server::open()
	{
		setIP(spec_.bind);
		std::string error;
		if (!setTLS(spec_.tls_cert, spec_.tls_key, error))
		{
			Error() << "Server: port " << spec_.port << ": " << error;
			return false;
		}
		if (!start(spec_.port))
		{
			Error() << "Server: cannot listen on " << spec_.bind << ":" << spec_.port;
			return false;
		}
		std::string list;
		for (const auto &s : spec_.served())
			list += (list.empty() ? "" : ", ") + s;
		Info() << "Server: " << spec_.bind << ":" << spec_.port << " offers " << list << (hasTLS() ? ", over HTTPS" : "");
		return true;
	}

	// The key a request names in its Authorization header, found by its hash; null when it
	// names none of this server's. The hashes are compared, never the keys, so a lookup's
	// timing says nothing of a key.
	const ApiKey *Server::keyOf(const std::string &authorization) const
	{
		static const std::string bearer = "bearer ";
		if (authorization.size() <= bearer.size())
			return nullptr;
		std::string scheme = authorization.substr(0, bearer.size());
		Util::Convert::toLower(scheme);
		if (scheme != bearer)
			return nullptr;
		const auto it = keys_.find(Util::SHA256::hex(authorization.substr(bearer.size())));
		return it == keys_.end() ? nullptr : &it->second;
	}

	// Whether a request to a service of `scope` may go on with `key`, the key
	// it names; when not, the answer that says why. A server without keys is
	// this machine's own or declared public, and admits everything; one with
	// keys admits nothing guarded without a key that has the scope.
	bool Server::admit(int scope, const Services::Request &request, const ApiKey *key, Services::Response &response) const
	{
		if (!scope || !keyed())
			return true;
		if (!key)
		{
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

	// a key with a quota is counted by itself; a request without a key, on a limited server, by its address
	int Server::quota(const ApiKey *key) const
	{
		return key ? key->calls_hour : spec_.limited ? IO::Limits::PUBLIC_CALLS_AN_HOUR : 0;
	}

	bool Server::allow(const Service &service, Services::Request &request, const ApiKey *key, Services::Response &response, Allowance *&hour)
	{
		hour = nullptr;
		const int limit = quota(key);
		if (!limit || !service.metered(request))
			return true;
		const std::string who = key ? "key " + (key->name.empty() ? key->sha256 : key->name) : request.peer;
		const std::time_t now = std::time(nullptr);
		if (allowances.size() > 10000)
		{
			for (auto it = allowances.begin(); it != allowances.end();)
			{
				if (now - it->second.window >= 3600)
					it = allowances.erase(it);
				else
					++it;
			}
		}
		Allowance &a = allowances[who];
		if (now - a.window >= 3600)
		{
			a.window = now;
			a.used = a.refused = 0;
		}
		if (a.used < limit)
		{
			request.calls_left = limit - a.used;
			hour = &a;
			return true;
		}
		// a key is answered every time, it is somebody's; an address over its limit gets one 429 and
		// then nothing, a flood is not worth answers
		const int wait = (int)(3600 - (now - a.window));
		if (++a.refused == 1 || key)
		{
			if (a.refused == 1)
				Warning() << "Server: port " << spec_.port << ": " << who << " made its " << limit << " data calls this hour: a 429" << (key ? "" : ", then nothing") << " for " << wait << " s";
			response = Services::Response::problem(429, "Too Many Requests", std::string(key ? "This key" : "This address") + " has made its " + std::to_string(limit) + " data calls this hour: try again in " + std::to_string(wait) + " s." + (key ? "" : " Until then, further calls are not answered."));
			response.header = "Retry-After: " + std::to_string(wait);
			return false;
		}
		response.after = Services::Response::DROP;
		return false;
	}

	static Services::Request fromHTTP(const IO::HTTPRequest &r)
	{
		Services::Request request;
		request.method = r.method;
		request.path = r.path();
		request.query = r.query();
		request.body = r.body;
		request.authorization = r.authorization;
		request.origin = r.origin;
		request.host = r.host;
		request.peer = r.peer;
		request.local = r.local;
		request.owner = r.owner;
		return request;
	}

	// what the path takes, and the preflight of a page elsewhere; no key is asked, a browser sends none
	void Server::Options(IO::TCPServerConnection &c, const IO::HTTPRequest &r)
	{
		const Services::Request request = fromHTTP(r);
		std::lock_guard<std::mutex> lock(services_mtx);
		for (auto &s : services)
		{
			if (s->scope(request) == Service::NOT_MINE)
				continue;
			const std::string methods = s->methods(request);
			std::string header = "Allow: " + methods + ", OPTIONS";
			if (!localBind())
				header += "\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: " + methods +
						  "\r\nAccess-Control-Allow-Headers: Authorization, Content-Type\r\nAccess-Control-Max-Age: 86400";
			Response(c, "text/plain", std::string(), IO::Reply(204).with(header));
			return;
		}
		Response(c, "text/plain", std::string("Not found."), IO::Reply(404));
	}

	void Server::Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip)
	{
		if (extra && extra(*this, c, r, accept_gzip))
			return;
		Services::Request request = fromHTTP(r);

		std::lock_guard<std::mutex> lock(services_mtx);
		Services::Response response;
		std::string rate;
		bool handled = false;
		try
		{
			const ApiKey *key = keyed() ? keyOf(request.authorization) : nullptr;
			request.scopes = !keyed() ? ApiKey::DATA | ApiKey::SYSTEM : key ? key->scopes : 0;
			request.owner = request.owner || (key && (key->scopes & ApiKey::SYSTEM));
			request.station = key ? key->station : -1;
			for (auto &s : services)
			{
				const int scope = s->scope(request);
				if (scope == Service::NOT_MINE)
					continue;
				Allowance *hour = nullptr;
				if (!admit(scope, request, key, response) || !allow(*s, request, key, response, hour) || s->handle(request, response))
				{
					// charged when the answer is known: a large page more than one call, a refusal one
					if (hour)
					{
						hour->used += response.status < 400 ? s->cost(request) : 1;
						rate = "X-RateLimit-Limit: " + std::to_string(quota(key)) + "\r\nX-RateLimit-Remaining: " + std::to_string(quota(key) - hour->used);
					}
					handled = true;
					break;
				}
			}
		}
		catch (const std::exception &e)
		{
			Error() << "Server: " << request.path << ": " << e.what();
			response = Services::Response::problem(500, "Internal error", "The request could not be answered.");
			handled = true;
		}
		if (!handled)
			response = Services::Response::problem(404, "Not found", "No such resource: " + request.path);
		if (response.after != Services::Response::KEEP)
			c.close_after_send = true;
		if (response.after == Services::Response::DROP)
			return;
		if (!rate.empty())
			response.header += (response.header.empty() ? "" : "\r\n") + rate;
		IO::Reply reply(response.status);
		reply.header = response.header;
		reply.cache = response.cache;
		reply.cors = response.cors && !localBind();
		if (response.stream)
			upgradeSSE(c, response.stream, response.stream_topic, response.stream_backlog);
		else if (response.raw)
		{
			reply.gzip = response.gzipped;
			ResponseRaw(c, response.type, response.raw, (int)response.raw_size, reply);
		}
		else
		{
			reply.gzip = accept_gzip;
			Response(c, response.type, response.shared ? *response.shared : response.body, reply);
		}
	}
}
