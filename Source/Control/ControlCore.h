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
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "Message.h"
#include "Stream.h"

#include "PlaceCatalogue.h"
#include "JSON.h"

class DeviceManager;

class ChannelActivity : public StreamIn<AIS::Message>
{
public:
	std::atomic<uint32_t> count[4]{};

	void Receive(const AIS::Message *msg, int len, TAG &tag) override
	{
		for (int i = 0; i < len; i++)
		{
			int idx = msg[i].getChannel() - 'A';
			if (idx >= 0 && idx < 4)
				count[idx]++;
		}
	}
};

// The configuration file as the panel edits it, and what the panel offers beside it.
class ControlCore
{
public:
	ControlCore(const std::string &config_file, int port_override = 0,
				const std::string &bind = "127.0.0.1");
	~ControlCore();

	// the engine's switch as the file has it, and written there
	bool engineSwitch() const { return engine_switch; }
	void keepEngineSwitch(bool on);

	std::shared_ptr<PlaceCatalogue> getPlaces();
	std::string placeSettings();
	bool enablePlaces(std::string &error);
	std::string getConfig();
	std::string getPanelConfig();
	bool setConfig(const std::string &json, std::string &error);
	bool readLegacyConfig(std::string &content);
	std::string getDeviceListJSON();
	std::string getSerialListJSON();

	int getViewerPort() const
	{
		return control_port < 65535 ? control_port + 1 : 0;
	}
	// the port the status reports: 0 while the viewer could not bind it
	int reportedViewerPort() const
	{
		return viewer_serving ? getViewerPort() : 0;
	}
	void setViewerServing(bool on) { viewer_serving = on; }

	// True once per config write from the control API. The managed loop polls it
	// so live settings are applied by the owner, including while the receiver
	// runs.
	bool consumeConfigChanged() { return config_dirty.exchange(false); }

	ChannelActivity &getChannelActivity() { return channel_activity; }

	int getControlPort() const { return control_port; }
	// where the tracking backup goes when "tracking" names no file
	std::string defaultStatsFile() const;
	const std::string &getConfigFile() const { return config_file; }
	const std::string &getBindAddress() const { return bind_address; }
	std::string configFolderPath() const;
	// whether the panel's own server has "tls", and its files: the command line's, else the file's
	bool panelTLS(std::string &cert, std::string &key);
	// as they are written, for the panel to show; true when the command line sets them
	bool panelTLSFields(std::string &cert, std::string &key);
	// for this run only, and not written to the file
	void setPanelTLSFromCommandLine(const std::string &cert, const std::string &key);
	// from the panel: a pair that loads, or two empty ones for plain HTTP; written to the file
	bool setPanelTLS(const std::string &cert, const std::string &key, std::string &error);

	bool authRequired() const
	{
		return bind_address != "127.0.0.1" && bind_address != "localhost";
	}

	bool hasPassword();
	bool verifyPassword(const std::string &password);
	bool checkSetupCode(std::string code);
	void setPassword(const std::string &password);
	bool wizardPending();

	static std::string randomHex(size_t length);

	// bracket the managed loop's engine scope: no device rescans while active
	void beginEngineSession()
	{
		std::lock_guard<std::mutex> lock(scan_mtx);
		engine_busy = true;
	}
	void endEngineSession()
	{
		std::lock_guard<std::mutex> lock(scan_mtx);
		engine_busy = false;
	}

private:
	std::string config_file;
	int control_port = 8118;
	std::string bind_address = "127.0.0.1";
	ChannelActivity channel_activity;
	std::string password_hash;
	bool panel_tls = false;
	std::string panel_tls_cert, panel_tls_key;
	std::string cli_tls_cert, cli_tls_key;
	std::string password_salt;
	int password_iterations = 0;
	static const int PASSWORD_ITERATIONS = 10000;
	std::string setup_code;
	bool wizard_flag = false;

	std::mutex mtx;
	bool engine_switch = false;

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
	void refreshPlaces(const JSON::JSON &root);
	std::string defaultPlaceDirectory() const;
	std::atomic<bool> config_dirty{false};
	std::atomic<bool> viewer_serving{false};

	void createDefaultConfig();
	void addMainDefaults();
	void announceSetupCode();
	JSON::Document load();
	void applyAuthFields(const JSON::Value &control);
	void adopt(const JSON::JSON &root);
	bool validate(const std::string &json, std::string &error);
	bool writeFileAtomic(const std::string &path, const std::string &content,
						 std::string &error);
	// Read-modify-write of the config file under file_mtx. The callback returns
	// false to leave the file untouched; a failure is logged as "cannot <what>",
	// said in `error` too when given, and returns false.
	bool mutateConfig(const char *what,
					  const std::function<bool(JSON::Document &)> &fn,
					  std::string *error = nullptr);
	bool allowedPaths(const JSON::JSON &saved, std::string &error);
	static void removeAuth(JSON::JSON &control);
	void putAuth(JSON::Document &doc);
};
