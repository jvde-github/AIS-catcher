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

#include "DB.h"
#include "AIS-catcher.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "Geodesy.h"
#include <cstdlib>
#include "PositionCheck.h"
#include "Logger.h"

#include <fstream>
#include <unordered_map>

//-----------------------------------
// simple ship database

void DB::setup() {
  std::lock_guard<std::mutex> lock(mtx);

  int nships = 4096;

  if (server_mode) {
    nships *= 32;
    nbuckets = 262147;
  }
  if (max_ships > 0) {
    nships = max_ships;
    nbuckets = 2 * nships + 1;
  }
  if (nships != 4096)
    Info() << "DB: internal ship database extended to " << nships << " ships";

  destinations.setup(nships);
  ships.setup(nships, nbuckets);
  visits.setup(nships);
  nships_ = nships;
  if (std::getenv("AIS_EVENTS_TRACE") && !tracing) {
    fixes_wanted = tracing = true;
    events >> tracer;
  }
  sampler.enable(fixes_wanted, nships);

  if (track_memory_kb == 0)
    track_memory_kb = server_mode ? 4096 : 1024;

  int path_blocks = paths.setup((long)track_memory_kb * 1024, nships);
  Debug() << "DB: track store " << track_memory_kb << " KB (" << path_blocks
          << " blocks)";

  changes.setup(nships);

  binary.setup(server_mode ? 65536 : 256);
  evict_horizon = 0;
}

// the ships a reader is shown; under the lock
int DB::keptCount(const Keep &keep) {
  if (!keep)
    return ships.size();
  int count = 0;
  ships.forEach([&](int ptr) {
    count += keep(ships[ptr]);
    return true;
  });
  return count;
}

bool DB::forEachTrackPoint(uint32_t mmsi, std::time_t since,
                           const std::function<void(const PathStore::Point &)> &f) {
  std::lock_guard<std::mutex> lock(mtx);
  const int ptr = ships.find(mmsi);
  if (ptr == SHIP_NIL)
    return false;
  forEachTrackPointHeld(ptr, time(nullptr), since, f);
  return true;
}

// how far an idle ship may wander before a new track point is warranted:
// moored is fenders and GPS wander, anchored swings on the chain, otherwise
// status is unknown (Class B carries none)
static int idleBand(int status) {
  const int NAV_ANCHORED = 1, NAV_MOORED = 5; // ITU-R M.1371
  if (status == NAV_MOORED)
    return 50;
  if (status == NAV_ANCHORED)
    return 200;
  return 100;
}

void DB::addToPath(int ptr) {
  const Ship &ship = ships[ptr];

  if (isValidCoord(ship.lat, ship.lon))
    paths.add(ptr, ship.lat, ship.lon, ship.cog, ship.heading, ship.speed,
              idleBand(ship.status), ship.last_signal);
}

template <size_t N>
static void copyField(char (&dst)[N], const std::string &s) {
  size_t n = MIN(s.size(), N - 1);
  std::memcpy(dst, s.data(), n);
  dst[n] = '\0';
}

// A binary payload carries the position of whatever it describes - a buoy, a
// sensor station, a tidal window - never the sender's, so 6/25/26 are excluded
// alongside 8; 17 reports the DGNSS reference station.
static bool carriesOwnPosition(int type) {
  return type != 6 && type != 8 && type != 17 && type != 25 && type != 26;
}

// A first value is initialisation, not a change: the ship record is where the
// current value lives.
void DB::logTextChange(const Ship &ship, int field, const char *old_value,
                       const std::string &value) {
  if (old_value[0] && value != old_value)
    changes.addText(ships.find(ship.mmsi), (StaticStore::Field)field,
                    value.c_str(), ship.last_signal);
}

// an IMO number has seven digits, the last the sum of the other six weighted
// 7 down to 2, mod 10; the type 5 field also carries flag-state numbers (from
// 10,000,000) and the odd corrupted value, neither of which is one
static bool isIMO(int imo) {
  if (imo < 1000000 || imo > 9999999)
    return false;
  int sum = 0;
  for (int w = 2, n = imo / 10; w <= 7; w++, n /= 10)
    sum += (n % 10) * w;
  return sum % 10 == imo % 10;
}

// an ENI has eight digits, the first three the authority that issued it, and
// a seagoing vessel's is 9 followed by its IMO number; 00000000 is none
static bool isENI(const std::string &eni) {
  if (eni.size() != 8 || eni == "00000000")
    return false;
  for (char c : eni)
    if (c < '0' || c > '9')
      return false;
  return eni[0] != '9' || isIMO(std::atoi(eni.c_str() + 1));
}

void DB::updateFields(const JSON::Member &p, const AIS::Message *msg,
                      Ship &ship, bool allowApproximate, bool &positionUpdated,
                      bool &staticUpdated) {
  switch (p.Key()) {
  case AIS::KEY_LAT:
  case AIS::KEY_LON:
    if (carriesOwnPosition(msg->type()) &&
        (msg->type() != 27 || allowApproximate || ship.getApproximate())) {
      (p.Key() == AIS::KEY_LAT ? ship.lat : ship.lon) = p.Get().getFloat();
      positionUpdated = true;
    }
    break;
  case AIS::KEY_SHIPTYPE: {
    // the inland static report (DAC 200 FID 10) carries an ERI code, which
    // counts only when it is one of the table's: 876 is no ship type, and 8000
    // ("type unknown") fills an empty type but never replaces one
    const int t = p.Get().getInt();
    const bool inland = msg->type() == 6 || msg->type() == 8;
    if (t && (!inland || Ship::eriClass(t) >= 0 || (t == 8000 && !ship.shiptype))) {
      ship.shiptype = t;
      staticUpdated = true;
    }
    break;
  }
  case AIS::KEY_IMO:
    if (isIMO(p.Get().getInt()))
      ship.IMO = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_MONTH:
  case AIS::KEY_DAY:
  case AIS::KEY_HOUR:
  case AIS::KEY_MINUTE:
    if (msg->type() == 5) {
      (p.Key() == AIS::KEY_MONTH  ? ship.month
       : p.Key() == AIS::KEY_DAY  ? ship.day
       : p.Key() == AIS::KEY_HOUR ? ship.hour
                                  : ship.minute) = (char)p.Get().getInt();
      staticUpdated = true;
    }
    break;
  case AIS::KEY_HEADING: {
    // the field is nine bits: 0-359 a heading, 511 "not available", the rest
    // not a heading at all - a transponder that sends 496 has none to give
    const int h = p.Get().getInt();
    ship.heading = h >= 0 && h < 360 ? h : HEADING_UNDEFINED;
    break;
  }
  case AIS::KEY_DRAUGHT: {
    // an inland vessel reports draught twice and the two disagree; the DAC 200
    // FID 10 value is the finer one, so the type 5 field is dropped once it is
    // heard
    const float d = p.Get().getFloat();
    const bool inland = msg->type() == 6 || msg->type() == 8;

    if (d != 0 && (inland || !ship.getInlandDraught())) {
      const bool first = ship.draught == DRAUGHT_UNDEFINED || ship.draught <= 0;
      changes.addNumeric(ships.find(ship.mmsi), StaticStore::DRAUGHT,
                         first ? (uint8_t)(d * 10 + 0.5f)
                               : (uint8_t)(ship.draught * 10 + 0.5f),
                         (uint8_t)(d * 10 + 0.5f), ship.last_signal, first);
      if (!first)
        noteDraught(ship, d);
      ship.draught = d;
      ship.setInlandDraught(inland);
      staticUpdated = true;
    }
  } break;
  case AIS::KEY_COURSE:
    ship.cog = p.Get().getFloat();
    break;
  case AIS::KEY_SPEED:
    if (msg->type() == 9 && p.Get().getInt() != 1023)
      ship.speed = (float)p.Get().getInt();
    else if (p.Get().getFloat() != 102.3f)
      ship.speed = p.Get().getFloat();
    break;
  case AIS::KEY_STATUS: {
    const int st = p.Get().getInt();
    if (ship.status != STATUS_UNDEFINED)
      changes.addNumeric(ships.find(ship.mmsi), StaticStore::STATUS,
                         (uint8_t)ship.status, (uint8_t)st, ship.last_signal);
    noteStatus(ship, st);
    ship.status = st;
  } break;
  case AIS::KEY_TO_BOW:
    ship.to_bow = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_TO_STERN:
    ship.to_stern = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_TO_PORT:
    ship.to_port = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_TO_STARBOARD:
    ship.to_starboard = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_RECEIVED_STATIONS:
    ship.received_stations = p.Get().getInt();
    break;
  case AIS::KEY_ALT:
    if (msg->type() == 9)
      ship.altitude = p.Get().getInt();
    break;
  case AIS::KEY_VIRTUAL_AID:
    ship.setVirtualAid(p.Get().getBool());
    staticUpdated = true;
    break;
  case AIS::KEY_CS:
    ship.setCSUnit(
        p.Get().getBool() ? 2 : 1); // 1=SOTDMA (false), 2=Carrier Sense (true)
    break;
  case AIS::KEY_RAIM:
    ship.setRAIM(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
    break;
  case AIS::KEY_DTE:
    ship.setDTE(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=ready, 2=not ready
    break;
  case AIS::KEY_ASSIGNED:
    ship.setAssigned(
        p.Get().getBool() ? 2 : 1); // 0=unknown, 1=autonomous, 2=assigned
    break;
  case AIS::KEY_DISPLAY:
    ship.setDisplay(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
    break;
  case AIS::KEY_DSC:
    ship.setDSC(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
    break;
  case AIS::KEY_BAND:
    ship.setBand(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
    break;
  case AIS::KEY_MSG22:
    ship.setMsg22(p.Get().getBool() ? 2 : 1); // 0=unknown, 1=false, 2=true
    break;
  case AIS::KEY_OFF_POSITION:
    ship.setOffPosition(
        p.Get().getBool() ? 2 : 1); // 0=unknown, 1=on position, 2=off position
    break;
  case AIS::KEY_MANEUVER:
    ship.setManeuver(p.Get().getInt()); // 0=not available, 1=no special,
                                        // 2=special (direct value)
    break;
  case AIS::KEY_NAME:
  case AIS::KEY_SHIPNAME:
    logTextChange(ship, StaticStore::SHIPNAME, ship.shipname,
                  p.Get().getString());
    noteText(ship, Tracking::Kind::NAME, ship.shipname, p.Get().getString());
    copyField(ship.shipname, p.Get().getString());
    staticUpdated = true;
    break;
  case AIS::KEY_CALLSIGN:
    logTextChange(ship, StaticStore::CALLSIGN, ship.callsign,
                  p.Get().getString());
    noteText(ship, Tracking::Kind::CALLSIGN, ship.callsign, p.Get().getString());
    copyField(ship.callsign, p.Get().getString());
    staticUpdated = true;
    break;
  case AIS::KEY_VENDORID:
    copyField(ship.vendorid, p.Get().getString());
    staticUpdated = true;
    break;
  case AIS::KEY_MODEL:
    ship.unit_model = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_SERIAL:
    ship.unit_serial = p.Get().getInt();
    staticUpdated = true;
    break;
  case AIS::KEY_COUNTRY_CODE:
    copyField(ship.country_code, p.Get().getString());
    break;
  case AIS::KEY_DESTINATION: {
    const std::string &d = p.Get().getString();
    const bool changed = d != ship.destination;
    logTextChange(ship, StaticStore::DESTINATION, ship.destination, d);
    noteDestination(ship, d);
    copyField(ship.destination, d);
    if (changed)
      destinations.match(ships.find(ship.mmsi), ship);
    staticUpdated = true;
  } break;
  case AIS::KEY_VIN: {
    // every inland static report carries the ENI, so one that is not an ENI
    // clears what an earlier report left
    std::string eni;
    for (char c : p.Get().getString())
      if (c != ' ')
        eni += c;
    if (isENI(eni))
      std::memcpy(ship.vin, eni.c_str(), sizeof(ship.vin));
    else
      ship.vin[0] = '\0';
    staticUpdated = true;
    break;
  }
  }
}

bool DB::updateShip(const JSON::JSON &data, TAG &tag, Ship &ship) {
  const AIS::Message *msg = (AIS::Message *)data.binary;

  bool positionUpdated = false, staticUpdated = false;

  int type = msg->type();
  int repeat = msg->repeat();

  // determine whether we accept msg 27 to update lat/lon
  bool allowApproxLatLon = false;
  if (type == 27) {
    int timeout = 10 * 60;
    repeat = 0;

    if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
      timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

    if (msg->getRxTimeUnix() - ship.last_signal > timeout)
      allowApproxLatLon = true;
  }

  // direct reception clears the repeated flag immediately; a relayed copy sets
  // it only when the ship has not been heard directly for over a minute
  if (repeat == 0) {
    ship.last_direct_signal = msg->getRxTimeUnix();
    ship.setRepeat(0);
  } else if (msg->getRxTimeUnix() - ship.last_direct_signal > 60)
    ship.setRepeat(1);

  ship.mmsi = msg->mmsi();
  ship.count++;
  ship.group_mask |= tag.group;
  ship.last_group = tag.group;
  ship.heardBy(tag.station_id, msg->getRxTimeUnix());

  std::time_t prev_signal = ship.last_signal;
  ship.last_signal = msg->getRxTimeUnix();

  ship.ppm = tag.ppm;
  ship.level = tag.level;
  ship.markType(type);

  if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
    ship.orOpChannels(1 << (msg->getChannel() - 'A'));

  // ETA arrives as four separate keys, so it is compared around the whole
  // message rather than at an assignment site
  const char eta_before[4] = {ship.month, ship.day, ship.hour, ship.minute};
  const bool eta_was_set =
      ship.month != ETA_MONTH_UNDEFINED || ship.day != ETA_DAY_UNDEFINED ||
      ship.hour != ETA_HOUR_UNDEFINED || ship.minute != ETA_MINUTE_UNDEFINED;

  // a type 5 says what the ship has now: an IMO number or ETA it leaves out
  // (the decoder drops "not available") is gone, so a wrong value heard once
  // does not stay for as long as the ship keeps sending static reports
  if (type == 5) {
    if (ship.IMO != IMO_UNDEFINED) {
      ship.IMO = IMO_UNDEFINED;
      staticUpdated = true;
    }
    if (eta_was_set) {
      ship.month = ETA_MONTH_UNDEFINED;
      ship.day = ETA_DAY_UNDEFINED;
      ship.hour = ETA_HOUR_UNDEFINED;
      ship.minute = ETA_MINUTE_UNDEFINED;
      staticUpdated = true;
    }
  }

  for (const auto &p : data.getMembers())
    updateFields(p, msg, ship, allowApproxLatLon, positionUpdated,
                 staticUpdated);

  const bool eta_now_set =
      ship.month != ETA_MONTH_UNDEFINED || ship.day != ETA_DAY_UNDEFINED ||
      ship.hour != ETA_HOUR_UNDEFINED || ship.minute != ETA_MINUTE_UNDEFINED;

  if (eta_was_set && eta_now_set &&
      (eta_before[0] != ship.month || eta_before[1] != ship.day ||
       eta_before[2] != ship.hour || eta_before[3] != ship.minute)) {
    changes.addEta(ships.find(ship.mmsi), (uint8_t)ship.month,
                   (uint8_t)ship.day, (uint8_t)ship.hour, (uint8_t)ship.minute,
                   ship.last_signal);
  }

  ship.setType();

  // Ship came back into dashboard scope after being gone long enough that
  // frontends will have dropped their cached entry. Replay static on the
  // next incremental poll by bumping last_static_signal.
  bool back_in_scope =
      prev_signal > 0 && ship.last_signal - prev_signal > time_history;

  if (staticUpdated || (back_in_scope && ship.last_static_signal > 0))
    ship.last_static_signal = ship.last_signal;

  if (positionUpdated) {
    ship.setApproximate(type == 27);

    if (ship.mmsi == own_mmsi) {
      updateStation(ship.lat, ship.lon);
    }
  }

  if (msg_save) {
    // raw sentences only; /api/message re-decodes on request
    ship.msg.clear();
    for (const auto &s : msg->sentences()) {
      ship.msg += s;
      ship.msg += '\n';
    }
  }
  return positionUpdated;
}

// a vessel makes no events for this long after a destination or status change,
// and for this long after saying TEST
static const int CHANGE_SETTLE_S = 30, TEST_MUTE_S = 300;
// a safety text broadcast again and again is told again after this long: sooner
// while a distress call goes on
static const int SAFETY_REPEAT_S = 1800, SAFETY_URGENT_REPEAT_S = 600;
// the one definition of lying still, shared with the visits and the sampler
static const float REST_STILL_KN = 0.5f, REST_MOVE_KN = 1.0f, REST_DRIFT_M = 350.0f;
static const int REST_TOLD_S = 60, REST_SILENT_S = 6 * 3600;

static std::string words(const std::string &text) {
  std::string s = " ";
  for (char c : text)
    s += std::isalnum((unsigned char)c) ? (char)std::toupper((unsigned char)c)
                                        : ' ';
  return s + ' ';
}

// nothing a decode gone wrong leaves behind
static bool plausibleText(const std::string &text) {
  int letters = 0;
  for (char c : text) {
    unsigned char u = (unsigned char)c;
    if (u < 32 || u > 126)
      return false;
    if (std::isalpha(u))
      letters++;
  }
  return text.size() >= 3 && letters >= 2;
}

// A safety message has no priority field: a distress device sending it
// (AIS-SART 970, man-overboard 972, EPIRB 974) or the words used for something
// serious say it matters; `s` is the text as words() gives it
static Tracking::Level safetyLevel(uint32_t from, const std::string &s) {
  auto has = [&](const char *w) {
    return s.find(std::string(" ") + w + " ") != std::string::npos;
  };
  auto wordStarts = [&](const char *w) {
    return s.find(std::string(" ") + w) != std::string::npos;
  };
  uint32_t prefix = from / 1000000;
  if (prefix == 970 || prefix == 972 || prefix == 974)
    return has("ACTIVE") || has("MAYDAY") || has("SART") || has("MOB") ||
                   has("EPIRB") || has("DISTRESS") || has("HELP")
               ? Tracking::Level::URGENT
               : Tracking::Level::ROUTINE;
  if (has("MAYDAY") || has("SOS") || has("DISTRESS") || has("MOB") ||
      has("OVERBOARD") || has("MAN OVER BOARD") || has("MAN OVERBOARD") ||
      has("SINKING") || wordStarts("CAPSIZ") || has("FIRE") || has("EMERGENCY"))
    return Tracking::Level::URGENT;
  if (has("PAN PAN") || has("PANPAN") || has("SECURITE") || has("ACCIDENT") ||
      has("COLLISION") || has("AGROUND") || has("GROUND") || has("GROUNDING") ||
      has("DANGER") || has("WARNING") || has("KEEP AWAY") || has("STAY AWAY") ||
      has("KEEP CLEAR") || has("NOT UNDER COMMAND") || has("NUC"))
    return Tracking::Level::NOTICE;
  return Tracking::Level::ROUTINE;
}

// a change quiets the vessel for a while; true when it already was, so a value
// flapping between two decodes never gets out
static bool settle(Ship &ship, std::time_t now) {
  bool quiet = ship.quiet_until > now;
  ship.quiet_until = MAX(ship.quiet_until, now + CHANGE_SETTLE_S);
  return quiet;
}

Tracking::Event DB::event(const Ship &ship, Tracking::Kind kind,
                          Tracking::Level level, std::time_t now) const {
  Tracking::Event e;
  e.kind = kind;
  e.level = level;
  e.t = now;
  e.mmsi = ship.mmsi;
  e.name = ship.shipname;
  e.lat = ship.lat;
  e.lon = ship.lon;
  if (!isValidCoord(e.lat, e.lon) && latlon_share &&
      isValidCoord(station_lat, station_lon)) {
    e.lat = station_lat;
    e.lon = station_lon;
  }
  e.speed = ship.speed;
  e.cog = ship.cog;
  e.heading = ship.heading;
  e.draught = ship.draught;
  return e;
}

// Caller holds mtx: Send runs every subscriber in this thread, under the lock
// the record was read under, so a subscriber does little and never calls back
void DB::emit(Tracking::Event &e) {
  e.seq = ++event_seq;
  events.Send(&e, 1, event_tag);
}

// Caller holds mtx. The changes a report brought, told once the record holds
// all of it, so each rests on the fix as it now is
void DB::flushEvents(int ptr) {
  for (Tracking::Event &e : pending) {
    const Ship &ship = ships[ptr];
    e.lat = ship.lat;
    e.lon = ship.lon;
    e.speed = ship.speed;
    e.cog = ship.cog;
    e.heading = ship.heading;
    e.draught = ship.draught;
    emit(e, ptr);
  }
  pending.clear();
}

void DB::emit(Tracking::Event &e, int ptr) {
  if (ptr != SHIP_NIL && e.kind != Tracking::Kind::FIX)
    sampleFix(ptr, e.t, true);
  emit(e);
}

void DB::setFixes(bool on) {
  fixes_wanted = on;
  if (nships_)
    sampler.enable(on, nships_);
}

// Caller holds mtx. The sampler's word on this report; `force` tells it
// whatever it says, because another event rests on it.
void DB::sampleFix(int ptr, std::time_t t, bool force) {
  if (!sampler.enabled())
    return;
  const Ship &ship = ships[ptr];
  if (!isValidCoord(ship.lat, ship.lon))
    return;
  FixSampler::Fix now;
  now.t = (uint32_t)t;
  now.lat = ship.lat;
  now.lon = ship.lon;
  now.speed = ship.speed;
  now.cog = ship.cog;
  now.heading = ship.heading;
  const bool resting = ship.idle_since != 0;
  FixSampler::Fix turn;
  FixSampler::Verdict v =
      sampler.judge(ptr, now.t, now.lat, now.lon, now.speed, resting, turn);
  if (v == FixSampler::TURN) {
    // the previous report was the turn: told with its own time and position
    Tracking::Event e = event(ship, Tracking::Kind::FIX, Tracking::Level::ROUTINE, turn.t);
    e.lat = turn.lat;
    e.lon = turn.lon;
    e.speed = turn.speed;
    e.cog = turn.cog;
    e.heading = turn.heading;
    emit(e);
    sampler.keep(ptr, turn);
    v = sampler.judge(ptr, now.t, now.lat, now.lon, now.speed, resting, turn);
  }
  if (force) {
    const FixSampler::Fix &k = sampler.lastKept(ptr);
    if (k.t == now.t && k.lat == now.lat && k.lon == now.lon)
      return; // already told
    v = FixSampler::NOW;
  }
  if (v == FixSampler::NOW) {
    Tracking::Event e = event(ship, Tracking::Kind::FIX, Tracking::Level::ROUTINE, t);
    emit(e);
    sampler.keep(ptr, now);
  } else
    sampler.note(ptr, now);
}

// Stopped, not merely slow: a ship at anchor swings with the tide at up to a
// knot, so one that is already slow and says it is anchored or moored counts
// as still. No speed at all is no evidence of anything.
bool DB::stillTest(const Ship &ship) {
  return ship.speed != SPEED_UNDEFINED &&
         (ship.speed < REST_STILL_KN ||
          (ship.speed < REST_MOVE_KN && (ship.status == 1 || ship.status == 5)));
}

// Caller holds mtx, on a validated position. Rest begins at the second still
// report in a row, dated from the first; it is told once a minute has passed;
// it ends at a report under way, or one that drifted from where the ship lay.
void DB::updateRest(int ptr, std::time_t now, bool still_before,
                    std::time_t previous) {
  Ship &ship = ships[ptr];
  const bool still = stillTest(ship);
  const bool moving = ship.speed != SPEED_UNDEFINED && ship.speed >= REST_MOVE_KN;
  if (ship.idle_since == 0) {
    if (still && still_before && previous > 0 &&
        now - previous <= (std::time_t)Tracking::RestStore::SAMPLE_S) {
      ship.idle_since = previous;
      ship.rest_told = false;
      Tracking::RestStore::Accumulator &a = rest.open(ptr, ship);
      Tracking::RestStore::add(a, ship, (uint32_t)(now - previous));
    }
    return;
  }
  Tracking::RestStore::Accumulator *a = rest.find(ptr);
  if (moving || (a && a->drift(ship) > REST_DRIFT_M)) {
    endRest(ptr, now, true);
    return;
  }
  if (!a) {
    // restored at rest: counting begins now
    a = &rest.open(ptr, ship);
    a->start_seen = false;
  }
  Tracking::RestStore::add(*a, ship, previous > 0 ? (uint32_t)(now - previous) : 0);
  if (!ship.rest_told && now - ship.idle_since >= REST_TOLD_S) {
    Tracking::Event e = event(ship, Tracking::Kind::STOP, Tracking::Level::ROUTINE,
                              ship.idle_since);
    emit(e, ptr);
    ship.rest_told = true;
  }
}

// Caller holds mtx. The rest ends at `t`: a departure seen, or silence judged
// at the last report. A rest never told ends without a word.
void DB::endRest(int ptr, std::time_t t, bool seen) {
  Ship &ship = ships[ptr];
  if (ship.rest_told) {
    Tracking::Event e = event(ship, Tracking::Kind::MOVE, Tracking::Level::ROUTINE, t);
    Tracking::RestStore::fill(e.stay, rest.find(ptr), ship, seen);
    emit(e, ptr);
  }
  ship.idle_since = 0;
  ship.rest_told = false;
  rest.close(ptr);
}

// Caller holds mtx. Six hours without a report closes a rest at the last one.
void DB::sweepRest(std::time_t now) {
  std::vector<int> silent;
  rest.forEach([&](int slot, const Tracking::RestStore::Accumulator &) {
    if (now - ships[slot].last_signal > REST_SILENT_S)
      silent.push_back(slot);
  });
  for (int slot : silent)
    endRest(slot, ships[slot].last_signal, false);
}

// a test silences its sender for a while: what follows an exercise is the
// exercise; the quiet after a test or a change holds a notice back, never a
// distress call
void DB::noteSafety(Ship &ship, const JSON::JSON &data) {
  std::string text;
  uint32_t to = 0;
  for (const auto &p : data.getMembers()) {
    if (p.Key() == AIS::KEY_TEXT)
      text = p.Get().getString();
    else if (p.Key() == AIS::KEY_DEST_MMSI)
      to = (uint32_t)p.Get().getInt();
  }
  if (!plausibleText(text))
    return;
  std::time_t now = std::time(nullptr);
  std::string w = words(text);
  if (w.find(" TEST ") != std::string::npos) {
    ship.quiet_until = now + TEST_MUTE_S;
    return;
  }
  Tracking::Level level = safetyLevel(ship.mmsi, w);
  if (level == Tracking::Level::ROUTINE ||
      (level == Tracking::Level::NOTICE && ship.quiet_until > now))
    return;
  Tracking::Event e = event(ship, Tracking::Kind::SAFETY, level, now);
  e.repeat = safetyRepeat(ship.mmsi, w, level, now);
  e.safety.to = to;
  const int recipient = to ? ships.find(to) : SHIP_NIL;
  e.safety.to_name = recipient != SHIP_NIL ? ships[recipient].shipname : nullptr;
  e.to.setString(&text);
  emit(e, ships.find(ship.mmsi));
}

// Whether this text from this sender was told within the wait: a beacon or a
// coast station repeats its message every minute or so for hours. A repeat
// still reaches the ticker, which counts it; the journal, and the API,
// webhooks and MQTT it feeds, take it once per wait. The last SAFETY_TOLD
// texts are remembered, by a hash of their words.
bool DB::safetyRepeat(uint32_t mmsi, const std::string &words, Tracking::Level level, std::time_t now) {
  uint32_t sum = 2166136261u;
  for (unsigned char c : words) {
    sum ^= c;
    sum *= 16777619u;
  }
  const std::time_t wait = level == Tracking::Level::URGENT ? SAFETY_URGENT_REPEAT_S : SAFETY_REPEAT_S;
  for (auto &t : safety_told)
    if (t.mmsi == mmsi && t.sum == sum) {
      if (now - t.at < wait)
        return true;
      t.at = now;
      return false;
    }
  SafetyTold &t = safety_told[safety_next];
  t.mmsi = mmsi;
  t.sum = sum;
  t.at = now;
  safety_next = (safety_next + 1) % SAFETY_TOLD;
  return false;
}

// a value that names no place: a vessel leaving or arriving at one is no news
static bool namesAPlace(const std::string &text) {
  static const char *NOTHING[] = {
      "UNKNOWN",       "UNK",    "NA",  "NONE", "NIL", "NOTAVAILABLE",
      "NODESTINATION", "NODEST", "TBA", "TBD",  "TBN", "UNSPECIFIED"};
  if (!plausibleText(text))
    return false;
  std::string s;
  for (char c : text)
    if (std::isalnum((unsigned char)c))
      s += (char)std::toupper((unsigned char)c);
  for (const char *w : NOTHING)
    if (s == w)
      return false;
  return true;
}

// the event is the change, so the first destination a vessel reports only
// starts the clock, and one that stood for nothing knowable is left alone
void DB::noteDestination(Ship &ship, const std::string &v) {
  if (v.empty() || v == ship.destination)
    return;
  std::time_t now = std::time(nullptr);
  std::string was = ship.destination;
  if (settle(ship, now) || !namesAPlace(v) || !namesAPlace(was))
    return;
  Tracking::Event e = event(ship, Tracking::Kind::DESTINATION,
                            Tracking::Level::ROUTINE, now);
  e.setText(was.c_str(), v.c_str());
  queue(e);
}

// a name or a callsign that differs from the last heard; the first is
// initialisation, as the static store takes it
void DB::noteText(const Ship &ship, Tracking::Kind kind, const char *old_value,
                  const std::string &value) {
  if (!old_value[0] || value == old_value)
    return;
  Tracking::Event e = event(ship, kind, Tracking::Level::ROUTINE, std::time(nullptr));
  e.setText(old_value, value.c_str());
  queue(e);
}

// a vessel's draught, in metres as the message gives it: a change is news, the
// first reading only a baseline
void DB::noteDraught(Ship &ship, float d) {
  if (ship.draught == DRAUGHT_UNDEFINED || ship.draught <= 0 || d <= 0 ||
      d == ship.draught)
    return;
  std::time_t now = std::time(nullptr);
  if (settle(ship, now))
    return;
  Tracking::Event e = event(ship, Tracking::Kind::DRAUGHT,
                            Tracking::Level::ROUTINE, now);
  e.from.setFloat(ship.draught);
  e.to.setFloat(d);
  queue(e);
}

// a vessel's navigation status, named the way the message names it; the status
// it starts out with, and one going undefined, only start the clock
void DB::noteStatus(Ship &ship, int status) {
  const std::vector<std::string> &names = AIS::LookupTable_nav_status;
  if (status == ship.status || status < 0 || status >= (int)names.size())
    return;
  std::time_t now = std::time(nullptr);
  int had = ship.status;
  if (settle(ship, now) || had < 0 || had >= (int)names.size() ||
      had == STATUS_UNDEFINED || status == STATUS_UNDEFINED)
    return;
  Tracking::Event e = event(ship, Tracking::Kind::STATUS,
                            Tracking::Level::ROUTINE, now);
  e.from.setInt(had);
  e.to.setInt(status);
  queue(e);
}

// a recycled slot may still own the evicted ship's track, so every create
// is paired with a path wipe
// a record from elsewhere replaces what the slot holds; distance and the
// relationships follow from its fields. Caller holds mtx.
void DB::install(int ptr, const Ship &s) {
  Ship &ship = ships[ptr];
  ship = s;
  locate(ptr);
  destinations.match(ptr, ship);
}

// Every scan leans on the LRU being the recency order: readers stop at the first
// record older than their cutoff, and Load refuses a backup that is not
// chronological. A create puts its record at the front whatever its age, so
// after a seed the order is rebuilt here - touching oldest first leaves the
// newest at the front. Slot indices are stable and the caller holds mtx.
void DB::restoreOrder() {
  std::vector<int> order;
  order.reserve(ships.size());
  ships.forEach([&](int ptr) {
    order.push_back(ptr);
    return true;
  });
  // ties broken the other way round: the pass ends newest first, MMSI rising
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return ships[a].last_signal != ships[b].last_signal
               ? ships[a].last_signal < ships[b].last_signal
               : ships[a].mmsi > ships[b].mmsi;
  });
  for (size_t i = 0; i < order.size(); i++)
    ships.touch(order[i]);
}

void DB::putShip(const Ship &s) {
  if (s.mmsi == 0)
    return;
  std::lock_guard<std::mutex> lock(mtx);
  install(claimShip(s.mmsi), s);
  full_refresh_at = time(nullptr);
}

int DB::claimShip(uint32_t mmsi) {
  int ptr = ships.find(mmsi);
  if (ptr == SHIP_NIL) {
    ptr = ships.create(mmsi);
    // the recycled record still holds the evicted ship
    evict_horizon = MAX(evict_horizon, ships[ptr].last_signal);
    paths.wipe(ptr);
    changes.wipe(ptr);
    visits.erase(ptr);
    sampler.wipe(ptr);
    rest.close(ptr);
    destinations.erase(ptr);
    ships[ptr].reset();
  } else
    ships.touch(ptr);

  return ptr;
}

// The table's filter, once per message and before the message reaches the
// readers of the stream. A diagnostic is let through with its tag marked, so it
// counts in the figures without touching any vessel, position, path or coverage.
DB::Admission DB::admit(const AIS::Message *msg, TAG &tag) {
  std::lock_guard<std::recursive_mutex> configurationLock(configuration_mtx);
  const auto result = filter.include(*msg, tag);
  if (result == AIS::Filter::Result::NotIncluded)
    return DROP;
  if (result == AIS::Filter::Result::IncludedWithError) {
    tag.validated = false;
    tag.lat = LAT_UNDEFINED;
    tag.lon = LON_UNDEFINED;
    tag.distance = DISTANCE_UNDEFINED;
    tag.angle = ANGLE_UNDEFINED;
    tag.shipname[0] = 0;
    tag.shipclass = CLASS_UNKNOWN;
    tag.speed = SPEED_UNDEFINED;
    // Equal timestamps prevent History from counting a new vessel.
    tag.previous_signal = msg->getRxTimeUnix();
    return COUNT;
  }
  return STORE;
}

void DB::count(const JSON::JSON *data, int len, TAG &tag) {
  Send(data, len, tag);
}

void DB::Receive(const JSON::JSON *data, int len, TAG &tag) {
  std::lock_guard<std::recursive_mutex> configurationLock(configuration_mtx);
  const AIS::Message *msg = (AIS::Message *)data[0].binary;
  int type = msg->type();

  if (type < 1 || type > 28)
    return;


  std::unique_lock<std::mutex> lock(mtx);

  if (!isValidCoord(station_lat, station_lon) &&
      isValidCoord(tag.station_lat, tag.station_lon)) {
    updateStation(tag.station_lat, tag.station_lon);
  }

  // A copy of a transmission the record already took: it must not count, move
  // the ship, or reorder the table - find() instead of claimShip(), because a
  // touch without a fresh stamp breaks the newest-first walk. An unknown ship
  // is not created here either (a reset record at the head does the same
  // damage), but the message still travels with its cleared tag.
  bool copy = quality_mask && (tag.quality & quality_mask);

  int ptr;
  if (copy) {
    copies_dropped++;
    ptr = ships.find(msg->mmsi());
    if (ptr == SHIP_NIL) {
      lock.unlock();
      Send(data, len, tag);
      return;
    }
  } else
    ptr = claimShip(msg->mmsi());

  // update ship and tag data
  Ship &ship = ships[ptr];

  // save some data for later on
  tag.previous_signal = ship.last_signal;

  float lat_old = ship.lat;
  float lon_old = ship.lon;
  const bool stopped_old =
      ship.speed != SPEED_UNDEFINED && ship.speed < REST_STILL_KN;
  const bool still_before = stillTest(ship);

  bool newValidPosition = !copy && updateShip(data[0], tag, ship) &&
                          isValidCoord(ship.lat, ship.lon);
  // only a validated position reaches the visits and the track
  bool validated = false;
  if (newValidPosition) {
    const auto verdict = PositionCheck::judge(
        isValidCoord(lat_old, lon_old), lat_old, lon_old, stopped_old,
        (long)(msg->getRxTimeUnix() - tag.previous_signal), ship.lat, ship.lon,
        type == 9);
    validated = verdict == PositionCheck::VALIDATED;
    ship.setValidated(verdict);
  }
  // what the report changed on the record, now that it is whole
  if (!pending.empty())
    flushEvents(ptr);
  if (validated) {
    updateRest(ptr, msg->getRxTimeUnix(), still_before, tag.previous_signal);
    updatePlaceEvents(ptr, msg->getRxTimeUnix());
  }
  if (newValidPosition)
    destinations.position(ptr, ship);

  if (!copy && (type == 6 || type == 7 || type == 8 || type == 12 ||
                type == 13 || type == 14 || type == 23)) {
    const int recipients = (type == 7 || type == 13) ? 4 : 1;
    for (int i = 0; i < recipients; ++i) {
      int h = binary.process(data[0], ship.lat, ship.lon, i);
      if (h >= 0)
        binary.settle(h, [&](uint32_t m) { return ships.find(m) != SHIP_NIL; });
    }
    if (type == 12 || type == 14)
      noteSafety(ship, data[0]);
  }

  tag.shipclass = ship.shipclass;
  tag.speed = ship.speed;
  std::memcpy(tag.shipname, ship.shipname, sizeof(tag.shipname));

  tag.distance = DISTANCE_UNDEFINED;
  tag.angle = ANGLE_UNDEFINED;
  tag.validated = validated;

  if (newValidPosition) {
    if (validated && (type == 1 || type == 2 || type == 3 || type == 18 ||
                      type == 19 || type == 9)) {
      addToPath(ptr);
      sampleFix(ptr, ship.last_signal, false);
    }

    // measured from the station that heard it; the tracker's own position
    // stands in for a receiver that has none. A station that keeps its position
    // home measures the message for its own figures, but gives the vessel no
    // distance or bearing: from a vessel's position they would tell where it is
    const bool own = isValidCoord(tag.station_lat, tag.station_lon);
    const float from_lat = own ? tag.station_lat : station_lat;
    const float from_lon = own ? tag.station_lon : station_lon;
    if (isValidCoord(from_lat, from_lon)) {
      Util::Geodesy::distanceBearing(from_lat, from_lon, ship.lat, ship.lon,
                                     ship.distance, ship.angle);

      tag.distance = ship.distance;
      tag.angle = ship.angle;
      const StationRegistry::Station *heard = own ? stations.find(tag.station_id) : nullptr;
      if (!(own ? heard && heard->share_loc : latlon_share)) {
        ship.distance = DISTANCE_UNDEFINED;
        ship.angle = ANGLE_UNDEFINED;
      }
    }

    tag.lat = ship.lat;
    tag.lon = ship.lon;
  } else if (isValidCoord(lat_old, lon_old)) {
    tag.lat = lat_old;
    tag.lon = lon_old;
  } else {
    tag.lat = LAT_UNDEFINED;
    tag.lon = LON_UNDEFINED;
  }

  lock.unlock();
  Send(data, len, tag);
}

void DB::tick(std::time_t now) {
  // last_check/last_sweep are only written here, so they can be read before the
  // lock
#ifdef CHECK_DB_INTEGRITY
  bool check_due = now - last_check >= 60;
#else
  bool check_due = false;
#endif
  bool sweep_due = expire_fields && now - last_sweep >= time_history;
  bool rest_due = now - last_rest_sweep >= 60;

  if (!check_due && !sweep_due && !rest_due)
    return;

  std::lock_guard<std::mutex> lock(mtx);

  if (rest_due) {
    last_rest_sweep = now;
    sweepRest(now);
  }

#ifdef CHECK_DB_INTEGRITY
  if (check_due) {
    last_check = now;
    checkIntegrity();
  }
#endif

  if (!sweep_due)
    return;

  last_sweep = now;

  ships.forEach([&](int ptr) {
    Ship &ship = ships[ptr];
    char destination[sizeof(ship.destination)];
    std::memcpy(destination, ship.destination, sizeof(destination));
    const bool hadPosition = isValidCoord(ship.lat, ship.lon);
    ship.decayAndExpire();
    if (hadPosition && !isValidCoord(ship.lat, ship.lon)) {
      visits.refresh(ptr, {}, now, place_markers.index().get(),
                     [&](const VisitTracker::Visit &v, bool entering, uint32_t at) {
                       crossing(ptr, v, entering, at);
                     });
      full_refresh_at = now;
    }
    if (std::strcmp(destination, ship.destination))
      destinations.match(ptr, ship);
    return true;
  });
}

void DB::takeSnapshot(Snapshot &s) {
  std::lock_guard<std::mutex> lock(mtx);
  s.ships = ships;
  s.visits = visits;
  s.destinations = destinations;
  s.places = place_markers.index();
}

bool DB::Save(std::ofstream &file) {
  if (snapshot_save) {
    bool ok = false;
    withSnapshot([&](const Snapshot &s) {
      ok = writeTable(file, s.ships, s.visits, s.places.get());
    });
    return ok;
  }
  std::lock_guard<std::mutex> lock(mtx);
  return writeTable(file, ships, visits, place_markers.index().get());
}

bool DB::writeTable(std::ofstream &file, const SlotTable<Ship, uint32_t> &table,
                    const VisitTracker &visits, const PlaceIndex *places) {
  // Write magic number and version
  int magic = _DB_MAGIC;
  int version = _DB_VERSION;

  if (!file.write((const char *)&magic, sizeof(int)))
    return false;
  if (!file.write((const char *)&version, sizeof(int)))
    return false;

  // Write ship count first
  int count = table.size();
  if (!file.write((const char *)&count, sizeof(int)))
    return false;

  // Find the last ship by going count steps from first
  int ptr = table.front();
  for (int i = 1; i < count; i++) {
    if (ptr == SHIP_NIL)
      break;
    ptr = table.next(ptr);
  }

  // Write ships from last ship backwards to first
  int ships_written;
  for (ships_written = 0; ships_written < count; ships_written++) {
    if (ptr == SHIP_NIL)
      break;

    if (!table[ptr].Save(file))
      return false;

    ptr = table.prev(ptr);
  }

  if (!visits.save(file, places, [&](size_t slot) { return table[slot].mmsi; }))
    return false;

  Debug() << "DB: Saved " << ships_written << " ships to backup";
  return true;
}

bool DB::Load(std::ifstream &file) {
  std::lock_guard<std::mutex> lock(mtx);

  int magic = 0, version = 0;

  if (!file.read((char *)&magic, sizeof(int)))
    return false;
  if (!file.read((char *)&version, sizeof(int)))
    return false;

  if (magic != _DB_MAGIC || version < 1 || version > _DB_VERSION) {
    Warning() << "DB: Invalid backup file format. Magic: " << std::hex << magic
              << ", Version: " << version;
    return false;
  }

  // Read number of ships in backup
  int ship_count = 0;
  if (!file.read((char *)&ship_count, sizeof(int)))
    return false;

  if (ship_count < 0 || ship_count > ships.capacity()) {
    Warning() << "DB: Invalid ship count in backup file: " << ship_count;
    return false;
  }

  // Read all ships into a temporary buffer first, validating before modifying
  // DB state
  std::vector<Ship> temp_ships(ship_count);
  std::time_t previous_signal = 0;

  for (int i = 0; i < ship_count; i++) {
    if (!temp_ships[i].Load(file)) {
      Error() << "DB: Failed to read ship " << i << " from backup file";
      return false;
    }

    // Not persisted; treat all loaded ships as having static data
    temp_ships[i].last_static_signal = temp_ships[i].last_signal;

    // Not persisted; stale records are left to the sweep
    temp_ships[i].type_ttl = temp_ships[i].msg_type;

    if (temp_ships[i].mmsi == 0) {
      Error() << "DB: Ship with empty MMSI at index " << i;
      return false;
    }

    if (i > 0 && temp_ships[i].last_signal < previous_signal) {
      Error() << "DB: Ships not in chronological order at index " << i;
      return false;
    }

    previous_signal = temp_ships[i].last_signal;
  }

  VisitTracker restored;
  restored.setup(ship_count);
  std::unordered_map<uint32_t, int> slots;
  for (int i = 0; i < ship_count; ++i)
    if (!slots.emplace(temp_ships[i].mmsi, i).second)
      return false;
  if (version >= 2 &&
      !restored.load(
          file, place_markers.index().get(),
          [&](uint32_t key) {
            auto it = slots.find(key);
            return it == slots.end() ? -1 : it->second;
          },
          [&](size_t slot) { return temp_ships[slot].last_signal; }, ship_count,
          version))
    return false;

  // All validated, now apply to DB
  for (int i = 0; i < ship_count; i++) {
    int ptr = claimShip(temp_ships[i].mmsi);
    visits.restore(ptr, restored.record(i));
    install(ptr, temp_ships[i]);
    if (ships[ptr].idle_since)
      rest.open(ptr, ships[ptr]).start_seen = false;
  }

  full_refresh_at = time(nullptr);
  Info() << "DB: Restored " << ship_count << " ships from backup";
  return true;
}

#ifdef CHECK_DB_INTEGRITY
void DB::checkIntegrity() {
  std::vector<std::string> errors;
  int n = ships.validate(errors);

  int live = 0;
  for (int ptr = ships.front(); ptr != SHIP_NIL; ptr = ships.next(ptr)) {
    if (ships.key(ptr) != ships[ptr].mmsi) {
      errors.push_back("slot " + std::to_string(ptr) + " key " +
                       std::to_string(ships.key(ptr)) + " but mmsi " +
                       std::to_string(ships[ptr].mmsi));
      n++;
    }

    if (ships[ptr].mmsi != 0)
      live++;
  }

  if (live != ships.size()) {
    errors.push_back("count " + std::to_string(ships.size()) + " but " +
                     std::to_string(live) + " live records");
    n++;
  }

  n += paths.check(errors);

  for (std::size_t i = 0; i < errors.size(); i++)
    Error() << "DB integrity: " << errors[i];

  if (n == 0)
    Debug() << "DB integrity: OK (" << live << " ships)";
}
#endif

// A catalogue update also refreshes stationary ships, without inventing
// crossings.
void DB::setPlaces(std::shared_ptr<const PlaceIndex> next) {
  std::lock_guard<std::mutex> lock(mtx);
  if (next == place_markers.index())
    return;
  visits.remap(place_markers.index().get(), next.get());
  const auto changedBounds =
      place_markers.adopt(std::move(next), binary.sequence());
  full_refresh_at = time(nullptr);
  destinations.setPlaces(place_markers.index(), ships);
  // a few edited outlines: only the ships inside them; a reload: everyone
  const bool everyone = changedBounds.size() > 64;
  ships.forEach([&](int ptr) {
    const Ship &ship = ships[ptr];
    if (everyone ||
        std::any_of(changedBounds.begin(), changedBounds.end(),
                    [&](const PlaceMarkers::Bounds &b) {
                      return ship.lon >= b[0] && ship.lon <= b[1] &&
                             ship.lat >= b[2] && ship.lat <= b[3];
                    }))
      refreshPlaceMembership(ptr);
    return true;
  });
}

// Caller holds mtx. Only validated positions come here, so the tracker never
// sees a single damaged sentence.
void DB::updatePlaceEvents(int ptr, std::time_t now) {
  auto &ship = ships[ptr];
  // the rest detector's word, so a visit's idle minutes and a stay agree
  const bool stopped = ship.idle_since != 0;
  const auto *index = place_markers.index().get();
  visits.update(ptr, place_markers.containing(ship.lat, ship.lon), now, stopped,
                index,
                [&](const VisitTracker::Visit &visit, bool entering,
                    uint32_t observed) { crossing(ptr, visit, entering, observed); });
}

// Caller holds mtx. Every crossing goes on the stream, seen or inferred; a
// guard zone is what the user asked to be told about, so the ticker shows a
// seen crossing of one, while ports, sectors and waters are crossed all day.
void DB::crossing(int ptr, const VisitTracker::Visit &visit, bool entering,
                  uint32_t observed) {
  const auto *index = place_markers.index().get();
  const auto *entry = index ? index->find(visit.id) : nullptr;
  if (!entry)
    return;
  const Ship &ship = ships[ptr];
  Tracking::Event e =
      event(ship, entering ? Tracking::Kind::ENTER : Tracking::Kind::LEAVE,
            entry->metadata->is(PlaceKind::GuardZone) ? Tracking::Level::NOTICE
                                                      : Tracking::Level::ROUTINE,
            observed);
  e.crossing.place = visit.id;
  e.crossing.number = entry->number;
  e.crossing.revision = entry->revision;
  e.crossing.seen = entering ? visit.entryTime() != 0 : visit.exitTime() != 0;
  e.crossing.announced = entry->metadata->announced();
  e.crossing.name = entry->metadata->name.c_str();
  emit(e, ptr);
}

// Caller holds mtx (or is configuring before input starts).
void DB::updateDistance(Ship &ship) {
  ship.distance = DISTANCE_UNDEFINED;
  ship.angle = ANGLE_UNDEFINED;
  if (latlon_share && isValidCoord(station_lat, station_lon) &&
      isValidCoord(ship.lat, ship.lon))
    Util::Geodesy::distanceBearing(station_lat, station_lon, ship.lat, ship.lon,
                                   ship.distance, ship.angle);
}

// Every ship caches its distance and bearing to the station position they
// were last computed for. They are recomputed, and clients told to refetch
// everything, only once the station has moved further than the displayed
// precision from that position: GPS jitter must not turn every poll into a
// full dump.
void DB::updateStation(float lat, float lon) {
  if (station_lat == lat && station_lon == lon)
    return;
  station_lat = lat;
  station_lon = lon;
  const bool valid = isValidCoord(lat, lon),
             was = isValidCoord(distance_lat, distance_lon);
  if (valid == was &&
      (!valid || (std::fabs(lat - distance_lat) < 0.001f &&
                  std::fabs(lon - distance_lon) < 0.001f)))
    return;
  distance_lat = lat;
  distance_lon = lon;
  ships.forEach([&](int ptr) {
    updateDistance(ships[ptr]);
    return true;
  });
  full_refresh_at = time(nullptr);
}

void DB::measureFrom(const std::function<bool(const Ship &, float &, float &)> &origin) {
  std::lock_guard<std::mutex> lock(mtx);
  ships.forEach([&](int ptr) {
    Ship &ship = ships[ptr];
    float lat, lon;
    if (isValidCoord(ship.lat, ship.lon) && origin(ship, lat, lon))
      Util::Geodesy::distanceBearing(lat, lon, ship.lat, ship.lon, ship.distance, ship.angle);
    else
      updateDistance(ship);
    return true;
  });
}

// Caller holds mtx. What follows from a ship's position: distance and the
// places it is in.
void DB::locate(int ptr) {
  Ship &ship = ships[ptr];
  updateDistance(ship);
  refreshPlaceMembership(ptr);
}

void DB::refreshPlaceMembership(int ptr) {
  const Ship &ship = ships[ptr];
  visits.refresh(ptr, place_markers.containing(ship.lat, ship.lon),
                 ship.last_signal, place_markers.index().get(),
                 [&](const VisitTracker::Visit &v, bool entering, uint32_t at) {
                   crossing(ptr, v, entering, at);
                 });
}
