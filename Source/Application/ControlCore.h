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

#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <ctime>

#include "Stream.h"
#include "Message.h"

// Counts decoded messages per AIS channel; the control server streams the
// changes as a light SSE feed driving the hub's activity LEDs
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

// Engine control for managed mode (-E): the config file is the single source
// of truth and the engine (RunState) can be started, stopped and restarted
// in-process. Command methods are thread-safe so any binding (HTTP control
// server, JNI, native UI) can drive the engine; the supervisor loop in
// Main.cpp consumes the requested state.
class ControlCore
{
public:
	enum class EngineState
	{
		Stopped,
		Running
	};

	ControlCore(const std::string &config_file, int port_override = 0, const std::string &bind = "127.0.0.1");

	// command interface, thread-safe
	void startEngine();
	void stopEngine();
	void restartEngine();

	EngineState getEngineState();
	long long getUptime();

	std::string getConfig();
	bool setConfig(const std::string &json, std::string &error);
	std::string getDeviceListJSON();
	std::string getSerialListJSON();

	// the actually bound port of the persistent viewer, 0 if none
	void setViewerPort(int p) { viewer_port = p; }
	int getViewerPort() const { return viewer_port; }

	ChannelActivity &getChannelActivity() { return channel_activity; }

	int getControlPort() const { return control_port; }
	const std::string &getConfigFile() const { return config_file; }
	const std::string &getBindAddress() const { return bind_address; }

	// local mode: bound to loopback only, no password needed; any other bind
	// address exposes the control server and makes authentication mandatory
	bool authRequired() const { return bind_address != "127.0.0.1" && bind_address != "localhost"; }

	bool hasPassword();
	bool verifyPassword(const std::string &password);
	void setPassword(const std::string &password);

	// supervisor interface (managed-mode loop in Main.cpp)
	bool engineDesired();
	void reportRunning();
	void reportStopped();
	void engineFailed();
	void waitForCommand();

private:
	std::string config_file;
	int control_port = 8110;
	std::string bind_address = "127.0.0.1";
	std::atomic<int> viewer_port{0};
	ChannelActivity channel_activity;
	std::string password_hash;
	std::string password_salt;

	std::mutex mtx;
	std::condition_variable cv;
	bool desired = false;
	bool restart_pending = false;
	EngineState state = EngineState::Stopped;
	std::time_t engine_start_time = 0;

	std::mutex file_mtx;

	void createDefaultConfig();
	void readManagedFields(int port_override);
	void refreshAuthFields(const std::string &json);
	bool validate(const std::string &json, std::string &error);
	bool writeFileAtomic(const std::string &path, const std::string &content, std::string &error);
	void persistEngineField(bool on);
	void persistControlAuth(const std::string &hash, const std::string &salt);
};
