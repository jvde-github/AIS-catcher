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
#ifdef HASPORTS
#include "libport.h"
#endif
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

using namespace PlaceGeometry;
namespace {
using namespace AIS;
// Place data is written back as it was read: unknown keys by name, numbers
// exactly, so editing never moves or collapses a vertex.
JSON::Serializer faithful(const JSON::Pool &pool) {
  JSON::Serializer out(JSON_DICT_SETTING);
  out.setPool(&pool);
  out.setExactFloats(true);
  return out;
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
const std::vector<JSON::Value> &array(const JSON::Value &v) {
  if (!v.isArray())
    throw std::runtime_error("Expected array");
  return v.getArray();
}
long integer(const JSON::JSON &o, int key, long min, long max) {
  const auto &v = field(o, key);
  if (!v.isInt() || v.getInt() < min || v.getInt() > max)
    throw std::runtime_error(
        "Invalid " + std::string(AIS::KeyMap[key][JSON_DICT_SETTING].p));
  return v.getInt();
}
const JSON::JSON &object(const JSON::JSON &o, int key) {
  const auto &v = field(o, key);
  if (!v.isObject())
    throw std::runtime_error("Expected place object");
  return v.getObject();
}
const size_t BUDGET_VERTICES = 4000000, BUDGET_BYTES = 268435456;
const size_t FEATURE_BYTES = 524288;
void footprint(const PlaceIndex::Entry &e, size_t &vertices, size_t &bytes) {
  if (e.polygons->empty())
    return;
  bytes += e.feature->size();
  for (const auto &part : *e.polygons)
    for (const auto &ring : part.rings) {
      vertices += ring.size();
      bytes += ring.size() * sizeof(PlacePoint);
    }
}
bool overBudget(size_t vertices, size_t bytes) {
  return vertices > BUDGET_VERTICES || bytes > BUDGET_BYTES;
}
} // namespace

JSON::Document PlaceCatalogue::parse(const std::string &json) {
  if (json.size() > FEATURE_BYTES)
    throw std::runtime_error("Place file exceeds 512 KiB");
  JSON::Parser parser(JSON_DICT_SETTING);
  parser.setPreserveUnknown(true);
  return parser.parse(json);
}

// Validates and compiles one parsed feature.
PlaceCatalogue::Place PlaceCatalogue::validate(JSON::JSON &root,
                                               JSON::Pool &pool, bool saving) {
  Place a;
  auto metadata = std::make_shared<PlaceMetadata>();
  auto &m = *metadata;
  if (text(root, KEY_SETTING_MODEL_TYPE) != "Feature")
    throw std::runtime_error("Expected one GeoJSON Feature");
  m.uuid = text(root, KEY_SETTING_ID);
  if (!Util::Helper::isUUID(m.uuid, true))
    throw std::runtime_error("Place ID must be a lowercase UUID");
  const auto &p = object(root, KEY_PLACE_PROPERTIES);
  if (integer(p, KEY_PLACE_SCHEMA_VERSION, 0, 2147483646) != 3)
    throw std::runtime_error("Unsupported place schema version");
  a.revision = integer(p, KEY_PLACE_REVISION, 0, 2147483646);
  m.name = text(p, KEY_PLACE_NAME);
  if (!PlaceMetadata::parseKind(text(p, KEY_PLACE_PLACE_TYPE), m.kind))
    throw std::runtime_error("Unknown place type");
  auto optionalText = [&](const JSON::JSON &obj, int key, size_t limit) {
    return obj[key] ? text(obj, key, limit) : std::string();
  };
  auto strings = [&](const JSON::Value &value, size_t limit) {
    std::vector<std::string> result;
    for (const auto &v : array(value)) {
      if (!v.isString() || v.getString().empty() ||
          v.getString().size() > 120 || result.size() >= limit)
        throw std::runtime_error("Invalid place list");
      if (std::find(result.begin(), result.end(), v.getString()) !=
          result.end())
        throw std::runtime_error("Duplicate place list value");
      result.push_back(v.getString());
    }
    return result;
  };
  if (p[KEY_PLACE_PORT_UNLOCODE] || p[KEY_PORT_PARENT] ||
      p[KEY_PLACE_UNLOCODE] || p[KEY_PORT_SIZE])
    throw std::runtime_error("Use codes, part_of and rank in schema 3");
  for (const auto &member : p.getMembers())
    if (member.Key() < 0 && pool.extraName(member.Key()) == "details")
      throw std::runtime_error("Migrate details to schema 2 properties");
  if (const auto *parent = p[KEY_PLACE_PART_OF]) {
    if (parent->getType() != JSON::Value::Type::EMPTY) {
      m.partOf = text(p, KEY_PLACE_PART_OF, 36);
      if (!Util::Helper::isUUID(m.partOf, true) || m.partOf == m.uuid)
        throw std::runtime_error("Invalid parent UUID");
    }
  }
  if (const auto *redirect = p[KEY_PLACE_REDIRECT])
    if (redirect->getType() != JSON::Value::Type::EMPTY)
      a.compiled.redirect = text(p, KEY_PLACE_REDIRECT, 36);
  if (!a.compiled.redirect.empty() &&
      (!Util::Helper::isUUID(a.compiled.redirect, true) || a.compiled.redirect == m.uuid))
    throw std::runtime_error("Invalid redirect UUID");
  if (p[KEY_PLACE_NUMBER])
    a.compiled.number = integer(p, KEY_PLACE_NUMBER, 1, 2147483646);
  if (p[KEY_PLACE_CODES]) {
    const auto &codes = object(p, KEY_PLACE_CODES);
    for (const auto &member : codes.getMembers())
      strings(member.Get(), 32);
    if (codes[KEY_PLACE_UNLOCODE]) {
      a.compiled.codes = strings(*codes[KEY_PLACE_UNLOCODE], 32);
      if (!m.is(PlaceKind::Port))
        throw std::runtime_error("Only a port designates a UN/LOCODE");
      for (const auto &code : a.compiled.codes) {
        if (code.size() != 5)
          throw std::runtime_error("UN/LOCODE needs five characters");
        for (size_t i = 0; i < code.size(); ++i)
          if (!((code[i] >= 'A' && code[i] <= 'Z') ||
                (i > 1 && code[i] >= '0' && code[i] <= '9')))
            throw std::runtime_error("Invalid UN/LOCODE");
      }
      if (!a.compiled.codes.empty())
        m.code = a.compiled.codes.front();
    }
  }
  m.country = optionalText(p, KEY_PLACE_COUNTRY, 2);
  if (!m.country.empty() &&
      (m.country.size() != 2 || m.country[0] < 'A' || m.country[0] > 'Z' ||
       m.country[1] < 'A' || m.country[1] > 'Z'))
    throw std::runtime_error("Country needs two uppercase letters");
  if (m.country.empty() && !m.code.empty())
    m.country = m.code.substr(0, 2);
  a.compiled.ownCountry = !m.country.empty();
  if (p[KEY_PLACE_ALIASES])
    a.compiled.aliases = strings(*p[KEY_PLACE_ALIASES], 64);
  m.category = optionalText(p, KEY_PLACE_CATEGORY, 60);

  const auto &g = object(root, KEY_PLACE_GEOMETRY);
  const auto shape =
      PlaceGeometry::compile(text(g, KEY_SETTING_MODEL_TYPE),
                             field(g, KEY_PLACE_COORDINATES), m.is(PlaceKind::Water));
  a.compiled.polygons = shape.parts;
  a.compiled.xmin = shape.xmin;
  a.compiled.xmax = shape.xmax;
  a.compiled.ymin = shape.ymin;
  a.compiled.ymax = shape.ymax;
  a.compiled.size = shape.size;
  a.compiled.lat = shape.lat;
  a.compiled.lon = shape.lon;
  a.compiled.metadata = metadata;
  a.compiled.revision = a.revision + (saving ? 1 : 0);
  if (p[KEY_PLACE_RANK])
    a.compiled.markerSize = integer(p, KEY_PLACE_RANK, 0, 3);
  if (saving) {
    JSON::Value props = *root[KEY_PLACE_PROPERTIES], revision;
    revision.setInt(a.compiled.revision);
    props.getObject().Set(KEY_PLACE_REVISION, revision);
  }
  std::string canonical;
  JSON::Writer writer(canonical);
  faithful(pool).stringify(root, writer);
  writer.finish();
  if (canonical.size() > FEATURE_BYTES)
    throw std::runtime_error("Place file exceeds 512 KiB");
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
  for (const auto &file : Util::Helper::getFilesInDirectory(directory)) {
    if (file.size() < 9 || file.substr(file.size() - 8) != ".geojson")
      continue;
    std::vector<std::string> added;
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
      const auto load = [&](JSON::JSON &feature) {
        auto a = validate(feature, doc.pool);
        a.file = file;
        a.collectionFile = collection;
        if (a.revision < 1)
          throw std::runtime_error("Invalid saved revision");
        check(a, nullptr);
        added.push_back(a.compiled.metadata->uuid);
        insert(std::move(a), nullptr);
      };
      if (collection) {
        for (auto value : array(field(doc.root, KEY_PLACE_FEATURES))) {
          if (!value.isObject())
            throw std::runtime_error("Expected place object");
          load(value.getObject());
        }
      } else
        load(doc.root);
    } catch (const std::exception &e) {
      // a file is taken whole or not at all
      for (const auto &uuid : added)
        erase(uuid);
      ++skipped;
      Warning() << "Places: skipped " << file << ": " << e.what();
    }
  }
  std::sort(places.begin(), places.end(), [](const Place &a, const Place &b) {
    return a.compiled.metadata->uuid < b.compiled.metadata->uuid;
  });
  for (auto &entry : places) {
    byUUID[entry.compiled.metadata->uuid] = entry.number = nextNumber++;
  }
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
    auto doc = parse(json);
    // a place gets its number the first time it is saved, and keeps it
    const auto *properties = doc.root[KEY_PLACE_PROPERTIES];
    if (!remove && properties && properties->isObject() &&
        !properties->getObject()[KEY_PLACE_NUMBER]) {
      uint32_t number = 0;
      for (const auto &p : places)
        number = MAX(number, p.compiled.number);
      if (number >= 2147483646)
        throw std::runtime_error("Place number space exhausted");
      JSON::Value no, props = *properties;
      no.setInt(number + 1);
      props.getObject().Set(KEY_PLACE_NUMBER, no);
    }
    auto a = validate(doc.root, doc.pool, !remove);
    const std::string uuid = a.compiled.metadata->uuid;
    const auto found = byUUID.find(uuid);
    const Place *old = found == byUUID.end() ? nullptr : &places[found->second];
    if (a.revision != (old ? old->revision : 0) || (remove && !old))
      throw std::runtime_error(
          "Place changed elsewhere. Reload before saving or deleting.");
    if (old && old->compiled.number &&
        a.compiled.number != old->compiled.number)
      throw std::runtime_error("A place keeps its number");
    a.file = old ? old->file : uuid + ".geojson";
    a.collectionFile = old && old->collectionFile;
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
      erase(uuid);
    } else {
      check(a, old);
      if (!old && nextNumber >= UINT32_MAX)
        throw std::runtime_error(
            "Place ID space exhausted; restart to reload IDs");
      a.number = old ? old->number : static_cast<uint32_t>(nextNumber);
      a.revision = a.compiled.revision;
      if (!write(a, false))
        return false;
      if (!old)
        ++nextNumber;
      insert(std::move(a), old);
    }
    rebuild();
    changed = true;
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}

// The rules a place keeps with the rest: its own UUID, number and codes; no
// cycle through part_of or redirect; the budget. `old` is the place it
// replaces.
void PlaceCatalogue::check(const Place &candidate, const Place *old) const {
  const auto &c = candidate.compiled;
  const std::string &uuid = c.metadata->uuid;
  if (!old && byUUID.count(uuid))
    throw std::runtime_error("Duplicate place UUID: " + uuid);
  if (c.number) {
    const auto it = numbers.find(c.number);
    if (it != numbers.end() && it->second != uuid)
      throw std::runtime_error("Duplicate place number");
  }
  if (c.redirect.empty())
    for (const auto &code : c.codes) {
      const auto it = codes.find(code);
      if (it != codes.end() && it->second != uuid)
        throw std::runtime_error("Duplicate designated port code: " + code);
    }
  const auto at = [&](const std::string &id) -> const PlaceIndex::Entry * {
    const auto it = byUUID.find(id);
    return it == byUUID.end() ? nullptr : &places[it->second].compiled;
  };
  // the catalogue holds no cycle, so a new one must pass through here
  for (int relation = 0; relation < 3; ++relation)
    for (const auto *e = &c; e;) {
      const auto &next =
          relation == 1 || (relation == 2 && !e->redirect.empty())
              ? e->redirect
              : e->metadata->partOf;
      if (next == uuid)
        throw std::runtime_error("Place relationship cycle");
      e = at(next);
    }
  size_t v = 0, b = 0, ov = 0, ob = 0;
  footprint(c, v, b);
  if (old)
    footprint(old->compiled, ov, ob);
  if (overBudget(vertices - ov + v, bytes - ob + b))
    throw std::runtime_error("Place polygon budget exceeded");
}

// What a place registers in the catalogue, added or taken away.
void PlaceCatalogue::account(const Place &p, bool add) {
  const auto &c = p.compiled;
  size_t v = 0, b = 0;
  footprint(c, v, b);
  vertices += add ? v : -v;
  bytes += add ? b : -b;
  if (c.number) {
    if (add)
      numbers[c.number] = c.metadata->uuid;
    else
      numbers.erase(c.number);
  }
  if (c.redirect.empty())
    for (const auto &code : c.codes) {
      if (add)
        codes[code] = c.metadata->uuid;
      else
        codes.erase(code);
    }
}

// A checked place joins, in the slot of the one it replaces.
void PlaceCatalogue::insert(Place a, const Place *old) {
  const std::string uuid = a.compiled.metadata->uuid;
  if (old)
    account(*old, false);
  account(a, true);
  if (old)
    places[byUUID.at(uuid)] = std::move(a);
  else {
    byUUID[uuid] = places.size();
    places.push_back(std::move(a));
  }
}

void PlaceCatalogue::erase(const std::string &uuid) {
  const size_t at = byUUID.at(uuid);
  account(places[at], false);
  places.erase(places.begin() + at);
  byUUID.clear();
  for (size_t i = 0; i < places.size(); ++i)
    byUUID[places[i].compiled.metadata->uuid] = i;
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
  std::vector<Place *> owner(nextNumber, nullptr);
  for (auto &a : places) {
    auto e = a.compiled;
    e.id = a.number;
    next->entries[a.number] = std::move(e);
    owner[a.number] = &a;
  }
  for (auto &e : next->entries) {
    if (e.id == UINT32_MAX)
      continue;
    next->byUUID.emplace(e.metadata->uuid, e.id);
    if (!e.live())
      continue;
    if (!e.polygons->empty())
      next->polygonCandidates.push_back(e.id);
    for (const auto &code : e.codes)
      next->byCode.emplace(PlaceIndex::normalized(code), e.id);
    if (e.metadata->is(PlaceKind::Port)) {
      next->byName[PlaceIndex::normalized(e.metadata->name)].push_back(e.id);
      for (const auto &alias : e.aliases) {
        auto &ids = next->byName[PlaceIndex::normalized(alias)];
        if (std::find(ids.begin(), ids.end(), e.id) == ids.end())
          ids.push_back(e.id);
      }
    }
  }
  for (auto &e : next->entries) {
    if (e.id == UINT32_MAX)
      continue;
    const auto it =
        next->byUUID.find(e.redirect.empty() ? e.metadata->partOf : e.redirect);
    if (it != next->byUUID.end())
      e.parent = it->second;
  }
  // a place without a country of its own takes the nearest one above it
  for (auto &e : next->entries) {
    if (e.id == UINT32_MAX)
      continue;
    std::string country = e.ownCountry ? e.metadata->country : std::string();
    for (auto ancestor = next->find(e.parent); ancestor && country.empty();
         ancestor = next->find(ancestor->parent))
      if (ancestor->ownCountry)
        country = ancestor->metadata->country;
    // stored back, so later rebuilds copy only on a change
    if (country != e.metadata->country) {
      auto metadata = std::make_shared<PlaceMetadata>(*e.metadata);
      metadata->country = country;
      e.metadata = owner[e.id]->compiled.metadata = std::move(metadata);
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

void PlaceIndex::matchAll(double lat, double lon,
                          std::vector<uint32_t> &ids) const {
  ids.clear();
  if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 ||
      lon < -180 || lon > 180)
    return;
  lat = static_cast<float>(lat);
  lon = static_cast<float>(lon);
  candidates(lat, lon, [&](const Entry &a) {
    for (const auto &part : *a.polygons) {
      if (part.xmin > lon)
        break;
      if (lon > part.xmax || lat < part.ymin || lat > part.ymax)
        continue;
      if (!contains(part, lon, lat))
        continue;
      ids.push_back(a.id);
      break;
    }
  });
}

void PlaceCatalogue::writeStatus(JSON::Writer &writer) {
  std::lock_guard<std::mutex> lock(mutex);
  writer.key("places")
      .beginObject()
      .kv("loaded", (long long)places.size())
      .kv("skipped", (long long)skipped)
      .kv("vertices", (long long)vertices)
      .kv("bytes", (long long)bytes)
      .endObject();
}

void PlaceIndex::Entry::writeSummary(JSON::Writer &w, uint64_t sequence,
                                     int minZoom) const {
  w.beginObject()
      .kv("id", "place-" + std::to_string(id))
      .kv("kind", 10)
      .kv("seq", (long long)sequence)
      .kv("runtime_id", id)
      .kv("uuid", metadata->uuid)
      .kv("lat", lat)
      .kv("lon", lon)
      .kv("label", metadata->name)
      .kv("place_type", metadata->type())
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
  if (!metadata->country.empty())
    w.kv("country", metadata->country);
  w.kv("revision", revision)
      .kv("category", metadata->category)
      .kv("rank", markerSize)
      .endObject();
}

int PlaceIndex::Entry::closeZoom() const {
  switch (metadata->kind) {
  case PlaceKind::Section:
    return SECTION_ZOOM;
  case PlaceKind::Berth:
    return BERTH_ZOOM;
  case PlaceKind::Terminal:
  case PlaceKind::Mooring:
    return TERMINAL_ZOOM;
  default:
    return 0;
  }
}

const PlaceIndex::Entry *PlaceIndex::resolve(const std::string &uuid) const {
  auto it = byUUID.find(uuid);
  const Entry *e = it == byUUID.end() ? nullptr : find(it->second);
  // the catalogue refuses redirect cycles; the bound only guards a bad index
  for (size_t n = 0; e && !e->redirect.empty() && n < entries.size(); ++n) {
    it = byUUID.find(e->redirect);
    e = it == byUUID.end() ? nullptr : find(it->second);
  }
  return e && e->redirect.empty() ? e : nullptr;
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
  if (it != byCode.end())
    return find(it->second);
#ifdef HASPORTS
  libport::Info port;
  if (libport::lookup(code.c_str(), port)) {
    const auto canonical = byCode.find(port.code);
    if (canonical != byCode.end())
      return find(canonical->second);
  }
#endif
  return nullptr;
}
