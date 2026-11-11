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

#include "MsgOut.h"
#include "Receiver.h"
#include "StationRegistry.h"
#include "SiteConfig.h"
#include "Screen.h"
#include "SystemFacts.h"

class Site;
class Tracker;

// One engine run: the receivers, built by the configuration reader, opened and run on a site
// that outlives it and keeps every consumer of their messages. Managed mode constructs a new
// one for every start/stop cycle.
struct Engine : EngineWiring
{
	std::vector<std::unique_ptr<Receiver>> receivers;
	// as the reader made them; the site's from the moment the engine runs
	Outputs outputs;
	// what the reader told the engine itself, beside its receivers
	std::string told;

	// Managed mode's save while running: receivers told the same carry on, the others open, the unlisted stop.
	// False when the engine itself is told differently; throws when a receiver does not open.
	bool follow(Engine &next, const StationRegistry &configured, const StationDefaults &defaults);

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

	// The first receiver is there from the start, for the configuration reader to fill.
	Engine();

	// The site outlives the engine, so detaching must happen even when run()
	// threw halfway or was never called at all.
	~Engine();

	// open: the stations resolved and told to the site, the devices opened
	void open(Site &site, const StationRegistry &configured, const StationDefaults &defaults, bool tracked);
	// run: until stopped; returns 0, -2 when -T stopped the run, -1 when a failure did
	int run();

	// Stop the receivers and drop the site's references into this object.
	// Idempotent.
	void detach();

	DeviceManager &scanner() { return receivers.front()->devices(); }

	// what the site's status pages say of this session
	EngineFacts facts();

	void unwire() override;
	void feed(Tracker &t, const TrackerSpec *own) override;
	uint64_t route(const std::vector<std::string> &zones, uint64_t groups_in, const std::string &what) override;
	void wire(IO::OutputMessage &o) override;
	uint64_t radios() override;

private:
	typedef std::chrono::steady_clock::time_point time_point;

	bool detached = false;
	// set by open()
	Site *site = nullptr;
	bool tracked = false;
	// what every receiver's decoders tag their messages with
	std::string tags;
	// whether a receiver asks for the rates, and whether every device delivers
	// by callback: a file input pumps its data from the main loop instead
	bool oneverbose = false, iscallback = true;

	// a receiver's device and models; all of them numbered; one started
	void open(Receiver &r);
	void number();
	void play(Receiver &r);
	// the phases of run(), in their order
	void tellSharing();
	void start();
	int loop();
	void printRates();
	bool watchdogExpired(const time_point &now, time_point &since, std::vector<uint64_t> &counts, int &code);
	void reportTotals();

#ifdef HASAPI
	void publishReceivers();
#endif
};
