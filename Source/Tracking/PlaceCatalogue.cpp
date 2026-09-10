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

#include "PlaceCatalogue.h"
#include "Helper.h"
#include "Logger.h"
#include "Parser.h"
#include "Writer.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

namespace {
using namespace AIS;
// The telemetry writer uses six decimal places. Place files must round-trip
// their original doubles so editing does not move or collapse vertices.
void preciseValue(const JSON::Value &v, JSON::Writer &w) {
  if (v.isObject()) {
    w.beginObject();
    for (const auto &m : v.getObject().getMembers()) {
      w.key(AIS::KeyMap[m.Key()][JSON_DICT_SETTING]);
      preciseValue(m.Get(), w);
    }
    w.endObject();
  } else if (v.isArray()) {
    w.beginArray();
    for (const auto &item : v.getArray())
      preciseValue(item, w);
    w.endArray();
  } else if (v.isArrayString()) {
    w.beginArray();
    for (const auto &item : v.getStringArray())
      w.val(item);
    w.endArray();
  } else if (v.isFloat()) {
    if (!std::isfinite(v.getFloat()))
      throw std::runtime_error("Non-finite place number");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << v.getFloat();
    w.raw_val(out.str());
  } else if (v.isInt())
    w.val(v.getInt());
  else if (v.isString())
    w.val(v.getString());
  else if (v.isBool())
    w.val(v.getBool());
  else
    w.val_null();
}
const JSON::Value &field(const JSON::JSON &o, int key) {
  const auto *v = o[key];
  if (!v)
    throw std::runtime_error(
        "Missing required field: " +
        std::string(AIS::KeyMap[key][JSON_DICT_SETTING].p));
  return *v;
}
std::string text(const JSON::JSON &o, int key, size_t max = 120) {
  const auto &v = field(o, key);
  if (!v.isString() || v.getString().empty() || v.getString().size() > max)
    throw std::runtime_error(
        "Enter a valid " + std::string(AIS::KeyMap[key][JSON_DICT_SETTING].p));
  return v.getString();
}
const JSON::JSON &object(const JSON::JSON &o, int key) {
  const auto &v = field(o, key);
  if (!v.isObject())
    throw std::runtime_error("Expected place object");
  return v.getObject();
}
bool validID(const std::string &id) {
  if (id.size() != 36)
    return false;
  for (size_t i = 0; i < id.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (id[i] != '-')
        return false;
    } else if (!((id[i] >= '0' && id[i] <= '9') ||
                 (id[i] >= 'a' && id[i] <= 'f')))
      return false;
  }
  return true;
}
// Validate and calculate in double precision; retain compiled vertices as
// floats.
struct Point {
  double x, y;
  Point(double x, double y) : x(x), y(y) {}
  Point(PlacePoint p) : x(p.x), y(p.y) {}
};
using Ring = std::vector<Point>;
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
template <class P> bool inside(Point p, const std::vector<P> &r) {
  bool result = false;
  for (size_t i = 1; i < r.size(); ++i) {
    Point a = r[i - 1], b = r[i];
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
      result = !result;
  }
  return result;
}
const std::vector<JSON::Value> &array(const JSON::Value &v) {
  if (!v.isArray())
    throw std::runtime_error("Expected coordinate array");
  return v.getArray();
}
std::vector<Ring> polygon(const JSON::Value &v, size_t &count) {
  std::vector<Ring> rings;
  for (const auto &rv : array(v)) {
    Ring r;
    for (const auto &pv : array(rv)) {
      const auto &p = array(pv);
      if (++count > 2000 || p.size() != 2 ||
          !(p[0].isFloat() || p[0].isInt()) ||
          !(p[1].isFloat() || p[1].isInt()))
        throw std::runtime_error(
            "Use 2D coordinates, with at most 2000 vertices per place");
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
    for (const auto &other : rings) {
      for (size_t i = 1; i < r.size(); ++i)
        for (size_t j = 1; j < other.size(); ++j)
          if (intersects(r[i - 1], r[i], other[j - 1], other[j]))
            throw std::runtime_error("Polygon rings overlap");
    }
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
} // namespace

PlaceCatalogue::Place PlaceCatalogue::validate(const std::string &json,
                                               bool saving) {
  if (json.size() > 131072)
    throw std::runtime_error("Place file exceeds 128 KiB");
  JSON::Parser parser(JSON_DICT_SETTING);
  parser.setSkipUnknown(true);
  auto doc = parser.parse(json);
  Place a;
  auto metadata = std::make_shared<PlaceMetadata>();
  auto &m = *metadata;
  if (text(doc.root, KEY_SETTING_MODEL_TYPE) != "Feature")
    throw std::runtime_error("Expected one GeoJSON Feature");
  m.uuid = text(doc.root, KEY_SETTING_ID);
  if (!validID(m.uuid))
    throw std::runtime_error("Place ID must be a lowercase UUID");
  const auto &p = object(doc.root, KEY_PLACE_PROPERTIES);
  text(p, KEY_PLACE_NAME);
  const auto &version = field(p, KEY_PLACE_SCHEMA_VERSION);
  if (!version.isInt() || version.getInt() != 1)
    throw std::runtime_error("Unsupported place schema version");
  const auto &rev = field(p, KEY_PLACE_REVISION);
  if (!rev.isInt() || rev.getInt() < 0 || rev.getInt() > 2147483646)
    throw std::runtime_error("Invalid place revision");
  a.revision = rev.getInt();
  m.name = text(p, KEY_PLACE_NAME);
  m.type = text(p, KEY_PLACE_PLACE_TYPE);
  const auto code = [&](int key) -> std::string {
    const std::string value = text(p, key, 5);
    if (value.size() != 5)
      throw std::runtime_error(
          "UN/LOCODE needs five characters, for example NLRTM");
    for (size_t i = 0; i < 5; ++i)
      if (!((value[i] >= 'A' && value[i] <= 'Z') ||
            (i > 1 && value[i] >= '0' && value[i] <= '9')))
        throw std::runtime_error(
            "Use an uppercase UN/LOCODE, for example NLRTM");
    return value;
  };
  if (m.type == "port") {
    m.code = code(KEY_PLACE_UNLOCODE);
    if (p[KEY_PORT_PARENT]) {
      m.parentCode = code(KEY_PORT_PARENT);
      if (m.parentCode == m.code)
        throw std::runtime_error("A port cannot be its own parent");
    }
  } else if (m.type == "berth" || m.type == "anchorage") {
    m.code = code(KEY_PLACE_PORT_UNLOCODE);
  } else if (m.type == "custom")
    m.category = text(p, KEY_PLACE_CATEGORY, 60);
  else
    throw std::runtime_error("Unknown place type");
  if (p[KEY_PLACE_UNLOCODE] && m.type != "port")
    throw std::runtime_error("Use port_unlocode for a berth or anchorage");

  const auto &g = object(doc.root, KEY_PLACE_GEOMETRY);
  const auto &coords = field(g, KEY_PLACE_COORDINATES);
  auto type = text(g, KEY_SETTING_MODEL_TYPE);
  size_t count = 0;
  std::vector<std::vector<Ring>> parts;
  double pointLon = 0, pointLat = 0;
  if (type == "Point") {
    const auto &xy = array(coords);
    if (xy.size() != 2 || !(xy[0].isInt() || xy[0].isFloat()) ||
        !(xy[1].isInt() || xy[1].isFloat()))
      throw std::runtime_error("Point needs longitude and latitude");
    pointLon = xy[0].getFloat();
    pointLat = xy[1].getFloat();
    if (!std::isfinite(pointLon) || !std::isfinite(pointLat) ||
        std::abs(pointLon) > 180 || std::abs(pointLat) > 90)
      throw std::runtime_error("Point outside longitude/latitude bounds");
  } else if (type == "Polygon")
    parts.push_back(polygon(coords, count));
  else if (type == "MultiPolygon") {
    if (array(coords).empty())
      throw std::runtime_error("Place is empty");
    const auto inPart = [](Point p, const std::vector<Ring> &part) {
      for (const auto &ring : part)
        for (size_t i = 1; i < ring.size(); ++i)
          if (on(ring[i - 1], ring[i], p))
            return false;
      if (!inside(p, part[0]))
        return false;
      for (size_t i = 1; i < part.size(); ++i)
        if (inside(p, part[i]))
          return false;
      return true;
    };
    for (const auto &p : array(coords)) {
      auto part = polygon(p, count);
      for (const auto &other : parts) {
        for (const auto &r : part)
          for (const auto &s : other)
            for (size_t i = 1; i < r.size(); ++i)
              for (size_t j = 1; j < s.size(); ++j)
                if (cross(r[i - 1], r[i], s[j - 1]) *
                            cross(r[i - 1], r[i], s[j]) <
                        -1e-24 &&
                    cross(s[j - 1], s[j], r[i - 1]) *
                            cross(s[j - 1], s[j], r[i]) <
                        -1e-24)
                  throw std::runtime_error(
                      "Separate polygon parts must not overlap");
        const auto overlaps = [&](const std::vector<Ring> &from,
                                  const std::vector<Ring> &to) {
          const auto &ring = from[0];
          for (size_t i = 1; i < ring.size(); ++i) {
            if (inPart(ring[i], to) ||
                inPart(Point((ring[i].x + ring[i - 1].x) / 2,
                             (ring[i].y + ring[i - 1].y) / 2),
                       to))
              return true;
          }
          // An interior scanline probe also detects coincident polygons.
          std::vector<double> ys;
          for (const auto &p : ring)
            ys.push_back(p.y);
          std::sort(ys.begin(), ys.end());
          for (size_t k = 1; k < ys.size(); ++k) {
            if (ys[k] == ys[k - 1])
              continue;
            double y = (ys[k] + ys[k - 1]) / 2;
            std::vector<double> xs;
            for (const auto &r : from)
              for (size_t i = 1; i < r.size(); ++i)
                if ((r[i].y > y) != (r[i - 1].y > y))
                  xs.push_back(r[i - 1].x + (r[i].x - r[i - 1].x) *
                                                (y - r[i - 1].y) /
                                                (r[i].y - r[i - 1].y));
            std::sort(xs.begin(), xs.end());
            for (size_t i = 1; i < xs.size(); i += 2)
              if (inPart(Point((xs[i] + xs[i - 1]) / 2, y), to))
                return true;
          }
          return false;
        };
        if (overlaps(part, other) || overlaps(other, part))
          throw std::runtime_error("Separate polygon parts overlap");
      }
      parts.push_back(part);
    }
  } else
    throw std::runtime_error(
        "Only Point, Polygon and MultiPolygon places are supported");
  PlaceIndex::Entry e;
  e.id = UINT32_MAX;
  const auto compile = [&](const std::vector<Ring> &rings) {
    PlaceIndex::Part compiled;
    for (const auto &ring : rings) {
      std::vector<PlacePoint> points;
      points.reserve(ring.size());
      for (const auto &p : ring)
        points.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});
      if (compiled.rings.empty()) {
        for (const auto &p : points) {
          compiled.xmin = std::min(compiled.xmin, p.x);
          compiled.xmax = std::max(compiled.xmax, p.x);
          compiled.ymin = std::min(compiled.ymin, p.y);
          compiled.ymax = std::max(compiled.ymax, p.y);
        }
      }
      compiled.rings.push_back(std::move(points));
    }
    return compiled;
  };
  auto compiledParts = std::make_shared<std::vector<PlaceIndex::Part>>();
  for (const auto &part : parts)
    compiledParts->push_back(compile(part));
  std::stable_sort(compiledParts->begin(), compiledParts->end(),
                   [](const PlaceIndex::Part &a, const PlaceIndex::Part &b) {
                     return a.xmin < b.xmin;
                   });
  constexpr double radians = 3.14159265358979323846 / 180;
  for (const auto &part : *compiledParts) {
    for (size_t j = 0; j < part.rings.size(); ++j) {
      const auto &ring = part.rings[j];
      double size = 0;
      // Spherical surface place, proportional to square metres (R² omitted).
      for (size_t i = 1; i < ring.size(); ++i)
        size +=
            (static_cast<double>(ring[i].x) - ring[i - 1].x) * radians *
            (std::sin(ring[i].y * radians) + std::sin(ring[i - 1].y * radians));
      e.size += (j == 0 ? 1 : -1) * std::abs(size) / 2;
      if (j == 0)
        for (const auto &p : ring) {
          e.xmin = std::min(e.xmin, static_cast<double>(p.x));
          e.xmax = std::max(e.xmax, static_cast<double>(p.x));
          e.ymin = std::min(e.ymin, static_cast<double>(p.y));
          e.ymax = std::max(e.ymax, static_cast<double>(p.y));
        }
    }
  }
  e.polygons = compiledParts;
  e.metadata = metadata;
  e.revision = a.revision + (saving ? 1 : 0);
  a.compiled = std::move(e);
  // Compute the port marker once, from the validated source coordinates.
  double widest = 0, markerLon = 0, markerLat = 0;
  for (const auto &part : parts) {
    double ymin = 90, ymax = -90;
    for (const auto &p : part[0]) {
      ymin = std::min(ymin, p.y);
      ymax = std::max(ymax, p.y);
    }
    double y = (ymin + ymax) / 2;
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
        markerLon = (xs[i] + xs[i - 1]) / 2;
        markerLat = y;
      }
  }
  if (type != "Point" && widest <= 0)
    throw std::runtime_error("Polygon has no interior marker point");
  if (type == "Point") {
    markerLon = pointLon;
    markerLat = pointLat;
  }
  a.compiled.lon = markerLon;
  a.compiled.lat = markerLat;
  if (const auto *size = p[KEY_PORT_SIZE]) {
    if (!size->isInt() || size->getInt() < 0 || size->getInt() > 3)
      throw std::runtime_error("Port size must be 0 to 3");
    a.compiled.markerSize = size->getInt();
  }
  if (saving) {
    JSON::Value props = *doc.root[KEY_PLACE_PROPERTIES], revision;
    revision.setInt(a.compiled.revision);
    props.getObject().Set(KEY_PLACE_REVISION, revision);
  }
  std::string canonical;
  JSON::Writer writer(canonical);
  writer.beginObject();
  for (const auto &member : doc.root.getMembers()) {
    writer.key(AIS::KeyMap[member.Key()][JSON_DICT_SETTING]);
    preciseValue(member.Get(), writer);
  }
  writer.endObject();
  writer.finish();
  a.compiled.feature =
      std::make_shared<const std::string>(std::move(canonical));
  return a;
}

PlaceCatalogue::PlaceCatalogue(const std::string &dir) : directory(dir) {
#ifdef _WIN32
  int result = _mkdir(directory.c_str());
#else
  int result = mkdir(directory.c_str(), 0750);
#endif
  if (result != 0 && errno != EEXIST)
    Warning() << "Places: cannot create " << directory;
  std::set<std::string> loadedCodes, loadedIDs;
  size_t totalVertices = 0, totalBytes = 0;
  for (const auto &file : Util::Helper::getFilesInDirectory(directory)) {
    if (file.size() < 9 || file.substr(file.size() - 8) != ".geojson")
      continue;
    try {
      const std::string path = directory + "/" + file;
      std::ifstream input(path, std::ios::binary | std::ios::ate);
      if (!input || input.tellg() > 268435456)
        throw std::runtime_error("Place file unreadable or too large");
      JSON::Parser parser(JSON_DICT_SETTING);
      parser.setSkipUnknown(true);
      auto doc = parser.parse(Util::Helper::readFile(path));
      const bool collection =
          text(doc.root, KEY_SETTING_MODEL_TYPE) == "FeatureCollection";
      std::vector<Place> loaded;
      std::set<std::string> fileIDs, fileCodes;
      size_t fileVertices = 0, fileBytes = 0;
      const auto load = [&](const std::string &value) {
        auto a = validate(value);
        a.file = file;
        a.collectionFile = collection;
        if (a.revision < 1)
          throw std::runtime_error("Invalid saved revision");
        if (loadedIDs.count(a.compiled.metadata->uuid))
          throw std::runtime_error("Duplicate place UUID: " +
                                   a.compiled.metadata->uuid);
        if (!fileIDs.insert(a.compiled.metadata->uuid).second ||
            (a.compiled.metadata->type == "port" &&
             (!fileCodes.insert(a.compiled.metadata->code).second ||
              loadedCodes.count(a.compiled.metadata->code))))
          throw std::runtime_error("Duplicate place UUID or port code");
        if (!a.compiled.polygons->empty()) {
          fileBytes += a.compiled.feature->size();
          for (const auto &part : *a.compiled.polygons)
            for (const auto &ring : part.rings) {
              fileVertices += ring.size();
              fileBytes += ring.size() * sizeof(PlacePoint);
            }
        }
        if (totalVertices + fileVertices > 4000000 ||
            totalBytes + fileBytes > 268435456)
          throw std::runtime_error("Place polygon budget exceeded");
        loaded.push_back(std::move(a));
      };
      if (collection) {
        for (const auto &value : array(field(doc.root, KEY_PLACE_FEATURES))) {
          std::string json;
          JSON::Writer w(json);
          preciseValue(value, w);
          w.finish();
          load(json);
        }
      } else
        load(Util::Helper::readFile(path));
      totalVertices += fileVertices;
      totalBytes += fileBytes;
      loadedCodes.insert(fileCodes.begin(), fileCodes.end());
      loadedIDs.insert(fileIDs.begin(), fileIDs.end());
      for (auto &a : loaded)
        places.push_back(std::move(a));

    } catch (const std::exception &e) {
      ++skipped;
      Warning() << "Places: skipped " << file << ": " << e.what();
    }
  }
  // References may resolve through an external port catalogue, or later.
  // Keep the definition even if its parent has not been installed yet.
  std::sort(places.begin(), places.end(), [](const Place &a, const Place &b) {
    return a.compiled.metadata->uuid < b.compiled.metadata->uuid;
  });
  for (auto &entry : places)
    entry.number = static_cast<uint32_t>(nextNumber++);
  rebuild();
  Info() << "Places: loaded " << places.size() << " definitions from "
         << directory;
}

std::string PlaceCatalogue::collection() {
  std::lock_guard<std::mutex> lock(mutex);
  if (collectionJSON.empty()) {
    JSON::Writer w(collectionJSON);
    w.beginObject()
        .kv("place_version", index->version)
        .key("objects")
        .beginArray();
    for (const auto &place : places) {
      index->find(place.number)->writeSummary(w, 0);
    }
    w.endArray().endObject();
    w.finish();
  }
  return collectionJSON;
}

bool PlaceCatalogue::save(const std::string &json, bool remove,
                          std::string &error) {
  std::lock_guard<std::mutex> lock(mutex);
  try {
    auto a = validate(json, !remove);
    auto old = std::find_if(places.begin(), places.end(), [&](const Place &p) {
      return p.compiled.metadata->uuid == a.compiled.metadata->uuid;
    });
    if (a.revision != (old == places.end() ? 0 : old->revision) ||
        (remove && old == places.end()))
      throw std::runtime_error(
          "Place changed elsewhere. Reload before saving or deleting.");
    a.file = old == places.end() ? a.compiled.metadata->uuid + ".geojson"
                                 : old->file;
    a.collectionFile = old != places.end() && old->collectionFile;
    std::string path = directory + "/" + a.file;
    const auto write = [&](const Place &value, bool deleting) {
      if (!value.collectionFile) {
        if (deleting) {
          if (std::remove(path.c_str()) != 0)
            throw std::runtime_error("Could not delete place file");
          return true;
        }
        return Util::Helper::writeFileAtomic(
            path, *value.compiled.feature + "\n", error);
      }
      std::string output;
      JSON::Writer w(output);
      w.beginObject()
          .kv("type", "FeatureCollection")
          .key("features")
          .beginArray();
      for (const auto &pair : places) {
        const auto &p = pair;
        if (p.file != value.file)
          continue;
        if (p.compiled.metadata->uuid == value.compiled.metadata->uuid) {
          if (!deleting)
            w.raw_val(*value.compiled.feature);
        } else
          w.raw_val(*p.compiled.feature);
      }
      w.endArray().endObject();
      w.finish();
      return Util::Helper::writeFileAtomic(path, output + "\n", error);
    };
    if (remove) {
      if (!write(a, true))
        return false;
      places.erase(old);
    } else {
      check(a, old != places.end());
      if (old == places.end() && nextNumber >= UINT32_MAX)
        throw std::runtime_error(
            "Place ID space exhausted; restart to reload IDs");
      a.number =
          old == places.end() ? static_cast<uint32_t>(nextNumber) : old->number;
      a.revision = a.compiled.revision;
      if (!write(a, false))
        return false;
      if (old == places.end())
        ++nextNumber;
      if (old == places.end())
        places.push_back(std::move(a));
      else
        *old = std::move(a);
    }
    rebuild();
    changed = true;
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}

void PlaceCatalogue::check(const Place &candidate, bool replacing) const {
  size_t vertices = 0, bytes = 0;
  const auto count = [&](const Place &p) {
    if (p.compiled.polygons->empty())
      return;
    bytes += p.compiled.feature->size();
    for (const auto &part : *p.compiled.polygons)
      for (const auto &ring : part.rings) {
        vertices += ring.size();
        bytes += ring.size() * sizeof(PlacePoint);
      }
  };
  count(candidate);
  for (const auto &pair : places) {
    const auto &p = pair;
    if (replacing &&
        p.compiled.metadata->uuid == candidate.compiled.metadata->uuid)
      continue;
    if (candidate.compiled.metadata->type == "port" &&
        p.compiled.metadata->type == "port" &&
        p.compiled.metadata->code == candidate.compiled.metadata->code)
      throw std::runtime_error("Duplicate port code: " +
                               candidate.compiled.metadata->code);
    count(p);
  }
  if (vertices > 4000000 || bytes > 268435456)
    throw std::runtime_error(
        "Place polygon budget exceeded (4 million vertices / 256 MiB)");
}

std::string PlaceCatalogue::feature(uint32_t id, const std::string &version) {
  std::lock_guard<std::mutex> lock(mutex);
  if (version != index->version)
    return "{}";
  const auto *p = index->find(id);
  return p ? *p->feature : "{}";
}

std::shared_ptr<const PlaceIndex> PlaceCatalogue::snapshot() {
  std::lock_guard<std::mutex> lock(mutex);
  return index;
}

void PlaceCatalogue::rebuild() {
  auto next = std::make_shared<PlaceIndex>();
  static const auto epoch =
      std::chrono::system_clock::now().time_since_epoch().count();
  static std::atomic<unsigned long long> serial{0};
  next->version = std::to_string(epoch) + "-" + std::to_string(++serial);
  next->entries.resize(nextNumber);
  for (const auto &pair : places) {
    const auto &a = pair;
    auto e = a.compiled;
    e.id = a.number;
    next->entries[a.number] = std::move(e);
  }
  for (size_t i = 0; i < next->entries.size(); ++i) {
    const auto &e = next->entries[i];
    if (e.id == UINT32_MAX)
      continue;
    if (!e.polygons->empty())
      next->polygonCandidates.push_back(i);
    if (e.metadata->type == "port") {
      next->byCode.emplace(PlaceIndex::normalized(e.metadata->code), e.id);
      next->byName[PlaceIndex::normalized(e.metadata->name)].push_back(e.id);
    }
  }
  // a berth or anchorage appears on the map when its port does: it takes the
  // port's size class, so every writer derives the same zoom for both
  for (auto &e : next->entries) {
    if (e.id == UINT32_MAX || e.metadata->type == "port" || e.metadata->type == "custom")
      continue;
    const auto it = next->byCode.find(PlaceIndex::normalized(e.metadata->code));
    if (it != next->byCode.end())
      e.markerSize = next->entries[it->second].markerSize;
  }
  std::sort(next->polygonCandidates.begin(), next->polygonCandidates.end(),
            [&](uint32_t a, uint32_t b) {
              const auto &x = next->entries[a], &y = next->entries[b];
              return x.size == y.size ? x.id < y.id : x.size < y.size;
            });
  index = std::move(next);
  collectionJSON.clear();
}

void PlaceIndex::match(double lat, double lon,
                       std::array<uint32_t, 5> &ids) const {
  ids.fill(UINT32_MAX);
  const auto all = matchAll(lat, lon);
  std::copy_n(all.begin(), std::min(all.size(), ids.size()), ids.begin());
}

std::vector<uint32_t> PlaceIndex::matchAll(double lat, double lon) const {
  std::vector<uint32_t> ids;
  if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 ||
      lon < -180 || lon > 180)
    return ids;
  lat = static_cast<float>(lat);
  lon = static_cast<float>(lon);
  const auto contains = [](Point p, const std::vector<PlacePoint> &ring) {
    bool result = false;
    for (size_t i = 1; i < ring.size(); ++i) {
      const Point a = ring[i - 1], b = ring[i];
      // Preserve outer/hole boundary semantics while walking each edge once.
      if (on(a, b, p))
        return true;
      if ((a.y > p.y) != (b.y > p.y) &&
          p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
        result = !result;
    }
    return result;
  };
  candidates(lat, lon, [&](const Entry &a) {
    for (const auto &part : *a.polygons) {
      if (part.xmin > lon)
        break;
      if (lon > part.xmax || lat < part.ymin || lat > part.ymax)
        continue;
      if (!contains({lon, lat}, part.rings[0]))
        continue;
      bool hole = false;
      for (size_t i = 1; i < part.rings.size(); ++i)
        if (contains({lon, lat}, part.rings[i])) {
          hole = true;
          break;
        }
      if (hole)
        continue;
      ids.push_back(a.id);
      break;
    }
  });
  return ids;
}

void PlaceCatalogue::writeStatus(JSON::Writer &writer) {
  std::lock_guard<std::mutex> lock(mutex);
  size_t vertices = 0, parts = 0, geometryBytes = 0, jsonBytes = 0;
  for (const auto &item : places) {
    const auto &entry = item.compiled;
    jsonBytes += entry.feature->size();
    parts += entry.polygons->size();
    for (const auto &part : *entry.polygons)
      for (const auto &ring : part.rings) {
        vertices += ring.size();
        geometryBytes += ring.capacity() * sizeof(PlacePoint);
      }
  }
  writer.key("places")
      .beginObject()
      .kv("loaded", (long long)places.size())
      .kv("skipped", (long long)skipped)
      .kv("parts", (long long)parts)
      .kv("vertices", (long long)vertices)
      .kv("geometry_bytes", (long long)geometryBytes)
      .kv("definition_bytes", (long long)jsonBytes)
      .endObject();
}

void PlaceIndex::Entry::writeSummary(JSON::Writer &w, uint64_t sequence, int minZoom) const {
  w.beginObject()
      .kv("id", "place-" + std::to_string(id))
      .kv("kind", 10)
      .kv("seq", (long long)sequence)
      .kv("runtime_id", id)
      .kv("uuid", metadata->uuid)
      .kv("lat", lat)
      .kv("lon", lon)
      .kv("label", metadata->name)
      .kv("place_type", metadata->type)
      .kv("code", metadata->code)
      .kv("parent_unlocode", metadata->parentCode)
      .kv("has_geometry", !polygons->empty());
  // every place appears from the zoom of its size class: a port by its own, a
  // berth or anchorage by its port's, a custom area by the size it was given
  static const int zooms[] = {12, 11, 9, 7};
  w.kv("z", std::max(zooms[markerSize], minZoom));
  if (metadata->type == "port")
    w.kv("country", metadata->code.substr(0, 2));
  w.kv("revision", revision)
      .kv("category", metadata->category)
      .kv("size", markerSize)
      .endObject();
}

std::string PlaceIndex::normalized(const std::string &text) {
  std::string result;
  for (unsigned char c : text) {
    if (c >= 'a' && c <= 'z')
      c -= 'a' - 'A';
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 128)
      result += c;
  }
  return result;
}
const PlaceIndex::Entry *
PlaceIndex::matchDestination(const std::string &text) const {
  const auto value = normalized(text);
  const auto code = byCode.find(value);
  if (code != byCode.end())
    return find(code->second);
  const auto name = byName.find(value);
  return name != byName.end() && name->second.size() == 1
             ? find(name->second.front())
             : nullptr;
}
const PlaceIndex::Entry *PlaceIndex::findPort(const std::string &code) const {
  const auto it = byCode.find(code);
  return it == byCode.end() ? nullptr : find(it->second);
}
