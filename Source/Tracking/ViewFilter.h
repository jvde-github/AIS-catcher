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

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

struct Ship;

// Which ships a reader sees. The terms are the viewer's own filter panel
// (frontend/packages/core/filter.js), under the same names and with the same
// meaning, so a filter saved in the viewer is a server's filter or an API
// query as it stands. To those it adds the stations that heard a ship and an
// allow and a deny list of MMSIs.
struct ViewFilter
{
	// heard by one of these within Ship::STATION_WINDOW, as Ship::forEachHeard
	// tells it; empty is any
	std::vector<int> stations;
	std::vector<std::string> hidden_buckets; // am as bm bs aton base sarte air
	std::vector<int> hidden_classes;         // ship class numbers, as the viewer's
	std::vector<int> hidden_statuses;        // navigational status 0-15
	bool has_speed_min = false, has_speed_max = false;
	float speed_min = 0, speed_max = 0; // knots
	int seen = 0;                       // minutes; 0 is any age
	int track_time = 0;                 // seconds of track served; 0 is what the tracker keeps
	bool place_geometry = true;         // a place's outline is served, not only its marker
	bool has_distance_min = false, has_distance_max = false;
	float distance_min = 0, distance_max = 0; // nautical miles, from the station that heard the ship
	std::string validated = "any";            // any | yes | no | pending
	std::string repeated = "any";             // any | only | never
	std::vector<uint32_t> mmsi_allow, mmsi_deny;

	static const float MOVING_KNOTS;

	// A term from a configuration or a query. Lists take one value per call,
	// or several separated by commas. Throws with a message for people.
	void set(const std::string &key, const std::string &value);
	static bool isKey(const std::string &key);

	bool passes(const Ship &s, std::time_t now) const;
	bool any() const { return *this != ViewFilter(); }
	// the oldest time a track is served from, 0 when the view does not cap it
	std::time_t trackFloor(std::time_t now) const { return track_time > 0 && now > track_time ? now - (std::time_t)track_time : 0; }
	bool allowsStation(int id) const { return stations.empty() || std::find(stations.begin(), stations.end(), id) != stations.end(); }

	bool operator==(const ViewFilter &o) const;
	bool operator!=(const ViewFilter &o) const { return !(*this == o); }

	static const char *bucketOf(int shipclass, float speed);
	static int classFilterId(int shipclass);
};
