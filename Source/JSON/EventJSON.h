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

#include <cstdio>
#include <ctime>
#include <string>

#include "Common.h"
#include "Convert.h"
#include "Event.h"
#include "Stream.h"
#include "Writer.h"

// An event as one JSON object, the shape the AIS and GPS lines have: `class`
// says what it is, the envelope in front, then the key names and units the
// scaled AIS JSON uses, so a reader of those lines reads these with the same
// code. A value the ship did not report is left off the line.
namespace Tracking {

class EventJSON {
public:
  static const char *kindName(Kind k) {
    switch (k) {
    case Kind::FIX:
      return "fix";
    case Kind::STOP:
      return "stop";
    case Kind::MOVE:
      return "move";
    case Kind::ENTER:
      return "enter";
    case Kind::LEAVE:
      return "leave";
    case Kind::STATUS:
      return "status";
    case Kind::DRAUGHT:
      return "draught";
    case Kind::DESTINATION:
      return "destination";
    case Kind::NAME:
      return "name";
    case Kind::CALLSIGN:
      return "callsign";
    case Kind::ETA:
      return "eta";
    case Kind::SAFETY:
      return "safety";
    case Kind::SILENT:
      return "silent";
    case Kind::HEARD:
      return "heard";
    }
    return "fix";
  }

  // `start` is the epoch: the process start time, so (start, seq) is the key
  static void write(JSON::Writer &w, const Event &e, std::time_t start) {
    w.beginObject()
        .kv("class", "EVENT")
        .kv("device", "AIS-catcher")
        .kv("version", VERSION_NUMBER)
        .kv("start", (long long)start)
        .kv("seq", (long long)e.seq)
        .kv("event", kindName(e.kind))
        .kv("rxtime", Util::Convert::toTimeStr(e.t))
        .kv("rxuxtime", (long long)e.t)
        .kv("mmsi", (long long)e.mmsi);
    if (e.name && e.name[0])
      w.kv("shipname", e.name);
    if (isValidCoord(e.lat, e.lon))
      w.kv("lat", e.lat).kv("lon", e.lon);
    if (e.speed != SPEED_UNDEFINED)
      w.kv("speed", e.speed);
    if (e.cog != COG_UNDEFINED)
      w.kv("course", e.cog);
    if (e.heading != HEADING_UNDEFINED)
      w.kv("heading", e.heading);
    if (e.draught != DRAUGHT_UNDEFINED && e.draught > 0)
      w.kv("draught", e.draught);
    switch (e.kind) {
    case Kind::ENTER:
    case Kind::LEAVE:
      if (e.crossing.number)
        w.kv("place", (long long)e.crossing.number);
      if (e.crossing.name)
        w.kv("name", e.crossing.name);
      w.kv("revision", (long long)e.crossing.revision).kv("seen", e.crossing.seen);
      break;
    case Kind::SAFETY:
      if (e.safety.to)
        w.kv("to", (long long)e.safety.to);
      w.kv("text", e.to.getCString());
      break;
    case Kind::SILENT:
      w.kv("seconds", (long long)e.silence.seconds);
      break;
    case Kind::MOVE:
      w.kv("seconds", (long long)e.stay.seconds).kv("reports", (long long)e.stay.reports);
      if (isValidCoord(e.stay.lat, e.stay.lon))
        w.key("mean").beginObject().kv("lat", e.stay.lat).kv("lon", e.stay.lon).endObject();
      w.kv("spread", e.stay.spread_m);
      if (e.stay.heading >= 0)
        w.kv("heading_mean", e.stay.heading).kv("heading_r", e.stay.heading_r);
      if (e.stay.draught_in > 0)
        w.kv("draught_in", e.stay.draught_in);
      if (e.stay.draught_out > 0)
        w.kv("draught_out", e.stay.draught_out);
      if (e.stay.destination_in[0])
        w.kv("destination_in", e.stay.destination_in);
      if (e.stay.destination_out[0])
        w.kv("destination_out", e.stay.destination_out);
      if (e.stay.eta[0])
        w.kv("eta", e.stay.eta);
      if (!e.stay.start_seen)
        w.kv("start_seen", false);
      if (!e.stay.end_seen)
        w.kv("end_seen", false);
      break;
    case Kind::STATUS:
    case Kind::DRAUGHT:
    case Kind::DESTINATION:
    case Kind::NAME:
    case Kind::CALLSIGN:
    case Kind::ETA:
      value(w, "was", e.from);
      value(w, "now", e.to);
      break;
    default:
      break;
    }
    w.endObject();
  }

private:
  static void value(JSON::Writer &w, const char *key, const JSON::Value &v) {
    if (v.isInt())
      w.kv(key, (long long)v.getInt());
    else if (v.isFloat())
      w.kv(key, v.getFloat());
    else if (v.isString() || v.isCString())
      w.kv(key, v.getCString());
  }
};

// Every event as a JSON line on stderr: a way to watch the stream on any
// receiver (AIS_EVENTS_TRACE=1) and the shape an output sends.
class EventTrace : public StreamIn<Event> {
  std::time_t start;
  std::string line;

public:
  EventTrace() : start(std::time(nullptr)) {}
  void Receive(const Event *data, int len, TAG &) override {
    for (int i = 0; i < len; i++) {
      line.clear();
      JSON::Writer w(line, 512);
      EventJSON::write(w, data[i], start);
      w.finish();
      std::fprintf(stderr, "%s\n", line.c_str());
    }
  }
};

} // namespace Tracking
