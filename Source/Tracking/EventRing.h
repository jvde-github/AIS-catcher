/* Copyright(c) 2021-2026 jvde.github@gmail.com

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
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "Common.h"
#include "Event.h"
#include "Keys.h"
#include "Stream.h"
#include "Writer.h"

// The ticker's view of the event stream: a safety message, a field of a
// vessel's record changing, a crossing of a place that asked to be announced.
// One ring per level, so routine events never push out a distress call. The
// same words from the same sender while the earlier event is live count on
// that event: a device left transmitting for days is one event that began
// days ago. Fixes, stops and moves never enter; they are history, not news.
//
// A subscriber of the database's stream (db.events >> ring). Receive copies
// what it keeps into its own strings; nothing here points at the event.
class EventRing : public StreamIn<Tracking::Event> {
public:
  using Kind = Tracking::Kind;
  using Level = Tracking::Level;
  static const int LEVELS = (int)Level::LEVELS;

  struct Entry {
    uint64_t seq = 0;
    std::time_t time = 0, first = 0;
    Kind kind = Kind::SAFETY;
    Level level = Level::ROUTINE;
    uint32_t from = 0, to = 0;
    FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
    int count = 1;
    // a change reads as `label`: `was` -> `text`; a safety message is `text`
    // alone
    std::string text, was, label, from_name, to_name;
  };

  // Escape received text before adding the three ticker formatting markers.
  static std::string escapeText(const std::string &text) {
    std::string out;
    for (char c : text) {
      if (c == '\\' || c == '*' || c == ':' || c == '[' || c == ']')
        out += '\\';
      out += c;
    }
    return out;
  }

  static std::string displayName(uint32_t mmsi, const std::string &name) {
    const std::string id = std::to_string(mmsi);
    if (id.compare(0, 3, "972") == 0)
      return "MOB device";
    if (id.compare(0, 3, "974") == 0)
      return "EPIRB";
    if (id.compare(0, 3, "970") == 0)
      return "AIS-SART";
    if (mmsi >= 2000000 && mmsi <= 9999999)
      return "VTS " + (name.empty() ? id : name);
    return name.empty() ? "MMSI " + id : name;
  }

  static std::string formatText(const Entry &e) {
    std::string out = e.label.empty() && e.level >= Level::NOTICE ? "⚠ " : "";
    out += "**" + escapeText(displayName(e.from, e.from_name)) + "**";
    if (e.kind == Kind::ENTER || e.kind == Kind::LEAVE)
      return out +
             (e.kind == Kind::ENTER ? " · ::entered:: [[" : " · ::left:: [[") +
             escapeText(e.text) + "]]";
    if (!e.label.empty())
      return out + " · ::" +
             escapeText(e.label + (e.was.empty() ? "" : " " + e.was)) +
             ":: → [[" + escapeText(e.text) + "]]";
    if (e.to)
      out += " → **" + escapeText(displayName(e.to, e.to_name)) + "**";
    out += " · [[" + escapeText(e.text) + "]]";
    if (e.count > 1)
      out += " (×" + std::to_string(e.count) + ")";
    return out;
  }

  EventRing() {
    for (int l = 0; l < LEVELS; l++)
      rings[l].reserve(capacity(l));
  }

  // the kinds the ticker shows; everything else is history
  static bool shown(Kind k) {
    switch (k) {
    case Kind::SAFETY:
    case Kind::DESTINATION:
    case Kind::STATUS:
    case Kind::DRAUGHT:
    case Kind::ENTER:
    case Kind::LEAVE:
      return true;
    default:
      return false;
    }
  }

  void Receive(const Tracking::Event *data, int len, TAG &) override {
    for (int i = 0; i < len; i++) {
      const Tracking::Event &e = data[i];
      last_seq = e.seq;
      if (!shown(e.kind))
        continue;
      if ((e.kind == Kind::ENTER || e.kind == Kind::LEAVE) && !e.crossing.announced)
        continue;
      Entry n;
      n.seq = e.seq;
      n.time = e.t;
      n.kind = e.kind;
      n.level = e.level;
      n.from = e.mmsi;
      n.from_name = e.name ? e.name : "";
      n.lat = e.lat;
      n.lon = e.lon;
      switch (e.kind) {
      case Kind::SAFETY:
        n.to = e.safety.to;
        n.to_name = e.safety.to_name ? e.safety.to_name : "";
        n.text = e.to.getCString();
        break;
      case Kind::ENTER:
      case Kind::LEAVE:
        n.text = e.crossing.name ? e.crossing.name : "";
        break;
      case Kind::DESTINATION:
        n.label = "destination";
        n.text = e.to.getCString();
        n.was = e.from.getCString();
        break;
      case Kind::DRAUGHT: {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f m", e.to.getFloat());
        n.text = buf;
        std::snprintf(buf, sizeof(buf), "%.1f m", e.from.getFloat());
        n.was = buf;
        n.label = "draught";
        break;
      }
      case Kind::STATUS:
        n.label = "status";
        n.text = statusName((int)e.to.getInt());
        n.was = statusName((int)e.from.getInt());
        break;
      default:
        break;
      }
      push(n);
    }
  }

  // the newest sequence the stream has reached, shown or not
  uint64_t sequence() const { return last_seq; }

  // the most one history page returns
  enum { MAX_PAGE = 100 };

  // the events after `since` of at least `level`, newest last; the horizon runs
  // from an event's onset, longer per level: a repeat counts on an event, it
  // does not make old news current
  void writeSince(JSON::Writer &w, uint64_t since, int level,
                  std::time_t now) const {
    std::vector<const Entry *> picked;
    for (int l = MAX(0, MIN(level, LEVELS - 1)); l < LEVELS; l++)
      for (const Entry &e : rings[l])
        if (e.seq > since && now - e.first <= HORIZON_S * (l + 1))
          picked.push_back(&e);
    std::sort(picked.begin(), picked.end(),
              [](const Entry *a, const Entry *b) { return a->seq < b->seq; });
    if ((int)picked.size() > LIMIT)
      picked.erase(picked.begin(), picked.end() - LIMIT);
    writeList(w, picked);
  }

  // One page of the events before `before`, of at least `level`, newest first;
  // a `before` of zero starts at the newest. No horizon: this reaches everything
  // the rings still hold. Paging runs on `seq`, so events arriving mid-read
  // never shift a page. `oldest` is the next `before`; `more` says whether
  // asking again is worth it.
  void writeBefore(JSON::Writer &w, uint64_t before, int level,
                   int limit) const {
    if (before == 0)
      before = UINT64_MAX;
    std::vector<const Entry *> picked;
    size_t retained = 0;
    for (int l = MAX(0, MIN(level, LEVELS - 1)); l < LEVELS; l++) {
      retained += rings[l].size();
      for (const Entry &e : rings[l])
        if (e.seq < before)
          picked.push_back(&e);
    }
    std::sort(picked.begin(), picked.end(),
              [](const Entry *a, const Entry *b) { return a->seq > b->seq; });

    limit = MAX(1, MIN(limit, (int)MAX_PAGE));
    const bool more = (int)picked.size() > limit;
    if (more)
      picked.resize(limit);

    w.kv("retained", (long long)retained).kv("more", more);
    if (!picked.empty())
      w.kv("oldest", (long long)picked.back()->seq);
    writeList(w, picked);
  }

private:
  enum { COLLAPSE_S = 4 * 3600, HORIZON_S = 600, LIMIT = 50 };

  static std::string statusName(int status) {
    const std::vector<std::string> &names = AIS::LookupTable_nav_status;
    return status >= 0 && status < (int)names.size() ? names[status]
                                                     : std::to_string(status);
  }

  // the entry keeps the sequence the stream gave the event, so a ticker's
  // `since` and a history page run on one number
  void push(Entry e) {
    std::vector<Entry> &r = rings[(int)e.level];
    for (Entry &have : r)
      if (e.kind != Kind::ENTER && e.kind != Kind::LEAVE && have.kind == e.kind &&
          have.from == e.from && have.to == e.to && have.text == e.text &&
          e.time - have.time <= COLLAPSE_S) {
        have.count++;
        have.time = e.time;
        return;
      }
    e.first = e.time;
    const int l = (int)e.level;
    if (r.size() < capacity(l))
      r.push_back(e);
    else {
      r[head[l]] = e;
      head[l] = (head[l] + 1) % capacity(l);
    }
  }

  // the names the ticker has always used
  static const char *kindName(Kind k) {
    switch (k) {
    case Kind::SAFETY:
      return "safety";
    case Kind::DESTINATION:
      return "destination";
    case Kind::STATUS:
      return "status";
    case Kind::DRAUGHT:
      return "draught";
    case Kind::ENTER:
      return "place_enter";
    case Kind::LEAVE:
      return "place_exit";
    default:
      return "safety";
    }
  }

  // the array both readers end with, so a history row and a ticker row are the
  // same object
  void writeList(JSON::Writer &w, const std::vector<const Entry *> &picked) const {
    w.key("events").beginArray();
    for (const Entry *e : picked) {
      w.beginObject()
          .kv("seq", (long long)e->seq)
          .kv("t", (long long)e->time)
          .kv("first", (long long)e->first)
          .kv("format", "ticker-v1")
          .kv("level", (int)e->level)
          .kv("kind", kindName(e->kind))
          .kv("mmsi", (long long)e->from);
      if (isValidCoord(e->lat, e->lon))
        w.kv("lat", e->lat).kv("lon", e->lon);
      if (e->count > 1)
        w.kv("count", e->count);
      w.kv("text", formatText(*e)).endObject();
    }
    w.endArray();
  }

  static size_t capacity(int level) {
    static const size_t c[LEVELS] = {128, 64, 64};
    return c[level];
  }
  std::vector<Entry> rings[LEVELS];
  size_t head[LEVELS] = {0, 0, 0};
  uint64_t last_seq = 0;
};
