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

#include <cmath>
#include <cstdint>
#include <vector>

#include "Common.h"
#include "Geodesy.h"

// Which positions are worth keeping for a track that lasts: a ship under way
// reports every few seconds, and a line through all of them is the same line
// as one through the points where it changed. A fix is kept when the straight
// line from the last kept fix would miss the previous report by DEVIATION_M
// (a turn, kept at the report where it happened), when the speed moved by
// SPEED_KN, when INTERVAL has passed under way, and at rest when the ship has
// drifted REST_M or REST_INTERVAL has passed. No more than one fix per FLOOR
// seconds whatever the trigger, except one forced by another event. A value
// the ship did not report never decides anything.
//
// Two fixes per ship: the last kept and the last seen. Nothing here is
// allocated until the host asks for fixes, so a receiver that keeps only the
// hour of track in the path store pays nothing.
class FixSampler {
public:
  static const int DEVIATION_M = 50, REST_M = 200, FLOOR_S = 30;
  static const int INTERVAL_S = 10 * 60, REST_INTERVAL_S = 6 * 3600;
  static constexpr float SPEED_KN = 2.0f, REST_KN = 0.5f;

  struct Fix {
    uint32_t t = 0;
    float lat = LAT_UNDEFINED, lon = LON_UNDEFINED, speed = SPEED_UNDEFINED, cog = COG_UNDEFINED;
    int heading = HEADING_UNDEFINED;
    bool valid() const { return t != 0; }
  };

  // on: allocate the state; off: drop it and judge nothing
  void enable(bool on, size_t nships) {
    if (on && kept.empty()) {
      kept.assign(nships, Fix());
      seen.assign(nships, Fix());
    } else if (!on) {
      kept.clear();
      seen.clear();
    }
  }
  bool enabled() const { return !kept.empty(); }

  void wipe(size_t slot) {
    if (!enabled())
      return;
    kept[slot] = Fix();
    seen[slot] = Fix();
  }

  // What to tell about this report, judged before `keep` records it:
  //   TURN   the previous report was a turn: tell it, with its own time
  //   NOW    this report is worth keeping
  //   NONE   nothing
  // `resting` is the rest detector's word, so the two agree on what still is.
  enum Verdict { NONE, TURN, NOW };

  Verdict judge(size_t slot, uint32_t t, float lat, float lon, float speed,
                bool resting, Fix &turn) const {
    if (!enabled())
      return NONE;
    const Fix &k = kept[slot];
    if (!k.valid())
      return NOW;
    const Fix &s = seen[slot];
    if (resting) {
      if (t - k.t >= (uint32_t)REST_INTERVAL_S)
        return NOW;
      return metres(k.lat, k.lon, lat, lon) >= REST_M && t - k.t >= (uint32_t)FLOOR_S ? NOW : NONE;
    }
    // a turn at the previous report: the line from the kept fix to this one
    // misses it
    if (s.valid() && s.t > k.t && s.t - k.t >= (uint32_t)FLOOR_S &&
        offLine(k.lat, k.lon, lat, lon, s.lat, s.lon) >= DEVIATION_M) {
      turn = s;
      return TURN;
    }
    if (t - k.t < (uint32_t)FLOOR_S)
      return NONE;
    if (t - k.t >= (uint32_t)INTERVAL_S)
      return NOW;
    if (speed != SPEED_UNDEFINED && k.speed != SPEED_UNDEFINED &&
        std::fabs(speed - k.speed) >= SPEED_KN)
      return NOW;
    return NONE;
  }

  // the report was told (as a fix, or as the fix another event rests on)
  void keep(size_t slot, const Fix &f) {
    if (!enabled())
      return;
    kept[slot] = f;
    seen[slot] = f;
  }

  // the report was seen and not told
  void note(size_t slot, const Fix &f) {
    if (!enabled())
      return;
    seen[slot] = f;
  }

  const Fix &lastKept(size_t slot) const { return kept[slot]; }

private:
  std::vector<Fix> kept, seen;

  // metres between two positions, flat earth: the distances here are tens of
  // metres to a few kilometres
  static float metres(float lat1, float lon1, float lat2, float lon2) {
    const float k = Util::Geodesy::METERS_PER_DEGREE;
    const float dy = (lat2 - lat1) * k;
    const float dx = (lon2 - lon1) * k * std::cos(0.5f * (lat1 + lat2) * Util::Geodesy::PI_F / 180.0f);
    return std::sqrt(dx * dx + dy * dy);
  }

  // metres from point p to the line through a and b
  static float offLine(float alat, float alon, float blat, float blon,
                       float plat, float plon) {
    const float k = Util::Geodesy::METERS_PER_DEGREE;
    const float c = std::cos(alat * Util::Geodesy::PI_F / 180.0f);
    const float bx = (blon - alon) * k * c, by = (blat - alat) * k;
    const float px = (plon - alon) * k * c, py = (plat - alat) * k;
    const float len2 = bx * bx + by * by;
    if (len2 < 1.0f)
      return std::sqrt(px * px + py * py);
    // the cross product over the length is the distance to the line
    return std::fabs(bx * py - by * px) / std::sqrt(len2);
  }
};
