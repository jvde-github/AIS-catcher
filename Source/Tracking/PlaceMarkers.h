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

#include "PlaceCatalogue.h"
#include "Writer.h"
#include <algorithm>

// Owns the place marker feed and its immutable catalogue snapshot. The DB
// supplies the shared feed sequence and handles ships in changed polygon
// bounds.
class PlaceMarkers {
  std::shared_ptr<const PlaceIndex> snapshot;
  std::vector<uint64_t> sequences;
  std::vector<std::pair<uint32_t, uint64_t>> removed;
  uint64_t latest = 0; // newest row or removal; an older cursor skips the walk

public:
  using Bounds = std::array<double, 4>;
  const std::shared_ptr<const PlaceIndex> &index() const { return snapshot; }
  const std::string &version() const {
    static const std::string none;
    return snapshot ? snapshot->version : none;
  }
  std::vector<uint32_t> containing(double lat, double lon) const {
    return snapshot ? snapshot->matchAll(lat, lon) : std::vector<uint32_t>();
  }

  std::vector<Bounds> adopt(std::shared_ptr<const PlaceIndex> next,
                            uint64_t &sequence) {
    std::vector<Bounds> changed;
    if (next == snapshot)
      return changed;
    const uint64_t start = sequence;
    auto bounds = [&](const PlaceIndex::Entry &entry) {
      if (entry.polygons && !entry.polygons->empty())
        changed.push_back({{entry.xmin, entry.xmax, entry.ymin, entry.ymax}});
    };
    if (snapshot)
      for (const auto &old : snapshot->entries) {
        if (old.id == UINT32_MAX)
          continue;
        const auto *replacement = next ? next->find(old.id) : nullptr;
        const bool same =
            replacement && replacement->metadata->uuid == old.metadata->uuid;
        if (!same)
          removed.emplace_back(old.id, ++sequence);
        if (!same || old.polygons != replacement->polygons)
          bounds(old);
      }
    if (removed.size() > 256)
      removed.erase(removed.begin(), removed.begin() + 64);
    sequences.resize(next ? next->entries.size() : 0);
    if (next)
      for (const auto &entry : next->entries) {
        if (entry.id == UINT32_MAX)
          continue;
        const auto *old = snapshot ? snapshot->find(entry.id) : nullptr;
        const bool same = old && old->metadata->uuid == entry.metadata->uuid;
        if (!same || *old->feature != *entry.feature)
          sequences[entry.id] = ++sequence;
        if (!same || old->polygons != entry.polygons)
          bounds(entry);
      }
    removed.erase(std::remove_if(removed.begin(), removed.end(),
                                 [&](const std::pair<uint32_t, uint64_t> &r) {
                                   return next && next->find(r.first);
                                 }),
                  removed.end());
    if (sequence != start)
      latest = sequence;
    snapshot = std::move(next);
    return changed;
  }

  void writeRows(JSON::Writer &w, uint64_t since) const {
    if (snapshot && (!since || since < latest))
      for (const auto &entry : snapshot->entries) {
        if (entry.id == UINT32_MAX)
          continue;
        const auto sequence = sequences[entry.id];
        if (!since || sequence > since)
          entry.writeSummary(w, sequence);
      }
  }
  void writeRemoved(JSON::Writer &w, uint64_t since) const {
    if (since && since >= latest)
      return;
    for (const auto &entry : removed)
      if (entry.second > since)
        w.val("place-" + std::to_string(entry.first));
  }
};
