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
#include <memory>
#include <string>
#include <vector>

#include "ApiV1.h"
#include "Http.h"
#include "System.h"

namespace Services
{
	// The Model Context Protocol on POST /mcp, over streamable HTTP without
	// sessions: an assistant lists the tools and calls them. Every tool reads,
	// none writes, and each is one route of /api/v1 with the same scope and
	// filter as the server's api. On a server that offers the system, the
	// installation's tools too, for a caller that may read the system.
	class Mcp : public Service
	{
	public:
		Mcp(Tracker &tracker, std::function<StationRegistry()> stations, const ServerSpec &spec,
			std::function<SystemFacts()> facts = nullptr, EngineWriter engine = nullptr)
			: api(tracker, std::move(stations), spec), keyed(!spec.keys.empty())
		{
			if (facts)
				system.reset(new System(tracker, std::move(facts), std::move(engine)));
		}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		ApiV1 api;
		std::unique_ptr<System> system;
		bool keyed;

		// an Origin a browser on another site would send: refused, so a page
		// cannot reach a local server through a name it rebinds
		bool trusted(const std::string &origin) const;
	};
}
