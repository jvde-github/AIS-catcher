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

#include "AIS-catcher.h"

#include "Engine.h"
#include "ControlCore.h"
#include "Logger.h"

using namespace std::chrono;

static std::string alignModelName(const std::string &name)
{
	return "[" + name + "] " + std::string(name.length() < 37 ? 37 - name.length() : 0, ' ');
}

void Engine::detach()
{
	if (detached)
		return;
	detached = true;

	for (auto &r : receivers)
		r->stop();

#ifdef HASWEBVIEWER
	if (attached_viewer)
		attached_viewer->detachEngine();

	for (auto &s : servers)
		s->shutdown();
#endif
}

void Engine::run(WebViewer *viewer, ControlCore *control)
{
	attached_viewer = viewer;

	// -------------
	// set up the receiver and open the device

	stat.resize(receivers.size());
	msg_count.resize(receivers.size(), 0);

	bool has_server = false;
#ifdef HASWEBVIEWER
	// every viewer this run feeds, owned or not. The managed viewer differs only
	// in that it outlives the engine, so the caller starts and closes it.
	std::vector<WebViewer *> viewers;
	for (auto &s : servers)
		if (s->isActive())
			viewers.push_back(s.get());
	if (viewer)
		viewers.push_back(viewer);

	has_server = !viewers.empty();
#endif
	bool has_http = false;
	for (auto &o : msg)
		if (dynamic_cast<IO::HTTPStreamer *>(o.get())) { has_http = true; break; }

	int group = 0;

	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];
		r.SetKey(AIS::KEY_SETTING_OWN_MMSI, std::to_string(own_mmsi));

		if (has_server) r.setTags("DTM");
		if (has_http)   r.setTags("DT");

		r.setupDevice();

		// set up the decoding model(s), group is the last output group used
		r.setupModel(group, i);
	}

	// sharing is on by default as soon as there is anything to share with
	if (!xshare_defined && !comm_feed && (has_server || !msg.empty()))
	{
		if (!control)
			Warning() << "Hint: Use '-X on' to share with aiscatcher.org community (enables community overlay) or '-X off' to disable. Currently ON by default.";

		createCommunityFeed();
	}

	if (comm_feed && (control ? !comm_feed->hasUUID() : !xshare_defined))
	{
		uint64_t live_groups = 0;
		for (const auto &r : receivers)
		{
			Type t = r->getDeviceManager().InputType();
			if (t == Type::RTLSDR || t == Type::AIRSPY || t == Type::AIRSPYHF ||
				t == Type::HACKRF || t == Type::SDRPLAY || t == Type::HYDRASDR ||
				t == Type::SOAPYSDR)
				live_groups |= r->getGroupMask();
		}

		comm_feed->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(live_groups));
	}

	if (!control)
		Info() << "Community feed sharing: " << (comm_feed ? "enabled." : "disabled.");

	// zone-based output filtering: GROUPS_IN from zone overlap. The viewers do
	// this for themselves in attachEngine().
	for (auto &o : msg)
	{
		if (o->zones.empty()) continue;
		uint64_t mask = resolveZones(receivers, o->zones);
		if (!mask)
			Warning() << "Output has zone filter but no matching receivers — will receive nothing";
		o->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(mask));
	}

	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];

		// set up all the output and connect to the receiver outputs
		for (auto &o : msg)
			o->Connect(r);

		if (!control)
			screen.Connect(r);

		if (r.verbose || timeout_nomsg)
			stat[i].connect(r);
	}

	if (receivers.size() > 1)
	{
		Debug() << "Mutex: enabling exclusive on " << msg.size() << " message outputs + screen (" << receivers.size() << " receivers)";
		for (auto &o : msg)
			o->setExclusive(true);
		if (!control)
			screen.setExclusive(true);
	}
	else
	{
		Debug() << "Mutex: single receiver, all sinks lock-free";
	}

#ifdef HASWEBVIEWER
	// before attachEngine(), so the trackers are built with these already applied
	for (auto *v : viewers)
	{
		v->setOutputChannels(msg);
		v->setCommFeed(comm_feed);

		if (own_mmsi != -1)
		{
			v->SetKey(AIS::KEY_SETTING_SHARE_LOC, "true");
			v->SetKey(AIS::KEY_SETTING_OWN_MMSI, std::to_string(own_mmsi));
		}
	}

	for (auto *v : viewers)
		v->attachEngine(receivers);
#endif

	if (control)
		for (auto &r : receivers)
			for (int j = 0; j < r->Count(); j++)
				r->Output(j).Connect((StreamIn<AIS::Message> *)&control->getChannelActivity());

	for (auto &o : msg)
		o->Start();

#ifdef HASWEBVIEWER
	// the engine owns only the servers it built; the managed viewer is already
	// serving and is started and closed by the caller
	for (auto &s : servers)
		if (s->isActive())
			s->startServing();
#endif

	Debug() << "Starting receivers";
	for (auto &r : receivers)
		r->play();

	if (control)
	{
		control->reportRunning();
		Info() << "Control: engine started";

		for (auto &r : receivers)
			if (!r->verbose)
				r->logSummary();
	}

	const int SLEEP = 50;
#ifdef HASWEBVIEWER
	const int TICK_INTERVAL = 60;
	int tick_countdown = 0;
#endif
	auto time_start = high_resolution_clock::now();
	auto time_timeout_start = time_start;
	auto time_last = time_start;
	bool oneverbose = false, iscallback = true;

	for (auto &r : receivers)
	{
		oneverbose |= r->verbose;
		iscallback &= r->getDeviceManager().getDevice()->isCallback();
	}

	Debug() << "Entering main loop";
	while (!stop)
	{
		for (auto &r : receivers)
			stop = stop || !(r->getDeviceManager().getDevice()->isStreaming());

		// for non-callback devices (WAV file), the isStreaming() poll above pumps
		// the data, so the loop must run unthrottled
		if (iscallback)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

#ifdef HASWEBVIEWER
		// above the verbose/timeout shortcut below or it never runs by default
		if (--tick_countdown <= 0)
		{
			tick_countdown = TICK_INTERVAL * 1000 / SLEEP;

			std::time_t tick_now = std::time(nullptr);
			for (auto v : viewers)
				v->tick(tick_now);
		}
#endif

		if (!oneverbose && !timeout)
			continue;

		auto time_now = high_resolution_clock::now();

		if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >= screen.verboseUpdateTime)
		{
			time_last = time_now;

			for (int i = 0; i < (int)receivers.size(); i++)
			{
				Receiver &r = *receivers[i];
				if (r.verbose)
				{
					for (int j = 0; j < r.Count(); j++)
					{
						stat[i].statistics[j]->Stamp();
						std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
						Info() << alignModelName(name) << "received: " << stat[i].statistics[j]->getDeltaCount() << " msgs, total: "
							   << stat[i].statistics[j]->getCount() << " msgs, rate: " << stat[i].statistics[j]->getRate() << " msg/s";
					}
				}
			}
		}

		// watchdog: stop when any receiver went without messages for the timeout period
		if (timeout && timeout_nomsg)
		{
			bool one_stale = false;

			for (int i = 0; i < (int)receivers.size(); i++)
			{
				uint64_t count = 0;
				for (auto &s : stat[i].statistics)
					count += s->getCount();

				if (count == msg_count[i])
					one_stale = true;

				msg_count[i] = count;
			}

			if (!one_stale)
				time_timeout_start = time_now;
		}

		if (timeout && duration_cast<seconds>(time_now - time_timeout_start).count() >= timeout)
		{
			stop = true;
			if (timeout_nomsg)
				Warning() << "Watchdog: a receiver had no messages for " << timeout << " seconds, stopping.";
			else
			{
				exit_code = -2;
				Warning() << "Stop triggered by timeout after " << timeout << " seconds. (-T " << timeout << ")";
			}
		}
	}

	detach();

	for (int i = 0; i < (int)receivers.size(); i++)
	{
		Receiver &r = *receivers[i];

		std::stringstream ss;
		if (r.verbose)
		{
			ss << "----------------------\n";
			for (int j = 0; j < r.Count(); j++)
			{
				std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
				stat[i].statistics[j]->Stamp();
				ss << alignModelName(name) << "total: " << stat[i].statistics[j]->getCount() << " msgs" << "\n";
			}
		}

		if (r.Timing())
			for (int j = 0; j < r.Count(); j++)
				ss << alignModelName(r.Model(j)->getName()) << r.Model(j)->getTotalTiming() << " ms" << "\n";
		if (!ss.str().empty())
			Info() << ss.str();
	}
}
