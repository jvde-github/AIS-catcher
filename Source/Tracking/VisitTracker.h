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

// Where each ship is and has been: ten visits per ship, one place each. A
// visit names its place by runtime id and nothing else; a place that leaves
// the catalogue takes its visits with it. The DB feeds only validated
// positions, so every fix here is a ship. Caller holds the DB lock.
class VisitTracker {
public:
  static const size_t SLOTS = 10;
  // A fix this long after the last cannot time a crossing: the places are
  // taken as found.
  static const uint32_t SILENT = 3600;
  // Longest interval counted as idle; both fixes must say stopped.
  static const uint32_t SAMPLE = 600;
  // A crossing holds this long before it counts.
  static const uint32_t CONFIRM = 10;

  struct Visit {
    // ENTERING and LEAVING are crossings seen once, waiting for the next fix
    // to agree. Nobody outside sees an ENTERING visit; a LEAVING one is still
    // inside.
    enum State : uint8_t { FREE, ENTERING, INSIDE, LEAVING, DONE };
    enum : uint8_t { ENTRY_SEEN = 4, EXIT_SEEN = 8 }; // as the backup has them
    uint32_t id = UINT32_MAX;
    // Observed crossings when seen, else bounds: the first and the last fix
    // known inside. LEAVING parks its tentative exit in exited.
    uint32_t entered = 0, exited = 0;
    uint16_t idle = 0; // minutes stopped inside, saturating
    uint8_t state = FREE, seen = 0, carry = 0;
    static const uint16_t CALL_MINUTES = 1;

    bool empty() const { return state == FREE; }
    bool live() const { return !empty() && state != DONE; }
    bool inside() const { return state == INSIDE || state == LEAVING; }
    // shown, saved and exported: everything but a crossing still in doubt
    bool settled() const { return !empty() && state != ENTERING; }
    uint32_t entryTime() const { return seen & ENTRY_SEEN ? entered : 0; }
    uint32_t exitTime() const { return seen & EXIT_SEEN ? exited : 0; }
    // a call: the ship lay still for it, or the place is one nobody stops at
    bool call(bool requiresStop) const {
      return idle >= CALL_MINUTES || !requiresStop;
    }
    // the stay under way, and the calls before it
    bool shown(bool requiresStop) const {
      return settled() && (inside() || call(requiresStop));
    }
    void accrue(uint32_t seconds) {
      carry += seconds % 60;
      const uint32_t minutes = idle + seconds / 60 + carry / 60;
      carry %= 60;
      idle = minutes > UINT16_MAX ? UINT16_MAX : minutes;
    }
  };
  struct Record {
    std::array<Visit, SLOTS> visits;
    uint32_t last = 0;    // the last fix
    bool stopped = false; // the last fix said stopped
  };
  static_assert(sizeof(Visit) <= 20, "Visit storage must stay compact");
  static_assert(sizeof(Record) <= 208, "Ten visits must remain bounded");

private:
  template <class T> static bool put(std::ofstream &out, const T &value) {
    return bool(
        out.write(reinterpret_cast<const char *>(&value), sizeof(value)));
  }
  template <class T> static bool get(std::ifstream &in, T &value) {
    return bool(in.read(reinterpret_cast<char *>(&value), sizeof(value)));
  }
  std::vector<Record> records;
  RelationshipIndex<uint32_t, SLOTS> membership; // place -> ships with a visit
  // Zero is the unknown-time sentinel; unsigned seconds end in February 2106.
  static bool validTime(std::time_t now) {
    return now > 0 && uint64_t(now) <= UINT32_MAX;
  }
  void reindex(uint32_t slot) {
    std::array<uint32_t, SLOTS> ids;
    size_t count = 0;
    for (const auto &v : records[slot].visits)
      if (!v.empty())
        ids[count++] = v.id;
    membership.replace(slot, ids.data(), count);
  }
  static bool has(const std::vector<uint32_t> &ids, uint32_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  }
  static Visit *live(Record &r, uint32_t id) {
    for (auto &v : r.visits)
      if (v.live() && v.id == id)
        return &v;
    return nullptr;
  }
  static void end(Visit &v, uint32_t at) {
    v.state = Visit::DONE;
    v.exited = at;
  }
  // Reading order: the berth before the terminal before the port before the
  // water, the smaller first within a kind.
  static bool reads(const PlaceIndex::Entry &a, const PlaceIndex::Entry &b) {
    return std::make_tuple(a.metadata->rank(), a.size, a.id) <
           std::make_tuple(b.metadata->rank(), b.size, b.id);
  }
  // A fresh visit to `id` in a free slot, else the oldest completed stay's,
  // else the slot of the live one telling least, if the new place tells more.
  static Visit *allocate(Record &r, uint32_t id, const PlaceIndex &index) {
    Visit *slot = nullptr, *least = nullptr;
    for (auto &v : r.visits) {
      if (v.empty()) {
        slot = &v;
        break;
      }
      if (v.state == Visit::DONE) {
        if (!slot || v.exited < slot->exited)
          slot = &v;
      } else if (!least || !index.find(v.id) ||
                 reads(*index.find(least->id), *index.find(v.id)))
        least = &v;
    }
    if (!slot) {
      const auto *fresh = index.find(id), *held = index.find(least->id);
      if (held && !(fresh && reads(*fresh, *held)))
        return nullptr;
      slot = least;
    }
    *slot = Visit{};
    slot->id = id;
    return slot;
  }
  // insertion sort: at most ten entries, and GCC warns on std::sort here
  static void sortForReading(const Visit **v, size_t count,
                             const PlaceIndex *index) {
    const auto before = [&](const Visit *a, const Visit *b) {
      if (a->inside() != b->inside())
        return a->inside();
      const auto *x = index ? index->find(a->id) : nullptr,
                 *y = index ? index->find(b->id) : nullptr;
      return a->inside() && x && y && reads(*x, *y);
    };
    for (size_t i = 1; i < count; ++i)
      for (size_t j = i; j > 0 && before(v[j], v[j - 1]); --j)
        std::swap(v[j], v[j - 1]);
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
  bool insideOf(uint32_t slot, uint32_t id) const {
    for (const auto &v : records.at(slot).visits)
      if (v.inside() && v.id == id)
        return true;
    return false;
  }
  // the places the ship is inside, in reading order; UINT32_MAX ends the list
  std::array<uint32_t, SLOTS> insideIds(uint32_t slot,
                                        const PlaceIndex *index) const {
    std::array<const Visit *, SLOTS> ordered;
    size_t count = 0;
    for (const auto &v : records.at(slot).visits)
      if (v.inside())
        ordered[count++] = &v;
    sortForReading(ordered.data(), count, index);
    std::array<uint32_t, SLOTS> ids;
    ids.fill(UINT32_MAX);
    for (size_t i = 0; i < count; ++i)
      ids[i] = ordered[i]->id;
    return ids;
  }
  template <class F> void forEach(uint32_t id, F f) const {
    membership.forEach(id, f);
  }
  template <class F> void forEachVisit(uint32_t slot, F f) const {
    for (const auto &v : records.at(slot).visits)
      if (v.settled())
        f(v);
  }

  // Containment as found, without crossings: a restore, a catalogue change, a
  // fix after silence. Stays still inside continue, the rest ended at the
  // last fix inside.
  void refresh(uint32_t slot, const std::vector<uint32_t> &inside,
               std::time_t now, const PlaceIndex *index) {
    if (!validTime(now))
      return;
    auto &r = records.at(slot);
    const uint32_t at = r.last ? r.last : uint32_t(now);
    for (auto &v : r.visits) {
      if (!v.live())
        continue;
      if (v.state == Visit::ENTERING)
        v = Visit{};
      else if (!has(inside, v.id))
        end(v, at);
      else if (v.state == Visit::LEAVING) {
        v.state = Visit::INSIDE;
        v.exited = 0;
      }
    }
    if (index)
      for (auto id : inside)
        if (!live(r, id))
          if (auto *v = allocate(r, id, *index)) {
            v->state = Visit::INSIDE;
            v->entered = uint32_t(now);
          }
    r.last = std::max(r.last, uint32_t(now));
    r.stopped = false;
    reindex(slot);
  }

  // One fix. A crossing holds for CONFIRM seconds before it counts, and the
  // next fix can still cancel it. Emit gets each confirmed crossing.
  template <class Emit>
  void update(uint32_t slot, const std::vector<uint32_t> &inside,
              std::time_t now, bool stopped, const PlaceIndex *index,
              Emit emit) {
    if (!validTime(now))
      return;
    auto &r = records.at(slot);
    if (r.last && now <= r.last)
      return;
    if (!r.last || now - r.last > SILENT) {
      refresh(slot, inside, now, index);
      r.stopped = stopped;
      return;
    }
    const uint32_t elapsed = now - r.last;
    const bool watchedStop = stopped && r.stopped && elapsed <= SAMPLE;
    r.last = now;
    r.stopped = stopped;
    for (auto &v : r.visits) {
      if (!v.live())
        continue;
      const bool in = has(inside, v.id);
      if (watchedStop && in)
        v.accrue(elapsed);
      if (v.state == Visit::ENTERING) {
        if (!in)
          v = Visit{};
        else if (now - v.entered >= CONFIRM) {
          v.state = Visit::INSIDE;
          v.seen |= Visit::ENTRY_SEEN;
          emit(v, true, v.entered);
        }
      } else if (v.state == Visit::INSIDE) {
        if (!in) {
          v.state = Visit::LEAVING;
          v.exited = now;
        }
      } else if (in) {
        v.state = Visit::INSIDE;
        v.exited = 0;
      } else if (now - v.exited >= CONFIRM) {
        v.state = Visit::DONE;
        v.seen |= Visit::EXIT_SEEN;
        emit(v, false, v.exited);
      }
    }
    if (index)
      for (auto id : inside)
        if (!live(r, id))
          if (auto *v = allocate(r, id, *index)) {
            v->state = Visit::ENTERING;
            v->entered = now;
          }
    reindex(slot);
  }

  // A new catalogue: a visit follows its place by UUID, through redirects,
  // and a place that is gone takes its visits with it.
  void remap(const PlaceIndex *from, const PlaceIndex *to) {
    for (uint32_t slot = 0; slot < records.size(); ++slot) {
      auto &r = records[slot];
      bool changed = false;
      for (auto &v : r.visits) {
        if (v.empty())
          continue;
        const auto *old = from ? from->find(v.id) : nullptr;
        const auto *entry = old && to ? to->resolve(old->metadata->uuid) : nullptr;
        const uint32_t id = entry ? entry->id : UINT32_MAX;
        if (id == v.id)
          continue;
        changed = true;
        if (id == UINT32_MAX)
          v = Visit{};
        else
          v.id = id;
      }
      if (!changed)
        continue;
      // two stays now at one place are one, from the earlier entry
      for (auto &a : r.visits)
        for (auto &b : r.visits)
          if (&a != &b && a.live() && b.live() && a.id == b.id &&
              (a.entered < b.entered || (a.entered == b.entered && &a < &b))) {
            a.idle = std::max(a.idle, b.idle);
            b = Visit{};
          }
      reindex(slot);
    }
  }

  void writeVisits(JSON::Writer &w, uint32_t slot,
                   const PlaceIndex *index) const {
    std::array<const Visit *, SLOTS> ordered;
    size_t count = 0;
    if (index)
      for (const auto &v : records.at(slot).visits)
        if (const auto *e = v.empty() ? nullptr : index->find(v.id))
          if (v.shown(e->metadata->requiresStop()))
            ordered[count++] = &v;
    sortForReading(ordered.data(), count, index);
    w.key("visits").beginArray();
    for (size_t i = 0; i < count; ++i) {
      const auto &v = *ordered[i];
      const auto &m = *index->find(v.id)->metadata;
      w.beginObject()
          .kv("id", v.id)
          .kv("name", m.name)
          .kv("uuid", m.uuid)
          .kv("place_type", m.type())
          .kv("code", m.code)
          .kv("country", m.country)
          .kv("inside", v.inside())
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
  void writeChanges(JSON::Writer &w, uint32_t slot,
                    const PlaceIndex *index) const {
    if (!index)
      return;
    for (const auto &v : records.at(slot).visits) {
      const auto *e = v.settled() ? index->find(v.id) : nullptr;
      if (!e)
        continue;
      for (int exiting = 0; exiting < 2; ++exiting) {
        const auto t = exiting ? v.exitTime() : v.entryTime();
        if (t)
          w.beginObject()
              .kv("t", (long long)t)
              .kv("f", exiting ? 8 : 7)
              .kv("to", e->metadata->name)
              .kv("place_id", e->metadata->uuid)
              .kv("place_type", e->metadata->type())
              .kv("unlocode", e->metadata->code)
              .endObject();
      }
    }
  }

  // The places as a UUID table, then per ship its visits by table offset.
  template <class MMSI>
  bool save(std::ofstream &out, const PlaceIndex *index, MMSI mmsi) const {
    std::vector<uint32_t> offsets(index ? index->entries.size() : 0,
                                  UINT32_MAX);
    std::vector<uint32_t> table;
    const auto kept = [&](const Visit &v) {
      return v.settled() && v.id < offsets.size();
    };
    uint32_t count = 0;
    for (const auto &r : records) {
      bool any = false;
      for (const auto &v : r.visits)
        if (kept(v)) {
          any = true;
          if (offsets[v.id] == UINT32_MAX) {
            offsets[v.id] = table.size();
            table.push_back(v.id);
          }
        }
      count += any;
    }
    uint32_t n = table.size();
    put(out, n);
    for (auto id : table) {
      const auto &uuid = index->entries[id].metadata->uuid;
      uint32_t len = uuid.size();
      put(out, len);
      out.write(uuid.data(), len);
    }
    put(out, count);
    for (size_t s = 0; s < records.size(); ++s) {
      uint8_t nvisits = 0;
      for (const auto &v : records[s].visits)
        nvisits += kept(v);
      if (!nvisits)
        continue;
      uint32_t key = mmsi(s);
      put(out, key);
      put(out, nvisits);
      for (const auto &v : records[s].visits)
        if (kept(v)) {
          put(out, offsets[v.id]);
          // an exit under way is saved as still inside
          int64_t entry = v.entered,
                  exit = v.state == Visit::DONE ? v.exited : 0;
          put(out, entry);
          put(out, exit);
          uint8_t state = (v.inside() ? 1 : 0) | v.seen;
          put(out, state);
          put(out, v.idle);
        }
    }
    return bool(out);
  }
  // Visits from a backup, their places resolved against the catalogue as it
  // is. `heard` gives a restored ship's last signal, never the restart time.
  template <class Find, class Heard>
  bool load(std::ifstream &in, const PlaceIndex *index, Find find, Heard heard,
            uint32_t maxShips, int version) {
    uint32_t n = 0;
    if (!get(in, n) || uint64_t(n) > uint64_t(maxShips) * SLOTS)
      return false;
    std::vector<uint32_t> ids(n, UINT32_MAX);
    // before version 5 the table also carried name, type, code, part_of and
    // category
    const int strings = version >= 5 ? 1 : 6;
    for (uint32_t i = 0; i < n; ++i)
      for (int k = 0; k < strings; ++k) {
        uint32_t len;
        std::string text;
        if (!get(in, len) || len > 128 * 1024)
          return false;
        text.resize(len);
        if (len && !in.read(&text[0], len))
          return false;
        if (k == 0) {
          if (len != 36)
            return false;
          if (const auto *e = index ? index->resolve(text) : nullptr)
            ids[i] = e->id;
        }
      }
    uint32_t count;
    if (!get(in, count) || count > maxShips)
      return false;
    std::unordered_set<uint32_t> keys;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t key;
      uint8_t countVisits;
      if (!get(in, key) || !keys.insert(key).second || !get(in, countVisits) ||
          !countVisits || countVisits > SLOTS)
        return false;
      int slot = find(key);
      if (slot < 0 || size_t(slot) >= records.size())
        return false;
      const uint32_t known = uint32_t(heard(slot));
      size_t next = 0;
      for (uint8_t j = 0; j < countVisits; ++j) {
        uint32_t offset;
        int64_t entry, exit;
        uint8_t state;
        uint16_t idle = 0;
        if (!get(in, offset) || offset >= ids.size() || !get(in, entry) ||
            !get(in, exit) || !get(in, state) ||
            (version >= 3 && !get(in, idle)) ||
            (state & ~uint8_t(1 | Visit::ENTRY_SEEN | Visit::EXIT_SEEN)) ||
            entry < 0 || exit < 0 || uint64_t(entry) > UINT32_MAX ||
            uint64_t(exit) > UINT32_MAX || (exit && entry > exit) ||
            ((state & 1) && exit))
          return false;
        if (ids[offset] == UINT32_MAX)
          continue; // the place is gone
        // no entry time: anchor one still inside to the last signal, drop the
        // rest
        int64_t stamp = entry;
        if (!entry) {
          if (!(state & 1) || !known)
            continue;
          stamp = known;
        }
        auto &v = records[slot].visits[next++];
        v.id = ids[offset];
        v.entered = stamp;
        v.exited = exit;
        v.idle = idle;
        v.state = state & 1 ? Visit::INSIDE : Visit::DONE;
        // before version 3 only observed crossings were written
        v.seen = version >= 3
                     ? uint8_t(state & (Visit::ENTRY_SEEN | Visit::EXIT_SEEN))
                     : uint8_t((entry ? Visit::ENTRY_SEEN : 0) |
                               (exit ? Visit::EXIT_SEEN : 0));
      }
      reindex(slot);
    }
    return true;
  }
  void restore(uint32_t slot, const Record &r) {
    records.at(slot) = r;
    reindex(slot);
  }
};
