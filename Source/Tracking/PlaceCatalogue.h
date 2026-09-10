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

namespace JSON {
class Writer;
}
struct PlacePoint {
  float x, y;
};
struct PlaceMetadata {
  std::string uuid, name, type, code, parentCode, category;
};
struct PlaceIndex {
  struct Part {
    std::vector<std::vector<PlacePoint>> rings;
    float xmin = 180, xmax = -180, ymin = 90, ymax = -90;
  };
  struct Entry {
    uint32_t id = UINT32_MAX;
    std::shared_ptr<const std::vector<Part>>
        polygons; // ordered by minimum longitude
    std::shared_ptr<const PlaceMetadata> metadata;
    std::shared_ptr<const std::string> feature;
    double xmin = 180, xmax = -180, ymin = 90, ymax = -90, size = 0;
    double lat = 0, lon = 0;
    int markerSize = 0;
    long revision = 0;
    void writeSummary(JSON::Writer &, uint64_t sequence, int minZoom = -1) const;
  };
  std::string version;
  std::vector<Entry> entries;              // runtime ID order
  std::vector<uint32_t> polygonCandidates; // offsets, smallest polygon first
  std::unordered_map<std::string, uint32_t> byCode;
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
  static std::string normalized(const std::string &text);
  const Entry *matchDestination(const std::string &text) const;
  const Entry *findPort(const std::string &code) const;
  void match(double lat, double lon, std::array<uint32_t, 5> &ids) const;
  std::vector<uint32_t> matchAll(double lat, double lon) const;
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
  std::mutex mutex;
  std::shared_ptr<const PlaceIndex> index;
  std::string collectionJSON; // the collection as served, built once per index
  uint32_t nextNumber = 0;
  size_t skipped = 0;
  std::atomic<bool> changed{
      false}; // set by save(), consumed by the viewer that serves the store
  void rebuild();
  void check(const Place &candidate, bool replacing) const;
  static Place validate(const std::string &json, bool saving = false);

public:
  explicit PlaceCatalogue(const std::string &directory);
  std::string collection();
  std::string feature(uint32_t id, const std::string &version);
  void writeStatus(JSON::Writer &writer);
  std::shared_ptr<const PlaceIndex> snapshot();
  bool consumeChanged() { return changed.exchange(false); }
  bool save(const std::string &json, bool remove, std::string &error);
};
