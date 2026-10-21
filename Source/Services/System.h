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
	// The system plane: the installation, its receivers and outputs, its
	// health and its log. Read only; nothing here changes a setting.
	class System : public Service
	{
	public:
		// a station quiet for this long makes the health a warning
		enum { QUIET_S = 900 };

		System(Tracker &tracker, std::function<SystemFacts()> facts, EngineWriter engine)
			: tracker(tracker), facts(std::move(facts)), engine(std::move(engine)) {}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		Tracker &tracker;
		std::function<SystemFacts()> facts;
		EngineWriter engine;

		Response overview();
		Response receivers();
		Response outputs();
		Response health();
		Response log(const Request &request);
		Response events(const Request &request);
	};
}
