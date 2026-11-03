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
#include <chrono>
#include <memory>
#include <vector>

#include <functional>

#include "Receiver.h"
#include "StationRegistry.h"
#include "SiteConfig.h"
#include "Screen.h"
#include "SystemFacts.h"

class Site;
namespace IO
{
	class HubStreamer;
}

// One engine run: the receivers, their outputs and the servers that belong to them,
// built by the configuration reader and started by run(). Managed mode constructs a new
// one for every start/stop cycle.
struct Engine
{
	// Called by run() once the servers serve and before the receivers start:
	// a build that adds services attaches them here.
	static std::function<void(Engine &, Site &)> before_run;
	// Called by run() when the main loop has ended, while the receivers are
	// still alive: a build that added services saves and stops them here.
	static std::function<void(Engine &, Site &)> after_run;

	// Receivers and outputs
	std::vector<std::unique_ptr<Receiver>> receivers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;

	// Community feed output, owned by msg; set up at most once
	IO::HubStreamer *comm_feed = nullptr;

	// The receiving stations: those the configuration defines, and the default
	// station, 0, when a receiver names none and the file does not list it
	StationRegistry stations;
	// what the configuration says of the default station, for when it does not
	// list it: -N and a "server" entry feed it in every mode
	StationDefaults station_defaults;
	// managed mode's save: the stations as the file now has them, while the engine runs
	void refreshStations(const StationRegistry &configured);

	// what the configuration says of the site, which outlives the engine
	SiteConfig site_config;
	// called on each pass of the run loop: managed mode applies settings changes here
	std::function<void()> loop_hook;

	// Managed mode (-E) sets these before run(): the panel's rules apply, the
	// decoded messages feed its channel activity, and on_started tells it the
	// engine runs
	bool managed = false;
	StreamIn<AIS::Message> *activity = nullptr;
	std::function<void()> on_started;

	IO::ScreenOutput screen;

	int own_mmsi = -1;
	int timeout = 0;
	bool timeout_nomsg = false;
	bool xshare_defined = false;

	// The first receiver is there from the start, for the configuration
	// reader to fill: refreshDevices() runs on its device manager before any
	// configuration is read, and SoapySDR keeps its enumeration per instance,
	// so only receiver 0's SOAPYSDR can open an enumerated handle.
	Engine();

	// The site outlives the engine, so detaching must happen even when run()
	// threw halfway or was never called at all.
	~Engine();

	// returns 0, or -2 when -T stopped the run
	int run(Site &site);

	// Stop the receivers and drop the site's references into this object.
	// Idempotent.
	void detach();

	DeviceManager &scanner() { return receivers.front()->devices(); }

	void createCommunityFeed();

	// what the site's status pages say of this session
	EngineFacts facts();

private:
	typedef std::chrono::high_resolution_clock::time_point time_point;

	bool detached = false;
	// set when run() starts
	Site *site = nullptr;
	// whether a receiver asks for the rates, and whether every device delivers
	// by callback: a file input pumps its data from the main loop instead
	bool oneverbose = false, iscallback = true;

	// the phases of run(), in its order
	void resolveStations();
	void openReceivers();
	void configureSite();
	void shareFeed();
	void routeOutputs();
	void connectSinks();
	void feedTrackers();
	void start();
	int loop();
	void printRates();
	bool watchdogExpired(const time_point &now, time_point &since, std::vector<uint64_t> &counts, int &code);
	void reportTotals();

	bool hasServer() const;
#ifdef HASAPI
	void publishReceivers();
#endif
};
