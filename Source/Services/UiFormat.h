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

#include <cstdint>
#include <ctime>
#include <functional>
#include <string>

#include "DB.h"
#include "Figures.h"
#include "PlaneDB.h"
#include "StationRegistry.h"
#include "Writer.h"

namespace Services {

// The viewer's own formats, as its pages have always read them: the ship
// lists, a vessel with its changes and its track, the replay, the ticker, the
// map's objects, what lies around a point or in a place, KML, the statistics
// page and the aircraft. The ship table
// keeps the data and these write it, under its lock, as its friend: the
// formats are the viewer's, the table only holds what they are made of.
struct UiFormat {
  // `at`, when given, is the station the page draws, in place of the DB's own;
  // `list_stations` ends each row with the stations that heard the ship
  static std::string shipsCompact(DB &db, bool full = false, std::time_t since = 0,
                                  const std::string &configVersion = "",
                                  const DB::Keep &keep = DB::Keep(),
                                  const StationRegistry::Station *at = nullptr,
                                  bool list_stations = false);
  static std::string shipsTable(DB &db, std::time_t since = 0, const DB::Keep &keep = DB::Keep());
  // `at`, when given, is the station the page draws, in place of the DB's own
  static std::string ships(DB &db, bool full = false, const StationRegistry::Station *at = nullptr,
                           const DB::Keep &keep = DB::Keep());
  static std::string ship(DB &db, int mmsi);
  static std::string changes(DB &db, int mmsi);
  static std::string message(DB &db, uint32_t mmsi);

  // `oldest`: a view's cap on how far back tracks go, over the tracker's own
  static std::string path(DB &db, uint32_t mmsi, std::time_t oldest = 0);
  static std::string allPaths(DB &db, const DB::Keep &keep = DB::Keep(), std::time_t oldest = 0);
  static std::string allPathsSince(DB &db, std::time_t since, const DB::Keep &keep = DB::Keep());
  static std::string replayInfo(DB &db, std::time_t block, std::time_t oldest = 0);
  static std::string replayShips(DB &db, std::time_t since, std::time_t lookback, const DB::Keep &keep = DB::Keep(),
                                 std::time_t oldest = 0);
  static std::string replay(DB &db, std::time_t since, std::time_t until, const DB::Keep &keep, std::time_t lookback,
                            std::time_t oldest = 0);
  static std::string replay(DB &db, std::time_t since, std::time_t until, std::time_t lookback);

  // a place's lists: who is inside, who left or arrived, the visits, the
  // closest outside, and for a port the vessels bound for it
  static std::string placeShips(DB &db, uint32_t id, const std::string &version, const std::string &code,
                                const std::string &tab, unsigned hours, unsigned offset = 0, unsigned limit = 10,
                                const DB::Keep &keep = DB::Keep());
  // `skip` and `skip_station` are the vessel and the station asking, 0 and -1
  // for none; `with_stations` false keeps the stations, and so where they stand, out
  static std::string nearby(DB &db, float lat, float lon, uint32_t skip, int skip_station,
                            const DB::Keep &keep = DB::Keep(), bool with_stations = true);

  // the ticker: the events after `since`, and a page of those before `before`
  static std::string events(DB &db, uint64_t since, int level, const DB::Keep &keep = DB::Keep());
  static std::string eventHistory(DB &db, uint64_t before, int level, int limit, const DB::Keep &keep = DB::Keep());
  static std::string binaryMessages(DB &db, std::time_t since = 0, uint64_t marker = 0, uint32_t owner = 0);
  // `shown` leaves out a station the reader does not see
  static std::string mapObjects(DB &db, uint64_t since = 0, bool with_stations = true,
                                const std::function<bool(int)> &shown = nullptr);
  // what stands behind an object key: a marker's members, or a station's record
  static std::string object(DB &db, const std::string &key);

  // the statistics page: the totals and the last whole periods, and the plots' histories
  static void counters(JSON::Writer &w, Figures &f);
  static void history(JSON::Writer &w, Figures &f);
  // the aircraft: in full, and as the map's rows
  static std::string planes(PlaneDB &db);
  static std::string planesCompact(PlaneDB &db, std::time_t since = 0);

  // The vessel's record, the same on every host: the ship's body, its
  // message badge and riding station, and what it has reported changing.
  // `extra(w, ship, ptr)` writes what only the caller knows - a hub the
  // stations that heard it, a receiver nothing - before the object closes.
  template <typename F> static std::string vessel(DB &db, uint32_t mmsi, F extra) {
    std::lock_guard<std::mutex> lock(db.mtx);

    int ptr = db.ships.find(mmsi);
    if (ptr == DB::SHIP_NIL)
      return "{}";

    const Ship &ship = db.ships[ptr];
    std::time_t now = time(nullptr);

    db.content.clear();
    {
      JSON::Writer w(db.content, 1024);
      w.beginObject();
      w.kv("time", (long long)now).kv("place_version", db.place_markers.version());
      db.visits.writeVisits(w, ptr, db.place_markers.index().get());
      shipBody(w, ship, (long int)now - (long int)ship.last_signal,
                         isValidCoord(db.station_lat, db.station_lon));
      db.destinations.writeMatched(w, ptr);
      w.kv("binary", db.binary.badge(ship.mmsi, now))
          .kv_unless("station", db.stations.idFor(ship.mmsi), -1)
          .key("changes");
      voyageChanges(db, w, ptr);
      extra(w, ship, ptr);
      w.endObject();
    }
    return db.content;
  }

private:
  static void shipBody(JSON::Writer &w, const Ship &ship, long int delta_time, bool station_known);
  static void voyageChanges(DB &db, JSON::Writer &w, int ptr);
  // `carry_floor`: a windowed walk's point from before the window is left out when it ends before this
  static void singlePath(DB &db, int ptr, JSON::Writer &w, std::time_t since = 0, std::time_t until = 0,
                         std::time_t carry_floor = 0);

  // Shared scaffolding for the replay endpoints: eligibility reaches back by
  // `lookback` past the window start, so a vessel silent since before the
  // window still shows for as long as the viewer keeps it on the map, and
  // `emit` writes the per-ship value. The window start is clamped here, under
  // the lock that guards the horizon it is clamped to, and handed to `emit`;
  // `until` is the window end, 0 for an unbounded one.
  template <typename F>
  static std::string replayObjects(DB &db, std::time_t since, std::time_t lookback, std::time_t until, F emit,
                                   std::time_t oldest = 0) {
    std::lock_guard<std::mutex> lock(db.mtx);

    const std::time_t now = time(nullptr);
    const std::time_t floor = MAX(db.pathFloor(now), oldest);

    db.content.clear();
    {
      JSON::Writer w(db.content, 65536);
      w.beginObject();

      // a window that ends before the cutoff serves nothing
      if (until <= 0 || until >= floor) {
        since = MAX(since, floor);
        const std::time_t from = MAX(since > lookback ? since - lookback : 0, oldest);

        db.forEachRecentUnlocked(now, true, from,
                              [&](int ptr, const Ship &ship, long int) {
                                if (db.paths.hasSince(ptr, from))
                                  emit(w, ptr, ship, since);
                              });
      }
      w.endObject().raw("\n\n");
    }
    return db.content;
  }
};

}
