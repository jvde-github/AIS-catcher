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

#include <condition_variable>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "SiteConfig.h"
#include "Tracker.h"

// The events of the journal, posted to each webhook as they come: one event a
// request, or what came together when it batches, signed as Standard Webhooks
// has it when it has a secret. Each webhook keeps its own place in the journal
// and starts at the newest event; one that fails is tried again later, from the
// event that failed, waiting longer each time. Its own thread, so a slow
// address holds up only the webhooks.
class Webhooks
{
public:
	explicit Webhooks(Tracker &tracker) : tracker(tracker) {}
	~Webhooks() { stop(); }

	// the webhooks as configured now: one that stays keeps its place
	void configure(const std::vector<WebhookSpec> &specs);
	void stop();

private:
	struct Hook
	{
		WebhookSpec spec;
		std::string key; // the secret's bytes
		uint64_t cursor = 0;
		int failures = 0; // in a row, to wait longer after each
		std::time_t retry_at = 0;
	};

	Tracker &tracker;
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<Hook> hooks;
	bool running = false;
	std::thread worker;

	void loop();
	// false when the address did not take it
	bool deliver(Hook &hook, const std::vector<Journal::Entry> &events);
	bool wanted(const Hook &hook, const Journal::Entry &e);
};
