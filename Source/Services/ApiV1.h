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
#include <map>

#include "Http.h"
#include "Routes.h"
#include "ShipSource.h"
#include "StationRegistry.h"
#include "SiteConfig.h"
#include "Tracker.h"
#include "ViewFilter.h"

namespace Services
{
	// /api/v1: the tracker as data. Every list comes in the same envelope,
	// every time is UTC, every error is problem details. `view` is the
	// server's: what lies outside it this API does not know of.
	class ApiV1 : public Service
	{
	public:
		explicit ApiV1(const Context &context)
			: tracker(context.tracker), stations(context.stations), spec(context.spec), view(context.spec.view), services(context.spec.served()),
			  table(context.tracker.ships()), ships(context.ships ? *context.ships : table) {}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;
		bool metered(const Request &r) const override { return r.path != "/api/v1" && r.path != "/api/v1/openapi.json"; }
		// a limited server pages 25 rows unless asked for more, 250 at most; every 25 rows of a page count as one call
		static const int ROWS_A_CALL = 25, LIMITED_PAGE_MAX = 250;
		long long pageMax() const { return spec.limited ? LIMITED_PAGE_MAX : LIST_MAX; }
		int cost(const Request &request) const override
		{
			long long limit = ROWS_A_CALL;
			if (!spec.limited || !pageSize(request, limit))
				return 1;
			return (int)((limit + ROWS_A_CALL - 1) / ROWS_A_CALL);
		}
		// the widest bounding box a server of an area answers, degrees a side
		static const int AREA_MAX_DEG = 4;
		// on a limited server a page larger than the hour has left is cut to what is left
		bool pageSize(const Request &request, long long &limit) const
		{
			if (!request.integer("limit", limit, spec.limited ? ROWS_A_CALL : std::min<long long>(100, pageMax()), 1, pageMax()))
				return false;
			if (spec.limited && request.calls_left > 0)
				limit = std::min<long long>(limit, (long long)request.calls_left * ROWS_A_CALL);
			return true;
		}

	private:
		Tracker &tracker;
		std::function<StationRegistry()> stations;
		const ServerSpec &spec;
		const ViewFilter &view;
		std::vector<std::string> services;
		// the ship table itself, under its lock: the walk when no other source is given
		struct TableShips : ShipSource
		{
			DB &db;
			explicit TableShips(DB &db) : db(db) {}
			std::time_t forEachShip(std::time_t now, std::time_t floor, const Box *, const std::function<void(const Ship &)> &f) override
			{
				db.forEachRecent(now, floor, [&](int, const Ship &s, long) { f(s); });
				return now;
			}
		} table;
		ShipSource &ships;
		// the station the request's key names, for the length of handle(); its holder sees that station
		int owner_station = -1;
		// which stations' locations the request sees: the server's, and the station of its key
		Located located(const StationRegistry &registry) const { return Services::located(spec, registry).allow(owner_station); }

		// a path as the client should call it: under public_url where one is set
		std::string href(const std::string &path) const;
		// a server of an area offers only the routes marked for it
		bool offered(const Route *r) const { return !spec.area_only || (r && r->area); }

		Response index();
		Response openapi();
		Response stationList();
		Response station(int id);
		Response statistics(int station, const Request &request);
		Response vessels(const Request &request);
		Response vessel(uint32_t mmsi);
		Response track(uint32_t mmsi, const Request &request);
		Response vesselVisits(uint32_t mmsi);
		Response places(const Request &request);
		Response place(const std::string &uuid);
		Response placeVessels(const std::string &uuid, const Request &request);
		Response placeVisits(const std::string &uuid, const Request &request);
		Response events(const Request &request);
		Response aircraftList(const Request &request);
		Response aircraft(uint32_t icao);
		bool aircraftShown(Response &refusal);

		bool shows(uint32_t mmsi);
		// per station, the vessels it heard within Ship::STATION_WINDOW that the server shows
		std::map<int, int> heardLately();
	};
}
