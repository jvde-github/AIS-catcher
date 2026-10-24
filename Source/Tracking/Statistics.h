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
#include <atomic>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

#include "AIS-catcher.h"

#include "Convert.h"
#include "JSONAIS.h"
#include "Message.h"
#include "Stream.h"
#include "Writer.h"

// Distance from the receiver, in nautical miles, beyond which a position is
// treated as implausible and the message left out of the statistics.
const int LONG_RANGE_CUTOFF_DEFAULT = 2500;

// ----------------------------
// Class to log message count stat

class MessageStatistics {
public:
  static const int RADAR_BUCKETS = 18, FURTHEST = 3;
  // a vessel among the farthest of a period; an empty place has mmsi 0
  struct Far {
    uint32_t mmsi;
    uint32_t time;
    float nm, bearing;
  };
  // the figures as they stand, copied under the lock, for whoever writes them out
  struct Values {
    int count, exclude, vessels, active_minutes, distance_count;
    int msg[28], errors[4], channel[4];
    float level_min, level_max, ppm, distance;
    float radarA[RADAR_BUCKETS], radarB[RADAR_BUCKETS];
    Far furthest[FURTHEST];
  };

private:
  std::mutex mtx;

  static const int _MAGIC = 0x4f82b;
  static const int _VERSION = 5;
  static const int _RADAR_BUCKETS = 18;

  int _LONG_RANGE_CUTOFF = LONG_RANGE_CUTOFF_DEFAULT;

  int _count, _exclude, _vessels;
  int _msg[28];
  int _errors[4]; // flagged messages, undersized, oversized, checksum
  int _channel[4];

  float _level_min, _level_max, _ppm, _distance;
  double _distance_sum;
  int _distance_count;
  float _radarA[_RADAR_BUCKETS];
  float _radarB[_RADAR_BUCKETS];

  // the minutes in which a message was heard, as the figures that hold this count them
  int _active_minutes;
  // the farthest vessels, each once, the farthest first
  static const int _FURTHEST = FURTHEST;
  Far _far[_FURTHEST];

  // a vessel's position at `nm`: in the list when it is farther than one there,
  // in its own place when it is there already
  void noteFar(uint32_t mmsi, uint32_t time, float nm, float bearing) {
    if (!(nm > 0))
      return;
    int at = _FURTHEST - 1;
    for (int i = 0; i < _FURTHEST; i++)
      if (_far[i].mmsi == mmsi) {
        if (nm <= _far[i].nm)
          return;
        at = i;
        break;
      }
    if (_far[at].mmsi && _far[at].mmsi != mmsi && nm <= _far[at].nm)
      return;
    _far[at] = {mmsi, time, nm, bearing};
    for (int i = at; i > 0 && _far[i].nm > _far[i - 1].nm; i--)
      std::swap(_far[i], _far[i - 1]);
  }

public:
  MessageStatistics() { Clear(); }

  int getCount() { return _count; }
  void setCutoff(int cutoff) {
    std::lock_guard<std::mutex> lock(mtx);
    _LONG_RANGE_CUTOFF = cutoff;
  }
  void clearVessels() { _vessels = 0; }
  void addActiveMinute() {
    std::lock_guard<std::mutex> l{this->mtx};
    _active_minutes++;
  }
  // the farthest validated position, -1 when there is none; the `i`th farthest vessel's, -1 when fewer
  float maxDistance() {
    std::lock_guard<std::mutex> l{this->mtx};
    return _distance > 0 ? _distance : -1;
  }
  float farDistance(int i) {
    std::lock_guard<std::mutex> l{this->mtx};
    return i >= 0 && i < _FURTHEST && _far[i].mmsi ? _far[i].nm : -1;
  }

  void Clear() {
    std::lock_guard<std::mutex> l{this->mtx};

    std::memset(_msg, 0, sizeof(_msg));
    std::memset(_errors, 0, sizeof(_errors));
    std::memset(_channel, 0, sizeof(_channel));
    std::memset(_radarA, 0, sizeof(_radarA));
    std::memset(_radarB, 0, sizeof(_radarB));

    _count = _vessels = _exclude = _active_minutes = 0;
    std::memset(_far, 0, sizeof(_far));
    _distance = _ppm = 0;
    _distance_sum = 0.0;
    _distance_count = 0;
    _level_min = 1e6;
    _level_max = -1e6;
  }

  void Add(const AIS::Message &m, const TAG &tag, bool new_vessel = false) {

    std::lock_guard<std::mutex> l{this->mtx};

    if (m.type() > 28 || m.type() < 1)
      return;

    _count++;
    if (tag.quality & MESSAGE_QUALITY_ERRORS)
      _errors[0]++;
    if (tag.quality & MESSAGE_QUALITY_UNDERSIZED)
      _errors[1]++;
    if (tag.quality & MESSAGE_QUALITY_OVERSIZED)
      _errors[2]++;
    if (tag.quality & MESSAGE_QUALITY_CHECKSUM)
      _errors[3]++;
    if (new_vessel)
      _vessels++;

    _msg[m.type() - 1]++;
    if (m.getChannel() >= 'A' && m.getChannel() <= 'D')
      _channel[m.getChannel() - 'A']++;

    if (tag.level == LEVEL_UNDEFINED || tag.ppm == PPM_UNDEFINED)
      _exclude++;
    else {
      _level_min = MIN(_level_min, tag.level);
      _level_max = MAX(_level_max, tag.level);
      _ppm += tag.ppm;
    }

    // for range we ignore atons
    if (m.type() == 21 || m.type() == 28)
      return;

    if (!tag.validated || tag.distance > _LONG_RANGE_CUTOFF ||
        (m.repeat() > 0 && m.type() != 27))
      return;

    if (tag.distance > _distance) {
      _distance = tag.distance;
    }
    noteFar(m.mmsi(), (uint32_t)m.getRxTimeUnix(), tag.distance, (float)tag.angle);
    _distance_sum += tag.distance;
    _distance_count++;

    if (m.type() == 18 || m.type() == 19 || m.type() == 24) {
      if (tag.angle >= 0 && tag.angle < 360) {
        int bucket = tag.angle / (360 / _RADAR_BUCKETS);
        if (tag.distance > _radarB[bucket])
          _radarB[bucket] = tag.distance;
      }
    } else if (m.type() <= 3 || m.type() == 5 || m.type() == 27) {
      if (tag.angle >= 0 && tag.angle < 360) {
        int bucket = tag.angle / (360 / _RADAR_BUCKETS);
        if (tag.distance > _radarA[bucket])
          _radarA[bucket] = tag.distance;
      }
    }
  }

  // another's figures counted in: the counts add up, the extremes take the extreme
  void add(MessageStatistics &o) {
    if (&o == this)
      return;
    std::lock(mtx, o.mtx);
    std::lock_guard<std::mutex> a{mtx, std::adopt_lock}, b{o.mtx, std::adopt_lock};
    _count += o._count;
    _exclude += o._exclude;
    _vessels += o._vessels;
    for (int i = 0; i < 28; i++)
      _msg[i] += o._msg[i];
    for (int i = 0; i < 4; i++) {
      _errors[i] += o._errors[i];
      _channel[i] += o._channel[i];
    }
    _level_min = MIN(_level_min, o._level_min);
    _level_max = MAX(_level_max, o._level_max);
    _ppm += o._ppm;
    _distance = MAX(_distance, o._distance);
    // the minutes any one heard in, as a floor of what all of them did
    _active_minutes = MAX(_active_minutes, o._active_minutes);
    for (int i = 0; i < _FURTHEST; i++)
      if (o._far[i].mmsi)
        noteFar(o._far[i].mmsi, o._far[i].time, o._far[i].nm, o._far[i].bearing);
    _distance_sum += o._distance_sum;
    _distance_count += o._distance_count;
    for (int i = 0; i < _RADAR_BUCKETS; i++) {
      _radarA[i] = MAX(_radarA[i], o._radarA[i]);
      _radarB[i] = MAX(_radarB[i], o._radarB[i]);
    }
  }

  Values values() {
    std::lock_guard<std::mutex> l{this->mtx};
    Values v;
    v.count = _count;
    v.exclude = _exclude;
    v.vessels = _vessels;
    v.active_minutes = _active_minutes;
    v.distance_count = _distance_count;
    std::memcpy(v.msg, _msg, sizeof(_msg));
    std::memcpy(v.errors, _errors, sizeof(_errors));
    std::memcpy(v.channel, _channel, sizeof(_channel));
    v.level_min = _level_min;
    v.level_max = _level_max;
    v.ppm = _ppm;
    v.distance = _distance;
    std::memcpy(v.radarA, _radarA, sizeof(_radarA));
    std::memcpy(v.radarB, _radarB, sizeof(_radarB));
    std::memcpy(v.furthest, _far, sizeof(_far));
    return v;
  }

  void print(std::ostream &out, const char *indent = "") {
    std::lock_guard<std::mutex> l{this->mtx};

    int c = _count - _exclude;
    bool has_level = c > 0 && _level_min <= _level_max;

    out << indent << "messages: " << _count << "\n";
    if (_distance_count > 0) {
      out << indent << "distance: max=" << _distance
          << " avg=" << (_distance_sum / _distance_count)
          << " (n=" << _distance_count << ")\n";
    }
    if (has_level) {
      out << indent << "signal:   min=" << _level_min << " max=" << _level_max
          << " ppm=" << (_ppm / c) << "\n";
    }
    out << indent << "channel:  A=" << _channel[0] << " B=" << _channel[1];
    if (_channel[2])
      out << " C=" << _channel[2];
    if (_channel[3])
      out << " D=" << _channel[3];
    out << "\n";

    bool any_type = false;
    for (int i = 0; i < 28; i++)
      if (_msg[i] > 0) {
        any_type = true;
        break;
      }
    if (any_type) {
      out << indent << "by type: ";
      for (int i = 0; i < 28; i++)
        if (_msg[i] > 0)
          out << " " << (i + 1) << "=" << _msg[i];
      out << "\n";
    }
  }

#define W(x) file.write((const char *)&(x), sizeof(x))
#define R(x) file.read((char *)&(x), sizeof(x))

  bool Save(std::ofstream &file) {
    std::lock_guard<std::mutex> l{this->mtx};

    int magic = _MAGIC;
    int version = _VERSION;

    return (bool)(W(magic) && W(version) && W(_count) && W(_vessels) &&
                  W(_msg) && W(_channel) && W(_level_min) && W(_level_max) &&
                  W(_ppm) && W(_distance) && W(_radarA) && W(_radarB) &&
                  W(_errors) && W(_active_minutes) && W(_far));
  }

  bool Load(std::ifstream &file) {
    std::lock_guard<std::mutex> l{this->mtx};

    std::memset(_errors, 0, sizeof(_errors));
    std::memset(_far, 0, sizeof(_far));
    _active_minutes = 0;
    int magic = 0, version = 0;
    if (!(R(magic) && R(version) && R(_count)))
      return false;
    if (magic != _MAGIC)
      return false;
    if (version < 1 || version > _VERSION)
      return false;

    if (version >= 2 && !R(_vessels))
      return false;
    if (version < 2)
      _vessels = 0;

    // v3 grew _msg from 27 to 28 entries to include type 28 (AtoN single-slot
    // report).
    int n = (version >= 3) ? 28 : 27;
    if (!file.read((char *)_msg, n * sizeof(int)))
      return false;
    if (version < 3)
      _msg[27] = 0;

    return (bool)(R(_channel) && R(_level_min) && R(_level_max) && R(_ppm) &&
                  R(_distance) && R(_radarA) && R(_radarB) &&
                  (version < 4 || R(_errors)) &&
                  (version < 5 || (R(_active_minutes) && R(_far))));
  }

#undef W
#undef R
};

class Counter : public StreamIn<JSON::JSON> {
  MessageStatistics stat;

public:
  void setCutOff(int c) { stat.setCutoff(c); }
  void Clear() { stat.Clear(); }
  void add(Counter &o) { stat.add(o.stat); }

  bool Load(std::ifstream &file) { return stat.Load(file); }
  bool Save(std::ofstream &file) { return stat.Save(file); }

  void Receive(const JSON::JSON *msg, int len, TAG &tag) {
    stat.Add(*((AIS::Message *)msg[0].binary), tag);
  }

  MessageStatistics::Values values() { return stat.values(); }
  void addActiveMinute() { stat.addActiveMinute(); }
  float maxDistance() { return stat.maxDistance(); }

  void print(std::ostream &out, const char *indent = "") {
    stat.print(out, indent);
  }
};

struct ByteCounter : public StreamIn<RAW> {
  AIS::Filter filter;
  // Split into two 32-bit atomics so armv6/armel don't need libatomic.
  // received_hi counts 4 GB wraps of received_lo. Byte precision preserved.
  std::atomic<uint32_t> received_lo{0};
  std::atomic<uint32_t> received_hi{0};
  virtual ~ByteCounter() {}

  void Receive(const RAW *data, int len, TAG &tag) {
    uint32_t add = (uint32_t)data[0].size;
    uint32_t before = received_lo.fetch_add(add, std::memory_order_relaxed);
    if (before + add < before)
      received_hi.fetch_add(1, std::memory_order_relaxed);
  }

  uint64_t received() const {
    uint32_t h1 = received_hi.load(std::memory_order_relaxed);
    uint32_t lo = received_lo.load(std::memory_order_relaxed);
    uint32_t h2 = received_hi.load(std::memory_order_relaxed);
    if (h1 != h2)
      lo = received_lo.load(std::memory_order_relaxed);
    return ((uint64_t)h2 << 32) | lo;
  }

  void Reset() {
    received_lo.store(0, std::memory_order_relaxed);
    received_hi.store(0, std::memory_order_relaxed);
  }
  void setFilter(const AIS::Filter &f) { filter = f; }
};
