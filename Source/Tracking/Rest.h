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
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "Common.h"
#include "Event.h"
#include "Geodesy.h"
#include "Ships.h"

namespace Tracking {

// What a vessel did while it lay still, gathered from its reports: the mean
// position and the spread around it, how steady the heading held, the draught
// and the destination on arrival. The record itself carries only when the rest
// began and whether it was told, and is saved with it; this is kept for the
// ships at rest and lost on a restart, which the move then says.
class RestStore {
public:
  // stopped time counts between reports this close; a longer gap is silence
  static const uint32_t SAMPLE_S = 600;

  struct Accumulator {
    uint32_t reports = 0, headings = 0, seconds = 0;
    float lat0 = LAT_UNDEFINED, lon0 = LON_UNDEFINED; // the origin of the local metres
    double sx = 0, sy = 0, sxx = 0, syy = 0;          // sums of the local position
    double hx = 0, hy = 0;                             // the heading unit vectors
    float draught_in = DRAUGHT_UNDEFINED;
    char destination_in[21] = {0};
    bool start_seen = true; // false: counting began after the rest did

    void local(const Ship &ship, double &x, double &y) const {
      const double k = Util::Geodesy::METERS_PER_DEGREE;
      y = (ship.lat - lat0) * k;
      x = (ship.lon - lon0) * k * std::cos(lat0 * Util::Geodesy::PI_F / 180.0f);
    }
    // metres from the mean position so far to the ship's position now
    float drift(const Ship &ship) const {
      if (!reports)
        return 0;
      double x, y;
      local(ship, x, y);
      const double dx = x - sx / reports, dy = y - sy / reports;
      return (float)std::sqrt(dx * dx + dy * dy);
    }
  };

  Accumulator *find(int slot) {
    auto it = store.find(slot);
    return it == store.end() ? nullptr : &it->second;
  }

  Accumulator &open(int slot, const Ship &ship) {
    Accumulator &a = store[slot];
    a = Accumulator();
    a.lat0 = ship.lat;
    a.lon0 = ship.lon;
    a.draught_in = ship.draught;
    copyText(a.destination_in, ship.destination);
    return a;
  }

  // a report while at rest; `gap` the seconds since the one before
  static void add(Accumulator &a, const Ship &ship, uint32_t gap) {
    double x, y;
    a.local(ship, x, y);
    a.reports++;
    a.sx += x;
    a.sy += y;
    a.sxx += x * x;
    a.syy += y * y;
    if (ship.heading != HEADING_UNDEFINED) {
      const double h = ship.heading * Util::Geodesy::PI_F / 180.0;
      a.hx += std::sin(h);
      a.hy += std::cos(h);
      a.headings++;
    }
    if (gap <= SAMPLE_S)
      a.seconds += gap;
  }

  void close(int slot) { store.erase(slot); }

  template <class F> void forEach(F f) {
    for (auto &kv : store)
      f(kv.first, kv.second);
  }

  // the stay as MOVE reports it: the accumulator when there is one, the record
  // as it is now for what the vessel says on leaving
  static void fill(Stay &s, const Accumulator *a, const Ship &ship, bool end_seen) {
    s.end_seen = end_seen;
    if (a && a->reports) {
      const double n = a->reports, mx = a->sx / n, my = a->sy / n;
      const double k = Util::Geodesy::METERS_PER_DEGREE;
      s.lat = (float)(a->lat0 + my / k);
      s.lon = (float)(a->lon0 + mx / (k * std::cos(a->lat0 * Util::Geodesy::PI_F / 180.0f)));
      const double var = (a->sxx / n - mx * mx) + (a->syy / n - my * my);
      s.spread_m = (float)std::sqrt(var > 0 ? var : 0);
      s.reports = a->reports;
      s.seconds = a->seconds;
      if (a->headings) {
        const double len = std::sqrt(a->hx * a->hx + a->hy * a->hy);
        s.heading_r = (float)(len / a->headings);
        double deg = std::atan2(a->hx, a->hy) * 180.0 / Util::Geodesy::PI_F;
        if (deg < 0)
          deg += 360;
        s.heading = (float)deg;
      } else
        s.heading = -1;
      s.draught_in = a->draught_in;
      copyText(s.destination_in, a->destination_in);
      s.start_seen = a->start_seen;
    } else {
      s.lat = ship.lat;
      s.lon = ship.lon;
      s.heading = -1;
      s.draught_in = DRAUGHT_UNDEFINED;
      s.start_seen = false;
    }
    s.draught_out = ship.draught;
    copyText(s.destination_out, ship.destination);
    if (ship.month != ETA_MONTH_UNDEFINED && ship.day != ETA_DAY_UNDEFINED) {
      // via a roomy buffer: the fields are bytes, the compiler cannot know their range
      char eta[32];
      std::snprintf(eta, sizeof(eta), "%02d-%02d %02d:%02d", (int)ship.month,
                    (int)ship.day, (int)ship.hour, (int)ship.minute);
      copyText(s.eta, eta);
    }
  }

private:
  std::unordered_map<int, Accumulator> store;
};

} // namespace Tracking
