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
#include <memory>
#include <vector>

#include <functional>

#include "Receiver.h"
#include "StationRegistry.h"
#include "SiteConfig.h"
#include "Network.h"
#include "Screen.h"

class ControlCore;
class Site;
class WebViewer;

// stop ends the running engine, stop_process the managed-mode loop around it
extern std::atomic<bool> stop;
extern std::atomic<bool> stop_process;

// One engine run: the receivers, their outputs and the servers that belong to them,
// built up by the command line parser or the config reader and then started by run().
// Managed mode constructs a new one for every start/stop cycle.
struct Engine {
	// Called by run() once the servers serve and before the receivers start:
	// a build that adds services attaches them here.
	static std::function<void(Engine &)> before_run;
	// Called by run() when the main loop has ended, while the receivers are
	// still alive: a build that added services saves and stops them here.
	static std::function<void(Engine &)> after_run;

	// Receivers and outputs
	std::vector<std::unique_ptr<Receiver>> receivers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;

#ifdef HASWEBVIEWER
	// the web viewers of -N and "server": each keeps its own ship table, fed
	// like an output by the receivers in its zones
	std::vector<std::unique_ptr<WebViewer>> viewers;
#endif

	// Community feed output, owned by msg; set up at most once
	IO::HubStreamer *comm_feed = nullptr;

	// The receiving stations: those the configuration defines, and the default
	// station, 0, when a receiver names none and the file does not list it
	StationRegistry stations;
	// what managed mode's configuration says of the default station, for when
	// it does not list it
	StationDefaults station_defaults;
	// managed mode's save: the stations as the file now has them, while the engine runs
	void refreshStations(const StationRegistry &configured);

	// what the configuration says of the site, which outlives the engine
	SiteConfig site_config;
	// called on each pass of the run loop: managed mode applies settings changes here
	std::function<void()> loop_hook;

	// Screen and statistics
	IO::ScreenOutput screen;
	std::vector<OutputStatistics> stat;
	std::vector<uint64_t> msg_count;

	// Configuration flags set during arg parsing / config reading
	int own_mmsi = -1;
	int timeout = 0;
	bool timeout_nomsg = false;
	bool xshare_defined = false;
	bool verbose = false;

	// One-shot flags
	bool no_run = false;
	bool show_copyright = true;
	bool list_devices = false;
	bool list_devices_JSON = false;
	bool list_support = false;
	bool list_options = false;

	// Run-time state
	int exit_code = 0;
	int nrec = 0;

	// out of line, as the web viewers are only declared here
	Engine();

	// The site outlives the engine, so detaching must happen even when run()
	// threw halfway or was never called at all.
	~Engine();

	// Open the devices, wire up the outputs and the site, and run until stopped.
	void run(Site &site, ControlCore *control = nullptr);

	// Stop the receivers and drop the site's references into this object.
	// Idempotent.
	void detach();

	// Each device option on the command line / in config defines a new receiver,
	// except the first which reuses the default-constructed one.
	Receiver &newReceiver() {
		if (++nrec > 1)
			receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
		return *receivers.back();
	}

	IO::OutputMessage &createCommunityFeed() {
		comm_feed = new IO::HubStreamer();
		msg.push_back(std::unique_ptr<IO::OutputMessage>(comm_feed));
		return *comm_feed;
	}

private:
	bool detached = false;
	// the site of the run, from its start
	Site *site = nullptr;

	void resolveStations();
	void countStationInputs();
	// the default station the configuration does not list; it rides a vessel
	// only when it is the only station
	StationRegistry::Station defaultStation(bool alone) const;
#ifdef HASAPI
	void publishReceivers();
#endif
};
