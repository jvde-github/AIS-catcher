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

#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "Http.h"
#include "SiteConfig.h"
#include "SystemFacts.h"
#include "Tracker.h"

namespace Services
{
	// The health, the same on /api/health and /api/v1/health: every station the server shows, its
	// receivers and its GPS, and the issues behind a status that is not ok. 503 when nothing receives
	// at all, and with `strict` when any station is down; a GPS never makes it 503. The statuses
	// are of the last verdict, half a minute old at most; the ages are of now.
	struct HealthAsk
	{
		bool strict = false; // ?strict, however it is given, but for 0 and false
		bool brief = false;	 // ?view=brief: the statuses and the issues, for whoever only colours a light
		int station = -1;	 // ?station=<number>: of that station alone
	};
	HealthAsk healthAsk(const Request &request);
	Response healthResponse(Tracker &tracker, const ServerSpec &spec, const std::function<StationRegistry()> &stations, const std::function<SystemFacts()> &facts, const HealthAsk &ask);

	// The system plane: the installation, its receivers and outputs, and its
	// log. Read only; nothing here changes a setting.
	class System : public Service
	{
	public:

		explicit System(const Context &context) : tracker(context.tracker), stations(context.stations), facts(context.facts) {}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		Tracker &tracker;
		std::function<StationRegistry()> stations;
		std::function<SystemFacts()> facts;

		Response overview();
		Response receivers();
		Response outputs();
		Response log(const Request &request);
		Response events(const Request &request);
	};
}
