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

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "PlaceGeometry.h"

namespace JSON {
class Writer;
class JSON;
class Pool;
struct Document;
} // namespace JSON
// Place types in reading order: most specific first. From the port up a
// crossing is announced; from the guard zone up nothing stops.
enum class PlaceKind : uint8_t {
  Berth,
  Mooring,
  Terminal,
  Marina,
  Anchorage,
  Section,
  Port,
  GuardZone,
  Sector,
  Water
};
struct PlaceMetadata {
  std::string uuid, name, code, partOf, category, country;
  PlaceKind kind = PlaceKind::GuardZone;
  static const char *kindName(PlaceKind k) {
    static const char *const names[] = {
        "berth", "mooring", "terminal",  "marina", "anchorage",
        "section", "port",  "guardzone", "sector", "water"};
    return names[int(k)];
  }
  static bool parseKind(const std::string &type, PlaceKind &kind) {
    for (int i = 0; i <= int(PlaceKind::Water); ++i)
      if (type == kindName(PlaceKind(i))) {
        kind = PlaceKind(i);
        return true;
      }
    return false;
  }
  const char *type() const { return kindName(kind); }
  bool is(PlaceKind k) const { return kind == k; }
  bool requiresStop() const { return kind < PlaceKind::GuardZone; }
  bool announced() const { return kind >= PlaceKind::Port; }
  int rank() const { return int(kind); }
};
struct PlaceIndex {
  using Part = PlacePart;
  struct Entry {
    uint32_t id = UINT32_MAX;
    std::shared_ptr<const std::vector<Part>>
        polygons; // ordered by minimum longitude
    std::shared_ptr<const PlaceMetadata> metadata;
    std::shared_ptr<const std::string> feature;
    double xmin = 180, xmax = -180, ymin = 90, ymax = -90, size = 0;
    double lat = 0, lon = 0;
    int markerSize = 0;
    bool ownCountry = false;
    long revision = 0;
    uint32_t number = 0, parent = UINT32_MAX;
    std::string redirect;
    std::vector<std::string> codes, aliases;
    void writeSummary(JSON::Writer &, uint64_t sequence,
                      int minZoom = -1) const;
    // Detail classes have their own minimum zoom; other places use their size.
    static const int TERMINAL_ZOOM = 12, SECTION_ZOOM = 13, BERTH_ZOOM = 14;
    int closeZoom() const;
    // a place drawn here, not an empty slot or a redirect to another place
    bool live() const { return id != UINT32_MAX && redirect.empty(); }
  };
  std::string version;
  std::vector<Entry> entries;              // runtime ID order
  std::vector<uint32_t> polygonCandidates; // offsets, smallest polygon first
  std::unordered_map<std::string, uint32_t> byCode, byUUID;
  std::unordered_map<std::string, std::vector<uint32_t>> byName;
  // Deleted records leave empty slots until a directory reload.
  const Entry *find(uint32_t id) const {
    return id < entries.size() && entries[id].id != UINT32_MAX ? &entries[id]
                                                               : nullptr;
  }
  // Candidate selection is separate from exact geometry. An indexed host can
  // replace this selection without duplicating ring or hole semantics.
  template <class F> void candidates(double lat, double lon, F visit) const {
    for (auto offset : polygonCandidates) {
      const auto &entry = entries[offset];
      if (lon >= entry.xmin && lon <= entry.xmax && lat >= entry.ymin &&
          lat <= entry.ymax)
        visit(entry);
    }
  }
  // the place a UUID names now, following redirects; nullptr when gone
  const Entry *resolve(const std::string &uuid) const;
  static std::string normalized(const std::string &text);
  const Entry *matchDestination(const std::string &text) const;
  const Entry *findPort(const std::string &code) const;
  // every place holding the position, smallest first, into ids
  void matchAll(double lat, double lon, std::vector<uint32_t> &ids) const;
};

// Installation-owned place definitions. Geometry is standard GeoJSON; editing
// permissions belong to the control API, not to the map reader.
class PlaceCatalogue {
  struct Place {
    std::string file;
    bool collectionFile = false;
    uint32_t number = UINT32_MAX;
    long revision = 0;
    PlaceIndex::Entry compiled;
  };
  std::string directory;
  std::vector<Place> places;
  // what the places register: for the rules a place must keep with the rest
  std::unordered_map<std::string, size_t> byUUID;       // offset in places
  std::unordered_map<uint32_t, std::string> numbers;    // number -> uuid
  std::unordered_map<std::string, std::string> codes;   // code -> uuid
  size_t vertices = 0, bytes = 0;
  std::mutex mutex;
  std::shared_ptr<const PlaceIndex> index;
  std::string collectionJSON; // the collection as served, built once per index
  uint32_t nextNumber = 0;
  size_t skipped = 0;
  std::atomic<bool> changed{
      false}; // set by save(), consumed by the viewer that serves the store
  void rebuild();
  void check(const Place &candidate, const Place *old) const;
  void account(const Place &p, bool add);
  void insert(Place a, const Place *old);
  void erase(const std::string &uuid);
  static JSON::Document parse(const std::string &json);
  static Place validate(JSON::JSON &root, JSON::Pool &pool,
                        bool saving = false);

public:
  explicit PlaceCatalogue(const std::string &directory);
  std::string collection();
  std::string feature(uint32_t id, const std::string &version);
  void writeStatus(JSON::Writer &writer);
  std::shared_ptr<const PlaceIndex> snapshot();
  bool consumeChanged() { return changed.exchange(false); }
  bool save(const std::string &json, bool remove, std::string &error);
};
