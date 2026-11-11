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

#include <chrono>
#include <sstream>
#include <thread>

#include "AIS-catcher.h"

#include "Engine.h"
#include "Host.h"
#include "Network.h"
#include "Site.h"
#include "Stations.h"

#include <algorithm>
#include "Logger.h"

using namespace std::chrono;

static std::string alignModelName(const std::string &name)
{
	return "[" + name + "] " +
		   std::string(name.length() < 37 ? 37 - name.length() : 0, ' ');
}

Engine::Engine() { receivers.push_back(std::unique_ptr<Receiver>(new Receiver())); }

Engine::~Engine() { detach(); }

void Engine::detach()
{
	if (detached)
		return;
	detached = true;

	for (auto &r : receivers)
		r->stop();

	// the site stops the outputs and forgets the receivers
	if (site)
		site->detachEngine();
}

std::function<void(Site &)> Host::before_run;
std::function<void(Site &)> Host::after_run;

#ifdef HASAPI
void Engine::publishReceivers()
{
	std::vector<ReceiverState> states;
	for (std::size_t k = 0; k < receivers.size(); k++)
	{
		Receiver &r = *receivers[k];
		Device::Device *device = r.device();
		ReceiverState s;
		s.index = (int)k + 1;
		s.station = r.getStation();
		if (device)
		{
			s.input = device->getProduct();
			s.streaming = device->isStreaming() && device->isConnected();
			s.lost = device->isLost();
		}
		s.label = receiverLabel(receivers, k);
		s.messages = r.messages();
		s.bytes = r.bytesIn().received();
		if (const IO::OutputStats *link = device ? device->getConnectionStats() : nullptr)
		{
			s.connection = true;
			s.connect_ok = link->connect_ok.get();
			s.connect_fail = link->connect_fail.get();
			s.reconnects = link->reconnects.get();
			s.connected = link->connected.get();
		}
		states.push_back(s);
	}
	site->publishReceivers(states);
}
#endif

void Engine::open(Receiver &r)
{
	r.setOwnMMSI(own_mmsi);
	r.setTags(tags);
	r.setupDevice();
	// a plain run with one receiver has one thread delivering and nothing rewired under it
	r.setupModel(managed || receivers.size() > 1 ? &site->delivery() : nullptr);
	*r.device() >> site->received();
	*r.device() >> r.bytesIn();
}

// in their order, as groups_in and the status pages count them
void Engine::number()
{
	int group = 0;
	oneverbose = false;
	iscallback = true;
	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];
		r.number(i, group);
		oneverbose |= r.verbose;
		iscallback &= r.device()->isCallback();
	}
}

void Engine::play(Receiver &r)
{
	r.play();
	if (managed && !r.verbose)
		r.logSummary();
}

bool Engine::follow(Engine &next, const StationRegistry &configured, const StationDefaults &defaults)
{
	if (next.told != told)
		return false;
	Stations::resolve(configured, defaults, next.receivers, *site, false);

	const auto same = [](const std::unique_ptr<Receiver> &a, const std::unique_ptr<Receiver> &b) { return a->told == b->told && a->zones == b->zones; };
	if (receivers.size() == next.receivers.size() && std::equal(receivers.begin(), receivers.end(), next.receivers.begin(), same))
		return true;

	// a running receiver takes the place of the one told the same, with the zones that one has
	for (auto &n : next.receivers)
	{
		auto it = std::find_if(receivers.begin(), receivers.end(), [&](const std::unique_ptr<Receiver> &r) { return r && r->told == n->told; });
		if (it == receivers.end())
			continue;
		(*it)->zones = n->zones;
		n = std::move(*it);
	}
	// the others stop and close before a changed one opens, as it may be the same device
	std::size_t gone = 0;
	for (auto &r : receivers)
		if (r)
		{
			r->stop();
			gone++;
		}
	receivers.clear();
	receivers.swap(next.receivers);

	std::vector<Receiver *> fresh;
	for (auto &r : receivers)
		if (!r->device())
		{
			open(*r);
			fresh.push_back(r.get());
		}
	{
		std::lock_guard<std::mutex> held(site->delivery());
		number();
	}
	site->rewire(facts());
	for (Receiver *r : fresh)
		play(*r);
	if (gone || !fresh.empty())
		Info() << "Receivers: " << fresh.size() << " started and " << gone << " stopped, as the configuration now has them";
	return true;
}

// sharing is opt-in, via -X or the config's "sharing"
void Engine::tellSharing()
{
	if (managed)
		return;
	if (!outputs.sharing_defined && !outputs.feed && (tracked || !outputs.list.empty()))
		Warning() << "Hint: support the aiscatcher.org community by sharing your "
					 "feed with '-X on'. Sharing is currently OFF.";
	Info() << "Community feed sharing: " << (outputs.feed ? "enabled." : "disabled.");
}

uint64_t Engine::radios()
{
	uint64_t groups = 0;
	for (const auto &r : receivers)
		if (DeviceManager::isRadio(r->inputType()))
			groups |= r->getGroupMask();
	return groups;
}

uint64_t Engine::route(const std::vector<std::string> &zones, uint64_t groups_in, const std::string &what)
{
	return routeGroups(receivers, zones, groups_in, what);
}

void Engine::wire(IO::OutputMessage &o)
{
	for (auto &r : receivers)
	{
		o.Connect(*r);
		r->linkJSON();
	}
}

void Engine::unwire()
{
	for (auto &r : receivers)
	{
		r->unwire();
		screen.Connect(*r);
		r->linkJSON();
		if (activity)
			for (int j = 0; j < r->modelCount(); j++)
				r->Output(j).Connect(activity);
	}
}

EngineFacts Engine::facts()
{
	EngineFacts facts;
	for (std::size_t k = 0; k < receivers.size(); k++)
	{
		auto &r = receivers[k];
		Device::Device *device = r->device();
		if (!device)
			continue;
		DeviceFacts d;
		d.product = device->getProduct();
		d.vendor = device->getVendor().empty() ? "-" : device->getVendor();
		d.serial = device->getSerial().empty() ? "-" : device->getSerial();
		d.sample_rate = device->getRateDescription();
		d.label = receiverLabel(receivers, k);
		for (int j = 0; j < r->modelCount(); j++)
			d.model += (j ? "\n" : "") + r->model(j).getName();
		facts.push_back(d);
	}
	return facts;
}

void Engine::feed(Tracker &t, const TrackerSpec *own)
{
#ifdef HASAPI
	const uint64_t mask = own ? routeGroups(receivers, own->zones, own->groups_in, "A server with ships of its own") : GROUPS_ALL;
	for (auto &r : receivers)
	{
		for (int j = 0; j < r->modelCount(); j++)
		{
			if (!r->Output(j).canConnect(mask))
				continue;
			r->OutputJSON(j).Connect(&t.json());
			r->OutputGPS(j).Connect(&t.gps());
			r->OutputControl(j).Connect(&t.control());
			r->OutputADSB(j).Connect(&t.adsb());
		}
		r->linkJSON();
	}
#endif
}

void Engine::start()
{
	// the servers serve and the consumers are wired, the receivers have not
	// started: a build that adds services attaches them now
	if (Host::before_run)
		Host::before_run(*site);

	Debug() << "Starting receivers";
	for (auto &r : receivers)
		play(*r);

	if (on_started)
		on_started();
}

void Engine::printRates()
{
	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];
		if (r.verbose)
		{
			for (int j = 0; j < r.modelCount(); j++)
			{
				IO::StreamCounter &c = r.counter(j);
				c.Stamp();
				Info() << alignModelName(r.modelLabel(j))
					   << "received: " << c.getDeltaCount()
					   << " msgs, total: " << c.getCount()
					   << " msgs, rate: " << c.getRate()
					   << " msg/s";
			}
		}
	}
}

// -T: the timeout period counts from the start or, with timeout_nomsg, from
// the last look at which every receiver had delivered; once it has passed the
// run stops, with a word why: exit code -2 for the plain timeout, a failure's for a silent receiver
bool Engine::watchdogExpired(const time_point &now, time_point &since, std::vector<uint64_t> &counts, int &code)
{
	if (timeout_nomsg)
	{
		bool one_stale = false;
		counts.resize(receivers.size());

		for (int i = 0; i < (int)receivers.size(); i++)
		{
			const uint64_t count = receivers[i]->messages();

			if (count == counts[i])
				one_stale = true;

			counts[i] = count;
		}

		if (!one_stale)
			since = now;
	}

	if (duration_cast<seconds>(now - since).count() < timeout)
		return false;

	if (timeout_nomsg)
	{
		code = -1;
		Warning() << "Watchdog: a receiver had no messages for " << timeout
				  << " seconds, stopping.";
	}
	else
	{
		code = -2;
		Warning() << "Stop triggered by timeout after " << timeout
				  << " seconds. (-T " << timeout << ")";
	}
	return true;
}

// the main loop, until stopped: a device that stops ends it, as does -T
int Engine::loop()
{
	const int SLEEP = 50;
	// steady throughout: a step of the wall clock neither fires nor delays the watchdog
	auto time_start = steady_clock::now();
	auto time_timeout_start = time_start;
	auto time_last = time_start;
	auto next_poll = time_start;
	std::vector<uint64_t> msg_count(receivers.size(), 0);
	int code = 0;

	Debug() << "Entering main loop";
#ifdef HASAPI
	auto next_status = steady_clock::now();
#endif
	while (!stop)
	{
		for (auto &r : receivers)
			if (!r->device()->isActive())
			{
				// a file that ended stops the run; a device that went is a failure
				if (r->device()->isLost())
					stop_failed = true;
				stop = true;
			}

#ifdef HASAPI
		// the site's system pages, about once a second, and once more when a
		// receiver stops the session, so its loss is told
		if (stop || steady_clock::now() >= next_status)
		{
			next_status = steady_clock::now() + seconds(1);
			publishReceivers();
		}
#endif

		// for non-callback devices (WAV file), the isStreaming() poll above pumps
		// the data, so the loop must run unthrottled
		if (iscallback)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

		if (loop_hook)
			loop_hook();
		// outputs make and keep their connections here, off the decoding thread;
		// paced, as the loop runs unthrottled for a file input
		auto poll_now = steady_clock::now();
		if (poll_now >= next_poll)
		{
			next_poll = poll_now + milliseconds(SLEEP);
			site->pollOutputs();
		}

		if (!oneverbose && !timeout)
			continue;

		auto time_now = steady_clock::now();

		if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >=
							  screen.verboseUpdateTime)
		{
			time_last = time_now;
			printRates();
		}

		if (timeout && watchdogExpired(time_now, time_timeout_start, msg_count, code))
			stop = true;
	}
	return code;
}

void Engine::reportTotals()
{
	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];

		std::stringstream ss;
		if (r.verbose)
		{
			ss << "----------------------\n";
			for (int j = 0; j < r.modelCount(); j++)
			{
				r.counter(j).Stamp();
				ss << alignModelName(r.modelLabel(j))
				   << "total: " << r.counter(j).getCount() << " msgs" << "\n";
			}
		}

		if (r.timing())
			for (int j = 0; j < r.modelCount(); j++)
				ss << alignModelName(r.model(j).getName())
				   << r.model(j).getTotalTiming() << " ms" << "\n";
		if (!ss.str().empty())
			Info() << ss.str();
	}
}

void Engine::open(Site &on, const StationRegistry &configured, const StationDefaults &defaults, bool keeps_ships)
{
	site = &on;
	// a tracker of an earlier session stays, and is fed; the trackers read the decoders' tags
	tracked = keeps_ships || on.tracker() != nullptr;
	tags = std::string(tracked ? "DTM" : "") + (outputs.http ? "DT" : "");
	Stations::resolve(configured, defaults, receivers, *site);
	for (auto &r : receivers)
		open(*r);
	number();
	if (managed || receivers.size() > 1)
		Debug() << "Mutex: one delivery lock for " << receivers.size() << (receivers.size() == 1 ? " receiver" : " receivers");
	else
		Debug() << "Mutex: single receiver, all sinks lock-free";
}

int Engine::run()
{
	tellSharing();
	site->attachEngine(facts(), *this, std::move(outputs));
	start();

	const int code = loop();

	if (Host::after_run)
		Host::after_run(*site);

	detach();
	reportTotals();
	return code ? code : stop_failed ? -1 : 0;
}
