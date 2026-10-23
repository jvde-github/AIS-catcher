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

#include "Common.h"

// How ships are tracked: kept as one value so a viewer and the site's tracker
// read the same settings.
struct TrackingConfig {
  // keep every member resettable by assigning a default-constructed copy
  float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
  // every message is measured from lat and lon, whichever station heard it:
  // a web viewer with ships of its own measures from where its section says
  bool fixed = false;
  bool latlon_share = false;
  bool server_mode = false;
  bool msg_save = false;
  bool use_gps = true;
  uint32_t own_mmsi = 0;
  int time_history = 30 * 60;
  // oldest track and replay data served, 0 is unlimited
  int track_time = 3600;
  bool expire_fields = false;
  // 0 means "not configured": LONG_RANGE_CUTOFF_DEFAULT is used instead
  int cutoff = 0;
  // kilobytes of track storage; 0 keeps the default
  int track_memory = 0;
  // ship table size; 0 keeps the default (4096, or 32 times that in server
  // mode)
  int max_ships = 0;

  bool operator==(const TrackingConfig &o) const {
    return lat == o.lat && lon == o.lon && fixed == o.fixed && latlon_share == o.latlon_share && server_mode == o.server_mode &&
           msg_save == o.msg_save && use_gps == o.use_gps && own_mmsi == o.own_mmsi && time_history == o.time_history &&
           track_time == o.track_time && expire_fields == o.expire_fields && cutoff == o.cutoff &&
           track_memory == o.track_memory && max_ships == o.max_ships;
  }
};
