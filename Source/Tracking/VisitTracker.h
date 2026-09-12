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
#include "Writer.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <tuple>
#include <unordered_set>

// Five records are the only per-ship visit/crossing state. Metadata is shared;
// a historical visit never retains polygon geometry. Caller holds the DB lock.
class VisitTracker {
public:
  struct Visit {
    std::shared_ptr<const PlaceMetadata> place;
    uint32_t id = UINT32_MAX;
    // Both ends are stamped whenever anything is known: an observed crossing,
    // or else the moment containment was established and the last moment it
    // held. The SEEN bits say which, so entryTime()/exitTime() still expose
    // only crossings while the raw stamps keep every visit a bounded interval
    // that (mmsi, place, entered) identifies. A pending crossing parks its
    // tentative time in the matching stamp until it confirms.
    uint32_t entered = 0, exited = 0;
    // Minutes stopped inside, with the seconds not yet carried into them.
    // Saturating: a ship moored for 45 days stops counting.
    uint16_t idle = 0;
    enum : uint8_t {
      INSIDE = 1,
      PENDING = 2,
      ENTRY_SEEN = 4,
      EXIT_SEEN = 8,
      SEEN = ENTRY_SEEN | EXIT_SEEN
    };
    uint8_t flags = 0;
    uint8_t carry = 0;
    bool empty() const { return !place; }
    bool hasRuntimeId() const { return !empty() && id != UINT32_MAX; }
    bool inside() const { return flags & INSIDE; }
    bool pending() const { return flags & PENDING; }
    bool confirmed() const { return inside() != pending(); }
    bool active() const { return inside() || pending(); }
    uint32_t entryTime() const { return flags & ENTRY_SEEN ? entered : 0; }
    uint32_t exitTime() const { return flags & EXIT_SEEN ? exited : 0; }
    uint32_t pendingTime() const { return inside() ? entered : exited; }
    // A crossing, an interval, or confirmed presence: anything to say. A
    // tentative entry is nothing yet and says nothing until it confirms.
    bool worth() const {
      return !empty() && (confirmed() || (!pending() && (entered || exited)));
    }
    void accrue(uint32_t seconds) {
      carry += seconds % 60;
      const uint32_t minutes = idle + seconds / 60 + carry / 60;
      carry %= 60;
      idle = minutes > UINT16_MAX ? UINT16_MAX : minutes;
    }
    void cancelPending() {
      if (pending())
        (inside() ? entered : exited) = 0;
      flags &= ~PENDING;
    }
    void baseline(bool remains) {
      cancelPending();
      flags = (flags & SEEN) | (remains ? INSIDE : 0);
    }
  };
  struct Record {
    std::array<Visit, 5> visits;
    uint32_t last = 0;
  };
  static_assert(sizeof(Visit) <= 32, "Visit storage must stay compact");
  static_assert(sizeof(Record) <= 168, "Five visits must remain bounded");

  // How close two fixes must be for the crossing between them to be an event
  // worth timing. Inside and then outside within it: the ship left, and the
  // later fix says near enough when. Further apart, or after any silence at
  // all, we know only that it went - never when. Silence itself ends nothing,
  // because a Class B can fall quiet for hours in a berth it never leaves.
  static const uint32_t SILENT = 3600;
  // The longest interval that counts as watched. A visit tolerates a silence
  // far longer than any one sample, and none of that gap is time the ship was
  // seen lying still - idle is what was observed, never what was assumed.
  static const uint32_t SAMPLE = 600;

private:
  template <class T> static bool put(std::ofstream &out, const T &value) {
    return bool(
        out.write(reinterpret_cast<const char *>(&value), sizeof(value)));
  }
  template <class T> static bool get(std::ifstream &in, T &value) {
    return bool(in.read(reinterpret_cast<char *>(&value), sizeof(value)));
  }
  std::vector<Record> records;
  RelationshipIndex<uint32_t, 5> membership;
  // Zero is the unknown-time sentinel; unsigned seconds end in February 2106.
  static bool validTime(std::time_t now) {
    return now > 0 && uint64_t(now) <= UINT32_MAX;
  }
  void reindex(uint32_t slot) {
    std::array<uint32_t, 5> ids;
    size_t count = 0;
    for (const auto &v : records[slot].visits)
      if (v.hasRuntimeId())
        ids[count++] = v.id;
    membership.replace(slot, ids.data(), count);
  }
  static bool has(const std::vector<uint32_t> &ids, uint32_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  }
  static Visit *findActive(Record &r, uint32_t id) {
    for (auto &v : r.visits)
      if (!v.empty() && v.id == id && v.active())
        return &v;
    return nullptr;
  }
  Visit *allocate(Record &r, uint32_t id, const PlaceIndex &index,
                  bool &dirty) {
    // Lowest rank wins: empty, oldest completed, pending exit, largest inside.
    auto rank = [&](const Visit &v) {
      const auto *entry = index.find(v.id);
      const int category = v.empty()     ? 0
                           : !v.active() ? 1
                           : !v.inside() ? 2
                                         : 3;
      return std::make_tuple(category, category == 1 ? v.exited : 0u,
                             category >= 2 && entry ? -entry->size : 0.,
                             category >= 2 ? UINT32_MAX - v.id : 0u);
    };
    auto chosen = std::min_element(
        r.visits.begin(), r.visits.end(),
        [&](const Visit &a, const Visit &b) { return rank(a) < rank(b); });
    const auto *old = index.find(chosen->id), *next = index.find(id);
    // Pending exits yield to current containment regardless of polygon size.
    if (chosen->inside() && old &&
        std::make_pair(old->size, old->id) <
            std::make_pair(next->size, next->id))
      return nullptr;
    *chosen = Visit{};
    chosen->id = id;
    chosen->place = next->metadata;
    dirty = true;
    return &*chosen;
  }

public:
  void setup(size_t capacity) {
    records.clear();
    records.resize(capacity);
    membership.setup(capacity);
  }
  void erase(uint32_t slot) {
    membership.erase(slot);
    records.at(slot) = Record{};
  }
  const Record &record(uint32_t slot) const { return records.at(slot); }
  std::array<uint64_t, 5> packed(uint32_t slot) const {
    std::array<uint64_t, 5> ids;
    ids.fill(UINT64_MAX);
    size_t count = 0;
    for (const auto &v : records.at(slot).visits)
      if (v.hasRuntimeId())
        ids[count++] = uint64_t(v.id) * 2 + v.inside();
    return ids;
  }
  template <class F> void forEach(uint32_t id, F f) const {
    membership.forEach(id, f);
  }
  template <class F> void forEachVisit(uint32_t slot, F f) const {
    for (const auto &v : records.at(slot).visits)
      if (v.worth())
        f(v);
  }

  // Operator edits/reloads/restores establish containment without crossings.
  // Existing entry times survive when the ship is still in the same place.
  // Containment as found, without a crossing: what the ship is inside now
  // continues, and what it is no longer inside ended at the last moment it was
  // known to be there. Nothing here invents a time nobody observed.
  void baseline(uint32_t slot, const std::vector<uint32_t> &inside,
                std::time_t now, const PlaceIndex *index) {
    if (now < 0 || uint64_t(now) > UINT32_MAX)
      return;
    auto &r = records.at(slot);
    bool dirty = false;
    // an end nobody saw is the last moment the ship was known to be inside
    const uint32_t held = r.last ? r.last : uint32_t(now);
    for (auto &v : r.visits)
      if (!v.empty() && v.active()) {
        const bool remains = has(inside, v.id);
        v.baseline(remains);
        if (!remains && v.entered && !v.exited)
          v.exited = held;
        // a tentative entry that never confirmed leaves nothing to show
        if (!v.active() && !v.entered && !v.exited) {
          v = Visit{};
          dirty = true;
        }
      }
    if (index)
      for (auto id : inside)
        if (!findActive(r, id))
          if (auto *v = allocate(r, id, *index, dirty)) {
            v->baseline(true);
            v->entered = uint32_t(now);
          }
    r.last = now;
    if (dirty)
      reindex(slot);
  }
  template <class Emit>
  void update(uint32_t slot, const std::vector<uint32_t> &inside,
              std::time_t now, bool stopped, const PlaceIndex *index,
              Emit emit) {
    if (!validTime(now))
      return;
    auto &r = records.at(slot);
    if (r.last && now <= r.last)
      return;
    // Too long since the last fix for a crossing to be timed: take the places
    // as found. A ship still inside is still on the same visit, however long
    // it was quiet; one that has moved on ended its visit when last seen.
    if (!r.last || now - r.last > SILENT) {
      baseline(slot, inside, now, index);
      return;
    }
    const uint32_t elapsed = now - r.last;
    r.last = now;
    bool dirty = false;
    for (auto &v : r.visits)
      if (!v.empty() && v.active()) {
        if (stopped && v.inside() && elapsed <= SAMPLE)
          v.accrue(elapsed);
        const bool observedInside = has(inside, v.id);
        if (observedInside == v.confirmed()) {
          v.baseline(observedInside); // reversal cancels the tentative time
        } else if (!v.pending()) {
          v.flags = (v.flags & Visit::SEEN) |
                    (observedInside ? Visit::INSIDE : 0) | Visit::PENDING;
          (observedInside ? v.entered : v.exited) = now;
        } else if (now - v.pendingTime() >= 10) {
          const auto observed = v.pendingTime();
          v.flags &= ~Visit::PENDING;
          v.flags |= v.inside() ? Visit::ENTRY_SEEN : Visit::EXIT_SEEN;
          emit(v, v.inside(), observed);
        }
        if (!v.active() && !v.entered && !v.exited) {
          v = Visit{};
          dirty = true;
        }
      }
    if (index)
      for (auto id : inside)
        if (!findActive(r, id))
          if (auto *v = allocate(r, id, *index, dirty)) {
            v->flags = Visit::INSIDE | Visit::PENDING;
            v->entered = now;
          }
    if (dirty)
      reindex(slot);
  }
  // Map every retained reference by UUID, including completed visits. A visit
  // follows its place's current name and type; only the UUID is identity.
  void remap(const PlaceIndex *index) {
    std::unordered_map<std::string, const PlaceIndex::Entry *> byUUID;
    if (index)
      for (const auto &e : index->entries)
        if (e.metadata)
          byUUID.emplace(e.metadata->uuid, &e);
    for (size_t slot = 0; slot < records.size(); ++slot) {
      bool dirty = false;
      for (auto &v : records[slot].visits)
        if (!v.empty()) {
          auto it = byUUID.find(v.place->uuid);
          const auto id = it == byUUID.end() ? UINT32_MAX : it->second->id;
          dirty |= v.id != id;
          v.id = id;
          if (it != byUUID.end())
            v.place = it->second->metadata;
          if (id == UINT32_MAX) {
            // a place that vanished ends its visits where the ship was last
            // heard, so the interval stays bounded like a gap-ended one
            const bool held = v.confirmed();
            v.baseline(false);
            if (held && v.entered && !v.exited)
              v.exited = records[slot].last;
            if (!v.entered && !v.exited) {
              v = Visit{};
              dirty = true;
            }
          }
        }
      if (dirty)
        reindex(slot);
    }
  }
  void writeVisits(JSON::Writer &w, uint32_t slot,
                   const PlaceIndex *index) const {
    std::array<const Visit *, 5> ordered;
    size_t count = 0;
    for (const auto &v : records.at(slot).visits)
      if (!v.empty())
        ordered[count++] = &v;
    // The summary uses the first inside visit; keep smallest-place preference.
    // Five entries at most: an insertion sort, which also keeps GCC from
    // reasoning about std::sort's 16-element threshold against a 5-slot array.
    const auto before = [&](const Visit *a, const Visit *b) {
      if (a->inside() != b->inside())
        return a->inside();
      const auto *x = index ? index->find(a->id) : nullptr,
                 *y = index ? index->find(b->id) : nullptr;
      return a->inside() && x && y &&
             std::make_pair(x->size, x->id) < std::make_pair(y->size, y->id);
    };
    for (size_t i = 1; i < count; ++i)
      for (size_t j = i; j > 0 && before(ordered[j], ordered[j - 1]); --j)
        std::swap(ordered[j], ordered[j - 1]);
    w.key("visits").beginArray();
    for (size_t i = 0; i < count; ++i) {
      const auto &v = *ordered[i];
      w.beginObject().key("id");
      if (v.hasRuntimeId())
        w.val(v.id);
      else
        w.val_null();
      w.kv("name", v.place->name)
          .kv("inside", v.inside())
          .kv("pending", v.pending())
          .kv("idle", (long long)v.idle)
          .key("entered");
      if (v.entryTime())
        w.val(v.entryTime());
      else
        w.val_null();
      w.key("exited");
      if (v.exitTime())
        w.val(v.exitTime());
      else
        w.val_null();
      w.endObject();
    }
    w.endArray();
  }
  void writeChanges(JSON::Writer &w, uint32_t slot) const {
    for (const auto &v : records.at(slot).visits)
      if (!v.empty()) {
        for (int exiting = 0; exiting < 2; ++exiting) {
          const auto t = exiting ? v.exitTime() : v.entryTime();
          if (t)
            w.beginObject()
                .kv("t", (long long)t)
                .kv("f", exiting ? 8 : 7)
                .kv("to", v.place->name)
                .kv("place_id", v.place->uuid)
                .kv("place_type", v.place->type)
                .kv("unlocode", v.place->code)
                .endObject();
        }
      }
  }

  // A sparse section keyed by MMSI. Shared metadata is written once, and each
  // visit stores a table offset, never a process-local catalogue ID.
  template <class MMSI> bool save(std::ofstream &out, MMSI mmsi) const {
    const auto retained = [](const Visit &v) { return v.worth(); };
    std::vector<std::shared_ptr<const PlaceMetadata>> table;
    std::unordered_map<const PlaceMetadata *, uint32_t> offsets;
    uint32_t count = 0;
    for (size_t s = 0; s < records.size(); ++s) {
      bool any = false;
      for (const auto &v : records[s].visits)
        if (retained(v)) {
          any = true;
          if (offsets.emplace(v.place.get(), table.size()).second)
            table.push_back(v.place);
        }
      if (any)
        ++count;
    }
    uint32_t n = table.size();
    put(out, n);
    for (const auto &p : table)
      for (const auto *str : {&p->uuid, &p->name, &p->type, &p->code,
                              &p->parentCode, &p->category}) {
        uint32_t len = str->size();
        put(out, len);
        out.write(str->data(), len);
      }
    put(out, count);
    for (size_t s = 0; s < records.size(); ++s) {
      uint8_t nvisits = 0;
      for (const auto &v : records[s].visits)
        if (retained(v))
          ++nvisits;
      if (!nvisits)
        continue;
      uint32_t key = mmsi(s);
      put(out, key);
      put(out, nvisits);
      for (const auto &v : records[s].visits)
        if (retained(v)) {
          put(out, offsets.at(v.place.get()));
          // A tentative exit is not persisted: the ship is inside until it
          // confirms, and a backup saying otherwise would refuse to load.
          int64_t entry = v.entered, exit = v.pending() ? 0 : v.exited;
          put(out, entry);
          put(out, exit);
          uint8_t state = (v.confirmed() ? 1 : 0) | (v.flags & Visit::SEEN);
          put(out, state);
          put(out, v.idle);
        }
    }
    return bool(out);
  }
  template <class Find>
  bool load(std::ifstream &in, Find find, uint32_t maxShips, int version) {
    uint32_t n = 0;
    if (!get(in, n) || uint64_t(n) > uint64_t(maxShips) * 5)
      return false;
    std::vector<std::shared_ptr<const PlaceMetadata>> table;
    for (uint32_t i = 0; i < n; ++i) {
      auto p = std::make_shared<PlaceMetadata>();
      for (auto *str : {&p->uuid, &p->name, &p->type, &p->code, &p->parentCode,
                        &p->category}) {
        uint32_t len;
        if (!get(in, len) || len > 128 * 1024)
          return false;
        str->resize(len);
        if (len && !in.read(&(*str)[0], len))
          return false;
      }
      if (p->uuid.size() != 36)
        return false;
      table.push_back(p);
    }
    uint32_t count;
    if (!get(in, count) || count > maxShips)
      return false;
    std::unordered_set<uint32_t> seen;
    // A visit with no entry time has no identity. Restoring re-establishes
    // containment as a coverage gap does: one still inside is stamped with this
    // moment, one already over is dropped.
    const uint32_t restored_at = uint32_t(std::time(nullptr));
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t key;
      uint8_t countVisits;
      if (!get(in, key) || !seen.insert(key).second || !get(in, countVisits) ||
          !countVisits || countVisits > 5)
        return false;
      int slot = find(key);
      if (slot < 0 || size_t(slot) >= records.size())
        return false;
      for (uint8_t j = 0; j < countVisits; ++j) {
        uint32_t offset;
        int64_t entry, exit;
        uint8_t state;
        uint16_t idle = 0;
        if (!get(in, offset) || offset >= table.size() || !get(in, entry) ||
            !get(in, exit) || !get(in, state) ||
            (version >= 3 && !get(in, idle)) ||
            (state & ~uint8_t(Visit::INSIDE | Visit::SEEN)) || entry < 0 ||
            exit < 0 || uint64_t(entry) > UINT32_MAX ||
            uint64_t(exit) > UINT32_MAX || (exit && entry > exit) ||
            ((state & Visit::INSIDE) && exit))
          return false;
        // `entry` stays as written: the SEEN bits below read it, so a
        // re-established containment gains a time without claiming a crossing.
        int64_t stamp = entry;
        if (!entry) {
          if (!(state & Visit::INSIDE))
            continue;
          stamp = restored_at;
        }
        auto &v = records[slot].visits[j];
        v.place = table[offset];
        v.entered = stamp;
        v.exited = exit;
        v.idle = idle;
        // Before the stamps carried their own SEEN bits only crossings were
        // written, so there a time that is present is a time that was seen.
        v.flags = (state & Visit::INSIDE) |
                  (version >= 3 ? (state & Visit::SEEN)
                                : uint8_t((entry ? Visit::ENTRY_SEEN : 0) |
                                          (exit ? Visit::EXIT_SEEN : 0)));
      }
    }
    return true;
  }
  void restore(uint32_t slot, const Record &r) {
    records.at(slot) = r;
    reindex(slot);
  }
};
