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

#include <memory>
#include <stdexcept>
#include <thread>

#include "AIS-catcher.h"

#include "Config.h"
#include "ControlServer.h"
#include "Helper.h"
#include "JSON.h"
#include "Logger.h"
#include "ManagedMain.h"
#include "JSON/Parser.h"

#if defined(HASWEBVIEWER) && defined(HASAPI)
#include "Server.h"
#include "Site.h"
#endif

namespace Managed {

bool isInvocation(const std::vector<std::string> &args) {
  for (const auto &a : args)
    if (a == "-E")
      return true;
  return false;
}

#if !defined(HASWEBVIEWER) || !defined(HASAPI)

int run(const std::vector<std::string> &) {
  throw std::runtime_error(
      "managed mode (-E) is not available in this build: it requires the web "
      "viewer and the API (build with WEBVIEWER=ON and API=ON)");
}

#else

// The places are the panel's editor's, one catalogue it edits and the tracker
// reads, wherever the configuration keeps them
static void usePanelPlaces(ControlCore &core, SiteConfig &site) {
  site.places.clear();
  site.catalogue = core.getPlaces();
}

// What a save says: the site with the place editor's catalogue, and the
// stations.
struct Saved {
  SiteConfig site;
  StationRegistry stations;
};

static Saved readSaved(ControlCore &core) {
  JSON::Parser parser(JSON_DICT_SETTING);
  auto doc = parser.parse(core.getConfig());
  Saved saved;
  StationDefaults defaults;
  // the listed stations first: a server's station_id describes one only where the file does not
  if (const auto *v = doc.root[AIS::KEY_SETTING_STATIONS])
    Config::stationsFromJSON(*v, saved.stations);
  Config::managedSite(doc.root, core.getControlPort(), core.getViewerPort(), core.getBindAddress(), saved.site, defaults, &saved.stations);
  usePanelPlaces(core, saved.site);
  return saved;
}

// Applies the site, mounts the viewer beside the control panel once, and says
// whether the viewer serves. A port that is taken leaves the panel up without
// a viewer, and the next apply tries again.
static void applySite(ControlCore &core, ControlServer &server, Site &site, const SiteConfig &cfg) {
  site.configure(cfg);
  Services::Server *viewer = site.server(core.getViewerPort());
  // one exposed port is enough for a station reached through a VPN or proxy
  if (viewer && !viewer->isMounted())
    server.mount("/viewer", viewer);
  if (!viewer)
    Error() << "Control: viewer not started on port " << core.getViewerPort() << "; the control panel stays available";
  core.setViewerServing(viewer != nullptr);
}

// the site as the configuration has it now, without an engine
static void applySaved(ControlCore &core, ControlServer &server, Site &site) {
  try {
    applySite(core, server, site, readSaved(core).site);
  } catch (const std::exception &e) {
    Error() << "Control: viewer configuration failed: " << e.what();
  }
}

// what the panel changed: a place edited, or the configuration saved, which
// the servers take at once; a running engine takes the stations as saved too
static void syncSite(ControlCore &core, ControlServer &server, Site &site, Engine *engine = nullptr) {
  auto places = core.getPlaces();
  if (places && places->consumeChanged())
    site.refreshPlaces();
  if (!core.consumeConfigChanged())
    return;
  try {
    const Saved saved = readSaved(core);
    applySite(core, server, site, saved.site);
    if (engine)
      engine->refreshStations(saved.stations);
  } catch (const std::exception &e) {
    Error() << "Control: configuration not applied: " << e.what();
  }
}

// construct before the Engine so the session covers ~Engine's device close
struct EngineSessionGuard {
  ControlCore &core;
  EngineSessionGuard(ControlCore &c) : core(c) { core.beginEngineSession(); }
  ~EngineSessionGuard() { core.endEngineSession(); }
};

// One engine session: read the config, start the receivers, run until stopped
// or failed. The site and its viewer serve throughout.
static void runEngine(ControlCore &core, ControlServer &server,
                      const std::string &config_file, Site &site) {
  EngineSessionGuard session(core);
  Engine engine;
  Config c(engine);
  c.setManaged(core.getControlPort(), core.getViewerPort(), core.getBindAddress());
  // stdout is the journal or the container log here: silent unless "screen"
  // in the config says otherwise, where the command line prints NMEA by default
  engine.screen.setScreen("0");

  try {
    engine.receivers.back()->getDeviceManager().refreshDevices();

    core.consumeConfigChanged();
    c.read(config_file);
    c.finish();
    for (auto &r : engine.receivers)
      if (r->getDeviceManager().InputType() == Type::NONE &&
          r->getDeviceManager().SerialNumber().empty())
        throw std::runtime_error(
            "no input device selected, configure one under Inputs");

    usePanelPlaces(core, engine.site_config);
    engine.loop_hook = [&] { syncSite(core, server, site, &engine); };

    engine.run(site, &core);
  } catch (std::exception const &e) {
    Error() << e.what();

    // the throw skipped run()'s own detach() and ~Engine only runs at the end
    // of this call
    engine.detach();
    core.engineFailed();
  }
  core.reportStopped();
  Info() << "Control: engine stopped";
}

// The back-off before an automatic restart. False when a command or the
// shutdown cut it short.
static bool waitForRetry(ControlCore &core, ControlServer &server, Site &site, int delay) {
  Warning() << "Control: engine stopped unexpectedly, restarting in " << delay
            << " seconds";
  const int seq = core.commandSequence();
  for (int i = 0;
       i < delay * 4 && !stop_process && core.commandSequence() == seq; i++) {
    // the engine is down for the whole wait, so the panel's changes apply here
    // as well as when idle
    syncSite(core, server, site);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  return !stop_process && core.engineDesired();
}

// What each station is doing now, for its owner's Stations tab: where it
// stands and by what, when its position last moved, when it was last heard,
// and the vessel it rides as the ship table has it. The panel asks for a
// session, so a station that keeps its position private shows it here too.
static std::string stationsLive(Site &site) {
  std::string out;
  JSON::Writer w(out);
  const std::time_t now = std::time(nullptr);
  Tracker *tracker = site.tracker();
  w.beginObject().key("stations").beginArray();
  site.withStations([&](const StationRegistry &registry) {
    registry.forEach([&](const StationRegistry::Station &s) {
      w.beginObject().kv("id", s.id);
      if (tracker) {
        const Tracker::Position at = tracker->stationPositionPrivate(s.id);
        const char *source = Tracker::sourceName(at.source);
        if (source)
          w.kv("source", source);
        if (isValidCoord(at.lat, at.lon))
          w.kv("lat", at.lat).kv("lon", at.lon);
        if (at.time)
          w.kv("fix_age_s", (long long)(now - at.time));
        const std::time_t heard = tracker->lastHeard(s.id);
        if (heard)
          w.kv("heard_age_s", (long long)(now - heard));
        if (s.mmsi)
          tracker->ships().withShip(s.mmsi, [&](const Ship &ship, int) {
            w.key("vessel").beginObject().kv("mmsi", (long long)s.mmsi).kv("name", std::string(ship.shipname))
                .kv("report_age_s", (long long)(now - ship.last_signal)).endObject();
          });
      }
      w.endObject();
    });
  });
  w.endArray();
  // when the backup was last written, for the History section's "last saved"
  if (const std::time_t saved = site.lastBackup())
    w.kv("backup_age_s", (long long)(now - saved));
  // the receivers as the engine sees them: whether each delivers, and how much
  const SystemFacts facts = site.systemFacts();
  w.kv("running", facts.running).key("receivers").beginArray();
  for (const auto &r : facts.receivers) {
    w.beginObject().kv("index", r.index).kv("station", r.station).kv("input", r.input).kv("label", r.label)
        .kv("state", r.status())
        .kv("messages", (unsigned long long)r.messages).kv("per_minute", (unsigned long long)r.per_minute);
    if (tracker) {
      const std::time_t heard = tracker->lastHeard(r.station);
      if (heard)
        w.kv("heard_age_s", (long long)(now - heard));
    }
    w.endObject();
  }
  w.endArray();
  w.endObject();
  w.finish();
  return out;
}

static int serve(const std::string &config_file, int port,
                 const std::string &bind) {
  Logger::getInstance().setMinLevel(LogLevel::DEBUG);

  ControlCore core(config_file, port, bind);
  // outlives every engine session, and serves the viewer between them
  Site site;
  ControlServer server(core);
  server.setStationsLive([&site] { return stationsLive(site); });
  server.setServerAddress([&site](int port, std::string &host) { return site.serverAddress(port, host); });
  server.setServerPorts([&site] { return site.serverPorts(); });

  if (core.authRequired())
    Info() << "Control: managed mode, config file \"" << config_file
           << "\", bound to " << core.getBindAddress() << ", password required";
  else
    Info() << "Control: managed mode, config file \"" << config_file
           << "\", local access only";

  server.start();
  site.start();
  Config::setConfigBaseDir(config_file);
  applySaved(core, server, site);

  while (!stop_process) {
    if (!core.engineDesired()) {
      // the panel's changes apply while the engine is stopped; the rest is
      // picked up by the engine start
      syncSite(core, server, site);
      core.waitForCommand();
      continue;
    }

    const int delay = core.consumeRetryDelay();
    if (delay > 0 && !waitForRetry(core, server, site, delay))
      continue;

    stop = false;
    if (core.engineDesired())
      runEngine(core, server, config_file, site);
  }

  server.close();
  site.stop();
  return 0;
}

int run(const std::vector<std::string> &args) {
  const std::string usage = "AIS-catcher -E [config file] [bind address:port]";

  if (args.size() < 2 || args.size() > 4 || args[1] != "-E")
    throw std::runtime_error(
        "in managed mode all settings live in the config file: " + usage);

  auto allDigits = [](const std::string &s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
  };

  std::string file = "config.json", bind = "127.0.0.1";
  int port = 0;

  std::string addr;
  if (args.size() == 3)
    addr = args[2];
  else if (args.size() == 4) {
    file = args[2];
    addr = args[3];
  }

  if (!addr.empty()) {
    std::string protocol, user, pass, host, portstr, path;
    Util::Parse::URL(addr, protocol, user, pass, host, portstr, path);

    if (allDigits(addr))
      port = Util::Parse::Integer(addr, 1, 65535);
    else if (!host.empty() && allDigits(portstr)) {
      bind = host;
      port = Util::Parse::Integer(portstr, 1, 65535);
    } else
      throw std::runtime_error(
          "Control: listen address must be [ip:]port, e.g. 127.0.0.1:8118: " +
          usage);
  }

  return serve(file, port, bind);
}

#endif

} // namespace Managed
