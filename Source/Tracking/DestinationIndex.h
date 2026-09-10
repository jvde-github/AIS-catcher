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
#include "RelationshipIndex.h"
#include "Ships.h"
#include "Writer.h"
#ifdef HASPORTS
#include "libport.h"
#endif

// Owns ship-slot -> destination code, reverse membership, matching and
// first-fix retries. Codes do not require a local place. All access is under
// the DB lock.
class DestinationIndex {
  RelationshipIndex<uint32_t, 1> membership;
  std::vector<std::string> codes{""}; // interned once; id 0 is no destination
  std::unordered_map<std::string, uint32_t> code_ids;
  std::vector<uint8_t> waiting; // matched without a fix; retried on the first
  std::shared_ptr<const PlaceIndex> places;

  void replace(uint32_t slot, const std::string &next) {
    uint32_t id = 0;
    if (!next.empty()) {
      const auto it = code_ids.emplace(next, codes.size());
      if (it.second)
        codes.push_back(next);
      id = it.first->second;
    }
    membership.replace(slot, &id, id ? 1 : 0);
  }

public:
  void setup(size_t capacity) {
    membership.setup(capacity);
    waiting.assign(capacity, 0);
  }
  const std::string &code(uint32_t slot) const {
    const auto *key = membership.first(slot);
    return codes[key ? *key : 0];
  }
  void erase(uint32_t slot) {
    waiting.at(slot) = 0;
    replace(slot, "");
  }
  void match(uint32_t slot, const Ship &ship) {
    std::string next;
#ifdef HASPORTS
    next = libport::match(ship.destination, ship.lat, ship.lon).code;
#else
    if (const auto *entry =
            places ? places->matchDestination(ship.destination) : nullptr)
      next = entry->metadata->code;
#endif
    waiting.at(slot) = next.empty() && ship.destination[0] &&
                       !isValidCoord(ship.lat, ship.lon);
    replace(slot, next);
  }
  void position(uint32_t slot, const Ship &ship) {
    if (waiting.at(slot) && isValidCoord(ship.lat, ship.lon))
      match(slot, ship);
  }
  template <class Table>
  void setPlaces(std::shared_ptr<const PlaceIndex> next, Table &ships) {
    if (places == next)
      return;
    places = std::move(next);
#ifndef HASPORTS
    // a name match depends on the catalogue; a libport match does not
    ships.forEach([&](int slot) {
      if (ships[slot].destination[0])
        match(slot, ships[slot]);
      return true;
    });
#endif
  }
  template <class F> void forEach(const std::string &code, F visit) const {
    const auto it = code_ids.find(code);
    if (it != code_ids.end())
      membership.forEach(it->second, visit);
  }
  void writeMatched(JSON::Writer &w, uint32_t slot) const {
    const auto &key = code(slot);
    if (key.empty())
      return;
    w.key("matched_port").beginObject().kv("code", key);
    if (const auto *entry = places ? places->findPort(key) : nullptr) {
      w.kv("country", key.substr(0, 2))
          .kv("name", entry->metadata->name)
          .kv("size", entry->markerSize)
          .kv("lat", entry->lat).kv("lon", entry->lon);
      w.endObject();
      return;
    }
#ifdef HASPORTS
    libport::Info port;
    if (libport::lookup(key.c_str(), port)) {
      w.kv("country", port.country).kv("name", port.name).kv("size", port.size);
      if (port.has_position)
        w.kv("lat", port.lat).kv("lon", port.lon);
      w.endObject();
      return;
    }
#endif
    w.kv("country", "").kv("name", "").kv("size", 0).endObject();
  }
};
