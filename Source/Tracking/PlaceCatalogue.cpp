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
void preciseValue(const JSON::Value &v, JSON::Writer &w, const JSON::Pool &pool) {
  if (v.isObject()) {
    w.beginObject();
    for (const auto &m : v.getObject().getMembers()) {
      if (m.Key() < 0) w.key(pool.extraName(m.Key()));
      else w.key(AIS::KeyMap[m.Key()][JSON_DICT_SETTING]);
      preciseValue(m.Get(), w, pool);
    }
    w.endObject();
  } else if (v.isArray()) {
    w.beginArray();
    for (const auto &item : v.getArray())
      preciseValue(item, w, pool);
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
  return std::abs(cross(a, b, p)) < 1e-12 && p.x >= MIN(a.x, b.x) &&
         p.x <= MAX(a.x, b.x) && p.y >= MIN(a.y, b.y) &&
         p.y <= MAX(a.y, b.y);
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

// A schema 1 file, as every earlier release wrote it, is read as schema 2: the
// code moves under codes, a custom area becomes an area, and the port it named
// is resolved to that port's UUID once every file is loaded. The file on disk
// is left as it is until the place is next saved.
std::string upgradeLegacy(const std::string &json, std::string &parentCode) {
  JSON::Parser parser(JSON_DICT_SETTING);
  parser.setPreserveUnknown(true);
  auto doc = parser.parse(json);
  const auto *properties = doc.root[KEY_PLACE_PROPERTIES];
  if (!properties || !properties->isObject())
    return json;
  const auto &p = properties->getObject();
  const auto *version = p[KEY_PLACE_SCHEMA_VERSION];
  if (!version || !version->isInt() || version->getInt() != 1)
    return json;
  const auto nameOf = [&](int key) {
    return key < 0 ? doc.pool.extraName(key)
                   : std::string(AIS::KeyMap[key][JSON_DICT_SETTING].p);
  };
  const auto optional = [&](const JSON::JSON &o, int key) {
    const auto *v = o[key];
    return v && v->isString() ? v->getString() : std::string();
  };
  const std::string type = optional(p, KEY_PLACE_PLACE_TYPE);
  const std::string code = optional(p, KEY_PLACE_UNLOCODE);
  std::string category = optional(p, KEY_PLACE_CATEGORY);
  parentCode = type == "port" ? optional(p, KEY_PORT_PARENT)
                              : optional(p, KEY_PLACE_PORT_UNLOCODE);
  const JSON::JSON *details = nullptr;
  for (const auto &m : p.getMembers())
    if (nameOf(m.Key()) == "details" && m.Get().isObject())
      details = &m.Get().getObject();
  std::string terminal, own;
  std::vector<std::pair<std::string, const JSON::Value *>> source, attributes;
  if (details)
    for (const auto &m : details->getMembers()) {
      const std::string key = nameOf(m.Key());
      if (key == "terminal" && m.Get().isString())
        terminal = m.Get().getString();
      else if (key == "code" && m.Get().isString())
        own = m.Get().getString();
      else if (key == "source" || key == "url" || key == "osm" || key == "confidence")
        source.emplace_back(key, &m.Get());
      else
        attributes.emplace_back(key, &m.Get());
    }
  for (auto &c : category)
    c = std::tolower(c);
  std::string out;
  JSON::Writer w(out);
  w.beginObject();
  for (const auto &m : doc.root.getMembers()) {
    const std::string name = nameOf(m.Key());
    w.key(name);
    if (name != "properties") {
      preciseValue(m.Get(), w, doc.pool);
      continue;
    }
    w.beginObject();
    for (const auto &q : p.getMembers()) {
      const std::string key = nameOf(q.Key());
      if (key == "unlocode" || key == "port_unlocode" || key == "parent_unlocode" ||
          key == "category" || key == "details")
        continue;
      w.key(key);
      if (key == "schema_version")
        w.val(2);
      else if (key == "place_type" && type == "custom")
        w.val("area");
      else
        preciseValue(q.Get(), w, doc.pool);
    }
    if (!code.empty() || !own.empty()) {
      w.key("codes").beginObject();
      if (!code.empty())
        w.key("unlocode").beginArray().val(code).endArray();
      if (!own.empty())
        w.key("own").beginArray().val("local:" + own).endArray();
      w.endObject();
    }
    if (validID(terminal)) {
      w.kv("part_of", terminal);
      parentCode.clear();
    }
    if (!category.empty() || !attributes.empty()) {
      w.key("attributes").beginObject();
      if (!category.empty())
        w.kv("area_subtype", category);
      for (const auto &item : attributes) {
        w.key(item.first);
        preciseValue(*item.second, w, doc.pool);
      }
      w.endObject();
    }
    if (!source.empty()) {
      w.key("geometry_source").beginObject();
      for (const auto &item : source) {
        w.key(item.first);
        preciseValue(*item.second, w, doc.pool);
      }
      w.endObject();
    }
    w.endObject();
  }
  w.endObject();
  w.finish();
  return out;
}
} // namespace

PlaceCatalogue::Place PlaceCatalogue::validate(const std::string &json,
                                               bool saving) {
  if (json.size() > 131072)
    throw std::runtime_error("Place file exceeds 128 KiB");
  Place a;
  const std::string upgraded = upgradeLegacy(json, a.legacyParent);
  JSON::Parser parser(JSON_DICT_SETTING);
  parser.setPreserveUnknown(true);
  auto doc = parser.parse(upgraded);
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
  if (!version.isInt() || version.getInt() != 2)
    throw std::runtime_error("Unsupported place schema version");
  const auto &rev = field(p, KEY_PLACE_REVISION);
  if (!rev.isInt() || rev.getInt() < 0 || rev.getInt() > 2147483646)
    throw std::runtime_error("Invalid place revision");
  a.revision = rev.getInt();
  m.name = text(p, KEY_PLACE_NAME);
  m.type = text(p, KEY_PLACE_PLACE_TYPE);
  const std::set<std::string> types{"port", "section", "terminal", "berth",
                                    "anchorage", "mooring", "marina", "area"};
  if (!types.count(m.type)) throw std::runtime_error("Unknown place type");
  auto optionalText = [&](const JSON::JSON &obj, int key, size_t limit) {
    return obj[key] ? text(obj, key, limit) : std::string();
  };
  auto strings = [&](const JSON::Value &value, size_t limit, bool uuids) {
    std::vector<std::string> result;
    for (const auto &v : array(value)) {
      if (!v.isString() || v.getString().empty() || v.getString().size() > 120 ||
          (uuids && !validID(v.getString())) || result.size() >= limit)
        throw std::runtime_error("Invalid place list");
      if (std::find(result.begin(), result.end(), v.getString()) != result.end())
        throw std::runtime_error("Duplicate place list value");
      result.push_back(v.getString());
    }
    return result;
  };
  if (p[KEY_PLACE_PORT_UNLOCODE] || p[KEY_PORT_PARENT] || p[KEY_PLACE_UNLOCODE] || p[KEY_PLACE_CATEGORY])
    throw std::runtime_error("Use codes and part_of in schema 2");
  for (const auto &member : p.getMembers())
    if (member.Key() < 0 && doc.pool.extraName(member.Key()) == "details")
      throw std::runtime_error("Migrate details to schema 2 properties");
  if (const auto *parent = p[KEY_PLACE_PART_OF]) {
    if (parent->getType() != JSON::Value::Type::EMPTY) {
      m.partOf = text(p, KEY_PLACE_PART_OF, 36);
      if (!validID(m.partOf) || m.partOf == m.uuid)
        throw std::runtime_error("Invalid parent UUID");
    }
  }
  if (const auto *redirect = p[KEY_PLACE_REDIRECT])
    if (redirect->getType() != JSON::Value::Type::EMPTY)
      a.compiled.redirect = text(p, KEY_PLACE_REDIRECT, 36);
  if (!a.compiled.redirect.empty() &&
      (!validID(a.compiled.redirect) || a.compiled.redirect == m.uuid))
    throw std::runtime_error("Invalid redirect UUID");
  if (const auto *n = p[KEY_PLACE_NUMBER]) {
    if (!n->isInt() || n->getInt() <= 0 || n->getInt() > 2147483646)
      throw std::runtime_error("Invalid place number");
    a.compiled.number = n->getInt();
  }
  if (p[KEY_PLACE_CODES]) {
    const auto &codes = object(p, KEY_PLACE_CODES);
    for (const auto &member : codes.getMembers()) strings(member.Get(), 32, false);
    if (codes[KEY_PLACE_UNLOCODE]) {
      a.compiled.codes = strings(*codes[KEY_PLACE_UNLOCODE], 32, false);
      if (m.type != "port") throw std::runtime_error("Only a port designates a UN/LOCODE");
      for (const auto &code : a.compiled.codes) {
        if (code.size() != 5) throw std::runtime_error("UN/LOCODE needs five characters");
        for (size_t i = 0; i < code.size(); ++i)
          if (!((code[i] >= 'A' && code[i] <= 'Z') ||
                (i > 1 && code[i] >= '0' && code[i] <= '9')))
            throw std::runtime_error("Invalid UN/LOCODE");
      }
      if (!a.compiled.codes.empty()) m.code = a.compiled.codes.front();
    }
  }
  m.country = optionalText(p, KEY_PLACE_COUNTRY, 2);
  if (!m.country.empty() && (m.country.size() != 2 ||
      m.country[0] < 'A' || m.country[0] > 'Z' || m.country[1] < 'A' || m.country[1] > 'Z'))
    throw std::runtime_error("Country needs two uppercase letters");
  if (m.country.empty() && !m.code.empty()) m.country = m.code.substr(0, 2);
  if (p[KEY_PLACE_ALIASES]) a.compiled.aliases = strings(*p[KEY_PLACE_ALIASES], 64, false);
  if (p[KEY_PLACE_SERVES]) a.compiled.serves = strings(*p[KEY_PLACE_SERVES], 64, true);
  if (p[KEY_PLACE_ATTRIBUTES]) {
    const auto &attrs = object(p, KEY_PLACE_ATTRIBUTES);
    m.category = optionalText(attrs, KEY_PLACE_AREA_SUBTYPE, 60);
  }
  if (m.type == "area" && m.category.empty())
    throw std::runtime_error("An area needs attributes.area_subtype");

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
  PlaceIndex::Entry e = std::move(a.compiled);
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
          compiled.xmin = MIN(compiled.xmin, p.x);
          compiled.xmax = MAX(compiled.xmax, p.x);
          compiled.ymin = MIN(compiled.ymin, p.y);
          compiled.ymax = MAX(compiled.ymax, p.y);
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
          e.xmin = MIN(e.xmin, static_cast<double>(p.x));
          e.xmax = MAX(e.xmax, static_cast<double>(p.x));
          e.ymin = MIN(e.ymin, static_cast<double>(p.y));
          e.ymax = MAX(e.ymax, static_cast<double>(p.y));
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
      ymin = MIN(ymin, p.y);
      ymax = MAX(ymax, p.y);
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
    if (member.Key() < 0) writer.key(doc.pool.extraName(member.Key()));
    else writer.key(AIS::KeyMap[member.Key()][JSON_DICT_SETTING]);
    preciseValue(member.Get(), writer, doc.pool);
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
  std::set<uint32_t> loadedNumbers;
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
      parser.setPreserveUnknown(true);
      auto doc = parser.parse(Util::Helper::readFile(path));
      const bool collection =
          text(doc.root, KEY_SETTING_MODEL_TYPE) == "FeatureCollection";
      std::vector<Place> loaded;
      std::set<std::string> fileIDs, fileCodes;
      std::set<uint32_t> fileNumbers;
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
        if (!fileIDs.insert(a.compiled.metadata->uuid).second)
          throw std::runtime_error("Duplicate place UUID");
        if (a.compiled.number && (!fileNumbers.insert(a.compiled.number).second || loadedNumbers.count(a.compiled.number)))
          throw std::runtime_error("Duplicate place number");
        if (a.compiled.redirect.empty()) for (const auto &code : a.compiled.codes)
          if (!fileCodes.insert(code).second || loadedCodes.count(code))
            throw std::runtime_error("Duplicate designated port code: " + code);
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
          preciseValue(value, w, doc.pool);
          w.finish();
          load(json);
        }
      } else
        load(Util::Helper::readFile(path));
      totalVertices += fileVertices;
      totalBytes += fileBytes;
      loadedCodes.insert(fileCodes.begin(), fileCodes.end());
      loadedIDs.insert(fileIDs.begin(), fileIDs.end());
      loadedNumbers.insert(fileNumbers.begin(), fileNumbers.end());
      for (auto &a : loaded)
        places.push_back(std::move(a));

    } catch (const std::exception &e) {
      ++skipped;
      Warning() << "Places: skipped " << file << ": " << e.what();
    }
  }
  // the port a schema 1 file named, by code, is one of the ports just loaded
  std::unordered_map<std::string, std::string> portByCode;
  for (const auto &a : places)
    if (a.compiled.redirect.empty())
      for (const auto &code : a.compiled.codes)
        portByCode.emplace(code, a.compiled.metadata->uuid);
  for (auto &a : places) {
    if (a.legacyParent.empty())
      continue;
    const auto it = portByCode.find(a.legacyParent);
    if (it == portByCode.end()) {
      Warning() << "Places: " << a.compiled.metadata->name << " names port "
                << a.legacyParent << ", which is not drawn here";
      continue;
    }
    JSON::Parser parser(JSON_DICT_SETTING);
    parser.setPreserveUnknown(true);
    auto doc = parser.parse(*a.compiled.feature);
    JSON::Value properties = *doc.root[KEY_PLACE_PROPERTIES];
    properties.getObject().Set(KEY_PLACE_PART_OF, it->second, doc.pool);
    JSON::Value root;
    root.setObject(&doc.root);
    std::string linked;
    JSON::Writer writer(linked);
    preciseValue(root, writer, doc.pool);
    writer.finish();
    auto resolved = validate(linked);
    a.compiled = std::move(resolved.compiled);
    a.legacyParent.clear();
  }
  std::sort(places.begin(), places.end(), [](const Place &a, const Place &b) {
    return a.compiled.metadata->uuid < b.compiled.metadata->uuid;
  });
  for (auto &entry : places)
    entry.number = static_cast<uint32_t>(nextNumber++);
  if (!places.empty()) check(places.front(), true);
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
    if (old != places.end() && old->compiled.number &&
        a.compiled.number != old->compiled.number)
      throw std::runtime_error("A place keeps its number");
    if (!remove && !a.compiled.number) {
      // a place gets its number the first time it is saved, and keeps it
      uint32_t number = 0;
      for (const auto &p : places)
        number = MAX(number, p.compiled.number);
      if (number >= 2147483646)
        throw std::runtime_error("Place number space exhausted");
      JSON::Parser parser(JSON_DICT_SETTING);
      parser.setPreserveUnknown(true);
      auto doc = parser.parse(*a.compiled.feature);
      JSON::Value no;
      no.setInt(number + 1);
      JSON::Value properties = *doc.root[KEY_PLACE_PROPERTIES];
      properties.getObject().Set(KEY_PLACE_NUMBER, no);
      JSON::Value root;
      root.setObject(&doc.root);
      std::string numbered;
      JSON::Writer writer(numbered);
      preciseValue(root, writer, doc.pool);
      writer.finish();
      a = validate(numbered, true);
    }
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
    if (candidate.compiled.number && candidate.compiled.number == p.compiled.number)
      throw std::runtime_error("Duplicate place number");
    if (candidate.compiled.redirect.empty() && p.compiled.redirect.empty())
      for (const auto &code : candidate.compiled.codes)
        if (std::find(p.compiled.codes.begin(), p.compiled.codes.end(), code) != p.compiled.codes.end())
          throw std::runtime_error("Duplicate designated port code: " + code);
    count(p);
  }
  std::unordered_map<std::string, const PlaceIndex::Entry *> links;
  for (const auto &p : places) links[p.compiled.metadata->uuid] = &p.compiled;
  links[candidate.compiled.metadata->uuid] = &candidate.compiled;
  for (const auto &start : links) {
    for (int relation = 0; relation < 3; ++relation) {
      std::set<std::string> seen;
      auto current = start.second;
      while (current) {
        if (!seen.insert(current->metadata->uuid).second)
          throw std::runtime_error("Place relationship cycle");
        const auto &next = relation == 1 || (relation == 2 && !current->redirect.empty()) ? current->redirect : current->metadata->partOf;
        auto it = links.find(next);
        current = it == links.end() ? nullptr : it->second;
      }
    }
  }
  for (const auto &record : links) for (const auto &uuid : record.second->serves) {
    auto it = links.find(uuid);
    if (uuid == record.first ||
        (it != links.end() && it->second->metadata->type != "port"))
      throw std::runtime_error("Serves must reference ports");
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
  static std::atomic<uint32_t> serial{0};
  next->version = std::to_string(epoch) + "-" + std::to_string(++serial);
  next->entries.resize(nextNumber);
  for (const auto &pair : places) {
    const auto &a = pair;
    auto e = a.compiled;
    e.id = a.number;
    next->entries[a.number] = std::move(e);
  }
  for (auto &e : next->entries) {
    if (e.id == UINT32_MAX) continue;
    next->byUUID.emplace(e.metadata->uuid, e.id);
    if (!e.redirect.empty()) continue;
    if (!e.polygons->empty()) next->polygonCandidates.push_back(e.id);
    for (const auto &code : e.codes) next->byCode.emplace(PlaceIndex::normalized(code), e.id);
    if (e.metadata->type == "port") {
      next->byName[PlaceIndex::normalized(e.metadata->name)].push_back(e.id);
      for (const auto &alias : e.aliases) {
        auto &ids = next->byName[PlaceIndex::normalized(alias)];
        if (std::find(ids.begin(), ids.end(), e.id) == ids.end()) ids.push_back(e.id);
      }
    }
  }
  for (auto &e : next->entries) {
    if (!e.metadata)
      continue;
    const auto it = next->byUUID.find(e.redirect.empty() ? e.metadata->partOf
                                                         : e.redirect);
    if (it != next->byUUID.end())
      e.parent = it->second;
  }
  // a place within a port appears on the map when its port does: it takes the
  // port's size class, so every writer derives the same zoom for both. A port
  // that is part of no other port is the one a call is counted at.
  for (auto &e : next->entries) {
    if (!e.metadata)
      continue;
    const bool port = e.metadata->type == "port";
    e.rootPort = port;
    for (auto ancestor = next->find(e.parent); ancestor;
         ancestor = next->find(ancestor->parent))
      if (ancestor->metadata->type == "port") {
        if (port)
          e.rootPort = false;
        else if (e.metadata->type != "area")
          e.markerSize = ancestor->markerSize;
        break;
      }
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
  std::copy_n(all.begin(), MIN(all.size(), ids.size()), ids.begin());
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
      .kv("part_of", metadata->partOf)
      .kv("no", number)
      .kv("redirect_to", redirect)
      .kv("has_geometry", !polygons->empty());
  // every place appears from the zoom of its size class: a port by its own, an
  // anchorage by its port's, an area by the size it was given; a terminal
  // and a berth only close in, where there is room for them
  static const int zooms[] = {12, 11, 9, 7};
  const int close = closeZoom();
  w.kv("z", MAX(close ? close : zooms[markerSize], minZoom));
  if (metadata->type == "port")
    w.kv("country", metadata->country);
  w.kv("revision", revision)
      .kv("category", metadata->category)
      .kv("size", markerSize)
      .endObject();
}

int PlaceIndex::Entry::closeZoom() const {
  if (metadata->type == "berth")
    return BERTH_ZOOM;
  if (metadata->type == "terminal" || metadata->type == "mooring")
    return TERMINAL_ZOOM;
  return 0;
}

bool PlaceIndex::belongsTo(uint32_t child, uint32_t ancestor) const {
  for (size_t n = 0; n < entries.size(); ++n) {
    if (child == ancestor) return true;
    const auto *p = find(child);
    if (!p || p->parent == UINT32_MAX) return false;
    child = p->parent;
  }
  return false;
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
