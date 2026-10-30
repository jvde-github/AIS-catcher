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
#include <thread>

#include "AIS-catcher.h"

#include "Engine.h"
#include "Network.h"
#include "Site.h"
#include "Stations.h"

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

void Engine::createCommunityFeed() {
  comm_feed = new IO::HubStreamer();
  msg.push_back(std::unique_ptr<IO::OutputMessage>(comm_feed));
}

void Engine::refreshStations(const StationRegistry &configured) {
  Stations::refresh(stations, configured, receivers, site);
}

#ifdef HASAPI
// what the site's system service says of each receiver: its station, its
// input, whether it streams, and the messages its models decoded
void Engine::publishReceivers() {
  std::vector<ReceiverState> states;
  for (std::size_t k = 0; k < receivers.size(); k++) {
    Receiver &r = *receivers[k];
    Device::Device *device = r.device();
    ReceiverState s;
    s.index = (int)k + 1;
    s.station = r.getStation();
    if (device) {
      s.input = device->getProduct();
      s.streaming = device->isStreaming();
      s.lost = device->isLost();
    }
    s.label = receiverLabel(receivers, k);
    s.messages = r.messages();
    states.push_back(s);
  }
  site->publishReceivers(states);
}
#endif

void Engine::resolveStations() {
  Stations::resolve(stations, station_defaults, receivers, site);
}

bool Engine::hasServer() const {
#ifdef HASAPI
  return site->tracker() || site_config.wantsTracker() || !site_config.servers.empty();
#else
  return false;
#endif
}

// the receivers' tags, their devices and their models; group is the last output group used
void Engine::openReceivers() {
  // the trackers read the decoders' tags
  const bool has_server = hasServer();
  bool has_http = false;
  for (auto &o : msg)
    if (dynamic_cast<IO::HTTPStreamer *>(o.get())) {
      has_http = true;
      break;
    }

  int group = 0;

  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];
    r.setOwnMMSI(own_mmsi);

    if (has_server)
      r.setTags("DTM");
    if (has_http)
      r.setTags("DT");

    r.setupDevice();

    // set up the decoding model(s)
    r.setupModel(group, i);
  }

  for (auto &r : receivers) {
    oneverbose |= r->verbose;
    iscallback &= r->device()->isCallback();
  }
}

// the site's tracker and servers as the configuration asks for them
void Engine::configureSite() {
  site->configure(site_config);
#ifdef HASAPI
  // a server that cannot listen stops the start, as the status pages of before did;
  // managed mode keeps its panel up and tries again on the next apply
  if (!managed) {
    const std::vector<int> ports = site->serverPorts();
    for (const auto &s : site_config.servers)
      if (std::find(ports.begin(), ports.end(), s.port) == ports.end())
        throw std::runtime_error("cannot open port " + std::to_string(s.port) + " on " + s.bind + " for the server.");
  }
#endif
}

// the community feed: a word to those not sharing, and only the radios' groups for the feed
void Engine::shareFeed() {
  // sharing is opt-in via -X or the config's "sharing": the silent get an encouraging word, not a feed
  if (!xshare_defined && !comm_feed && (hasServer() || !msg.empty()) && !managed)
    Warning() << "Hint: support the aiscatcher.org community by sharing your "
                 "feed with '-X on'. Sharing is currently OFF.";

  if (comm_feed && (managed ? !comm_feed->hasUUID() : !xshare_defined)) {
    uint64_t live_groups = 0;
    for (const auto &r : receivers)
      if (DeviceManager::isRadio(r->inputType()))
        live_groups |= r->getGroupMask();

    comm_feed->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(live_groups));
  }

  if (!managed)
    Info() << "Community feed sharing: "
           << (comm_feed ? "enabled." : "disabled.");
}

// each output takes the receivers its zones route to it
void Engine::routeOutputs() {
  const bool any_zoned = anyZoned(receivers);
  std::map<std::string, int> nth;
  for (auto &o : msg) {
    const std::string label = o->getType() + " #" + std::to_string(++nth[o->getType()]);
    if (o->zones.empty() && !any_zoned)
      continue;
    o->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(routeGroups(receivers, o->zones, o->StreamIn<AIS::Message>::getGroupsIn(),
                                                                     o->getDescription().empty() ? label : o->getDescription())));
  }
}

// the outputs, the screen, the counters and the panel's activity behind every
// model; the site counts the bytes every device delivers
void Engine::connectSinks() {
  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];

    // set up all the output and connect to the receiver outputs
    for (auto &o : msg)
      o->Connect(r);

    screen.Connect(r);

    // the site's system service counts each receiver's messages too
    r.connectCounters();

    if (activity)
      for (int j = 0; j < r.modelCount(); j++)
        r.Output(j).Connect(activity);
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

  for (auto &r : receivers)
    *r->device() >> site->received();
}

EngineFacts Engine::facts() {
  EngineFacts facts;
  for (std::size_t k = 0; k < receivers.size(); k++) {
    auto &r = receivers[k];
    Device::Device *device = r->device();
    if (!device)
      continue;
    DeviceFacts d;
    d.product = device->getProduct();
    d.vendor = device->getVendor().empty() ? "-" : device->getVendor();
    d.serial = device->getSerial().empty() ? "-" : device->getSerial();
    d.sample_rate = device->getRateDescription();
    d.label = receiverLabel(receivers, k);
    for (int j = 0; j < r->modelCount(); j++)
      d.model += (j ? "\n" : "") + r->model(j).getName();
    facts.devices.push_back(d);
  }
  for (auto &o : msg)
    facts.outputs.push_back(o.get());
  facts.sharing = comm_feed != nullptr;
  facts.sharing_uuid = comm_feed != nullptr && comm_feed->hasUUID();
  return facts;
}

// the site's tracker takes every receiver; a server's own takes those in its zones and groups
void Engine::feedTrackers() {
#ifdef HASAPI
  const auto feed = [&](Tracker *t, uint64_t mask) {
    for (auto &r : receivers)
      for (int j = 0; j < r->modelCount(); j++) {
        if (!r->Output(j).canConnect(mask))
          continue;
        r->OutputJSON(j).Connect(&t->json());
        r->OutputGPS(j).Connect(&t->gps());
        r->OutputControl(j).Connect(&t->control());
        r->OutputADSB(j).Connect(&t->adsb());
      }
  };
  if (Tracker *tracker = site->tracker())
    feed(tracker, GROUPS_ALL);
  for (const auto &own : site->ownTrackers())
    feed(own.tracker, routeGroups(receivers, own.spec.zones, own.spec.groups_in, "A server with ships of its own"));
#endif
}

// the outputs start, the services attach, the receivers play
void Engine::start() {
  for (auto &o : msg)
    o->Start();

  // the servers serve and the tracker is wired, the receivers have not
  // started: a build that adds services attaches them now
  if (before_run)
    before_run(*this);

  Debug() << "Starting receivers";
  for (auto &r : receivers)
    r->play();

  if (on_started)
    on_started();
  if (managed)
    for (auto &r : receivers)
      if (!r->verbose)
        r->logSummary();
}

// the rates of every receiver that asks for them
void Engine::printRates() {
  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];
    if (r.verbose) {
      for (int j = 0; j < r.modelCount(); j++) {
        IO::StreamCounter &c = r.counter(j);
        c.Stamp();
        Info() << alignModelName(r.modelLabel(j))
               << "received: " << c.getDeltaCount()
               << " msgs, total: " << c.getCount()
               << " msgs, rate: " << c.getRate()
               << " msg/s";
      }
    }
  }
}

// -T: the timeout period counts from the start or, with timeout_nomsg, from
// the last look at which every receiver had delivered; once it has passed the
// run stops, with a word why and exit code -2 for the plain timeout
bool Engine::watchdogExpired(const time_point &now, time_point &since, std::vector<uint64_t> &counts, int &code) {
  if (timeout_nomsg) {
    bool one_stale = false;

    for (int i = 0; i < (int)receivers.size(); i++) {
      const uint64_t count = receivers[i]->messages();

      if (count == counts[i])
        one_stale = true;

      counts[i] = count;
    }

    if (!one_stale)
      since = now;
  }

  if (duration_cast<seconds>(now - since).count() < timeout)
    return false;

  if (timeout_nomsg)
    Warning() << "Watchdog: a receiver had no messages for " << timeout
              << " seconds, stopping.";
  else {
    code = -2;
    Warning() << "Stop triggered by timeout after " << timeout
              << " seconds. (-T " << timeout << ")";
  }
  return true;
}

// the main loop, until stopped: a device that stops ends it, as does -T
int Engine::loop() {
  const int SLEEP = 50;
  auto time_start = high_resolution_clock::now();
  auto time_timeout_start = time_start;
  auto time_last = time_start;
  // steady: high_resolution_clock is the wall clock on libstdc++
  auto next_poll = steady_clock::now();
  std::vector<uint64_t> msg_count(receivers.size(), 0);
  int code = 0;

  Debug() << "Entering main loop";
#ifdef HASAPI
  auto next_status = steady_clock::now();
#endif
  while (!stop) {
    for (auto &r : receivers)
      stop = stop || !(r->device()->isActive());

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
      printRates();
    }

    if (timeout && watchdogExpired(time_now, time_timeout_start, msg_count, code))
      stop = true;
  }
  return code;
}

// the totals of every receiver that asked for the rates, and the timing of every model that was timed
void Engine::reportTotals() {
  for (int i = 0; i < (int)receivers.size(); i++) {
    Receiver &r = *receivers[i];

    std::stringstream ss;
    if (r.verbose) {
      ss << "----------------------\n";
      for (int j = 0; j < r.modelCount(); j++) {
        r.counter(j).Stamp();
        ss << alignModelName(r.modelLabel(j))
           << "total: " << r.counter(j).getCount() << " msgs" << "\n";
      }
    }

    if (r.timing())
      for (int j = 0; j < r.modelCount(); j++)
        ss << alignModelName(r.model(j).getName())
           << r.model(j).getTotalTiming() << " ms" << "\n";
    if (!ss.str().empty())
      Info() << ss.str();
  }
}

int Engine::run(Site &on) {
  site = &on;
  resolveStations();
  openReceivers();
  configureSite();
  shareFeed();
  routeOutputs();
  connectSinks();
  site->attachEngine(facts());
  feedTrackers();
  start();

  const int code = loop();

  if (after_run)
    after_run(*this);

  detach();
  reportTotals();
  return code;
}
