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

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace JSON {
class Value;
}

struct PlacePoint {
  float x, y;
};
// One polygon of a place: the outer ring, then its holes, and the box that
// spares the ring walk.
struct PlacePart {
  std::vector<std::vector<PlacePoint>> rings;
  float xmin = 180, xmax = -180, ymin = 90, ymax = -90;
};

// An outline is validated and measured in double; compiled, it keeps floats.
namespace PlaceGeometry {
struct Point {
  double x, y;
  Point(double x, double y) : x(x), y(y) {}
  Point(PlacePoint p) : x(p.x), y(p.y) {}
};
double cross(Point a, Point b, Point c);
bool on(Point a, Point b, Point p);
bool intersects(Point a, Point b, Point c, Point d);
// A point on the ring counts as inside. Edges wholly above or below the point
// are skipped before the boundary test.
template <class P> bool inside(Point p, const std::vector<P> &r) {
  bool result = false;
  for (size_t i = 1; i < r.size(); ++i) {
    const Point a = r[i - 1], b = r[i];
    if ((a.y < p.y && b.y < p.y) || (a.y > p.y && b.y > p.y))
      continue;
    if (on(a, b, p))
      return true;
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
      result = !result;
  }
  return result;
}
// inside the outer ring and in none of the holes
bool contains(const PlacePart &part, double lon, double lat);

// A place's outline as compiled from its GeoJSON: the parts by minimum
// longitude, the box, the area on the sphere (R² omitted) and a marker point
// inside it. A point place has no parts.
struct Shape {
  std::shared_ptr<const std::vector<PlacePart>> parts;
  double xmin = 180, xmax = -180, ymin = 90, ymax = -90, size = 0;
  double lat = 0, lon = 0;
};
// A drawn place holds at most PLACE_VERTICES corners. A water is tiled into
// parts, so it holds that many per part and WATER_VERTICES in all.
const size_t PLACE_VERTICES = 2000, WATER_VERTICES = 16000;
// Point, Polygon or MultiPolygon coordinates; throws with the reason an
// outline is refused.
Shape compile(const std::string &type, const JSON::Value &coordinates,
              bool water);
} // namespace PlaceGeometry
