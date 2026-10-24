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
#include <mutex>
#include <string>
#include <vector>

#include "HTTPServer.h"
#include "Http.h"
#include "SiteConfig.h"
#include "StationRegistry.h"
#include "System.h"
#include "Tracker.h"

namespace Services
{
	// One listener of the site: a port, an address and the services it
	// offers, all reading the site's tracker.
	class Server : public IO::HTTPServer
	{
	public:
		// `engine` writes the running engine's part of a status page
		Server(const ServerSpec &spec, Tracker &tracker, std::function<StationRegistry()> stations,
			   EngineWriter engine = nullptr, std::function<SystemFacts()> system = nullptr);
		~Server();

		bool open();
		const ServerSpec &spec() const { return spec_; }
		Tracker &reads() { return tracker; }
		// New settings on the same port and address: the services and streams
		// are built again, the listener, its connections and a mount stay
		void reconfigure(const ServerSpec &spec);

		void Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip) override;

		// A program built on AIS-catcher answers requests of its own here,
		// before the services: true when it did. Set before the server serves.
		typedef std::function<bool(IO::HTTPServer &, IO::TCPServerConnection &, const IO::HTTPRequest &, bool)> Extra;
		void setExtra(Extra e) { extra = std::move(e); }

	private:
		ServerSpec spec_;
		Tracker &tracker;
		std::function<StationRegistry()> stations;
		EngineWriter engine;
		std::function<SystemFacts()> system;
		Extra extra;
		// held through every request, answer included: an answer may point into a service
		std::mutex services_mtx;
		std::vector<std::unique_ptr<Service>> services;
		// the listeners on the tracker's messages: the viewer's live stream, the Prometheus counter
		std::unique_ptr<StreamIn<JSON::JSON>> messages, metrics;
		std::vector<StreamIn<JSON::JSON> *> tapped;
		void tap(StreamIn<JSON::JSON> *reader);
		std::shared_ptr<void> log;


		void build();
		void drop();
		const ApiKey *keyOf(const std::string &authorization) const;
		bool admit(int scope, const Services::Request &request, const ApiKey *key, Services::Response &response) const;
	};
}
