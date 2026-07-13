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
#include <functional>
#include <ctime>

#include "Stream.h"
#include "Message.h"

namespace JSON { class Value; }

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

class ControlCore
{
public:
	enum class EngineState
	{
		Stopped,
		Running
	};

	ControlCore(const std::string &config_file, int port_override = 0, const std::string &bind = "127.0.0.1");

	void startEngine();
	void stopEngine();
	void restartEngine();

	EngineState getEngineState();
	long long getUptime();

	std::string getConfig();
	bool setConfig(const std::string &json, std::string &error);
	std::string getDeviceListJSON();
	std::string getSerialListJSON();

	int getViewerPort() const { return control_port < 65535 ? control_port + 1 : 0; }

	void setConfigChangedCallback(std::function<void()> f) { config_changed = f; }

	ChannelActivity &getChannelActivity() { return channel_activity; }

	int getControlPort() const { return control_port; }
	const std::string &getConfigFile() const { return config_file; }
	const std::string &getBindAddress() const { return bind_address; }

	bool authRequired() const { return bind_address != "127.0.0.1" && bind_address != "localhost"; }

	bool hasPassword();
	bool verifyPassword(const std::string &password);
	void setPassword(const std::string &password);

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

private:
	std::string config_file;
	int control_port = 8118;
	std::string bind_address = "127.0.0.1";
	ChannelActivity channel_activity;
	std::string password_hash;
	std::string password_salt;

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

	void resetRetry()
	{
		auto_retry = false;
		retry_pending = false;
		retry_delay = 0;
	}
	std::atomic<int> command_seq{0};
	uint32_t status_epoch = 0;

	std::mutex file_mtx;
	std::function<void()> config_changed;

	void createDefaultConfig();
	void readManagedFields(int port_override);
	void applyAuthFields(const JSON::Value &control);
	void refreshAuthFields(const std::string &json);
	bool validate(const std::string &json, std::string &error);
	bool writeFileAtomic(const std::string &path, const std::string &content, std::string &error);
	void persistEngineField(bool on);
	void persistControlAuth(const std::string &hash, const std::string &salt);
};
