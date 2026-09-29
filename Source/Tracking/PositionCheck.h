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

#include "Geodesy.h"
#include <cstdlib>

// Whether a new position can be the ship's, held against what its record had
// before: where it was, whether it said it was stopped, and how long ago it
// was heard. Only a validated position reaches the track and the visits, so
// one damaged sentence with a wrong MMSI, a jump, or a stale fix from a
// station that delivers late moves neither. Stateless: a jump costs the fix
// after it as well, and a ship that really has moved is validated by its
// second fix there.
namespace PositionCheck {
// the values of the ship's validated flag
enum Verdict { UNKNOWN = 0, VALIDATED = 1, JUMP = 2 };

const float VESSEL_KNOTS = 60, AIRCRAFT_KNOTS = 600;
// half a mile and a second of slack: position rounding, whole-second stamps
const float SLACK_NM = 0.5f;
// this long on, the old position says nothing
const long STALE = 1800;
// a ship that was stopped does not lie this far off within this time
const float STOPPED_NM = 200 / 1852.0f;
const long STOPPED_WINDOW = 900;

// `known`: the record held a position; `stopped`: it reported no speed to
// speak of with it; `seconds`: since the ship was last heard.
inline Verdict judge(bool known, float lat0, float lon0, bool stopped,
                     long seconds, float lat, float lon, bool aircraft) {
  seconds = std::labs(seconds);
  if (!known || seconds > STALE)
    return UNKNOWN;
  float distance;
  int bearing;
  Util::Geodesy::distanceBearing(lat0, lon0, lat, lon, distance, bearing);
  if (distance > (aircraft ? AIRCRAFT_KNOTS : VESSEL_KNOTS) * (seconds + 1) / 3600.0f + SLACK_NM)
    return JUMP;
  if (!aircraft && stopped && seconds <= STOPPED_WINDOW && distance > STOPPED_NM)
    return JUMP;
  return VALIDATED;
}
} // namespace PositionCheck
