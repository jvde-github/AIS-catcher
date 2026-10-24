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
#include <memory>
#include <mutex>
#include <string.h>
#include <vector>

#include "AIS.h"
#include "JSONAIS.h"

#include "JSON.h"
#include "Keys.h"
#include "Writer.h"

// define CHECK_DB_INTEGRITY to validate ship and path structures once a minute

#include "BinaryStore.h"
#include "DestinationIndex.h"
#include "EventJSON.h"
#include "EventRing.h"
#include "FixSampler.h"
#include "Rest.h"
#include "PathStore.h"
#include "PlaceMarkers.h"
#include "Ships.h"
#include "SlotTable.h"
#include "StaticStore.h"
#include <functional>
#include "StationRegistry.h"
#include "VisitTracker.h"

namespace Services {
struct UiFormat;
}

class DB : public StreamIn<JSON::JSON>,
           public StreamIn<AIS::GPS>,
           public StreamIn<AIS::Control>,
           public StreamOut<JSON::JSON> {
  // the viewer's formats, written from the table under its lock
  friend struct Services::UiFormat;

  JSON::Serializer builder{JSON_DICT_FULL};

  std::string content;
  float station_lat = LAT_UNDEFINED, station_lon = LON_UNDEFINED;
  // the station position the cached ship distances were computed for
  float distance_lat = LAT_UNDEFINED, distance_lon = LON_UNDEFINED;
  int time_history = 30 * 60;
  // oldest track and replay data served, 0 is unlimited
  int track_time = 3600;
  // newest last_signal among recycled ships: older scenes are missing vessels
  std::time_t evict_horizon = 0;
  bool latlon_share = false;
  bool server_mode = false;
  bool msg_save = false;
  bool use_gps = true;
  bool gps_position = false;
  uint32_t own_mmsi = 0;

  int nbuckets = 8209;
  // 0 = unset, resolved at setup: the server-mode default leaves room for the
  // anchor slots
  int track_memory_kb = 0;
  // 0 = default sizing
  int max_ships = 0;

  bool expire_fields = false;
  std::time_t last_sweep = 0;
  uint16_t quality_mask = 0;
  uint64_t copies_dropped = 0;

  static const int SHIP_NIL = SlotTable<Ship, uint32_t>::NIL;

  SlotTable<Ship, uint32_t> ships;
  // what the table keeps beside each record: the places a ship visits, the
  // port it is bound for, and the place markers the map shows
  VisitTracker visits;
  DestinationIndex destinations;
  PlaceMarkers place_markers;
  std::time_t full_refresh_at = 0; // older ship deltas require a full response

  void updateDistance(Ship &ship);

public:
  // Distances again from an origin per ship, for a host whose stations each
  // have a position; `origin` false leaves a ship as it is, or without a
  // distance when `clear`
  void measureFrom(const std::function<bool(const Ship &, float &, float &)> &origin, bool clear);
  // A ship's track back to `since`, never past what is served, newest point
  // first, under the lock; false when the ship is not known
  bool forEachTrackPoint(uint32_t mmsi, std::time_t since,
                         const std::function<void(const PathStore::Point &)> &f);
  // The same for the ship in slot `ptr`, for a caller already under the lock,
  // such as a forEachRecent walk
  template <typename F>
  void forEachTrackPointHeld(int ptr, std::time_t now, std::time_t since, F f) const {
    const std::time_t floor = MAX(since, pathFloor(now));
    for (uint32_t r = paths.tail(ptr);
         PathStore::isPoint(r) && (std::time_t)paths.at(r).end() >= floor;
         r = paths.at(r).prev)
      f(paths.at(r));
  }
  // how long a ship stays on the map after its last message, in seconds
  int timeHistory() {
    std::lock_guard<std::mutex> lock(mtx);
    return time_history;
  }
  // a client holding ship data older than this must start over
  std::time_t fullRefreshAt() {
    std::lock_guard<std::mutex> lock(mtx);
    return full_refresh_at;
  }
  // the places ships are matched against, null without a catalogue
  std::shared_ptr<const PlaceIndex> placeIndex() {
    std::lock_guard<std::mutex> lock(mtx);
    return place_markers.index();
  }
  // Under the lock, each settled visit with its ship and its place: all of
  // them, or a place's (`place` a runtime id), or a ship's
  template <typename F> void forEachVisit(F f) {
    std::lock_guard<std::mutex> lock(mtx);
    const auto *index = place_markers.index().get();
    if (!index)
      return;
    ships.forEach([&](int ptr) {
      visits.forEachVisit(ptr, [&](const VisitTracker::Visit &v) {
        if (const auto *e = index->find(v.id))
          f(ships[ptr], v, *e);
      });
      return true;
    });
  }
  // a ship's reported changes, newest first, and the port its destination names
  template <typename F> bool forEachChangeOf(uint32_t mmsi, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    const int ptr = ships.find(mmsi);
    if (ptr == SHIP_NIL)
      return false;
    changes.forEachEntry(ptr, f);
    return true;
  }
  std::string matchedPort(uint32_t mmsi) {
    std::lock_guard<std::mutex> lock(mtx);
    return matchedPortHeld(mmsi);
  }
  template <typename F> bool forEachVisitOf(uint32_t mmsi, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    const int ptr = ships.find(mmsi);
    if (ptr == SHIP_NIL)
      return false;
    const auto *index = place_markers.index().get();
    if (index)
      visits.forEachVisit(ptr, [&](const VisitTracker::Visit &v) {
        if (const auto *e = index->find(v.id))
          f(ships[ptr], v, *e);
      });
    return true;
  }

private:
  void updateStation(float lat, float lon);
  void locate(int ptr);
  void install(int ptr, const Ship &s);
  void refreshPlaceMembership(int ptr);
  void updatePlaceEvents(int ptr, std::time_t now);

public:
  void setPlaces(std::shared_ptr<const PlaceIndex> next);

  // What a dump needs, copied under the lock in one go so the file is written
  // with the lock released: the ships in their recency order, their visits,
  // the port each is bound for, and the places the visits refer to. A
  // standalone receiver's table is a few MB; the hub's 500,000 ships are some
  // 220 MB, kept between dumps so the copy allocates nothing under the lock.
  struct Snapshot {
    SlotTable<Ship, uint32_t> ships;
    VisitTracker visits;
    DestinationIndex destinations;
    std::shared_ptr<const PlaceIndex> places;
    // the port a ship in slot `ptr` is bound for, as the table held it
    const std::string &matchedPort(int ptr) const { return destinations.code(ptr); }
  };
  // The kept copy, handed to f with the table's lock released. The first dump
  // to arrive refreshes it under that lock; one that arrives while another is
  // still reading shares the copy as it is, a few seconds old at most, which
  // both the backup and an export can live with - and nobody waits for a copy
  // that is already there. The backup uses it when told to (setSnapshotSave),
  // an exporter whenever it likes.
  template <typename F> void withSnapshot(F f) {
    struct Reading {
      DB &db;
      Reading(DB &d) : db(d) {
        std::lock_guard<std::mutex> lock(db.snapshot_mtx);
        if (db.snapshot_readers++ == 0)
          db.takeSnapshot(db.snapshot);
      }
      ~Reading() {
        std::lock_guard<std::mutex> lock(db.snapshot_mtx);
        db.snapshot_readers--;
      }
    } reading(*this);
    f(static_cast<const Snapshot &>(snapshot));
  }
  // Save() copies the table and writes the file outside the lock. Off by
  // default: the file is written under the lock, and no copy is ever kept.
  void setSnapshotSave(bool b) { snapshot_save = b; }

  // For consumers already holding mtx through withShip/withShips/forEach.
  const std::string &matchedPortHeld(uint32_t mmsi) const {
    static const std::string empty;
    const int slot = ships.find(mmsi);
    return slot == SHIP_NIL ? empty : destinations.code(slot);
  }

  // Locked reads for code that lives outside the DB: one ship by MMSI, or all
  // of them.
  template <typename F> bool withShip(uint32_t mmsi, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    int ptr = ships.find(mmsi);
    if (ptr == SHIP_NIL)
      return false;
    f(ships[ptr], ptr);
    return true;
  }

  // A batch of ships under one lock; keep f short - copy fields out, work
  // later.
  template <typename F> void withShips(const uint32_t *mmsi, size_t n, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    for (size_t i = 0; i < n; i++) {
      int ptr = ships.find(mmsi[i]);
      f(i, ptr == SHIP_NIL ? nullptr : &ships[ptr]);
    }
  }
  // A ship silent this long has stopped being present - the same span a visit
  // survives, so a stay and its ship never disagree about when it ended.
  // Shared by the place dialog, the visits export and the loader's sweep.
  static const std::time_t VISIT_SILENT = VisitTracker::SILENT;
  // A vessel in a place now, as a place's lists name it: inside and heard of
  // lately, and where a stay needs a stop, a vessel that has lain still in it.
  // A ship steaming through, an aid or a base station is not news at a berth.
  static bool insideNow(const Ship &s, const VisitTracker::Visit &v, bool requiresStop, std::time_t now) {
    return v.inside() && now - s.last_signal <= VISIT_SILENT && (!requiresStop || (s.shipclass < CLASS_PLANE && v.idle > 0));
  }
  // Chunked like withShips so the exporter never holds the lock for a whole
  // pass. The place's metadata lives only as long as the lock.
  template <typename F> void withVisits(const uint32_t *mmsi, size_t n, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    const auto *index = place_markers.index().get();
    if (!index)
      return;
    for (size_t i = 0; i < n; i++) {
      const int ptr = ships.find(mmsi[i]);
      if (ptr == SHIP_NIL)
        continue;
      const std::time_t heard = ships[ptr].last_signal;
      visits.forEachVisit(ptr, [&](const VisitTracker::Visit &v) {
        if (const auto *entry = index->find(v.id))
          f(mmsi[i], v, *entry->metadata, heard);
      });
    }
  }
  // Locked write for an operator correction: the record under `mmsi`, mutable.
  template <typename F> bool withShipMutable(uint32_t mmsi, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    int ptr = ships.find(mmsi);
    if (ptr == SHIP_NIL)
      return false;
    Ship &ship = ships[ptr];
    char destination[sizeof(ship.destination)];
    std::memcpy(destination, ship.destination, sizeof(destination));
    const auto lat = ship.lat, lon = ship.lon;
    f(ship);
    if (lat != ship.lat || lon != ship.lon)
      locate(ptr);
    if (std::strcmp(destination, ship.destination) != 0)
      destinations.match(ptr, ship);
    else
      destinations.position(ptr, ship);
    return true;
  }
  // Drops the ship and its relationships; the slot is the next recycled.
  // Path/static history is wiped when that slot is claimed again.
  bool deleteShip(uint32_t mmsi) {
    std::lock_guard<std::mutex> lock(mtx);
    int ptr = ships.find(mmsi);
    if (ptr == SHIP_NIL)
      return false;
    destinations.erase(ptr);
    visits.erase(ptr);
    ships[ptr].reset();
    return ships.remove(mmsi);
  }
  // Callbacks run under mtx and must not re-enter DB. Order is unspecified.
  template <class F> void forEachShipInPort(const std::string &code, F visit) {
    std::lock_guard<std::mutex> lock(mtx);
    destinations.forEach(code,
                         [&](uint32_t ptr) { return visit(ships[ptr], ptr); });
  }
  // most recently heard first; f returns false to stop the walk
  template <typename F> void forEachShip(F f) {
    std::lock_guard<std::mutex> lock(mtx);
    ships.forEach([&](int ptr) { return f(ships[ptr], ptr); });
  }
  // The ships heard since `since`, newest first, under the table's lock. The
  // window is the caller's own - no time_history clamp - and outside callers
  // must come through here: the walk follows LRU links the decode path relinks.
  template <typename F>
  void forEachRecent(std::time_t now, std::time_t since, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    forEachRecentUnlocked(now, true, since, f);
  }
  int capacity() const { return ships.capacity(); }
  // Quality bits that mark a message as a copy: delivered, but it does not
  // move a ship, count, or add a path point. Zero (default) takes everything.
  void setQualityMask(uint16_t m) { quality_mask = m; }
  uint16_t getQualityMask() const { return quality_mask; }
  // a host that counts each station's messages says when one was last heard; station
  // records and rows carry it, so a connected but silent station can be drawn as such
  void setStationHeard(std::function<std::time_t(int)> f) { station_heard = std::move(f); }
  // messages the mask turned away
  uint64_t getCopiesDropped() const { return copies_dropped; }
  // Puts a record in the table under its MMSI, replacing what is there; for
  // seeding from another source. The places follow from the position.
  void putShip(const Ship &s);

  struct SeedCount {
    int created = 0;
    int present = 0; // already in the table, left untouched
    int full = 0;    // refused: the table is at its seeding ceiling
  };

  // Hands out blank records during a seed; only DB makes one.
  class Seeder {
    DB &db;
    SeedCount &count;
    const int ceiling;

  public:
    Seeder(DB &d, SeedCount &c)
        : db(d), count(c), ceiling(d.ships.capacity() - d.ships.capacity() / 10) {
    }
    // A ship the table holds was heard here, and a file is older than what it
    // said itself, so it is left alone.
    template <typename Fill> void add(uint32_t mmsi, Fill fill) {
      if (mmsi < 1 || mmsi > 999999999)
        return;
      if (db.ships.find(mmsi) != SHIP_NIL) {
        count.present++;
        return;
      }
      // creation recycles the LRU tail: a full table would drop ships the
      // network is still hearing, and raise the eviction horizon with them
      if (db.ships.size() >= ceiling) {
        count.full++;
        return;
      }
      const int ptr = db.claimShip(mmsi);
      Ship &ship = db.ships[ptr];
      ship.reset();
      ship.mmsi = mmsi;
      fill(ship);
      ship.setType();
      db.locate(ptr);
      db.destinations.match(ptr, ship);
      count.created++;
    }
  };

  // Seeding from another source in one locked pass: `body(seeder)` walks the
  // caller's records and calls seeder.add(mmsi, fill) for each. Nothing outside
  // observes the table until it returns, so the order is restored at the end.
  template <typename Body> void seedShips(Body body, SeedCount &count) {
    std::lock_guard<std::mutex> lock(mtx);
    Seeder seeder(*this, count);
    body(seeder);
    if (count.created) {
      restoreOrder();
      full_refresh_at = time(nullptr);
    }
  }

private:
  PathStore paths;
  StaticStore changes;

  std::mutex mtx;

  Snapshot snapshot;
  std::mutex snapshot_mtx; // guards the reader count; the copy itself is read-only while anyone reads
  int snapshot_readers = 0;
  bool snapshot_save = false;
  void takeSnapshot(Snapshot &s);
  static bool writeTable(std::ofstream &file, const SlotTable<Ship, uint32_t> &table,
                         const VisitTracker &visits, const PlaceIndex *places);

  void updateFields(const JSON::Member &p, const AIS::Message *msg, Ship &ship,
                    bool allowApproximate, bool &positionUpdated,
                    bool &staticUpdated);

  bool updateShip(const JSON::JSON &, TAG &, Ship &);
  void addToPath(int ptr);
  int claimShip(uint32_t mmsi);
  void restoreOrder();

  // the two scope rules fold into one cutoff; 0 passes everything. Caller holds
  // mtx.
  template <typename F>
  void forEachRecentUnlocked(std::time_t now, bool full, std::time_t since,
                             F f) {
    std::time_t cutoff = full ? since : MAX(since, now - time_history);
    ships.forEach([&](int ptr) {
      const Ship &ship = ships[ptr];
      if (ship.last_signal < cutoff)
        return false;

      f(ptr, ship, (long int)now - (long int)ship.last_signal);
      return true;
    });
  }

  int keptCount(const std::function<bool(const Ship &)> &keep);

  // Oldest time any path data is served: the track_time cap, never reaching
  // past the eviction horizon into scenes that are missing recycled vessels.
  std::time_t pathFloor(std::time_t now) const {
    std::time_t cutoff =
        track_time > 0 && now > track_time ? now - track_time : 0;
    return MAX(cutoff, evict_horizon);
  }

  AIS::Filter filter;

  BinaryStore binary;
  StationRegistry stations;
  std::function<std::time_t(int)> station_heard; // set by the host, see setStationHeard
  // the ticker's view of the event stream, wired in the constructor
  EventRing ring;
  uint64_t event_seq = 0; // the stream's sequence; the process start is the epoch
  TAG event_tag;
  // which positions become fix events; nothing until a host asks for fixes
  FixSampler sampler;
  bool fixes_wanted = false;
  int nships_ = 0;
  // AIS_EVENTS_TRACE: every event as a JSON line on stderr
  Tracking::EventTrace tracer;
  bool tracing = false;
  void sampleFix(int ptr, std::time_t t, bool force);
  // the one definition of lying still: the record says since when, this holds
  // what the ship did meanwhile
  Tracking::RestStore rest;
  std::time_t last_rest_sweep = 0;
  static bool stillTest(const Ship &ship);
  // a crossing the tracker confirmed or inferred, told on the stream
  void crossing(int ptr, const VisitTracker::Visit &v, bool entering, uint32_t observed);
  void updateRest(int ptr, std::time_t now, bool still_before, std::time_t previous);
  void endRest(int ptr, std::time_t t, bool seen);
  void sweepRest(std::time_t now);
#ifdef CHECK_DB_INTEGRITY
  void checkIntegrity();
  std::time_t last_check = 0;
#endif

  // an event with the fix block filled from the record; emit() gives it its
  // sequence and sends it on `events`
  Tracking::Event event(const Ship &ship, Tracking::Kind kind,
                        Tracking::Level level, std::time_t now) const;
  void emit(Tracking::Event &e);
  // with the slot known: a fix is told first, so the line passes through the event
  void emit(Tracking::Event &e, int ptr);
  // changes noted while a record is being updated wait until it is whole
  std::vector<Tracking::Event> pending;
  void queue(const Tracking::Event &e) { pending.push_back(e); }
  void flushEvents(int ptr);
  void noteSafety(Ship &ship, const JSON::JSON &data);
  // the safety texts told lately: a broadcast repeated is marked a repeat until a while has passed
  struct SafetyTold {
    uint32_t mmsi = 0, sum = 0;
    std::time_t at = 0;
  };
  static const int SAFETY_TOLD = 64;
  SafetyTold safety_told[SAFETY_TOLD];
  int safety_next = 0;
  bool safetyRepeat(uint32_t mmsi, const std::string &words, Tracking::Level level, std::time_t now);
  void noteDestination(Ship &ship, const std::string &v);
  void noteDraught(Ship &ship, float d);
  void noteText(const Ship &ship, Tracking::Kind kind, const char *old_value,
                const std::string &value);
  void noteStatus(Ship &ship, int status);
  void logTextChange(const Ship &ship, int field, const char *old_value,
                     const std::string &value);

  // serializes configuration with input and downstream sinks; reentrant, as a
  // whole reconfiguration may hold it while the setters take it too
  std::recursive_mutex configuration_mtx;

public:
  // every judgement the database makes about a vessel, told once; whoever
  // holds a view subscribes with >> (the ring here, the host wires the rest)
  StreamOut<Tracking::Event> events;
  DB() { events >> ring; }
  // sizes the table from the settings and empties it: before the first message
  void setup();
  // settings changed while messages flow: `apply` runs with input held and the table locked
  // input waits while the lock is held: the web viewer holds it across a
  // whole reconfiguration of its tables
  std::unique_lock<std::recursive_mutex> pauseInput() { return std::unique_lock<std::recursive_mutex>(configuration_mtx); }
  template <typename F> void configure(F apply) {
    std::lock_guard<std::recursive_mutex> configurationLock(configuration_mtx);
    std::lock_guard<std::mutex> lock(mtx);
    apply();
  }
  void tick(std::time_t now);
  void setTimeHistory(int t) {
    if (time_history != t)
      full_refresh_at = time(nullptr);
    time_history = t;
  }
  void setTrackTime(int t) { track_time = t; }
  void setExpireFields(bool b) { expire_fields = b; }
  void setTrackMemory(int kb) {
    if (kb > 0)
      track_memory_kb = kb;
  }
  void setMaxShips(int n) { max_ships = n; }
  void setShareLatLon(bool b) { latlon_share = b; }
  bool getShareLatLon() { return latlon_share; }

  void setLatLon(float lat, float lon) {
    updateStation(lat, lon);
    gps_position = false;
  }
  void setConfigPosition(float lat, float lon, bool gps) {
    use_gps = gps;
    if (gps && gps_position)
      return;
    updateStation(lat, lon);
    gps_position = false;
  }
  void setLat(float lat) {
    updateStation(lat, station_lon);
    gps_position = false;
  }
  void setLon(float lon) {
    updateStation(station_lat, lon);
    gps_position = false;
  }

  float getLat() { return station_lat; }
  float getLon() { return station_lon; }

  void setOwnMMSI(uint32_t mmsi) { own_mmsi = mmsi; }

  using StreamIn<JSON::JSON>::Receive;
  using StreamIn<AIS::GPS>::Receive;
  using StreamIn<AIS::Control>::Receive;

  // The stations a host keeps itself, as the control feed would tell them; a
  // station no longer listed goes
  void setStations(const std::vector<StationRegistry::Station> &list) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<int> gone;
    stations.forEach([&](const StationRegistry::Station &s) {
      if (std::none_of(list.begin(), list.end(), [&](const StationRegistry::Station &n) { return n.id == s.id; }))
        gone.push_back(s.id);
    });
    for (int id : gone) {
      StationRegistry::Station s;
      s.id = id;
      stations.put(s, true, binary.sequence());
    }
    for (const auto &s : list)
      stations.define(s, binary.sequence());
  }
  void setStation(const StationRegistry::Station &s) {
    std::lock_guard<std::mutex> lock(mtx);
    stations.define(s, binary.sequence());
  }

  void Receive(const AIS::Control *data, int len, TAG &) {
    std::lock_guard<std::mutex> lock(mtx);
    for (int i = 0; i < len; i++)
      if (data[i].topic == AIS::KEY_STATION && data[i].payload)
        stations.apply(*data[i].payload, binary.sequence());
  }

  // what the filter makes of a message: dropped, stored, or counted alone
  enum Admission { DROP, STORE, COUNT };
  Admission admit(const AIS::Message *msg, TAG &tag);
  void count(const JSON::JSON *data, int len, TAG &tag);
  void Receive(const JSON::JSON *data, int len, TAG &tag);
  void Receive(const AIS::GPS *data, int len, TAG &tag) {
    std::lock_guard<std::recursive_mutex> configurationLock(configuration_mtx);
    if (use_gps) {
      std::lock_guard<std::mutex> lock(mtx);
      updateStation(data[0].getLat(), data[0].getLon());
      gps_position = true;
    }
  }

  // Which ships a reader sees, for a host that shows only some: a station's,
  // or what a server's filter lets through. Empty keeps every one.
  typedef std::function<bool(const Ship &)> Keep;

  // fixes are sampled only while a subscriber wants them: the hub's journal, an output
  void setFixes(bool on);

  int getCount() {
    std::lock_guard<std::mutex> lock(mtx);
    return ships.size();
  }
  int getMaxCount() {
    std::lock_guard<std::mutex> lock(mtx);
    return ships.capacity();
  }

  void setServerMode(bool b) { server_mode = b; }
  void setMsgSave(bool b) { msg_save = b; }
  void setOptionKey(AIS::Keys key, const std::string &arg) {
    filter.SetOptionKey(key, arg);
  }
  // which messages the table keeps, from the next message on; see admit()
  void setFilter(const AIS::Filter &f) {
    std::lock_guard<std::recursive_mutex> lock(configuration_mtx);
    filter = f;
  }

  // a vessel the reader may see; under the lock
  bool keepHeld(uint32_t mmsi, const Keep &keep) const {
    if (!keep)
      return true;
    const int ptr = ships.find(mmsi);
    return ptr != SHIP_NIL && keep(ships[ptr]);
  }
  // the ship row's packed badge from inside a withShip/forEach callback, where
  // the lock is already held
  uint16_t binaryBadgeHeld(uint32_t mmsi, std::time_t now) const {
    return binary.badge(mmsi, now);
  }
  // the station riding a vessel, -1 for none, for callers already inside the locked walk
  int stationHeld(uint32_t mmsi) const { return stations.idFor(mmsi); }
  int getStation(uint32_t mmsi) {
    std::lock_guard<std::mutex> lock(mtx);
    return stations.idFor(mmsi);
  }
  // the map markers under the lock, for a caller cutting its own tiles; keep f
  // short
  template <typename F> void withMarkers(std::time_t now, F f) {
    std::lock_guard<std::mutex> lock(mtx);
    binary.forEachMarker(now, f);
  }
  int getBinaryTTL() const { return binary.ttl; }
  // every station on record; `riding` says its vessel is on the map, where the
  // ship row carries it and no object of its own is due
  template <typename F> void withStations(F f) {
    std::lock_guard<std::mutex> lock(mtx);
    stations.forEach([&](const StationRegistry::Station &s) {
      int ptr = s.mmsi ? ships.find(s.mmsi) : SHIP_NIL;
      bool riding =
          ptr != SHIP_NIL && isValidCoord(ships[ptr].lat, ships[ptr].lon);
      f(s, riding);
    });
  }

  // Persistence functions for ship database
  bool Save(std::ofstream &file);
  bool Load(std::ifstream &file);

private:
  static const int _DB_MAGIC = 0x41495346;
  static const int _DB_VERSION = 5;
};
