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

#include <ctime>
#include <functional>

#include "Common.h"
#include "Ships.h"

namespace Services
{
	// Where the API's walk over the ships comes from: the ship table under its lock, or a copy a
	// program built on AIS-catcher keeps for its readers, as the hub does for its tiles. The rows
	// of a page are read from the table either way; the walk decides which ships make the page.
	struct ShipSource
	{
		// degrees; west > east crosses the antimeridian
		struct Box
		{
			float west, south, east, north;
			bool contains(float lat, float lon) const
			{
				if (!isValidCoord(lat, lon) || lat < south || lat > north)
					return false;
				return west <= east ? lon >= west && lon <= east : lon >= west || lon <= east;
			}
		};

		virtual ~ShipSource() {}
		// Every ship heard since `floor`; `box` is the area the caller keeps. Returns the time they are as of.
		virtual std::time_t forEachShip(std::time_t now, std::time_t floor, const Box *box, const std::function<void(const Ship &)> &f) = 0;
	};
}
