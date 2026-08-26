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

#include <algorithm>
#include <cstdint>

#include "Common.h"
#include "Region.h"

// RegionTree.cpp, generated
extern const uint16_t region_levels;
extern const uint32_t region_count;
extern const uint32_t region_start[];
extern const uint16_t region_id[];
extern const uint16_t region_name_count;
extern const char *const region_name[];

namespace Region
{
	// 16-bit value spread to the even bit positions
	static inline uint32_t spread(uint32_t v)
	{
		v = (v | (v << 8)) & 0x00FF00FF;
		v = (v | (v << 4)) & 0x0F0F0F0F;
		v = (v | (v << 2)) & 0x33333333;
		v = (v | (v << 1)) & 0x55555555;
		return v;
	}

	// leaves tile the world in Morton order: the one holding a position is the last start not above its key
	int find(float lat, float lon)
	{
		if (!isValidCoord(lat, lon) || lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f)
			return NONE;

		const uint32_t n = 1u << region_levels;
		uint32_t x = (uint32_t)((lon + 180.0f) / 360.0f * n);
		uint32_t y = (uint32_t)((lat + 90.0f) / 180.0f * n);
		if (x >= n) x = n - 1;
		if (y >= n) y = n - 1;
		const uint32_t key = spread(x) | (spread(y) << 1);

		const uint32_t *p = std::upper_bound(region_start, region_start + region_count, key);
		const uint16_t r = region_id[(p - region_start) - 1];
		return r == 0xFFFF ? NONE : (int)r;
	}

	const char *name(int id)
	{
		return id >= 0 && id < region_name_count ? region_name[id] : "";
	}
}
