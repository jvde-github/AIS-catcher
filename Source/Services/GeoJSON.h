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

#include "Server.h"

namespace Services
{
	// The ships as GeoJSON at /geojson and their tracks at /allpath.geojson, as
	// other tools read them, through the server's view
	class GeoJSON : public Service
	{
	public:
		explicit GeoJSON(const Context &context) : tracker(context.tracker), view(context.spec.view) {}

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		Tracker &tracker;
		const ViewFilter &view;
	};
}
