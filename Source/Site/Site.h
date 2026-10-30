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

#include <condition_variable>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <map>
#include <thread>
#include <vector>

#include "MsgOut.h"
#include "SiteConfig.h"
#include "StationRegistry.h"
#include "Statistics.h"
#include "SystemFacts.h"
#ifdef HASAPI
#include "Server.h"
#include "Tracker.h"
#ifdef HASDELIVERY
#include "MqttState.h"
#include "Webhooks.h"
#endif
#else
class Tracker;
#endif

// What lives as long as the process: the stations, and, once a configuration
// asks for them, the tracker, the servers and the deliveries. An engine
// session attaches to it when it starts and leaves it when it stops; in
// managed mode it outlives them all.
class Site
{
public:
	~Site() { stop(); }

	// the stations of the last session to start, kept while none runs
	void publishStations(const StationRegistry &resolved);
	template <typename F>
	void withStations(F f) const
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		f(stations);
	}

	// What the running engine is made of, told by it at its start and
	// withdrawn at its stop, before its outputs go: what a status page says
	// of the devices, the outputs and the community feed.
	void attachEngine(const EngineFacts &facts);
	void detachEngine();
	// when the tracker's backup was last written, or read when the tracker
	// was made; 0 without a backup file
	std::time_t lastBackup()
	{
		std::lock_guard<std::mutex> lock(site_mtx);
		return backing_up ? last_backup : 0;
	}
	// the bytes every input delivers, connected to by each session's devices
	ByteCounter &received() { return bytes; }

	// The tracker and the servers the configuration asks for. A tracker, once
	// made, lives until the process ends; servers follow every change. A build
	// without the API has neither.
	void configure(const SiteConfig &cfg);
#ifdef HASAPI
	// the receivers as the running engine sees them, told about once a second;
	// a receiver that loses its device or gets it back is an event
	void publishReceivers(const std::vector<ReceiverState> &states);
	// what the system service reads: the process, the engine, its receivers
	SystemFacts systemFacts();
	Tracker *tracker() { return tracker_.get(); }
	// the trackers of servers' own, for the engine to feed through their zones and groups
	struct OwnTracker
	{
		Tracker *tracker;
		TrackerSpec spec;
	};
	std::vector<OwnTracker> ownTrackers();
	// the server on a port, to mount it elsewhere; null when none listens there
	Services::Server *server(int port);
	bool serverAddress(int port, std::string &host);
	std::vector<int> serverPorts();
	// the servers with HTTPS: the port, and when its certificate expires
	std::vector<std::pair<int, long long>> serverTLS();
#else
	Tracker *tracker() { return nullptr; }
#endif
	// a shared catalogue was edited: its new edition reaches the ships
	void refreshPlaces();

	// stops the heartbeat and the servers, and saves the tracker
	void stop();

private:
	mutable std::mutex stations_mtx;
	StationRegistry stations;

	StationRegistry stationsSnapshot() const
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		return stations;
	}
	// The stations as anything that serves them sees them: a station that
	// does not share where it is comes without a position, an altitude or the
	// vessel it rides, so no service can let one out.
	StationRegistry publicStations() const;

	std::mutex engine_mtx;
	EngineFacts engine;
	bool engine_running = false;
	std::time_t session_start = 0;
	// the engine as told, or none; whether one was running before
	bool resetEngine(const EngineFacts &facts, bool running);
	// the engine's part of a status page, as key-value pairs in an open object
	void writeEngine(JSON::Writer &w, EnginePart part);
	ByteCounter bytes;
	const std::time_t born = std::time(nullptr); // the process's run time counts from here

	std::mutex site_mtx;
	// how often the tracker is backed up; 0 for never
	int backup_minutes = 10;
#ifdef HASAPI
	std::vector<ReceiverState> receiver_states;
	// per receiver, its count at moments of the last minute, oldest first
	std::vector<std::deque<std::pair<std::time_t, uint64_t>>> receiver_counts;
	// an event of the installation itself, into the journal, of a station and
	// a receiver or of none (-1); not under site_mtx
	void systemEvent(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	void systemEventLocked(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	bool backup_failing = false;
	// the stations of this installation's receivers: which are quiet, and the
	// day each was last told to hear far; under site_mtx
	std::set<int> quiet_stations;
	std::map<int, std::time_t> far_told;
	void watchStations(std::time_t now, bool daily);
	// its parts, in its order: the quiet stations, told as they fall quiet and
	// hear again; every station's state for the map; a day of exceptional range
	std::set<int> tellQuiet(std::time_t now, const SystemFacts &f, const std::set<int> &mine, const StationRegistry &registry);
	void setStationStates(std::time_t now, const SystemFacts &f, const std::set<int> &mine, const std::set<int> &quiet, const StationRegistry &registry);
	void tellPropagation(std::time_t now, const std::set<int> &mine, const StationRegistry &registry);

	// the journal moving on wakes the heartbeat, for the MQTT state's events;
	// made before the tracker, so it outlives the journal that pings it
	struct JournalPing : public StreamIn<uint64_t>
	{
		Site &site;
		explicit JournalPing(Site &site) : site(site) {}
		void Receive(const uint64_t *, int, TAG &) override
		{
			if (site.events_wanted)
				site.poke();
		}
	} journal_ping{*this};
	std::atomic<bool> events_wanted{false};
	std::unique_ptr<Tracker> tracker_;
	std::vector<std::unique_ptr<Services::Server>> servers;
	// A server's own tracker, behind its filter; the site's tracker alone has a
	// backup. Made with its server; changed settings wait for the next engine, as
	// the running one feeds it, and one the configuration dropped stays until
	// that engine detaches.
	struct Own
	{
		int port = 0;
		TrackerSpec spec;
		std::unique_ptr<Tracker> tracker;
		bool retired = false;
	};
	std::vector<std::unique_ptr<Own>> own;
	Own *ownFor(const ServerSpec &spec);
	std::unique_ptr<Own> makeOwn(const ServerSpec &spec);
	// a tracker to keep: the site's or a server's own
	bool anyTracker() const { return tracker_ || !own.empty(); }
	// f on every tracker: the site's, then each server's own
	template <typename F>
	void eachTracker(F f)
	{
		if (tracker_)
			f(*tracker_);
		for (auto &o : own)
			f(*o->tracker);
	}
	// under site_mtx: the own trackers the configuration dropped go
	void dropRetired();
	// the parts of configure(), in its order
	void configureTracker(const SiteConfig &cfg);
	void configureServers(const SiteConfig &cfg);
	void restoreInto();
	bool engineAttached();
	// under site_mtx: the server on a port, null when none listens there
	Services::Server *serverOn(int port);
#else
	bool anyTracker() const { return false; }
#endif
#ifdef HASDELIVERY
	std::unique_ptr<Webhooks> webhooks;
	std::unique_ptr<MqttState> mqtt_state;
	void configureDeliveries(const SiteConfig &cfg);
#endif
	std::time_t last_tick = 0, last_backup = 0;
	std::string backup_file; // the file the configuration named when the tracker was made
	bool backing_up = false; // backups go to it: not without one, nor when its copy of an earlier version could not be kept
	// the seconds to the next beat
	int beat(std::time_t now);
	void backup();

	// the heartbeat: every 5 s, every second while the MQTT state connects or
	// sends, from the moment there is a tracker to keep
	std::thread heartbeat;
	std::mutex mtx;
	std::condition_variable cv;
	bool running = false, poked = false;
	void launch();
	void loop();
	// the next beat now; takes mtx alone
	void poke();
};
