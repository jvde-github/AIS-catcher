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

#include "Config.h"
#include "Logger.h"
#include "WebViewer.h"
#include <algorithm>

namespace {
bool sameStorage(const TrackingConfig &a, const TrackingConfig &b) {
  return a.track_memory == b.track_memory && a.max_ships == b.max_ships &&
         a.server_mode == b.server_mode;
}

// Effects are grouped by the resource they change, independently of where the
// configuration came from. Add a new runtime effect here and apply it below.
struct SettingsChanges {
  bool streams, backup, log, headers;
  SettingsChanges(const WebViewer::Settings &next,
                  const WebViewer::Settings *old)
      :
        streams(!old || next.realtime != old->realtime ||
                next.supportPrometheus != old->supportPrometheus),
        backup(!old || next.backup_file != old->backup_file ||
               next.backup_interval != old->backup_interval),
        log(!old || next.showlog != old->showlog),
        headers(!old || next.frame_ancestors != old->frame_ancestors) {}
};
} // namespace

Setting &WebViewer::SetKey(AIS::Keys key, const std::string &arg) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  configuration.SetKey(key, arg);
  return *this;
}

void WebViewer::resetSettings(int port) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  const bool managed = configuration.managed;
  configuration = ViewerConfiguration();
  configuration.setManaged(managed);
  setPort(port);
  // Keep effective settings and services until the prepared configuration is
  // applied. The caller stops input before a reset/reconfigure sequence.
}

void WebViewer::applySettings(ApplyMode mode) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  configuration.prepare();
  std::vector<std::unique_lock<std::mutex>> inputs;
  for (auto &s : states)
    inputs.emplace_back(s->database().configuration_mtx);

  const bool firstStart = !initialized;
  const Settings previous = settings;
  const Settings *old = initialized ? &previous : nullptr;
  settings = configuration.values;
  if (own_vessel > 0) {
    settings.tracking.own_mmsi = own_vessel;
    settings.tracking.latlon_share = true;
  }
  auto &frontend = configuration.frontend;
  frontend.restart_required.clear();

  // Resolve requested -> effective values in one place. Never overwrite the
  // request: repeated applications must keep reporting a deferred change.
  if (initialized && !sameStorage(settings.tracking, allocatedTracking)) {
    frontend.restart_required.push_back("Tracking storage (service restart)");
    settings.tracking.track_memory = allocatedTracking.track_memory;
    settings.tracking.max_ships = allocatedTracking.max_ships;
    settings.tracking.server_mode = allocatedTracking.server_mode;
  }
  if (old && mode == ApplyMode::Live) {
    if (!old->zones.empty() && settings.zones == old->zones)
      settings.groups_in = old->groups_in;
    if (settings.groups_in != old->groups_in || settings.zones != old->zones ||
        settings.split != old->split) {
      frontend.restart_required.push_back(
          "Receiver selection (receiver restart)");
      settings.groups_in = old->groups_in;
      settings.zones = old->zones;
      settings.split = old->split;
    }
  }
  if (bound_port && old &&
      (settings.port != old->port ||
       settings.bind_address != old->bind_address ||
       settings.reuse_port != old->reuse_port)) {
    frontend.restart_required.push_back("Listener (service restart)");
    settings.port = old->port;
    settings.port_set = old->port_set;
    settings.bind_address = old->bind_address;
    settings.reuse_port = old->reuse_port;
  }

  const SettingsChanges changes(settings, old);
  if (!configuration.managed && settings.places != localPlacesPath) {
    std::atomic_store(&placeStore,
                      settings.places.empty()
                          ? std::shared_ptr<PlaceCatalogue>()
                          : std::make_shared<PlaceCatalogue>(settings.places));
    localPlacesPath = settings.places;
  }
  const auto places = getPlaceSnapshot();
  sse_streamer.setObfuscate(!settings.showdecoder);
  raw_counter.setFilter(configuration.filter);
  if (changes.headers)
    setFrameAncestors(settings.frame_ancestors);
  if (!bound_port) {
    if (!settings.bind_address.empty())
      setIP(settings.bind_address);
    if (settings.reuse_port != -1)
      setReusePort(settings.reuse_port != 0);
  }

  for (auto &s : states) {
    const bool fresh = !s->isInitialized();
    s->database().configure(
        [&] { s->applyConfig(settings.tracking, configuration.filter); });
    if (fresh)
      s->setup();
    s->database().setPlaces(
        places); // pointer comparison; stationary ships refresh only on change
    if (fresh || changes.streams) {
      s->clearSinks();
      s->wireStreams();
    }
  }
  if (changes.streams) {
    if (settings.realtime) {
      sse_streamer.setSSE(this);
      states[0]->connectSink(sse_streamer);
    }
    if (settings.supportPrometheus)
      states[0]->connectSink(dataPrometheus);
  }

  // Start brings the services up; while serving they follow their settings
  const bool activate = mode == ApplyMode::Start || serving;
  if (changes.log || (activate && !serving)) {
    logger.Stop();
    if (activate && settings.showlog) {
      logger.setSSE(this);
      logger.Start();
    }
  }
  if (changes.backup || (activate && !serving)) {
    backup.stop();
    backup.setFilename(settings.backup_file);
    backup.setInterval(settings.backup_interval);
    backup.setTracker(states[0].get());
    // A restore belongs to initial startup, never to a filename change or
    // receiver restart. The DB already has the current configuration/catalogue.
    // A file that fails part-way leaves half the counters restored: clear them.
    if (firstStart && !settings.backup_file.empty() && !backup.load()) {
      Error() << "Statistics - cannot read file.";
      states[0]->clear();
    }
    if (activate)
      backup.start();
  }
  if (firstStart) {
    allocatedTracking = settings.tracking;
    initialized = true;
  }

  // Publish effective features, not requested values waiting for a restart.
  frontend.setStation(settings.station);
  frontend.setShareLoc(settings.tracking.latlon_share);
  frontend.setMsgSave(settings.tracking.msg_save);
  frontend.setReplay(settings.replay);
  frontend.setSplit(settings.split);
  frontend.setRealtime(settings.realtime);
  frontend.setLog(settings.showlog);
  frontend.setDecoder(settings.showdecoder);
  frontend.setKML(settings.KML);
  frontend.setGeoJSON(settings.GeoJSON);
  frontend.setPlaces(places != nullptr);
  frontend.setSharing(comm_feed != nullptr, comm_feed && comm_feed->hasUUID());
  frontend.setReceivers(states);
  publishConfig();
}

void WebViewer::publishConfig() {
  configJSON =
      configuration.frontend.json(configuration.plugins, &configVersion);
}

void WebViewer::setPlaceCatalogue(std::shared_ptr<PlaceCatalogue> store) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  std::atomic_store(&placeStore, std::move(store));
  if (!initialized)
    return;
  const auto places = getPlaceSnapshot();
  for (auto &s : states)
    s->database().setPlaces(places);
  configuration.frontend.setPlaces(places != nullptr);
  publishConfig();
}

void WebViewer::prepareManagedConfig(const std::string &configJSON) {
  JSON::Parser parser(JSON_DICT_SETTING);
  auto doc = parser.parse(configJSON);
  const auto *control = doc.root[AIS::KEY_SETTING_CONTROL];
  const auto *viewer = control && control->isObject()
                           ? control->getObject()[AIS::KEY_SETTING_VIEWER]
                           : nullptr;

  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  ViewerConfiguration next;
  next.setManaged(true);
  next.inherit(configuration);
  if (viewer && viewer->isObject())
    Config::setSettingsFromJSON(*viewer, next);
  // the hub owns the listener; the file cannot move it
  next.values.port = configuration.values.port;
  next.values.port_set = true;
  configuration = std::move(next);
}

void WebViewer::updateManagedConfig(const std::string &configJSON) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  prepareManagedConfig(configJSON);
  applySettings(ApplyMode::Live);
}
