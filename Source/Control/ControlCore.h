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
#include <condition_variable>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "Message.h"
#include "Stream.h"

#include "PlaceCatalogue.h"
#include "JSON.h"

class WebViewer;

class DeviceManager;

class ChannelActivity : public StreamIn<AIS::Message> {
public:
  std::atomic<uint32_t> count[4]{};

  void Receive(const AIS::Message *msg, int len, TAG &tag) override {
    for (int i = 0; i < len; i++) {
      int idx = msg[i].getChannel() - 'A';
      if (idx >= 0 && idx < 4)
        count[idx]++;
    }
  }
};

class ControlCore {
public:
  enum class EngineState { Stopped, Running };

  ControlCore(const std::string &config_file, int port_override = 0,
              const std::string &bind = "127.0.0.1");
  ~ControlCore();

  void startEngine();
  void stopEngine();
  void restartEngine();

  EngineState getEngineState();
  long long getUptime();

  std::shared_ptr<PlaceCatalogue> getPlaces();
  std::string placeSettings();
  bool enablePlaces(std::string &error);
  std::string getConfig();
  bool setConfig(const std::string &json, std::string &error);
  bool readLegacyConfig(std::string &content);
  std::string getDeviceListJSON();
  std::string getSerialListJSON();

#ifdef HASWEBVIEWER
  // A new places directory or a saved viewer section reaches the viewer through
  // whoever runs the loop, engine or hub, so settings apply while serving
  void syncViewer(WebViewer &viewer);
#endif
  int getViewerPort() const {
    return control_port < 65535 ? control_port + 1 : 0;
  }
  // the port the status reports: 0 while the viewer could not bind it
  int reportedViewerPort() const {
    return viewer_serving ? getViewerPort() : 0;
  }
  void setViewerServing(bool on) { viewer_serving = on; }

  // True once per config write from the control API. The managed loop polls it
  // so live settings are applied by the owner, including while the receiver
  // runs.
  bool consumeConfigChanged() { return config_dirty.exchange(false); }

  ChannelActivity &getChannelActivity() { return channel_activity; }

  int getControlPort() const { return control_port; }
  const std::string &getConfigFile() const { return config_file; }
  const std::string &getBindAddress() const { return bind_address; }

  bool authRequired() const {
    return bind_address != "127.0.0.1" && bind_address != "localhost";
  }

  bool hasPassword();
  bool verifyPassword(const std::string &password);
  void setPassword(const std::string &password);
  bool wizardPending();

  static std::string randomHex(size_t length);

  bool engineDesired();
  bool engineRetrying();
  uint32_t statusStamp();
  void reportRunning();
  void reportStopped();
  void engineFailed();
  void waitForCommand();

  int consumeRetryDelay();
  int commandSequence() { return command_seq; }

  // bracket the managed loop's engine scope: no device rescans while active
  void beginEngineSession() {
    std::lock_guard<std::mutex> lock(scan_mtx);
    engine_busy = true;
  }
  void endEngineSession() {
    std::lock_guard<std::mutex> lock(scan_mtx);
    engine_busy = false;
  }

private:
  std::string config_file;
  int control_port = 8118;
  std::string bind_address = "127.0.0.1";
  ChannelActivity channel_activity;
  std::string password_hash;
  std::string password_salt;
  bool wizard_flag = false;

  std::mutex mtx;
  std::condition_variable cv;
  bool desired = false;
  bool restart_pending = false;
  EngineState state = EngineState::Stopped;
  std::time_t engine_start_time = 0;

  static const int RETRY_DELAY_FIRST = 5;
  static const int RETRY_DELAY_MAX = 60;
  static const int RETRY_HEALTHY_UPTIME = 60;
  bool auto_retry = false;
  bool retry_pending = false;
  int retry_delay = 0;

  void resetRetry() {
    auto_retry = false;
    retry_pending = false;
    retry_delay = 0;
  }
  std::atomic<int> command_seq{0};
  uint32_t status_epoch = 0;

  // kept alive rather than built per request: constructing one instantiates
  // every device type, and some of those open their vendor API
  std::unique_ptr<DeviceManager> scanner;
  std::mutex scan_mtx;
  bool engine_busy = false; // guarded by scan_mtx

  std::mutex file_mtx;
  std::shared_ptr<PlaceCatalogue> place_store;
  std::mutex place_mtx;
  std::string place_directory;
  bool place_enabled = false;
  void refreshPlaces(const std::string &json);
  std::string defaultPlaceDirectory() const;
  std::atomic<bool> config_dirty{false};
  std::atomic<bool> viewer_serving{false};

  void createDefaultConfig();
  void addViewerDefaults();
  void readManagedFields(int port_override);
  void applyAuthFields(const JSON::Value &control);
  void refreshAuthFields(const std::string &json);
  bool validate(const std::string &json, std::string &error);
  bool writeFileAtomic(const std::string &path, const std::string &content,
                       std::string &error);
  // Read-modify-write of the config file under file_mtx. The callback returns
  // false to leave the file untouched; any failure is logged as "cannot
  // <what>".
  void mutateConfig(const char *what,
                    const std::function<bool(JSON::Document &)> &fn);
  void persistEngineField(bool on);
  void persistControlAuth(const std::string &hash, const std::string &salt);
};
