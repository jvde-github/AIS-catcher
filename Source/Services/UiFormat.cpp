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

#include "UiFormat.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "Geodesy.h"

namespace Services {

void UiFormat::shipBody(JSON::Writer &w, const Ship &ship, long int delta_time,
                        bool station_known) {
  w.kv("mmsi", ship.mmsi);

  if (isValidCoord(ship.lat, ship.lon)) {
    w.kv("lat", ship.lat).kv("lon", ship.lon);
    if (station_known && ship.distance != DISTANCE_UNDEFINED)
      w.kv("distance", ship.distance).kv("bearing", ship.angle);
    else
      w.kv_null("distance").kv_null("bearing");
  } else
    w.kv_null("lat").kv_null("lon").kv_null("distance").kv_null("bearing");

  w.kv_unless("level", ship.level, LEVEL_UNDEFINED)
      .kv("count", ship.count)
      .kv_unless("ppm", ship.ppm, PPM_UNDEFINED)
      .kv("group_mask", ship.group_mask)
      .kv("approx", (bool)ship.getApproximate())
      .kv_unless("heading", ship.heading, HEADING_UNDEFINED)
      .kv_unless("cog", ship.cog, COG_UNDEFINED)
      .kv_unless("speed", ship.speed, SPEED_UNDEFINED)
      .kv_unless("to_bow", ship.to_bow, DIMENSION_UNDEFINED)
      .kv_unless("to_stern", ship.to_stern, DIMENSION_UNDEFINED)
      .kv_unless("to_starboard", ship.to_starboard, DIMENSION_UNDEFINED)
      .kv_unless("to_port", ship.to_port, DIMENSION_UNDEFINED)
      .kv("shiptype", ship.shiptype)
      .kv("mmsi_type", ship.mmsi_type)
      .kv("shipclass", ship.shipclass)
      .kv("validated", ship.getValidated())
      .kv("msg_type", ship.msg_type)
      .kv("channels", ship.getChannels())
      .kv("country", ship.country_code)
      .kv("status", ship.status)
      .kv_unless("draught", ship.draught, DRAUGHT_UNDEFINED)
      .kv_unless("eta_month", (int)ship.month, ETA_MONTH_UNDEFINED)
      .kv_unless("eta_day", (int)ship.day, ETA_DAY_UNDEFINED)
      .kv_unless("eta_hour", (int)ship.hour, ETA_HOUR_UNDEFINED)
      .kv_unless("eta_minute", (int)ship.minute, ETA_MINUTE_UNDEFINED)
      .kv_unless("imo", ship.IMO, IMO_UNDEFINED)
      .kv("callsign", ship.callsign);

  if (ship.getVirtualAid())
    w.kv("shipname", ship.shipname, " [V]");
  else
    w.kv("shipname", ship.shipname);

  w.kv("destination", ship.destination)
      .kv("eni", ship.vin)
      .kv("vendorid", ship.vendorid)
      .kv_unless("model", ship.unit_model, -1)
      .kv_unless("serial", ship.unit_serial, -1)
      .kv("repeat", ship.getRepeat())
      .kv("last_signal", delta_time)
      .kv("last_group", ship.last_group)
      .kv_unless("altitude", ship.altitude, ALT_UNDEFINED)
      .kv_unless("received_stations", ship.received_stations,
                 RECEIVED_STATIONS_UNDEFINED);
}

static void compactDynamic(JSON::Writer &w, const Ship &ship, std::time_t now,
                           unsigned binary_badge, int riding,
                           const std::array<uint32_t, VisitTracker::SLOTS> &place_ids,
                           bool list_stations) {
  w.beginArray().val(ship.mmsi);
  if (isValidCoord(ship.lat, ship.lon))
    w.val(ship.lat).val(ship.lon).val_unless(ship.distance, DISTANCE_UNDEFINED);
  else
    w.val_null().val_null().val_null();

  w.val_unless(ship.heading, HEADING_UNDEFINED)
      .val_unless(ship.cog, COG_UNDEFINED)
      .val_unless(ship.speed, SPEED_UNDEFINED)
      .val(ship.status)
      .val((long long)(now > ship.last_signal ? now - ship.last_signal : 0))
      .val((unsigned long long)ship.flags.getPackedValue())
      .val(ship.shipclass)
      .val(ship.country_code)
      .val(binary_badge)
      .val_unless(riding, -1) // the station riding the vessel
      .beginArray();
  for (auto id : place_ids) {
    if (id == UINT32_MAX)
      break;
    w.val(id);
  }
  w.endArray();
  // the stations that heard it lately, the latest first
  if (list_stations) {
    w.beginArray();
    ship.forEachHeard([&](int station, std::time_t t) {
      if (t + Ship::ROW_WINDOW >= ship.last_signal)
        w.val(station);
    });
    w.endArray();
  }
  w.endArray();
}

// what the ships table lists beyond the map's row: the receiver's measurements
// and the classification the card and the table word themselves
static void compactTable(JSON::Writer &w, const Ship &ship) {
  w.beginArray().val(ship.mmsi);
  if (isValidCoord(ship.lat, ship.lon) && ship.distance != DISTANCE_UNDEFINED &&
      ship.angle != ANGLE_UNDEFINED)
    w.val(ship.angle);
  else
    w.val_null();
  w.val_unless(ship.level, LEVEL_UNDEFINED)
      .val_unless(ship.ppm, PPM_UNDEFINED)
      .val(ship.count)
      .val(ship.msg_type)
      .val(ship.last_group)
      .val(ship.group_mask)
      .val_unless(ship.altitude, ALT_UNDEFINED)
      .val_unless(ship.received_stations, RECEIVED_STATIONS_UNDEFINED)
      .val(ship.mmsi_type)
      .endArray();
}

static void compactStatic(JSON::Writer &w, const Ship &ship) {
  w.beginArray().val(ship.mmsi);

  if (ship.getVirtualAid())
    w.val(ship.shipname, " [V]");
  else
    w.val(ship.shipname);

  w.val(ship.callsign)
      .val(ship.destination)
      .val(ship.shiptype)
      .val_unless(ship.IMO, IMO_UNDEFINED)
      .val_unless(ship.to_bow, DIMENSION_UNDEFINED)
      .val_unless(ship.to_stern, DIMENSION_UNDEFINED)
      .val_unless(ship.to_port, DIMENSION_UNDEFINED)
      .val_unless(ship.to_starboard, DIMENSION_UNDEFINED)
      .val_unless(ship.draught, DRAUGHT_UNDEFINED)
      .val_unless((int)ship.month, ETA_MONTH_UNDEFINED)
      .val_unless((int)ship.day, ETA_DAY_UNDEFINED)
      .val_unless((int)ship.hour, ETA_HOUR_UNDEFINED)
      .val_unless((int)ship.minute, ETA_MINUTE_UNDEFINED)
      .val(ship.vin)
      .val(ship.vendorid)
      .val_unless(ship.unit_model, -1)
      .val_unless(ship.unit_serial, -1)
      .endArray();
}

std::string UiFormat::shipsCompact(DB &db, bool full, std::time_t since,
                               const std::string &configVersion, const DB::Keep &keep,
                               const StationRegistry::Station *at,
                               bool list_stations) {
  std::lock_guard<std::mutex> lock(db.mtx);

  if (since <= db.full_refresh_at)
    since = 0;
  std::time_t now = time(nullptr);

  const int count = db.keptCount(keep);

  db.content.clear();
  {
    JSON::Writer w(db.content, 65536);

    w.beginObject()
        .kv("config_version", configVersion)
        .kv("place_version", db.place_markers.version())
        .kv("full", since == 0)
        .kv("count", count)
        .kv("time", now)
        .kv("timeout", db.time_history);
    if (at) {
      if (isValidCoord(at->lat, at->lon))
        w.key("station").beginObject().kv("lat", at->lat).kv("lon", at->lon).kv("mmsi", at->mmsi).kv("gps", false).endObject();
    } else if (db.latlon_share && isValidCoord(db.station_lat, db.station_lon))
      w.key("station")
          .beginObject()
          .kv("lat", db.station_lat)
          .kv("lon", db.station_lon)
          .kv("mmsi", db.own_mmsi)
          .kv("gps", db.gps_position)
          .endObject();

    // --- Pass 1: dynamic array ---
    w.key("dynamic").beginArray();
    db.forEachRecentUnlocked(
        now, full, since, [&](int ptr, const Ship &ship, long int) {
          if (keep && !keep(ship))
            return;
          compactDynamic(w, ship, now, db.binary.badge(ship.mmsi, now),
                                   db.stations.idFor(ship.mmsi),
                                   db.visits.insideIds(ptr, db.place_markers.index().get()),
                                   list_stations);
        });
    w.endArray(); // dynamic

    // --- Pass 2: static array ---
    w.key("static").beginArray();
    db.forEachRecentUnlocked(now, full, since,
                          [&](int, const Ship &ship, long int) {
                            if ((since == 0 || ship.last_static_signal >= since) && (!keep || keep(ship)))
                              compactStatic(w, ship);
                          });
    w.endArray(); // static

    w.endObject();
    w.raw("\n\n");
  }
  return db.content;
}

std::string UiFormat::shipsTable(DB &db, std::time_t since, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  if (since <= db.full_refresh_at)
    since = 0;
  std::time_t now = time(nullptr);

  db.content.clear();
  {
    JSON::Writer w(db.content, 16384);
    w.beginObject()
        .kv("time", now)
        .kv("full", since == 0)
        .key("rows")
        .beginArray();
    db.forEachRecentUnlocked(
        now, false, since,
        [&](int, const Ship &ship, long int) {
          if (!keep || keep(ship))
            compactTable(w, ship);
        });
    w.endArray().endObject();
  }
  return db.content;
}

std::string UiFormat::ships(DB &db, bool full, const StationRegistry::Station *at, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);

  const int count = db.keptCount(keep);

  db.content.clear();
  {
    JSON::Writer w(db.content, 65536);
    w.beginObject().kv("count", count);

    if (at) {
      if (isValidCoord(at->lat, at->lon))
        w.key("station").beginObject().kv("lat", at->lat).kv("lon", at->lon).kv("mmsi", at->mmsi).kv("gps", false).endObject();
    } else if (db.latlon_share)
      w.key("station")
          .beginObject()
          .kv("lat", db.station_lat)
          .kv("lon", db.station_lon)
          .kv("mmsi", db.own_mmsi)
          .kv("gps", db.gps_position)
          .endObject();

    w.key("ships").beginArray();

    std::time_t now = time(nullptr);
    db.forEachRecentUnlocked(
        now, full, 0, [&](int ptr, const Ship &ship, long int delta_time) {
          if (keep && !keep(ship))
            return;
          w.beginObject();
          shipBody(w, ship, delta_time,
                             isValidCoord(db.station_lat, db.station_lon));
          db.destinations.writeMatched(w, ptr);
          w.endObject();
        });

    w.endArray().kv("error", false).endObject().raw("\n\n");
  }
  return db.content;
}

std::string UiFormat::changes(DB &db, int mmsi) {
  std::lock_guard<std::mutex> lock(db.mtx);

  std::string content;
  JSON::Writer w(content);
  voyageChanges(db, w, db.ships.find((uint32_t)mmsi));
  w.finish();
  return content;
}

std::string UiFormat::ship(DB &db, int mmsi) {
  return vessel(db, (uint32_t)mmsi, [](JSON::Writer &, const Ship &, int) {});
}

std::string UiFormat::message(DB &db, uint32_t mmsi) {
  std::lock_guard<std::mutex> lock(db.mtx);

  int ptr = db.ships.find(mmsi);
  if (ptr == DB::SHIP_NIL)
    return "";

  return db.ships[ptr].msg;
}

void UiFormat::voyageChanges(DB &db, JSON::Writer &w, int ptr) {
  w.beginArray();
  db.changes.writeEntries(w, ptr);
  if (ptr != DB::SHIP_NIL)
    db.visits.writeChanges(w, ptr, db.place_markers.index().get());
  w.endArray();
}

std::string UiFormat::allPaths(DB &db, const DB::Keep &keep, std::time_t oldest) {
  std::lock_guard<std::mutex> lock(db.mtx);

  db.content.clear();
  {
    JSON::Writer w(db.content, 65536);
    w.beginObject();

    std::time_t now = time(nullptr);
    std::time_t floor = MAX(db.pathFloor(now), oldest);
    db.forEachRecentUnlocked(now, false, 0,
                          [&](int ptr, const Ship &ship, long int) {
                            if (keep && !keep(ship))
                              return;
                            w.key(ship.mmsi);
                            singlePath(db, ptr, w, floor);
                          });
    w.endObject().raw("\n\n");
  }
  return db.content;
}

// Points are newest first, so the walk skips past the window, emits while it
// overlaps and stops once clear of it. A dwell point is emitted by every window
// its [time, end] span touches. A windowed walk (until > 0) also emits the
// first point wholly before the window — where the vessel was when it opened —
// without which a chunk cannot draw a ship that last reported before it began.
void UiFormat::singlePath(DB &db, int ptr, JSON::Writer &w, std::time_t since,
                                    std::time_t until, std::time_t carry_floor) {
  auto emit = [&](const PathStore::Point &p) {
    w.beginArray()
        .val(p.lat)
        .val(p.lon)
        .val(p.time)
        .val(p.end())
        .val_unless(p.sog, PathStore::NA)
        .val_unless(p.cog, PathStore::NA)
        .val_unless(p.hdg, PathStore::NA)
        .endArray();
  };

  w.beginArray();
  for (uint32_t r = db.paths.tail(ptr); PathStore::isPoint(r);
       r = db.paths.at(r).prev) {
    const PathStore::Point &p = db.paths.at(r);
    if ((std::time_t)p.end() < since) {
      if (until > 0 && (std::time_t)p.end() >= carry_floor)
        emit(p);
      break;
    }

    if (until <= 0 || (std::time_t)p.time <= until)
      emit(p);
  }
  w.endArray();
}

std::string UiFormat::allPathsSince(DB &db, std::time_t since, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);

  since = MAX(since, db.pathFloor(time(nullptr)));

  db.content.clear();
  {
    JSON::Writer w(db.content, 65536);
    w.beginObject();

    db.forEachRecentUnlocked(time(nullptr), true, since,
                          [&](int ptr, const Ship &ship, long int) {
                            if (db.paths.hasSince(ptr, since) && (!keep || keep(ship))) {
                              w.key(ship.mmsi);
                              singlePath(db, ptr, w, since);
                            }
                          });
    w.endObject().raw("\n\n");
  }
  return db.content;
}

std::string UiFormat::replayInfo(DB &db, std::time_t block, std::time_t oldest_shown) {
  std::lock_guard<std::mutex> lock(db.mtx);

  db.content.clear();
  {
    JSON::Writer w(db.content, 256);

    std::time_t now = time(nullptr);

    // bounds of the replayable timeline, 0 when empty; `newest` trails
    // `now` while the feed is quiet
    uint32_t oldest = 0, newest = 0;
    db.ships.forEach([&](int ptr) {
      uint32_t h = db.paths.head(ptr);
      if (PathStore::isPoint(h)) {
        uint32_t t = db.paths.at(h).time;
        uint32_t e = db.paths.at(db.paths.tail(ptr)).end();
        if (oldest == 0 || t < oldest)
          oldest = t;
        if (e > newest)
          newest = e;
      }
      return true;
    });

    // the client only asks for blocks within the bounds it is given
    std::time_t cutoff = MAX(db.pathFloor(now), oldest_shown);
    if (oldest && (std::time_t)oldest < cutoff)
      oldest = (uint32_t)cutoff;

    w.beginObject()
        .kv("now", now)
        .kv("oldest", oldest)
        .kv("newest", newest)
        .kv("block", block)
        .kv("granularity", (int)PathStore::GRANULARITY)
        .kv("dwell_gap", (int)PathStore::DWELL_GAP)
        .kv("point_format", 2)
        .endObject();
    w.raw("\n\n");
  }
  return db.content;
}

// Per-ship styling, sent once so the chunks stay geometry. Read straight off
// the record rather than through the static-signal gate: a name captured hours
// ago is what a replay of that period wants.
std::string UiFormat::replayShips(DB &db, std::time_t since, std::time_t lookback, const DB::Keep &keep, std::time_t oldest) {
  return replayObjects(db, 
      since, lookback, 0,
      [&keep](JSON::Writer &w, int, const Ship &ship, std::time_t) {
        if (keep && !keep(ship))
          return;
        w.key(ship.mmsi)
            .beginObject()
            .kv("c", ship.shipclass)
            .kv("n", ship.shipname)
            .kv("f", ship.country_code)
            .kv_unless("t", ship.shiptype, 0);

        // all-zero is how AtoN and many Class B units say "not applicable"
        if (ship.to_bow != DIMENSION_UNDEFINED &&
            ship.to_stern != DIMENSION_UNDEFINED &&
            ship.to_port != DIMENSION_UNDEFINED &&
            ship.to_starboard != DIMENSION_UNDEFINED &&
            ship.to_bow + ship.to_stern > 0 &&
            ship.to_port + ship.to_starboard > 0)
          w.key("d")
              .beginArray()
              .val(ship.to_bow)
              .val(ship.to_stern)
              .val(ship.to_port)
              .val(ship.to_starboard)
              .endArray();

        w.endObject();
      }, oldest);
}

std::string UiFormat::replay(DB &db, std::time_t since, std::time_t until,
                              std::time_t lookback) {
  return replay(db, since, until, DB::Keep(), lookback);
}

std::string UiFormat::replay(DB &db, std::time_t since, std::time_t until, const DB::Keep &keep,
                              std::time_t lookback, std::time_t oldest) {
  return replayObjects(db, since, lookback, until,
                             [&db, until, oldest, &keep](JSON::Writer &w, int ptr,
                                                          const Ship &ship, std::time_t from) {
                               if (keep && !keep(ship))
                                 return;
                               w.key(ship.mmsi);
                               singlePath(db, ptr, w, from, until, oldest);
                             }, oldest);
}

std::string UiFormat::path(DB &db, uint32_t mmsi, std::time_t oldest) {
  std::lock_guard<std::mutex> lock(db.mtx);
  int ptr = db.ships.find(mmsi);

  db.content.clear();
  {
    JSON::Writer w(db.content, 1024);
    if (ptr != DB::SHIP_NIL)
      singlePath(db, ptr, w, MAX(db.pathFloor(time(nullptr)), oldest));
    else
      w.beginArray().endArray();
  }
  return db.content;
}

// What lies around a point: the nearest ships, stations and places in one
// answer. A vessel, a receiver and a harbour all ask the same question about
// their own position, and the ship walk is the expensive one, so it is asked
// once rather than again on every tab.
// isValidCoord turns away (0,0) and the two sentinels and nothing else: a
// seeded record can hold a NaN, or a position nowhere on the globe. A NaN range
// then compares false against everything, which makes the sort intransitive.
static bool placedOnGlobe(float lat, float lon) {
  return isValidCoord(lat, lon) && lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f; // false for a NaN, as written
}

std::string UiFormat::nearby(DB &db, float lat, float lon, uint32_t skip,
                              int skip_station, const DB::Keep &keep,
                              bool with_stations) {
  static const float HORIZON_NM = 99.0f;
  static const size_t KEEP = 10;
  // written this way round so a NaN fails it; a plain range test would pass one
  if (!isValidCoord(lat, lon) || !(lat >= -90.0f && lat <= 90.0f) ||
      !(lon >= -180.0f && lon <= 180.0f))
    return "{\"error\":\"Invalid position\"}";
  std::lock_guard<std::mutex> lock(db.mtx);
  const auto now = time(nullptr);

  struct Near {
    uint32_t key; // ship slot, station id, or place runtime id
    float range;
    int bearing;
  };
  // ties break on the key so two answers to the same question agree
  auto nearer = [](const Near &a, const Near &b) {
    return a.range != b.range ? a.range < b.range : a.key < b.key;
  };
  auto consider = [&](std::vector<Near> &list, float &reach, uint32_t key,
                      float range, int bearing) {
    list.push_back({key, range, bearing});
    std::sort(list.begin(), list.end(), nearer);
    if (list.size() > KEEP) {
      list.pop_back();
      reach = list.back().range; // nothing farther can join now
    }
  };
  auto off = [&](float plat, float plon, float &range, int &bearing) {
    Util::Geodesy::distanceBearing(lat, lon, plat, plon, range, bearing);
  };

  std::vector<Near> ships_near, stations_near, places_near;
  ships_near.reserve(KEEP + 1);
  stations_near.reserve(KEEP + 1);
  places_near.reserve(KEEP + 1);
  size_t ship_total = 0, station_total = 0, place_total = 0;

  float reach = HORIZON_NM;
  db.forEachRecentUnlocked(now, false, 0, [&](int ptr, const Ship &ship, long) {
    if (ship.mmsi == skip || ship.shipclass >= CLASS_PLANE ||
        !placedOnGlobe(ship.lat, ship.lon) || (keep && !keep(ship)))
      return;
    float range;
    int bearing;
    off(ship.lat, ship.lon, range, bearing);
    if (!(range <= reach)) // a NaN fails this, where `>` would let it through
      return;
    consider(ships_near, reach, (uint32_t)ptr, range, bearing);
  });
  ship_total = ships_near.size();

  reach = HORIZON_NM;
  db.stations.forEach([&](const StationRegistry::Station &s) {
    if (!with_stations || s.id == skip_station || !placedOnGlobe(s.lat, s.lon))
      return;
    float range;
    int bearing;
    off(s.lat, s.lon, range, bearing);
    if (!(range <= reach))
      return;
    consider(stations_near, reach, (uint32_t)s.id, range, bearing);
  });
  station_total = stations_near.size();

  reach = HORIZON_NM;
  const auto &index = db.place_markers.index();
  if (index)
    for (const auto &entry : index->entries) {
      // ports only, a port inside a larger one included: an anchorage or a
      // berth is a part of a port rather than an answer to "which port is
      // near", and a waterway is not a destination at all
      if (!entry.live() || !entry.metadata->is(PlaceKind::Port) ||
          !placedOnGlobe((float)entry.lat, (float)entry.lon))
        continue;
      float range;
      int bearing;
      off((float)entry.lat, (float)entry.lon, range, bearing);
      if (!(range <= reach))
        continue;
      consider(places_near, reach, entry.id, range, bearing);
    }
  place_total = places_near.size();

  std::string out;
  JSON::Writer w(out);
  w.beginObject()
      .kv("time", (long long)now)
      .kv("lat", lat)
      .kv("lon", lon)
      .key("counts")
      .beginObject()
      .kv("ships", (unsigned long long)ship_total)
      .kv("stations", (unsigned long long)station_total)
      .kv("places", (unsigned long long)place_total)
      .endObject();

  w.key("ships").beginArray();
  for (const auto &row : ships_near) {
    const Ship &s = db.ships[row.key];
    w.beginObject()
        .kv("mmsi", s.mmsi)
        .kv("shipname", s.shipname)
        .kv("country", s.country_code)
        .kv("shipclass", s.shipclass)
        .kv("timestamp", (long long)s.last_signal)
        .kv("range", row.range)
        .kv("bearing", row.bearing);
    if (s.speed == SPEED_UNDEFINED)
      w.kv_null("speed");
    else
      w.kv("speed", s.speed);
    if (s.cog == COG_UNDEFINED)
      w.kv_null("cog");
    else
      w.kv("cog", s.cog);
    w.endObject();
  }
  w.endArray();

  w.key("stations").beginArray();
  for (const auto &row : stations_near) {
    const auto *s = db.stations.find((int)row.key);
    if (!s)
      continue;
    w.beginObject()
        .kv("id", s->id)
        .kv("name", s->name)
        .kv("country", s->country)
        .kv("online", s->online)
        .kv("last", (long long)s->last)
        // the station's last message where the host counts it: fresher than
        // last, which the station list refreshes only every few minutes
        .kv("heard", (long long)(db.station_heard ? db.station_heard(s->id) : 0))
        .kv("range", row.range)
        .kv("bearing", row.bearing)
        .endObject();
  }
  w.endArray();

  w.key("places").beginArray();
  for (const auto &row : places_near) {
    const auto *entry = index->find(row.key);
    if (!entry)
      continue;
    const auto *parent = index->find(entry->parent);
    w.beginObject()
        .kv("runtime_id", entry->id)
        .kv("label", entry->metadata->name)
        .kv("place_type", entry->metadata->type())
        .kv("code", entry->metadata->code)
        .kv("part_of", entry->metadata->partOf)
        .kv("parent_name", parent ? parent->metadata->name : std::string())
        .kv("has_geometry", entry->polygons && !entry->polygons->empty())
        .kv("range", row.range)
        .kv("bearing", row.bearing);
    if (entry->metadata->is(PlaceKind::Port))
      w.kv("country", entry->metadata->country);
    w.endObject();
  }
  w.endArray();
  w.kv("place_version", index ? index->version : std::string());
  w.endObject();
  // here, not in the destructor: that runs after `out` has been moved into the
  // return value, leaving finish() to measure against a buffer it no longer has
  w.finish();
  return out;
}

std::string UiFormat::placeShips(DB &db, uint32_t id, const std::string &version,
                                  const std::string &code,
                                  const std::string &tab, unsigned hours,
                                  unsigned offset, unsigned limit, const DB::Keep &visible) {
  if (!limit || limit > 100 || offset > 10000000)
    return "{\"error\":\"Invalid place pagination\"}";
  std::lock_guard<std::mutex> lock(db.mtx);
  const auto &index = db.place_markers.index();
  const auto *place =
      index ? (id == UINT32_MAX ? index->findPort(code) : index->find(id))
            : nullptr;
  if (id != UINT32_MAX && (!place || version != index->version))
    return "{\"error\":\"Place changed; reopen its marker.\"}";
  const bool port = place ? place->metadata->is(PlaceKind::Port) : !code.empty();
  // A port, a berth or an anchorage is somewhere a vessel stops, so its list is
  // vessels that have lain still in it: an aid to navigation moored there by
  // definition, a base station on the quay and a ship merely steaming through
  // are all noise. A custom area is usually a stretch of water rather than a
  // destination, where passing through is the whole of what happens.
  const bool berthing = !place || place->metadata->requiresStop();
  auto vessel = [](const Ship &s) {
    return s.shipclass < CLASS_PLANE; // planes, stations, aids, EPIRBs are not
  };
  // closest is everyone's - a place with no outline can answer nothing else -
  // and expected is a port's, because it reads reported destinations
  const char *tabs[] = {"inside", "left", "arrived", "visits", "closest",
                        "expected"};
  const int ntabs = port ? 6 : 5;
  int selected = -1;
  for (int i = 0; i < ntabs; ++i)
    if (tab == tabs[i])
      selected = i;
  if (selected < 0 || (!place && code.empty()) || hours > 8760)
    return "{\"error\":\"Invalid place query\"}";
  const std::string destination = place ? place->metadata->code : code;
  const auto now = time(nullptr);
  const auto cutoff = hours ? now - hours * 3600LL : 0;
  std::array<size_t, 6> counts{};
  struct Row {
    uint32_t slot;
    const VisitTracker::Visit *visit;
    std::time_t stamp;
    float range; // nautical miles, closest only; negative elsewhere
  };
  std::vector<Row> rows;
  const size_t keep = (size_t)offset + limit;
  rows.reserve(std::min<size_t>(keep + 1, 101));
  const auto newer = [&](const Row &a, const Row &b) {
    if (a.stamp != b.stamp) return a.stamp > b.stamp;
    if (db.ships[a.slot].mmsi != db.ships[b.slot].mmsi)
      return db.ships[a.slot].mmsi < db.ships[b.slot].mmsi;
    // Repeat visits can share a timestamp; their retained slot breaks ties.
    return a.visit < b.visit;
  };
  // Keep only the prefix needed for this page, with the oldest row on top.
  auto add = [&](int kind, uint32_t slot, const VisitTracker::Visit *visit,
                 std::time_t stamp) {
    ++counts[kind];
    if (kind != selected)
      return;
    rows.push_back({slot, visit, stamp, -1.0f});
    std::push_heap(rows.begin(), rows.end(), newer);
    if (rows.size() > keep) {
      std::pop_heap(rows.begin(), rows.end(), newer);
      rows.pop_back();
    }
  };
  if (place)
    db.visits.forEach(place->id, [&](uint32_t slot) {
      if (visible && !visible(db.ships[slot]))
        return true;
      const VisitTracker::Visit *inside = nullptr, *left = nullptr,
                                *arrived = nullptr;
      for (const auto &v : db.visits.record(slot).visits) {
        // a stay that is over and was never a call is not history
        if (v.id != place->id || !v.shown(berthing))
          continue;
        const auto entry = v.entryTime(), exit = v.exitTime();
        if (v.inside() && (!inside || entry > inside->entryTime()))
          inside = &v;
        if (exit && exit >= cutoff && (!left || exit > left->exitTime()))
          left = &v;
        // An arrival is a ship that is still here: one that has since left is
        // a departure and nothing else, so the two lists never name it twice.
        if (v.inside() && entry && entry >= cutoff &&
            (!arrived || entry > arrived->entryTime()))
          arrived = &v;
        // An interval remains useful when its ends are observation bounds.
        const auto stamp = MAX(v.entered, v.exited);
        if (v.inside() || (stamp && stamp >= cutoff))
          add(3, slot, &v, v.entered ? v.entered : v.exited);
      }
      // present means heard lately, the same rule the export applies
      if (inside && DB::insideNow(db.ships[slot], *inside, berthing, now))
        add(0, slot, inside, db.ships[slot].last_signal);
      if (left)
        add(1, slot, left, left->exitTime());
      if (arrived)
        add(2, slot, arrived, arrived->entryTime());
      return true;
    });
  if (port)
    db.destinations.forEach(destination, [&](uint32_t slot) {
      if (visible && !visible(db.ships[slot]))
        return true;
      if (place && db.visits.insideOf(slot, place->id))
        return true;
      add(5, slot, nullptr, db.ships[slot].last_signal);
      return true;
    });

  // Closest: what lies around the place, whether or not it has an outline. For
  // a drawn place the ships already inside it are left out, so this answers
  // "what is just outside" - the question an outline drawn too tight raises.
  // The full set within the horizon supplies the count; retain the nearest
  // prefix needed by the requested page.
  // Ships only, wherever the place is: a buoy or a mast is near by
  // construction, never news.
  if (place && isValidCoord((float)place->lat, (float)place->lon)) {
    static const float HORIZON_NM = 99.0f;
    std::vector<Row> nearest;
    nearest.reserve(std::min<size_t>(keep + 1, 101));
    const auto nearer = [&](const Row &a, const Row &b) {
      return a.range != b.range ? a.range < b.range
                                : db.ships[a.slot].mmsi < db.ships[b.slot].mmsi;
    };
    db.forEachRecentUnlocked(now, false, 0, [&](int ptr, const Ship &ship, long) {
      if (visible && !visible(ship))
        return;
      if (!placedOnGlobe(ship.lat, ship.lon) || !vessel(ship))
        return;
      float range;
      int bearing;
      Util::Geodesy::distanceBearing((float)place->lat, (float)place->lon,
                                     ship.lat, ship.lon, range, bearing);
      if (!(range <= HORIZON_NM))
        return;
      if (db.visits.insideOf(ptr, place->id))
        return; // inside is its own tab
      ++counts[4];
      if (selected != 4) return;
      nearest.push_back({(uint32_t)ptr, nullptr, ship.last_signal, range});
      std::push_heap(nearest.begin(), nearest.end(), nearer);
      if (nearest.size() > keep) {
        std::pop_heap(nearest.begin(), nearest.end(), nearer);
        nearest.pop_back();
      }
    });
    if (selected == 4) {
      std::sort_heap(nearest.begin(), nearest.end(), nearer);
      rows = std::move(nearest);
    }
  }
  if (selected != 4) std::sort_heap(rows.begin(), rows.end(), newer);
  std::string out;
  JSON::Writer w(out);
  w.beginObject()
      .kv("time", (long long)now)
      .kv("tab", tab)
      .kv("has_geometry", place && place->polygons && !place->polygons->empty())
      .kv("total", (unsigned long long)counts[selected])
      .kv("offset", offset)
      .kv("limit", limit)
      .key("counts")
      .beginObject();
  for (int i = 0; i < ntabs; ++i)
    w.kv(tabs[i], (unsigned long long)counts[i]);
  w.endObject().key("ships").beginArray();
  for (size_t i = std::min<size_t>(offset, rows.size()); i < rows.size(); ++i) {
    const auto &row = rows[i];
    const auto &s = db.ships[row.slot];
    w.beginObject()
        .kv("mmsi", s.mmsi)
        .kv("shipname", s.shipname)
        .kv("country", s.country_code)
        .kv("shipclass", s.shipclass)
        .kv("timestamp", (long long)s.last_signal);
    if (row.range >= 0)
      w.kv("range", row.range);
    if (s.speed == SPEED_UNDEFINED)
      w.kv_null("speed");
    else
      w.kv("speed", s.speed);
    if (s.cog == COG_UNDEFINED)
      w.kv_null("cog");
    else
      w.kv("cog", s.cog);
    if (row.visit) {
      const auto &v = *row.visit;
      if (v.entryTime())
        w.kv("entered", v.entryTime());
      else
        w.kv_null("entered");
      if (v.exitTime())
        w.kv("exited", v.exitTime());
      else
        w.kv_null("exited");
      // An unobserved crossing is told as a bound beside the exact fields. A
      // pending crossing is not a settled bound.
      if (!v.entryTime() && v.entered)
        w.kv("entered_before", v.entered);
      if (!v.exitTime() && v.exited && !v.inside())
        w.kv("exited_after", v.exited);
      if (v.inside())
        w.kv("observed_until", db.visits.record(row.slot).last);
      w.kv("inside", v.inside());
    }
    w.endObject();
  }
  w.endArray().endObject();
  // Finalize before return can move the backing string.
  w.finish();
  return out;
}

// The ticker's rows, as ticker-v1 words them: who, and what in markers
// the page renders, the received text escaped so it cannot add its own
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

static std::string tickerText(const EventRing::Entry &e) {
  std::string out = e.label.empty() && e.level >= Tracking::Level::NOTICE ? "⚠ " : "";
  out += "**" + escapeText(displayName(e.from, e.from_name)) + "**";
  if (e.kind == Tracking::Kind::ENTER || e.kind == Tracking::Kind::LEAVE)
    return out +
           (e.kind == Tracking::Kind::ENTER ? " · ::entered:: [[" : " · ::left:: [[") +
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

// the array both readers end with, so a history row and a ticker row are the
// same object
static void tickerList(JSON::Writer &w, const std::vector<const EventRing::Entry *> &picked) {
  w.key("events").beginArray();
  for (const EventRing::Entry *e : picked) {
    w.beginObject()
        .kv("seq", (long long)e->seq)
        .kv("t", (long long)e->time)
        .kv("first", (long long)e->first)
        .kv("format", "ticker-v1")
        .kv("level", (int)e->level)
        .kv("kind", Tracking::journalName(e->kind))
        .kv("mmsi", (long long)e->from);
    if (isValidCoord(e->lat, e->lon))
      w.kv("lat", e->lat).kv("lon", e->lon);
    if (e->count > 1)
      w.kv("count", e->count);
    w.kv("text", tickerText(*e)).endObject();
  }
  w.endArray();
}

// the broadcasts a reader may see: those of the vessels it may see; nothing to ask when it sees all
static std::function<bool(uint32_t)> keptSender(DB &db, const DB::Keep &keep) {
  if (!keep)
    return nullptr;
  return [&db, &keep](uint32_t mmsi) { return db.keepHeld(mmsi, keep); };
}

std::string UiFormat::binaryMessages(DB &db, std::time_t since, uint64_t marker,
                                      uint32_t owner, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  db.content.clear();
  {
    JSON::Writer w(db.content, 4096);
    db.binary.writeJSON(w, time(nullptr), since, marker, owner, keptSender(db, keep));
  }
  return db.content;
}

std::string UiFormat::events(DB &db, uint64_t since, int level, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  db.content.clear();
  {
    JSON::Writer w(db.content, 4096);
    std::time_t now = time(nullptr);
    w.beginObject().kv("time", now).kv("seq", (long long)db.ring.sequence());
    tickerList(w, db.ring.since(since, level, now, [&](uint32_t m) { return db.keepHeld(m, keep); }));
    w.endObject();
  }
  return db.content;
}

std::string UiFormat::eventHistory(DB &db, uint64_t before, int level, int limit,
                                    const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  db.content.clear();
  {
    JSON::Writer w(db.content, 8192);
    w.beginObject()
        .kv("time", (long long)time(nullptr))
        .kv("seq", (long long)db.ring.sequence());
    std::size_t retained = 0;
    bool more = false;
    const auto picked = db.ring.before(before, level, limit, [&](uint32_t m) { return db.keepHeld(m, keep); }, retained, more);
    w.kv("retained", (long long)retained).kv("more", more);
    if (!picked.empty())
      w.kv("oldest", (long long)picked.back()->seq);
    tickerList(w, picked);
    w.endObject();
  }
  return db.content;
}

std::string UiFormat::object(DB &db, const std::string &key, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  db.content.clear();
  {
    JSON::Writer w(db.content, 4096);
    std::time_t now = time(nullptr);
    if (!key.empty() && key[0] == 's') {
      const int id = std::atoi(key.c_str() + 1);
      // a station riding a vessel is where that vessel is, as the map draws it
      const StationRegistry::Station *s = db.stations.find(id);
      const int ptr = s && s->mmsi ? db.ships.find(s->mmsi) : DB::SHIP_NIL;
      const bool riding = ptr != DB::SHIP_NIL && isValidCoord(db.ships[ptr].lat, db.ships[ptr].lon);
      w.beginObject().kv("time", now);
      db.stations.writeOne(w, id, riding, riding ? db.ships[ptr].lat : LAT_UNDEFINED, riding ? db.ships[ptr].lon : LON_UNDEFINED,
                        db.station_heard ? db.station_heard(id) : 0);
      w.endObject();
    } else
      db.binary.writeJSON(w, now, 0, std::strtoull(key.c_str(), nullptr, 16), 0, keptSender(db, keep));
  }
  return db.content;
}

std::string UiFormat::mapObjects(DB &db, uint64_t since, bool with_stations, const std::function<bool(int)> &shown, const DB::Keep &keep) {
  std::lock_guard<std::mutex> lock(db.mtx);
  db.content.clear();
  {
    JSON::Writer w(db.content, 4096);
    std::time_t now = time(nullptr);
    db.binary.refresh(now);
    w.beginObject()
        .kv("place_version", db.place_markers.version())
        .kv("time", now)
        .kv("seq", (long long)db.binary.sequence())
        .key("objects")
        .beginArray();
    db.binary.writeMarkerRows(w, since, keptSender(db, keep));
    if (with_stations)
      db.stations.writeRows(w, since, db.station_heard, shown);
    db.place_markers.writeRows(w, since);
    w.endArray().key("removed").beginArray();
    db.binary.writeRemoved(w, since);
    if (with_stations)
      db.stations.writeRemoved(w, since);
    db.place_markers.writeRemoved(w, since);
    w.endArray().endObject();
  }
  return db.content;
}

// The figures as the statistics page reads them, `hidden` without the range
static void stat(JSON::Writer &w, const MessageStatistics::Values &v, bool empty, bool hidden) {
  int c = v.count - v.exclude;
  bool has_level = c > 0 && v.level_min <= v.level_max;

  w.beginObject();
  w.kv("count", empty ? 0 : v.count);
  w.kv("vessels", empty ? 0 : v.vessels);
  w.key("errors").beginObject();
  const char *names[] = {"flagged", "undersized", "oversized", "checksum"};
  for (int i = 0; i < 4; i++)
    w.kv(names[i], empty ? 0 : v.errors[i]);
  w.endObject();
  if (empty || !has_level) {
    w.kv_null("level_min");
    w.kv_null("level_max");
    w.kv_null("ppm");
  } else {
    w.kv("level_min", v.level_min);
    w.kv("level_max", v.level_max);
    w.kv("ppm", v.ppm / c);
  }
  if (hidden) {
  } else if (empty)
    w.kv_null("dist");
  else
    w.kv("dist", v.distance);

  w.key("channel").beginArray();
  for (int i = 0; i < 4; i++)
    w.val(empty ? 0 : v.channel[i]);
  w.endArray();

  if (!hidden) {
    w.key("radar_a").beginArray();
    for (int i = 0; i < MessageStatistics::RADAR_BUCKETS; i++)
      w.val(empty ? 0.0f : v.radarA[i]);
    w.endArray();

    w.key("radar_b").beginArray();
    for (int i = 0; i < MessageStatistics::RADAR_BUCKETS; i++)
      w.val(empty ? 0.0f : v.radarB[i]);
    w.endArray();
  }

  w.key("msg").beginArray();
  for (int i = 0; i < 28; i++)
    w.val(empty ? 0 : v.msg[i]);
  w.endArray();

  w.endObject();
}

// a history as the plots read it: when its slot under way began, its
// interval, and newest first each slot's offset from it and its figures
template <typename H>
static void historyJSON(JSON::Writer &w, H &history, long int interval, bool hidden) {
  struct Slot {
    long int start;
    MessageStatistics::Values values;
    bool empty;
  };
  std::vector<Slot> slots;
  history.forEachSlot([&](long int start, MessageStatistics &s, bool empty) {
    slots.push_back(Slot{start, s.values(), empty});
  });

  w.beginObject();
  w.key("now").val(slots.back().start);
  w.key("interval").val(interval);
  w.key("time").beginArray();
  for (std::size_t i = 0; i < slots.size(); i++)
    w.val(-(int)i);
  w.endArray();
  w.key("stat").beginArray();
  for (auto it = slots.rbegin(); it != slots.rend(); ++it)
    stat(w, it->values, it->empty, hidden);
  w.endArray();
  w.endObject();
}

void UiFormat::counters(JSON::Writer &w, Figures &f, bool hidden) {
  const auto last = [&](MessageStatistics &s, bool empty) { stat(w, s.values(), empty, hidden); };
  if (hidden)
    w.kv("range_hidden", true);
  w.key("total");
  stat(w, f.total.values(), false, hidden);
  w.key("session");
  stat(w, f.session.values(), false, hidden);
  w.key("last_day");
  f.day.withLastSlot(last);
  w.key("last_hour");
  f.hour.withLastSlot(last);
  w.key("last_minute");
  f.minute.withLastSlot(last);
  w.kv("msg_rate", f.second.getAverage());
}

void UiFormat::history(JSON::Writer &w, Figures &f, bool hidden) {
  w.beginObject();
  if (hidden)
    w.kv("range_hidden", true);
  w.key("second");
  historyJSON(w, f.second, 1, hidden);
  w.key("minute");
  historyJSON(w, f.minute, 60, hidden);
  w.key("hour");
  historyJSON(w, f.hour, 3600, hidden);
  w.key("day");
  historyJSON(w, f.day, 86400, hidden);
  w.endObject();
}

// The aircraft as the map and the planes list read them
static void planeJSON(JSON::Writer &w, const Plane::ADSB &plane, long int delta_time)
{
    w.beginObject().kv("hexident", plane.hexident);

    if (isValidCoord(plane.lat, plane.lon))
    {
        w.kv("lat", plane.lat).kv("lon", plane.lon)
            .kv_unless("distance", plane.distance, DISTANCE_UNDEFINED)
            .kv_unless("bearing", plane.angle, ANGLE_UNDEFINED);
    }
    else
        w.kv_null("lat").kv_null("lon");

    w.kv_unless("level", plane.signalLevel, LEVEL_UNDEFINED)
        .kv("count", plane.nMessages)
        .kv("group_mask", plane.group_mask)
        .kv_unless("altitude", plane.altitude, ALTITUDE_UNDEFINED)
        .kv_unless("speed", plane.speed, SPEED_UNDEFINED)
        .kv_unless("heading", plane.heading, HEADING_UNDEFINED)
        .kv_unless("vertrate", plane.vertrate, VERT_RATE_UNDEFINED)
        .kv_unless("squawk", plane.squawk, SQUAWK_UNDEFINED)
        .kv_unless("category", plane.category, CATEGORY_UNDEFINED)
        .kv("airborne", plane.airborne)
        .kv("callsign", plane.callsign)
        .kv("message_types", plane.message_types)
        .kv("message_subtypes", plane.message_subtypes)
        .kv("last_group", plane.last_group);

    if (plane.country_code[0] != ' ')
        w.kv("country", {plane.country_code, 2});
    else
        w.kv_null("country");

    w.kv("last_signal", delta_time).endObject();
}

static void planeCompact(JSON::Writer &w, const Plane::ADSB &plane, std::time_t rx)
{
    w.beginArray().val(plane.hexident)
        .val_unless(plane.lat, LAT_UNDEFINED).val_unless(plane.lon, LON_UNDEFINED)
        .val_unless(plane.altitude, ALTITUDE_UNDEFINED).val_unless(plane.speed, SPEED_UNDEFINED)
        .val_unless(plane.heading, HEADING_UNDEFINED).val_unless(plane.vertrate, VERT_RATE_UNDEFINED)
        .val_unless(plane.squawk, SQUAWK_UNDEFINED)
        .val(plane.callsign).val(plane.airborne).val(plane.nMessages).val((long long)rx)
        .val_unless(plane.category, CATEGORY_UNDEFINED).val_unless(plane.signalLevel, LEVEL_UNDEFINED);

    if (plane.country_code[0] != ' ')
        w.val({plane.country_code, 2});
    else
        w.val_null();

    w.val_unless(plane.distance, DISTANCE_UNDEFINED)
        .val(plane.message_types).val(plane.message_subtypes)
        .val(plane.group_mask).val(plane.last_group)
        .val_unless(plane.angle, ANGLE_UNDEFINED).endArray();
}

std::string UiFormat::planes(PlaneDB &db) {
  std::lock_guard<std::mutex> lock(db.mtx);

  std::time_t now = std::time(nullptr);

  db.content.clear();
  {
    JSON::Writer w(db.content, 32768);

    w.beginObject().kv("count", db.table.size()).kv("time", (long long)now).key("planes").beginArray();

    db.forEachRecent(now, 0, [&](const Plane::ADSB &plane, std::time_t rx) {
      planeJSON(w, plane, (long int)(now - rx));
    });

    w.endArray().kv("error", false).endObject().raw("\n\n");
  }
  return db.content;
}

std::string UiFormat::planesCompact(PlaneDB &db, std::time_t since) {
  std::lock_guard<std::mutex> lock(db.mtx);

  std::time_t now = std::time(nullptr);

  db.content.clear();
  {
    JSON::Writer w(db.content, 32768);

    w.beginObject().kv("count", db.table.size()).kv("time", (long long)now).key("values").beginArray();

    db.forEachRecent(now, since, [&](const Plane::ADSB &plane, std::time_t rx) {
      planeCompact(w, plane, rx);
    });

    w.endArray().kv("error", false).endObject().raw("\n\n");
  }
  return db.content;
}

}
