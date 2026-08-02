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
#include <cmath>

// Spherical-earth helpers: angle conversion, distance and bearing. Header-only and dependency-free so the tracking hot paths
// can include it without pulling in the rest of the project.

namespace Util
{
	namespace Geodesy
	{
		const float PI_F = 3.14159265358979f;
		const float METERS_PER_DEGREE = 111120.0f;

		inline float deg2rad(float deg) { return deg * PI_F / 180.0f; }
		inline int rad2deg(float rad) { return (int)(360 + rad * 180 / PI_F) % 360; }

		// cos of a latitude in degrees, within 0.7% up to 85 degrees. Several
		// times cheaper than libm, which matters on per-message paths.
		inline float cosLat(float lat)
		{
			float x = deg2rad(lat), x2 = x * x;
			return 1.0f + x2 * (-0.5f + x2 * (0.0416667f - x2 * 0.00138889f));
		}

		// Squared distance in metres, equirectangular. Only valid over short
		// spans, where it is far cheaper than the great-circle form.
		inline float distanceSqMeters(float lat1, float lon1, float lat2, float lon2)
		{
			float dlat = (lat2 - lat1) * METERS_PER_DEGREE;
			float dlon = (lon2 - lon1) * METERS_PER_DEGREE * cosLat(lat2);
			return dlat * dlat + dlon * dlon;
		}

		// Great-circle distance in nautical miles and initial bearing in degrees.
		// https://www.movable-type.co.uk/scripts/latlong.html
		inline void distanceBearing(float lat1, float lon1, float lat2, float lon2, float &distance, int &bearing)
		{
			const float EarthRadius = 6371.0f;			// kilometers
			const float NauticalMilePerKm = 0.5399568f; // conversion factor

			lat1 = deg2rad(lat1);
			lon1 = deg2rad(lon1);
			lat2 = deg2rad(lat2);
			lon2 = deg2rad(lon2);

			float dlat = lat2 - lat1, dlon = lon2 - lon1;
			float a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
			distance = 2 * EarthRadius * NauticalMilePerKm * asin(sqrt(a));

			float y = sin(dlon) * cos(lat2);
			float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
			bearing = rad2deg(atan2(y, x));
		}
	}
}
