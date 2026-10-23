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

#include <ctime>

#include "Feeds.h"

namespace Services
{
	Feeds::Feeds(Tracker &tracker, const ServerSpec &spec, PrometheusCounter *metrics, bool geojson)
		: tracker(tracker), view(spec.view), metrics(metrics), geojson(geojson) {}

	int Feeds::scope(const Request &request) const
	{
		return request.path == "/metrics" || request.path == "/geojson" || request.path == "/allpath.geojson" ? (int)ApiKey::DATA : NOT_MINE;
	}

	bool Feeds::handle(const Request &request, Response &response)
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
		if (view.any())
		{
			const ViewFilter within = view;
			keep = [within](const Ship &s) { return within.passes(s, std::time(nullptr)); };
		}
		response.body = request.path == "/geojson" ? tracker.ships().getGeoJSON(keep) : tracker.ships().getAllPathGeoJSON(keep, view.trackFloor(std::time(nullptr)));
		return true;
	}
}
