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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AIS-catcher.h"

#include "PluginStore.h"
#include "ReceiverTracker.h"

// Browser-facing settings, served as versioned JSON for startup and live
// updates. An embedded snapshot remains available for older clients and offline
// bootstrap.
class FrontendConfig {
  std::string build_version;
  std::string build_describe;
  std::string context = "settings";
  std::string station;
  std::string webcontrol_http;
  bool share_location = false;
  bool save_messages = false;
  bool replay = true;
  bool realtime = false;
  bool log_enabled = false;
  bool decoder = false;
  bool managed = false;
  bool places = false;
  bool geojson = false, kml = false;
  bool sharing = false;
  bool sharing_uuid = false;
  bool split = true;
  std::vector<std::pair<int, std::string>> receivers;

public:
  FrontendConfig();

  void setContext(const std::string &ctx) { context = ctx; }
  void setWebControl(const std::string &url) { webcontrol_http = url; }
  void setShareLoc(bool b) { share_location = b; }
  void setMsgSave(bool b) { save_messages = b; }
  void setReplay(bool b) { replay = b; }
  void setSplit(bool b) { split = b; }
  void setRealtime(bool b) { realtime = b; }
  void setLog(bool b) { log_enabled = b; }
  void setDecoder(bool b) { decoder = b; }
  void setGeoJSON(bool b) { geojson = b; }
  void setKML(bool b) { kml = b; }
  void setPlaces(bool b) { places = b; }
  void setManaged(bool b) { managed = b; }
  void setSharing(bool s, bool uuid) {
    sharing = s;
    sharing_uuid = uuid;
  }
  void setStation(const std::string &name);
  void
  setReceivers(const std::vector<std::unique_ptr<ReceiverTracker>> &states);

  // the config blob followed by the plugin function body
  std::vector<std::string> restart_required;
  std::vector<std::string>
      maps; // JSON map-source descriptions, separate from executable plugins
  std::string json(const PluginStore &plugins,
                   std::string *version = nullptr) const;
  std::string render(const PluginStore &plugins) const;
};
