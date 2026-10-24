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

#include <string>
#include <vector>

#include "Writer.h"

namespace Services
{
	// The names the API gives what the tracker keeps as numbers, in the order
	// of those numbers: a vessel's class, its MMSI type, its navigational
	// status, whether its position held. The filters of /api/v1/vessels take
	// the same names.
	extern const char *const VESSEL_CLASSES[];
	extern const int N_VESSEL_CLASSES;
	extern const char *const MMSI_TYPES[];
	extern const int N_MMSI_TYPES;
	extern const char *const NAV_STATUSES[];
	extern const int N_NAV_STATUSES;
	extern const char *const VALIDATED[];
	extern const int N_VALIDATED;
	extern const char *const BINARY_KINDS[];
	extern const int N_BINARY_KINDS;

	// One parameter of a route: in the path or the query, the members of its
	// JSON Schema, what it means. A list takes several values separated by commas.
	struct Param
	{
		const char *name, *in;
		std::string schema;
		const char *description;
		bool required, list;
	};

	// One resource of /api/v1, as the index, the OpenAPI document and the MCP
	// tools describe it: the one place the API is written down. `response` names
	// the schema of what it answers, `geojson` the one it answers with format=geojson.
	struct Route
	{
		const char *path, *service, *id, *tag, *summary;
		std::vector<Param> params;
		const char *response, *type, *geojson;

		bool takes(const std::string &name) const;
	};

	const std::vector<Route> &routes();
	const Route *route(const std::string &path);

	// the OpenAPI 3.1 document of the routes the given services offer
	void writeOpenAPI(JSON::Writer &w, const std::vector<std::string> &services);
	// a JSON Schema object of the named parameters of a route, for a tool's input
	void writeInputSchema(JSON::Writer &w, const Route &r, const std::vector<std::string> &names);
}
