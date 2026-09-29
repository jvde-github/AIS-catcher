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

#include "PlaceGeometry.h"
#include "JSON.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace PlaceGeometry {

double cross(Point a, Point b, Point c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
bool on(Point a, Point b, Point p) {
  return std::abs(cross(a, b, p)) < 1e-12 && p.x >= std::min(a.x, b.x) &&
         p.x <= std::max(a.x, b.x) && p.y >= std::min(a.y, b.y) &&
         p.y <= std::max(a.y, b.y);
}
bool intersects(Point a, Point b, Point c, Point d) {
  return ((cross(a, b, c) > 0) != (cross(a, b, d) > 0) &&
          (cross(c, d, a) > 0) != (cross(c, d, b) > 0)) ||
         on(a, b, c) || on(a, b, d) || on(c, d, a) || on(c, d, b);
}
bool contains(const PlacePart &part, double lon, double lat) {
  if (!inside({lon, lat}, part.rings[0]))
    return false;
  for (size_t i = 1; i < part.rings.size(); ++i)
    if (inside({lon, lat}, part.rings[i]))
      return false;
  return true;
}

namespace {
using Ring = std::vector<Point>;

const std::vector<JSON::Value> &array(const JSON::Value &v) {
  if (!v.isArray())
    throw std::runtime_error("Expected coordinate array");
  return v.getArray();
}

// The rings of one polygon: closed, simple, the holes inside the outer ring
// and apart from each other.
std::vector<Ring> polygon(const JSON::Value &v, size_t &count, bool part) {
  std::vector<Ring> rings;
  for (const auto &rv : array(v)) {
    Ring r;
    for (const auto &pv : array(rv)) {
      const auto &p = array(pv);
      if (++count > PLACE_VERTICES || p.size() != 2 ||
          !(p[0].isFloat() || p[0].isInt()) ||
          !(p[1].isFloat() || p[1].isInt()))
        throw std::runtime_error(
            "Use 2D coordinates, with at most " +
            std::to_string(PLACE_VERTICES) +
            (part ? " vertices per part" : " vertices per place"));
      Point q{p[0].getFloat(), p[1].getFloat()};
      if (!std::isfinite(q.x) || !std::isfinite(q.y) || std::abs(q.x) > 180 ||
          std::abs(q.y) > 90)
        throw std::runtime_error(
            "Coordinate outside longitude/latitude bounds");
      if (!r.empty() && (std::abs(q.x - r.back().x) > 180 ||
                         (q.x == r.back().x && q.y == r.back().y)))
        throw std::runtime_error(
            "Duplicate vertex or unsplit antimeridian crossing");
      r.push_back(q);
    }
    if (r.size() < 4 || r.front().x != r.back().x || r.front().y != r.back().y)
      throw std::runtime_error(
          "Polygon rings must be closed with at least three corners");
    double signedArea = 0;
    for (size_t i = 1; i < r.size(); ++i) {
      signedArea += cross(r[0], r[i - 1], r[i]);
      for (size_t j = i + 2; j < r.size(); ++j) {
        if (i == 1 && j == r.size() - 1)
          continue;
        if (intersects(r[i - 1], r[i], r[j - 1], r[j]))
          throw std::runtime_error("Polygon edges cross or touch themselves");
      }
    }
    if (std::abs(signedArea) < 1e-12)
      throw std::runtime_error("Polygon has no area");
    for (const auto &other : rings)
      for (size_t i = 1; i < r.size(); ++i)
        for (size_t j = 1; j < other.size(); ++j)
          if (intersects(r[i - 1], r[i], other[j - 1], other[j]))
            throw std::runtime_error("Polygon rings overlap");
    if (!rings.empty() && !inside(r[0], rings[0]))
      throw std::runtime_error("A hole must lie inside its polygon");
    for (size_t i = 1; i < rings.size(); ++i)
      if (inside(r[0], rings[i]) || inside(rings[i][0], r))
        throw std::runtime_error("Polygon holes overlap");
    rings.push_back(r);
  }
  if (rings.empty())
    throw std::runtime_error("Polygon is empty");
  return rings;
}

PlacePart compiled(const std::vector<Ring> &rings) {
  PlacePart part;
  for (const auto &ring : rings) {
    std::vector<PlacePoint> points;
    points.reserve(ring.size());
    for (const auto &p : ring)
      points.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});
    if (part.rings.empty())
      for (const auto &p : points) {
        part.xmin = std::min(part.xmin, p.x);
        part.xmax = std::max(part.xmax, p.x);
        part.ymin = std::min(part.ymin, p.y);
        part.ymax = std::max(part.ymax, p.y);
      }
    part.rings.push_back(std::move(points));
  }
  return part;
}
} // namespace

Shape compile(const std::string &type, const JSON::Value &coords,
              bool water) {
  Shape shape;
  auto parts = std::make_shared<std::vector<PlacePart>>();
  std::vector<std::vector<Ring>> rings;
  size_t count = 0;
  if (type == "Point") {
    const auto &xy = array(coords);
    if (xy.size() != 2 || !(xy[0].isInt() || xy[0].isFloat()) ||
        !(xy[1].isInt() || xy[1].isFloat()))
      throw std::runtime_error("Point needs longitude and latitude");
    shape.lon = xy[0].getFloat();
    shape.lat = xy[1].getFloat();
    if (!std::isfinite(shape.lon) || !std::isfinite(shape.lat) ||
        std::abs(shape.lon) > 180 || std::abs(shape.lat) > 90)
      throw std::runtime_error("Point outside longitude/latitude bounds");
    shape.parts = parts;
    return shape;
  } else if (type == "Polygon")
    rings.push_back(polygon(coords, count, false));
  else if (type == "MultiPolygon") {
    if (array(coords).empty())
      throw std::runtime_error("Place is empty");
    for (const auto &p : array(coords)) {
      size_t partCount = 0;
      rings.push_back(polygon(p, water ? partCount : count, water));
      if (water && (count += partCount) > WATER_VERTICES)
        throw std::runtime_error("A water may hold at most " +
                                 std::to_string(WATER_VERTICES) +
                                 " vertices over all its parts");
    }
  } else
    throw std::runtime_error(
        "Only Point, Polygon and MultiPolygon places are supported");

  for (const auto &part : rings)
    parts->push_back(compiled(part));
  std::stable_sort(parts->begin(), parts->end(),
                   [](const PlacePart &a, const PlacePart &b) {
                     return a.xmin < b.xmin;
                   });
  constexpr double radians = 3.14159265358979323846 / 180;
  for (const auto &part : *parts)
    for (size_t j = 0; j < part.rings.size(); ++j) {
      const auto &ring = part.rings[j];
      double size = 0;
      for (size_t i = 1; i < ring.size(); ++i)
        size +=
            (static_cast<double>(ring[i].x) - ring[i - 1].x) * radians *
            (std::sin(ring[i].y * radians) + std::sin(ring[i - 1].y * radians));
      shape.size += (j == 0 ? 1 : -1) * std::abs(size) / 2;
      if (j == 0)
        for (const auto &p : ring) {
          shape.xmin = std::min(shape.xmin, double(p.x));
          shape.xmax = std::max(shape.xmax, double(p.x));
          shape.ymin = std::min(shape.ymin, double(p.y));
          shape.ymax = std::max(shape.ymax, double(p.y));
        }
    }
  // the marker: the middle of the widest chord across the outline's middle
  // latitude, from the source coordinates
  double widest = 0;
  for (const auto &part : rings) {
    double ymin = 90, ymax = -90;
    for (const auto &p : part[0]) {
      ymin = std::min(ymin, p.y);
      ymax = std::max(ymax, p.y);
    }
    const double y = (ymin + ymax) / 2;
    std::vector<double> xs;
    for (const auto &ring : part)
      for (size_t i = 1; i < ring.size(); ++i) {
        const auto &a = ring[i - 1], &b = ring[i];
        if ((a.y > y) != (b.y > y))
          xs.push_back(a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y));
      }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 1; i < xs.size(); i += 2)
      if (xs[i] - xs[i - 1] > widest) {
        widest = xs[i] - xs[i - 1];
        shape.lon = (xs[i] + xs[i - 1]) / 2;
        shape.lat = y;
      }
  }
  if (widest <= 0)
    throw std::runtime_error("Polygon has no interior marker point");
  shape.parts = parts;
  return shape;
}

} // namespace PlaceGeometry
