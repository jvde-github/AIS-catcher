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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <functional>
#include <mutex>

#include "Common.h"

// The engine's switch in managed mode: whether it should run, whether it does, and the wait before a restart.
class Supervisor
{
public:
	enum class State
	{
		Stopped,
		Running
	};

	// on: the switch as the configuration has it; keep: writes it there when it changes
	Supervisor(bool on, std::function<void(bool)> keep) : wanted(on), auto_retry(on), keep(std::move(keep)) {}

	void start()
	{
		command_seq++;
		{
			std::lock_guard<std::mutex> lock(mtx);
			resetRetry();
			if (wanted)
				return;
			wanted = true;
			restarting = false;
			epoch++;
		}
		keep(true);
		cv.notify_all();
	}

	void stop()
	{
		command_seq++;
		{
			std::lock_guard<std::mutex> lock(mtx);
			wanted = false;
			restarting = false;
			resetRetry();
			epoch++;
		}
		keep(false);
		StopRequest();
	}

	void restart()
	{
		command_seq++;
		bool running;
		{
			std::lock_guard<std::mutex> lock(mtx);
			wanted = true;
			running = state_ == State::Running;
			restarting = running;
			resetRetry();
			auto_retry = running;
			epoch++;
		}
		keep(true);

		if (running)
			StopRequest();
		else
			cv.notify_all();
	}

	State state()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return state_;
	}
	long long uptime()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return state_ == State::Running ? (long long)(std::time(nullptr) - started) : 0;
	}
	bool desired()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return wanted;
	}
	bool retrying()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return wanted && auto_retry && state_ != State::Running;
	}
	// another number whenever the status reads differently
	uint32_t stamp()
	{
		std::lock_guard<std::mutex> lock(mtx);
		const uint32_t bits = (state_ == State::Running ? 1 : 0) | (wanted ? 2 : 0) | ((wanted && auto_retry && state_ != State::Running) ? 4 : 0);
		return (epoch << 3) | bits;
	}

	// the managed loop's
	void reportRunning()
	{
		std::lock_guard<std::mutex> lock(mtx);
		state_ = State::Running;
		started = std::time(nullptr);
		auto_retry = false;
		epoch++;
	}
	// an engine that stops unasked is started again, after a wait that doubles
	void reportStopped()
	{
		std::lock_guard<std::mutex> lock(mtx);
		const bool was_running = state_ == State::Running;
		const bool healthy = was_running && std::time(nullptr) - started >= RETRY_HEALTHY_UPTIME;
		state_ = State::Stopped;
		epoch++;

		if (restarting)
			restarting = false;
		else if (wanted)
		{
			if (was_running || auto_retry)
			{
				if (healthy)
					retry_delay = 0;
				retry_delay = retry_delay ? MIN(retry_delay * 2, RETRY_DELAY_MAX) : RETRY_DELAY_FIRST;
				auto_retry = true;
				retry_pending = true;
			}
			else
			{
				wanted = false;
				resetRetry();
			}
		}
	}
	void failed()
	{
		std::lock_guard<std::mutex> lock(mtx);
		restarting = false;
	}
	void waitForCommand()
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::milliseconds(250), [this] { return wanted; });
	}
	int consumeRetryDelay()
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (!retry_pending)
			return 0;
		retry_pending = false;
		return retry_delay;
	}
	int commandSequence() { return command_seq; }

private:
	static const int RETRY_DELAY_FIRST = 5;
	static const int RETRY_DELAY_MAX = 60;
	static const int RETRY_HEALTHY_UPTIME = 60;

	std::mutex mtx;
	std::condition_variable cv;
	bool wanted = false;
	bool restarting = false;
	State state_ = State::Stopped;
	std::time_t started = 0;
	bool auto_retry = false;
	bool retry_pending = false;
	int retry_delay = 0;
	std::atomic<int> command_seq{0};
	uint32_t epoch = 0;
	std::function<void(bool)> keep;

	void resetRetry()
	{
		auto_retry = false;
		retry_pending = false;
		retry_delay = 0;
	}
};
