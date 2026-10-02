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

#include <cstdint>
#include <cstring>
#include <ctime>

#include "Common.h"
#include "JSON.h"

// What the ship database decided about a vessel, told once: a crossing, a
// stop, a sampled position, a field of the record changing, a safety message.
// The database runs the detectors on every validated fix and sends each event
// on a stream; whoever holds a view subscribes (the ticker's ring, the visits,
// an output, the hub's journal) and copies what it keeps while Receive runs.
//
// One shape for every kind: the sequence, the fix the event rests on in the
// record's own units, the kind, and a union over the kind's own fields, so
// nothing is empty, only inactive. Plain data: no heap, nothing to free. The
// strings behind `from` and `to` are the arrays here, or text the caller owns
// for the duration of Send; the ring and the outputs copy, nothing points past
// the call.
namespace Tracking {

enum class Kind : uint8_t {
  FIX = 1, // a sampled position, kept because the line would be wrong without it
  STOP,    // the vessel has lain still for a minute, stamped with the first stopped fix
  MOVE,    // it is under way again; carries the stay's statistics
  ENTER,   // crossed into a place, confirmed by the next fix
  LEAVE,   // crossed out of a place
  STATUS,  // navigation status changed
  DRAUGHT,
  DESTINATION,
  NAME,
  CALLSIGN,
  ETA,
  SAFETY, // a safety message
  SILENT, // an hour without a fix
  HEARD   // the first fix after
};

// what the ticker shows it as: routine events never push out a distress call
enum class Level : uint8_t { ROUTINE = 0, NOTICE = 1, URGENT = 2, LEVELS = 3 };

// A stay, as MOVE reports it: where the vessel lay and what it said while it
// lay there. The destinations are bounded by AIS itself.
// Plain data without initializers, so the union it sits in stays trivial;
// Event() clears it.
struct Stay {
  uint32_t seconds, reports;
  float lat, lon;             // mean position
  float spread_m;             // RMS distance from it
  float heading, heading_r;   // mean heading, and the length of the mean vector
  float draught_in, draught_out;
  char destination_in[21], destination_out[21];
  char eta[12];
  bool start_seen;            // false: the stay was already under way when counting began
};

struct Event {
  uint64_t seq = 0; // assigned when sent; the process start time is the epoch

  // the fix the event rests on
  std::time_t t = 0;
  uint32_t mmsi = 0;
  float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
  float speed = SPEED_UNDEFINED, cog = COG_UNDEFINED, draught = DRAUGHT_UNDEFINED;
  int heading = HEADING_UNDEFINED;

  Kind kind = Kind::FIX;
  Level level = Level::ROUTINE;

  union {
    struct {
      uint32_t place;     // the place's runtime id; a subscriber resolves number and name
      long revision;      // the place's revision it was judged against
      bool seen;          // a crossing observed, not inferred from containment
      bool announced;     // the place asked to be told about: a guard zone, not a sector crossed all day
      const char *name;   // the place's name, the catalogue's own string, for the duration of Send
    } crossing;           // ENTER LEAVE
    struct {
      uint32_t to;        // the addressed MMSI, 0 for a broadcast
      const char *to_name; // its name when known, for the duration of Send
    } safety;             // SAFETY
    struct {
      uint32_t seconds;
    } silence;            // SILENT
    Stay stay;            // MOVE
  };

  // STATUS DRAUGHT DESTINATION NAME CALLSIGN ETA: the old and the new value,
  // an int, a float or a string in one type · SAFETY: `to` is the message
  JSON::Value from, to;
  char from_s[21] = {0}, to_s[21] = {0};

  // the vessel's name as the record has it, for subscribers that display; the
  // record's own array, for the duration of Send
  const char *name = nullptr;

  Event() { std::memset(&stay, 0, sizeof(stay)); from.setNull(); to.setNull(); }

  // the string values point at the arrays, so an event can be copied whole
  void setText(const char *was, const char *now) {
    std::strncpy(from_s, was ? was : "", sizeof(from_s) - 1);
    std::strncpy(to_s, now ? now : "", sizeof(to_s) - 1);
    from.setCString(from_s);
    to.setCString(to_s);
  }
};

} // namespace Tracking
