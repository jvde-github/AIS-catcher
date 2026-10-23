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

#include "Http.h"
#include "StationRegistry.h"
#include "SiteConfig.h"
#include "Tracker.h"
#include "ViewFilter.h"

namespace Services
{
	// /api/v1: the tracker as data. Every list comes in the same envelope,
	// every time is UTC, every error is problem details. `filter` is the
	// server's: what lies outside it this API does not know of.
	class ApiV1 : public Service
	{
	public:
		ApiV1(Tracker &tracker, std::function<StationRegistry()> stations, const ServerSpec &spec = ServerSpec())
			: tracker(tracker), stations(std::move(stations)), filter(spec.filter), outlines(spec.place_geometry), services(spec.served()) {}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		Tracker &tracker;
		std::function<StationRegistry()> stations;
		ViewFilter filter;
		bool outlines; // a place's outline is served, not only its marker
		std::vector<std::string> services; // what the server offers beside this, for the index

		Response index();
		Response openapi();
		Response stationList();
		Response station(int id);
		Response statistics(int station);
		Response vessels(const Request &request);
		Response vessel(uint32_t mmsi);
		Response track(uint32_t mmsi, const Request &request);
		Response vesselVisits(uint32_t mmsi);
		Response places(const Request &request);
		Response place(const std::string &uuid);
		Response placeVessels(const std::string &uuid);
		Response placeVisits(const std::string &uuid, const Request &request);
		Response events(const Request &request);
		Response aircraftList(const Request &request);
		Response aircraft(uint32_t icao);
		bool aircraftShown(Response &refusal);

		bool shows(uint32_t mmsi);
	};
}
