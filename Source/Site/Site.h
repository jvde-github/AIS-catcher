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

// What lives as long as the process: the stations, the tracker, the servers
// and the deliveries. An engine session attaches to it when it starts and
// leaves it when it stops.
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

	// told by the running engine at its start and withdrawn at its stop,
	// before its outputs go
	void attachEngine(const EngineFacts &facts);
	void detachEngine();
	// when the tracker's backup was last written, or read when the tracker
	// was made; 0 without a backup file
	std::time_t lastBackup()
	{
		std::lock_guard<std::mutex> lock(site_mtx);
		return backing_up ? last_backup : 0;
	}
	ByteCounter &received() { return bytes; }

	// a tracker, once made, lives until the process ends; servers follow every change
	void configure(const SiteConfig &cfg);
#ifdef HASAPI
	// the receivers as the running engine sees them, told about once a second;
	// a receiver that loses its device or gets it back is an event
	void publishReceivers(const std::vector<ReceiverState> &states);
	SystemFacts systemFacts();
	Tracker *tracker() { return tracker_.get(); }
	struct OwnTracker
	{
		Tracker *tracker;
		TrackerSpec spec;
	};
	std::vector<OwnTracker> ownTrackers();
	Services::Server *server(int port);
	bool serverAddress(int port, std::string &host);
	std::vector<int> serverPorts();
	// the servers with HTTPS: the port, and when its certificate expires
	std::vector<std::pair<int, long long>> serverTLS();
	// for the station's owner: a private position shows here too
	std::string stationsLive();
	// each receiver's index and state, as the system service names it
	std::vector<std::pair<int, std::string>> receiverStates();
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

	std::mutex engine_mtx;
	EngineFacts engine;
	bool engine_running = false;
	std::time_t session_start = 0;
	// the engine as told, or none; whether one was running before
	bool resetEngine(const EngineFacts &facts, bool running);
	// the engine's part of a status page, as key-value pairs in an open object
	void writeEngine(JSON::Writer &w, EnginePart part);
	ByteCounter bytes;
	const std::time_t born = std::time(nullptr);

	std::mutex site_mtx;
	// 0 for never
	int backup_minutes = 10;
#ifdef HASAPI
	std::vector<ReceiverState> receiver_states;
	// per receiver, its count at moments of the last minute, oldest first
	std::vector<std::deque<std::pair<std::time_t, uint64_t>>> receiver_counts;
	// into the journal; not under site_mtx
	void systemEvent(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	void systemEventLocked(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	bool backup_failing = false;
	// the stations of this installation's receivers: which are quiet, and the
	// day each was last told to hear far; under site_mtx
	std::set<int> quiet_stations;
	std::map<int, std::time_t> far_told;
	void watchStations(std::time_t now, bool daily);
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
	// A server's own tracker, made with its server. Changed settings wait for the
	// next engine, as the running one feeds it, and one the configuration dropped
	// stays until that engine detaches.
	// A tracker of a server's own; servers with the same tracking settings share one
	struct Own
	{
		int port = 0; // the first server's, named in the log
		std::set<int> ports;
		TrackerSpec spec;
		std::unique_ptr<Tracker> tracker;
		bool retired = false;
		std::string backup_file;
	};
	std::vector<std::unique_ptr<Own>> own;
	// the tracker with these settings, else the one this port had
	Own *ownFor(const ServerSpec &spec);
	std::unique_ptr<Own> makeOwn(const ServerSpec &spec, const SiteConfig &cfg);
	bool anyTracker() const { return tracker_ || !own.empty(); }
	template <typename F>
	void eachTracker(F f)
	{
		if (tracker_)
			f(*tracker_);
		for (auto &o : own)
			f(*o->tracker);
	}
	// under site_mtx
	void dropRetired();
	void configureTracker(const SiteConfig &cfg);
	void configureServers(const SiteConfig &cfg);
	void restoreInto();
	// a tracker's backup read once, when it is made; false when the file must be left as it is
	bool restoreFrom(Tracker &t, const std::string &file);
	// the catalogue every tracker judges against: managed mode's shared one, else the places folder read once
	std::shared_ptr<PlaceCatalogue> catalogueFor(const SiteConfig &cfg);
	std::shared_ptr<PlaceCatalogue> places_catalogue;
	std::string places_dir;
	bool engineAttached();
	// under site_mtx
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
	std::string backup_file;
	bool backing_up = false;
	// the seconds to the next beat
	int beat(std::time_t now);
	void backup();

	// every 5 s, every second while the MQTT state connects or sends, from the
	// moment there is a tracker to keep
	std::thread heartbeat;
	std::mutex mtx;
	std::condition_variable cv;
	bool running = false, poked = false;
	void launch();
	void loop();
	// the next beat now; takes mtx alone
	void poke();
};
