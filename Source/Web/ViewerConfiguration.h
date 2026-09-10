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

#include "PlaceCatalogue.h"
#include "BackupManager.h"
#include "FrontendConfig.h"
#include "MapTiles.h"
#include <map>

// Configuration has three stages, shared by startup, live changes and
// attachment:
// 1. SetKey parses requested values and records resource paths; no file I/O.
// 2. prepare opens resources before runtime effects, reusing unchanged tile
// handles.
// 3. WebViewer::applySettings resolves effective values and applies
// differences. Only stage 3 touches listeners, workers, streams or tracking
// databases. Requested values must never be used as scratch for effective
// runtime settings.
class ViewerConfiguration : public Setting {
public:
  struct Values {
    uint64_t groups_in = 0xFFFFFFFFFFFFFFFF;
    std::vector<std::string> zones;

    // 0 with port_set asks the OS for an ephemeral port
    int port = 0;
    bool port_set = false;

    bool use_zlib = true;
    bool realtime = false;
    bool showlog = false;
    bool showdecoder = false;
    bool KML = false;
    bool GeoJSON = false;
    bool supportPrometheus = false;
    bool replay = true;
    bool split = true;

    std::string station, station_link;

    TrackingConfig tracking;
    std::string backup_file, frame_ancestors = "*";
    int backup_interval = -1;
    std::string
        bind_address;    // empty leaves the embedding host's listener alone
    std::string places;   // directory of area definitions, opened when applied
    int reuse_port = -1; // -1 leaves the embedding host's choice alone
  };

  Values values;
  AIS::Filter filter;
  FrontendConfig frontend;
  PluginStore plugins;
  std::vector<std::shared_ptr<MapTiles>> mapSources;
  bool managed = false;

  ViewerConfiguration() : Setting("WebViewer") {}
  Setting &SetKey(AIS::Keys key, const std::string &arg) override;
  std::string Get() override { return ""; }
  void setManaged(bool enabled) {
    managed = enabled;
    frontend.setManaged(enabled);
  }
  // Tile sources opened by the previous configuration are reused.
  // when the same setting text comes back
  void inherit(const ViewerConfiguration &previous) {
    previousTiles = previous.tiles;
  }
  void prepare();

private:
  std::vector<std::pair<AIS::Keys, std::string>> resources;
  bool prepared = false;
  std::map<std::string, std::vector<std::shared_ptr<MapTiles>>> tiles,
      previousTiles;
  void addTileSources(const std::string &paths, bool overlay, bool mbtiles);
};
