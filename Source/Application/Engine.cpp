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

#include <chrono>
#include <sstream>

#include "AIS-catcher.h"

#include "ControlCore.h"
#include "Engine.h"
#include "Site.h"

#include <algorithm>
#include "Logger.h"

using namespace std::chrono;

static std::string alignModelName(const std::string &name) {
  return "[" + name + "] " +
         std::string(name.length() < 37 ? 37 - name.length() : 0, ' ');
}

Engine::Engine() { receivers.push_back(std::unique_ptr<Receiver>(new Receiver())); }

Engine::~Engine() { detach(); }

void Engine::detach() {
  if (detached)
    return;
  detached = true;

  for (auto &r : receivers)
    r->stop();

  // the site lets go of the outputs before they go
  if (site)
    site->detachEngine();
}

std::function<void(Engine &)> Engine::before_run;
std::function<void(Engine &)> Engine::after_run;

// how many receivers name each station: one no receiver names takes GPS from every input
void Engine::countStationInputs() {
  StationRegistry counted;
  for (auto s : stations.ordered()) {
    s.inputs = 0;
    for (const auto &r : receivers)
      s.inputs += r->getStation() == s.id;
    counted.define(s);
  }
  stations = counted;
}

// Stations told again while the engine runs: the configured ones as the file
// now has them; one a receiver still names, or a receiver's own, as it was;
// then the site hears of them. Which receiver uses which station changes only
// with a restart.
void Engine::refreshStations(const StationRegistry &configured) {
  const auto named = [&](int id) {
    for (const auto &r : receivers)
      if (r->getStation() == id)
        return true;
    return false;
  };
  StationRegistry next;
  for (auto s : configured.ordered()) {
    // a configured station without a position keeps the one its receiver gave it
    const StationRegistry::Station *had = stations.find(s.id);
    if (!isValidCoord(s.lat, s.lon) && had) {
      s.lat = had->lat;
      s.lon = had->lon;
    }
    next.define(s);
  }
  for (const auto &s : stations.ordered())
    if (!configured.find(s.id) && named(s.id))
      next.define(s);
  stations = next;
  countStationInputs();
  if (site)
    site->publishStations(stations);
}

// what a receiver is called on the status pages: its own name, else its device's
static std::string receiverLabel(const Receiver &r, Device::Device *device) {
  if (!r.getDescription().empty() || !device)
    return r.getDescription();
  const std::string &serial = device->getSerial();
  if (serial == ".")
    return "Console";
  return device->getProduct() + (serial.empty() ? "" : " " + serial);
}

#ifdef HASAPI
// what the site's system service says of each receiver: its station, its
// input, whether it streams, and the messages its models decoded
void Engine::publishReceivers() {
  std::vector<ReceiverState> states;
  for (std::size_t k = 0; k < receivers.size(); k++) {
    Receiver &r = *receivers[k];
    Device::Device *device = r.getDeviceManager().getDevice();
    ReceiverState s;
    s.index = (int)k + 1;
    s.station = r.getStation();
    if (device) {
      s.input = device->getProduct();
      s.streaming = device->isStreaming();
      s.lost = device->isLost();
    }
    s.label = receiverLabel(r, device);
    if (k < stat.size())
      for (auto &c : stat[k].statistics)
        s.messages += c->getCount();
    states.push_back(s);
  }
  site->publishReceivers(states);
}
#endif

// Every receiver stands at a station: the one the configuration names, or the
// default station, 0. A station without a position takes its first receiver's
// with one; a receiver takes its station's.
void Engine::resolveStations() {
  std::vector<int> listed;
  stations.forEach([&](const StationRegistry::Station &s) { listed.push_back(s.id); });
  const bool configured = !listed.empty();
  bool at_default = false;
  for (const auto &r : receivers)
    at_default |= r->getStation() == 0;
  if (at_default && !stations.find(0))
    stations.define(defaultStation(!configured));
  for (std::size_t k = 0; k < receivers.size(); k++) {
    Receiver &r = *receivers[k];
    const int id = r.getStation();
    // Config::finish() saw to it that the configuration defines it
    const StationRegistry::Station *found = stations.find(id);
    if (!found)
      throw std::logic_error("station " + std::to_string(id) + " was not resolved");
    StationRegistry::Station s = *found;
    if (isValidCoord(s.lat, s.lon)) {
      if (r.hasPosition() && (r.getLat() != s.lat || r.getLon() != s.lon))
        Warning() << "Receiver " << k + 1 << ": the position of station " << id
                  << " is used, not the receiver's own";
    } else if (r.hasPosition()) {
      s.lat = r.getLat();
      s.lon = r.getLon();
      stations.define(s);
    }
    // once stations are configured, the receivers' messages say which station heard them
    const bool named = std::find(listed.begin(), listed.end(), id) != listed.end();
    r.placeAtStation(named ? s.name : std::string(), s.lat, s.lon, configured);
  }
  countStationInputs();
  site->publishStations(stations);
  if (receivers.size() < 2 && !configured)
    return;
  for (std::size_t k = 0; k < receivers.size(); k++) {
    const Receiver &r = *receivers[k];
    const StationRegistry::Station *s = stations.find(r.getStation());
    Info() << "Receiver " << k + 1 << ": station " << r.getStation() << " \""
           << (s ? s->name : std::string()) << "\""
           << (r.hasPosition() ? " at " + std::to_string(r.getLat()) + ", " + std::to_string(r.getLon())
                               : std::string(", no position"));
  }
}

// The default station the configuration does not list, as a viewer section
// describes it (-N STATION, LAT, LON, SHARE_LOC, USE_GPS, OWN_MMSI): named
// after the one receiver standing on it when that has a name of its own, else
// as the section says, else not at all; placed where the first such receiver
// with a position of its own is, else where the section puts it.
StationRegistry::Station Engine::defaultStation(bool alone) const {
  StationRegistry::Station s;
  s.id = 0;
  s.share_loc = station_defaults.share_loc;
  s.gps = station_defaults.gps;
  s.mmsi = alone ? station_defaults.mmsi : 0;
  s.alt = station_defaults.alt;
  std::size_t unnamed = 0;
  for (const auto &r : receivers) {
    if (r->getStation() != 0)
      continue;
    if (++unnamed == 1)
      s.name = r->getDescription();
    else
      s.name.clear();
    if (!isValidCoord(s.lat, s.lon) && r->hasPosition()) {
      s.lat = r->getLat();
      s.lon = r->getLon();
    }
  }
  if (s.name.empty())
    s.name = station_defaults.name;
  if (!isValidCoord(s.lat, s.lon) && station_defaults.placed()) {
    s.lat = station_defaults.lat;
    s.lon = station_defaults.lon;
  }
  return s;
}

void Engine::run(Site &on, ControlCore *control) {
  site = &on;
  resolveStations();

  // -------------
  // set up the receiver and open the device

  stat.resize(receivers.size());
  msg_count.resize(receivers.size(), 0);

  // the trackers read the decoders' tags
  bool has_server = false;
#ifdef HASAPI
  has_server = on.tracker() || site_config.wantsTracker() || !site_config.servers.empty();
#endif
  bool has_http = false;
  for (auto &o : msg)
    if (dynamic_cast<IO::HTTPStreamer *>(o.get())) {
      has_http = true;
      break;
    }

  int group = 0;

  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];
    r.SetKey(AIS::KEY_SETTING_OWN_MMSI, std::to_string(own_mmsi));

    if (has_server)
      r.setTags("DTM");
    if (has_http)
      r.setTags("DT");

    r.setupDevice();

    // set up the decoding model(s), group is the last output group used
    r.setupModel(group, i);
  }

  on.configure(site_config);
#ifdef HASAPI
  Tracker *tracker = on.tracker();
#endif

  // sharing is opt-in via -X: the silent get an encouraging word, not a feed
  if (!xshare_defined && !comm_feed && (has_server || !msg.empty()) && !control)
    Warning() << "Hint: support the aiscatcher.org community by sharing your "
                 "feed with '-X on'. Sharing is currently OFF.";

  if (comm_feed && (control ? !comm_feed->hasUUID() : !xshare_defined)) {
    uint64_t live_groups = 0;
    for (const auto &r : receivers) {
      Type t = r->getDeviceManager().InputType();
      if (t == Type::RTLSDR || t == Type::AIRSPY || t == Type::AIRSPYHF ||
          t == Type::HACKRF || t == Type::SDRPLAY || t == Type::HYDRASDR ||
          t == Type::SOAPYSDR)
        live_groups |= r->getGroupMask();
    }

    comm_feed->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(live_groups));
  }

  if (!control)
    Info() << "Community feed sharing: "
           << (comm_feed ? "enabled." : "disabled.");

  // Zones are routes. An output with zones takes the receivers that share one; once any receiver
  // is in a zone, an output without zones takes only the receivers without one. Narrowing only:
  // what an output already keeps out, as the community feed keeps out network inputs, stays out.
  // A server's own tracker is routed the same way.
  bool any_zoned = false;
  const uint64_t unzoned = resolveUnzoned(receivers, any_zoned);
  const auto route = [&](const std::vector<std::string> &zones, uint64_t groups_in, const char *what) {
    uint64_t mask = groups_in;
    if (!zones.empty()) {
      mask &= resolveZones(receivers, zones);
      if (!mask)
        Warning() << what << " has zone filter but no matching receivers — will receive nothing";
    } else if (any_zoned) {
      mask &= unzoned;
      if (!mask)
        Warning() << what << " without zones while every receiver has one — will receive nothing";
    }
    return mask;
  };
  for (auto &o : msg) {
    if (o->zones.empty() && !any_zoned)
      continue;
    o->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(route(o->zones, o->StreamIn<AIS::Message>::getGroupsIn(), "Output")));
  }

  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];

    // set up all the output and connect to the receiver outputs
    for (auto &o : msg)
      o->Connect(r);

    screen.Connect(r);

    // the site's system service counts each receiver's messages too
    stat[i].connect(r);
  }

  if (receivers.size() > 1) {
    Debug() << "Mutex: enabling exclusive on " << msg.size()
            << " message outputs + screen (" << receivers.size()
            << " receivers)";
    for (auto &o : msg)
      o->setExclusive(true);
    screen.setExclusive(true);
  } else {
    Debug() << "Mutex: single receiver, all sinks lock-free";
  }

  // what the site's status pages say of this session: its devices, a model
  // line per device, its outputs and the community feed
  {
    Site::EngineFacts facts;
    for (auto &r : receivers) {
      Device::Device *device = r->getDeviceManager().getDevice();
      if (!device)
        continue;
      Site::DeviceFacts d;
      d.product = device->getProduct();
      d.vendor = device->getVendor().empty() ? "-" : device->getVendor();
      d.serial = device->getSerial().empty() ? "-" : device->getSerial();
      d.sample_rate = device->getRateDescription();
      d.label = receiverLabel(*r, device);
      for (int j = 0; j < r->Count(); j++)
        d.model += (j ? "\n" : "") + r->Model(j)->getName();
      facts.devices.push_back(d);
      *device >> on.received();
    }
    for (auto &o : msg)
      facts.outputs.push_back(o.get());
    facts.sharing = comm_feed != nullptr;
    facts.sharing_uuid = comm_feed != nullptr && comm_feed->hasUUID();
    on.attachEngine(facts);
  }

#ifdef HASAPI
  // the site's tracker takes every receiver; a server's own takes those in its zones and groups
  const auto feed = [&](Tracker *t, uint64_t mask) {
    for (auto &r : receivers)
      for (int j = 0; j < r->Count(); j++) {
        if (!r->Output(j).canConnect(mask))
          continue;
        r->OutputJSON(j).Connect(&t->json());
        r->OutputGPS(j).Connect(&t->gps());
        r->OutputControl(j).Connect(&t->control());
        r->OutputADSB(j).Connect(&t->adsb());
      }
  };
  if (tracker)
    feed(tracker, GROUPS_ALL);
  for (const auto &own : on.ownTrackers())
    feed(own.tracker, route(own.spec.zones, own.spec.groups_in, "Web viewer"));
#endif

  if (control)
    for (auto &r : receivers)
      for (int j = 0; j < r->Count(); j++)
        r->Output(j).Connect(
            (StreamIn<AIS::Message> *)&control->getChannelActivity());

  for (auto &o : msg)
    o->Start();

  // the servers serve and the tracker is wired, the receivers have not
  // started: a build that adds services attaches them now
  if (before_run)
    before_run(*this);

  Debug() << "Starting receivers";
  for (auto &r : receivers)
    r->play();

  if (control) {
    control->reportRunning();
    Info() << "Control: engine started";

    for (auto &r : receivers)
      if (!r->verbose)
        r->logSummary();
  }

  const int SLEEP = 50;
  auto time_start = high_resolution_clock::now();
  auto time_timeout_start = time_start;
  auto time_last = time_start;
  // steady: high_resolution_clock is the wall clock on libstdc++
  auto next_poll = steady_clock::now();
  bool oneverbose = false, iscallback = true;

  for (auto &r : receivers) {
    oneverbose |= r->verbose;
    iscallback &= r->getDeviceManager().getDevice()->isCallback();
  }

  Debug() << "Entering main loop";
#ifdef HASAPI
  auto next_status = steady_clock::now();
#endif
  while (!stop) {
    for (auto &r : receivers)
      stop = stop || !(r->getDeviceManager().getDevice()->isActive());

#ifdef HASAPI
    // the site's system pages, about once a second, and once more when a
    // receiver stops the session, so its loss is told
    if (stop || steady_clock::now() >= next_status) {
      next_status = steady_clock::now() + seconds(1);
      publishReceivers();
    }
#endif

    // for non-callback devices (WAV file), the isStreaming() poll above pumps
    // the data, so the loop must run unthrottled
    if (iscallback)
      std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

    if (loop_hook)
      loop_hook();
    // outputs make and keep their connections here, off the decoding thread;
    // paced, as the loop runs unthrottled for a file input
    auto poll_now = steady_clock::now();
    if (poll_now >= next_poll) {
      next_poll = poll_now + milliseconds(SLEEP);
      for (auto &o : msg)
        o->poll();
    }

    if (!oneverbose && !timeout)
      continue;

    auto time_now = high_resolution_clock::now();

    if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >=
                          screen.verboseUpdateTime) {
      time_last = time_now;

      for (int i = 0; i < (int)receivers.size(); i++) {
        Receiver &r = *receivers[i];
        if (r.verbose) {
          for (int j = 0; j < r.Count(); j++) {
            stat[i].statistics[j]->Stamp();
            std::string name = r.Model(j)->getName() + " #" +
                               std::to_string(i) + "-" + std::to_string(j);
            Info() << alignModelName(name)
                   << "received: " << stat[i].statistics[j]->getDeltaCount()
                   << " msgs, total: " << stat[i].statistics[j]->getCount()
                   << " msgs, rate: " << stat[i].statistics[j]->getRate()
                   << " msg/s";
          }
        }
      }
    }

    // watchdog: stop when any receiver went without messages for the timeout
    // period
    if (timeout && timeout_nomsg) {
      bool one_stale = false;

      for (int i = 0; i < (int)receivers.size(); i++) {
        uint64_t count = 0;
        for (auto &s : stat[i].statistics)
          count += s->getCount();

        if (count == msg_count[i])
          one_stale = true;

        msg_count[i] = count;
      }

      if (!one_stale)
        time_timeout_start = time_now;
    }

    if (timeout &&
        duration_cast<seconds>(time_now - time_timeout_start).count() >=
            timeout) {
      stop = true;
      if (timeout_nomsg)
        Warning() << "Watchdog: a receiver had no messages for " << timeout
                  << " seconds, stopping.";
      else {
        exit_code = -2;
        Warning() << "Stop triggered by timeout after " << timeout
                  << " seconds. (-T " << timeout << ")";
      }
    }
  }

  if (after_run)
    after_run(*this);

  detach();

  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];

    std::stringstream ss;
    if (r.verbose) {
      ss << "----------------------\n";
      for (int j = 0; j < r.Count(); j++) {
        std::string name = r.Model(j)->getName() + " #" + std::to_string(i) +
                           "-" + std::to_string(j);
        stat[i].statistics[j]->Stamp();
        ss << alignModelName(name)
           << "total: " << stat[i].statistics[j]->getCount() << " msgs" << "\n";
      }
    }

    if (r.Timing())
      for (int j = 0; j < r.Count(); j++)
        ss << alignModelName(r.Model(j)->getName())
           << r.Model(j)->getTotalTiming() << " ms" << "\n";
    if (!ss.str().empty())
      Info() << ss.str();
  }
}
