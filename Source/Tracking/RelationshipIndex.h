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

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// Slot-indexed forward edges and compact reverse buckets. All access is under
// the owning DB lock. Back-references make deletion independent of bucket size.
template <class Key, size_t N> class RelationshipIndex {
  struct Edge {
    Key key{};
    uint32_t offset = 0;
  };
  struct Forward {
    std::array<Edge, N> edges;
    uint32_t count = 0;
  };
  struct Back {
    uint32_t slot;
    uint32_t edge;
  };
  std::vector<Forward> forward;
  std::unordered_map<Key, std::vector<Back>> reverse;

public:
  void setup(size_t capacity) {
    forward.clear();
    forward.resize(capacity);
    reverse.clear();
  }
  void erase(uint32_t slot) {
    auto &f = forward.at(slot);
    while (f.count) {
      auto &e = f.edges[--f.count];
      auto it = reverse.find(e.key);
      auto &bucket = it->second;
      const auto moved = bucket.back();
      bucket[e.offset] = moved;
      forward[moved.slot].edges[moved.edge].offset = e.offset;
      bucket.pop_back();
      if (bucket.empty())
        reverse.erase(it);
    }
  }
  void replace(uint32_t slot, const Key *keys, size_t count) {
    std::array<Key, N> unique;
    size_t size = 0;
    for (size_t i = 0; i < count; ++i) {
      if (std::find(unique.begin(), unique.begin() + size, keys[i]) !=
          unique.begin() + size)
        continue;
      if (size == N)
        throw std::length_error("Too many relationship keys");
      unique[size++] = keys[i];
    }
    auto &f = forward.at(slot);
    bool same = f.count == size;
    for (size_t i = 0; same && i < size; ++i)
      same = f.edges[i].key == unique[i];
    if (same)
      return;
    erase(slot);
    for (size_t i = 0; i < size; ++i) {
      auto &bucket = reverse[unique[i]];
      auto &e = f.edges[f.count];
      e.key = unique[i];
      e.offset = bucket.size();
      bucket.push_back({slot, f.count++});
    }
  }
  const Key *first(uint32_t slot) const {
    const auto &f = forward.at(slot);
    return f.count ? &f.edges[0].key : nullptr;
  }
  template <class F> void forEach(const Key &key, F visit) const {
    auto it = reverse.find(key);
    if (it != reverse.end())
      for (const auto &b : it->second)
        if (!visit(b.slot))
          break;
  }
};
