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

#include <map>
#include <vector>

#include "Ships.h"
#include "StationRegistry.h"

namespace Services
{
	// Which stations' locations a server shows. A station kept home gives no position and
	// nothing measured from there: distance and bearing, the vessel it rides, its range.
	// `own`: the tracker's own position, which measured what no listed station did.
	struct Located
	{
		std::map<int, bool> station;
		bool own = false;

		bool at(int id) const
		{
			auto it = station.find(id);
			return it == station.end() ? own : it->second;
		}
		bool of(const Ship &s) const { return at(s.heard[0].last ? s.heard[0].station : 0); }
		// one more station shown, to its owner
		Located &allow(int id)
		{
			if (id >= 0)
				station[id] = true;
			return *this;
		}
		// these stations, or every one when none is named
		bool all(const std::vector<int> &ids) const
		{
			if (ids.empty())
			{
				if (station.empty())
					return own;
				for (const auto &s : station)
					if (!s.second)
						return false;
				return true;
			}
			for (int id : ids)
				if (!at(id))
					return false;
			return true;
		}
		// whether the range figures of these stations stay home: one of them kept home, and a
		// range measured at all
		bool hidesRange(const std::vector<int> &ids, float max_distance) const { return max_distance > 0 && !all(ids); }
		// as each station's own share_loc says: for a reader without a server
		static Located from(const StationRegistry &stations, bool own)
		{
			Located l;
			l.own = own;
			stations.forEach([&](const StationRegistry::Station &s) { l.station[s.id] = s.share_loc; });
			return l;
		}
	};
}
