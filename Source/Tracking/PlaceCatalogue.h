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
#include <cmath>
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
    // The place as GeoJSON is read from its file when asked for, not kept: the
    // file, and the size and hash of the text served, which a file changed since
    // no longer matches. A place whose text is not its file's whole text (one of
    // a collection, or one read as an older schema) keeps that text here instead.
    std::shared_ptr<const std::string> path, text;
    uint64_t hash = 0;
    size_t bytes = 0;
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
    // the place's GeoJSON as loaded or saved; false when its file changed since
    bool readFeature(std::string &out) const;
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
  // Candidate selection is separate from exact geometry. A one-degree grid lists
  // per cell the places whose bounding box reaches into it; a place spanning
  // more cells than GRID_WIDE_CELLS is checked on every lookup instead. Both
  // lists hold ranks in polygonCandidates, so merging them keeps the smallest
  // place first. Without a grid every place is checked: a catalogue smaller
  // than GRID_MIN_PLACES goes without one, as the cell index alone takes 259 KB.
  // A place is listed in the cells its PARTS reach into, not its whole bounding
  // box: an ocean cut into tiles is then a handful of cells per tile, never wide.
  enum { GRID_COLS = 360, GRID_ROWS = 180, GRID_WIDE_CELLS = 256, GRID_MIN_PLACES = 256 };
  // A cell a part covers entirely is a hit for every position in it: the grid
  // says so with this bit on the rank, and the ring is never walked.
  enum : uint32_t { GRID_FULL = 0x80000000u };
  std::vector<uint32_t> gridStart; // a cell's first rank in gridRanks
  std::vector<uint32_t> gridRanks;
  std::vector<uint32_t> gridWide;
  void buildGrid();
  static int gridCol(double lon) {
    const int c = (int)std::floor(lon + 180);
    return c < 0 ? 0 : c >= GRID_COLS ? GRID_COLS - 1 : c;
  }
  static int gridRow(double lat) {
    const int r = (int)std::floor(lat + 90);
    return r < 0 ? 0 : r >= GRID_ROWS ? GRID_ROWS - 1 : r;
  }
  template <class F> void candidates(double lat, double lon, F visit) const {
    const auto test = [&](uint32_t offset, bool full) {
      const auto &entry = entries[offset];
      if (full || (lon >= entry.xmin && lon <= entry.xmax && lat >= entry.ymin &&
                   lat <= entry.ymax))
        visit(entry, full);
    };
    if (gridStart.empty()) {
      for (auto offset : polygonCandidates)
        test(offset, false);
      return;
    }
    if (!(lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180))
      return;
    const size_t cell = (size_t)gridRow(lat) * GRID_COLS + gridCol(lon);
    uint32_t i = gridStart[cell];
    const uint32_t end = gridStart[cell + 1];
    size_t w = 0;
    while (i < end || w < gridWide.size()) {
      const bool fromCell =
          w == gridWide.size() || (i < end && (gridRanks[i] & ~GRID_FULL) < gridWide[w]);
      const uint32_t entry = fromCell ? gridRanks[i++] : gridWide[w++];
      test(polygonCandidates[entry & ~GRID_FULL], fromCell && (entry & GRID_FULL));
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
    std::string parentCode; // a schema 1 parent named by UN/LOCODE, until resolved
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
  std::shared_ptr<const std::string> collectionJSON; // the collection as served, built once per index
  uint32_t nextNumber = 0;
  size_t skipped = 0;
  bool drifted = false; // a file changed on disk since it was loaded, said once
  std::atomic<bool> changed{
      false}; // set by save(), consumed by the viewer that serves the store
  void rebuild();
  void check(const Place &candidate, const Place *old) const;
  void account(const Place &p, bool add);
  void insert(Place a, const Place *old);
  void erase(const std::string &uuid);
  static JSON::Document parse(const std::string &json);
  static Place validate(JSON::JSON &root, JSON::Pool &pool,
                        bool saving = false, bool keepText = false);

public:
  explicit PlaceCatalogue(const std::string &directory);
  std::shared_ptr<const std::string> collection();
  std::string feature(uint32_t id, const std::string &version);
  void writeStatus(JSON::Writer &writer);
  std::shared_ptr<const PlaceIndex> snapshot();
  bool consumeChanged() { return changed.exchange(false); }
  bool save(const std::string &json, bool remove, std::string &error);
};
