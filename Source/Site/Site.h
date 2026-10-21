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
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "MsgOut.h"
#include "SiteConfig.h"
#include "StationRegistry.h"
#include "Statistics.h"
#include "SystemFacts.h"
#ifdef HASAPI
#include "Server.h"
#include "System.h"
#include "Tracker.h"
#ifdef HASDELIVERY
#include "MqttState.h"
#include "Webhooks.h"
#endif
#else
class Tracker;
#endif

// What lives as long as the process: the stations, and in time the tracker,
// the servers and the deliveries. An engine session attaches to it when it
// starts and leaves it when it stops; in managed mode it outlives them all.
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

	StationRegistry stationsSnapshot() const
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		return stations;
	}
	// The stations as anything that serves them sees them: a station that
	// does not share where it is comes without a position, an altitude or the
	// vessel it rides, so no service can let one out.
	StationRegistry publicStations() const;

	// What the running engine is made of, told by it at its start and
	// withdrawn at its stop, before its outputs go: what a status page says
	// of the devices, the outputs and the community feed.
	struct DeviceFacts
	{
		// model: the device's decoders, one a line; label: what a status page calls it
		std::string product, vendor, serial, model, sample_rate, label;
	};
	struct EngineFacts
	{
		std::vector<DeviceFacts> devices;
		std::vector<IO::OutputMessage *> outputs;
		bool sharing = false, sharing_uuid = false;
	};
	void attachEngine(const EngineFacts &facts);
	void detachEngine();
#ifdef HASAPI
	// the receivers as the running engine sees them, told about once a second;
	// a receiver that loses its device or gets it back is an event
	void publishReceivers(const std::vector<ReceiverState> &states);
	// what the system service reads: the process, the engine, its receivers
	SystemFacts systemFacts();
#endif
	// when the tracker's backup was last written, or read when the tracker
	// was made; 0 without a backup file
	std::time_t lastBackup()
	{
		std::lock_guard<std::mutex> lock(site_mtx);
		return backup_path.empty() ? 0 : last_backup;
	}
	// the bytes every input delivers, connected to by each session's devices
	ByteCounter &received() { return bytes; }
	// the engine's part of a status page, as key-value pairs in an open object
	void writeEngine(JSON::Writer &w, EnginePart part = ENGINE_ALL);

	// The tracker and the servers the configuration asks for. A tracker, once
	// made, lives until the process ends; servers follow every change. A build
	// without the API has neither.
	void configure(const SiteConfig &cfg);
#ifdef HASAPI
	Tracker *tracker() { return tracker_.get(); }
	// the server on a port, to mount it elsewhere; null when none listens there
	Services::Server *server(int port);
	bool serverAddress(int port, std::string &host);
	std::vector<int> serverPorts();
#else
	Tracker *tracker() { return nullptr; }
#endif
	// a shared catalogue was edited: its new edition reaches the ships
	void refreshPlaces();

	// the heartbeat, about once a second, engine or not, from the moment there
	// is a tracker to keep
	void start();
	// stops the heartbeat and the servers, and saves the tracker
	void stop();

private:
	mutable std::mutex stations_mtx;
	StationRegistry stations;

	std::mutex engine_mtx;
	EngineFacts engine;
	bool engine_running = false;
	std::time_t session_start = 0;
#ifdef HASAPI
	std::vector<ReceiverState> receiver_states;
	// per receiver, its count at moments of the last minute, oldest first
	std::vector<std::deque<std::pair<std::time_t, uint64_t>>> receiver_counts;
	// an event of the installation itself, into the journal, of a station and
	// a receiver or of none (-1); not under site_mtx
	void systemEvent(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	void systemEventLocked(Tracking::Level level, const std::string &what, const std::string &text, int station = -1, int receiver = -1);
	bool backup_failing = false;
#endif
	ByteCounter bytes;
	const std::time_t born = std::time(nullptr); // the process's run time counts from here

	std::mutex site_mtx;
	SiteConfig config;
#ifdef HASAPI
	std::unique_ptr<Tracker> tracker_;
	std::vector<std::unique_ptr<Services::Server>> servers;
#endif
#ifdef HASDELIVERY
	std::unique_ptr<Webhooks> webhooks;
	std::unique_ptr<MqttState> mqtt_state;
#endif
	std::time_t last_tick = 0, last_backup = 0;
	std::string backup_file; // the file the configuration named when the tracker was made
	std::string backup_path; // where backups go: that file, or one beside a viewer backup of an earlier version
	void beat(std::time_t now);
	void backup();

	std::thread heartbeat;
	std::mutex mtx;
	std::condition_variable cv;
	bool started = false, running = false;
	void launch();
	void loop();
};
