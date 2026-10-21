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


#include "Site.h"
#include "Helper.h"
#include "Logger.h"

#include <algorithm>
#include <chrono>
#include <map>

void Site::publishStations(const StationRegistry &resolved)
{
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		stations = resolved;
	}
#ifdef HASAPI
	std::lock_guard<std::mutex> lock(site_mtx);
	if (tracker_)
		tracker_->setStations(resolved);
#endif
}

StationRegistry Site::publicStations() const
{
	StationRegistry out;
	for (const auto &s : stationsSnapshot().ordered())
	{
		StationRegistry::Station copy = s;
		if (!s.share_loc)
		{
			copy.lat = LAT_UNDEFINED;
			copy.lon = LON_UNDEFINED;
			copy.alt = ALTITUDE_UNDEFINED;
			copy.mmsi = 0;
		}
		out.define(copy);
	}
	return out;
}

void Site::attachEngine(const EngineFacts &facts)
{
	{
		std::lock_guard<std::mutex> lock(engine_mtx);
		engine = facts;
		engine_running = true;
		session_start = std::time(nullptr);
#ifdef HASAPI
		receiver_states.clear();
		receiver_counts.clear();
#endif
	}
#ifdef HASAPI
	systemEvent(Tracking::Level::ROUTINE, "engine_started", "The engine started");
#endif
}

void Site::detachEngine()
{
	bool was = false;
	{
		std::lock_guard<std::mutex> lock(engine_mtx);
		was = engine_running;
		engine = EngineFacts();
		engine_running = false;
#ifdef HASAPI
		receiver_states.clear();
		receiver_counts.clear();
#endif
	}
#ifdef HASAPI
	if (was)
		systemEvent(Tracking::Level::NOTICE, "engine_stopped", "The engine stopped");
#else
	(void)was;
#endif
}

#ifdef HASAPI
void Site::publishReceivers(const std::vector<ReceiverState> &states)
{
	const std::time_t now = std::time(nullptr);
	std::vector<ReceiverState> lost, back;
	{
		std::lock_guard<std::mutex> lock(engine_mtx);
		if (!engine_running)
			return;
		receiver_counts.resize(states.size());
		std::vector<ReceiverState> next = states;
		for (std::size_t i = 0; i < next.size(); i++)
		{
			auto &samples = receiver_counts[i];
			samples.push_back(std::make_pair(now, next[i].messages));
			// keep one sample a minute old or more, and what came after it
			while (samples.size() > 1 && samples[1].first <= now - 60)
				samples.pop_front();
			next[i].per_minute = next[i].messages - samples.front().second;
			if (i < receiver_states.size())
			{
				const bool was_down = receiver_states[i].lost, down = next[i].lost;
				if (!was_down && down)
					lost.push_back(next[i]);
				else if (was_down && !down && next[i].streaming)
					back.push_back(next[i]);
			}
		}
		receiver_states.swap(next);
	}
	for (const auto &r : lost)
		systemEvent(Tracking::Level::URGENT, "receiver_lost", "Receiver " + std::to_string(r.index) + " lost its device", r.station, r.index);
	for (const auto &r : back)
		systemEvent(Tracking::Level::ROUTINE, "receiver_recovered", "Receiver " + std::to_string(r.index) + " is receiving again", r.station, r.index);
}

SystemFacts Site::systemFacts()
{
	SystemFacts f;
	std::lock_guard<std::mutex> lock(engine_mtx);
	f.running = engine_running;
	f.born = born;
	f.session = session_start;
	f.received = bytes.received();
	f.receivers = receiver_states;
	return f;
}

void Site::systemEvent(Tracking::Level level, const std::string &what, const std::string &text, int station, int receiver)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	systemEventLocked(level, what, text, station, receiver);
}

void Site::systemEventLocked(Tracking::Level level, const std::string &what, const std::string &text, int station, int receiver)
{
	if (!tracker_)
		return;
	EventRing::Entry e;
	e.kind = Tracking::Kind::SYSTEM;
	e.level = level;
	e.time = e.first = std::time(nullptr);
	e.label = what;
	e.text = text;
	e.station = station;
	e.receiver = receiver;
	tracker_->journal().append(e);
}
#endif

// each output, named by its description or by its type and place among that type: "UDP #2"
static void writeOutputs(JSON::Writer &w, const std::vector<IO::OutputMessage *> &outputs)
{
	std::map<std::string, int> nth;
	for (const auto *o : outputs)
		o->writeJSON(w, o->getType() + " #" + std::to_string(++nth[o->getType()]));
}

void Site::writeEngine(JSON::Writer &w, EnginePart part)
{
	std::lock_guard<std::mutex> lock(engine_mtx);
	// the engine's outputs, while they exist: the lock keeps them from going mid-sentence
	switch (part)
	{
	case ENGINE_SHARING:
		w.kv("sharing", engine.sharing).kv("sharing_uuid", engine.sharing_uuid);
		break;
	case ENGINE_RUNNING:
		w.kv("engine_running", engine_running);
		break;
	case ENGINE_OUTPUT_LIST:
		w.key("data").beginArray();
		writeOutputs(w, engine.outputs);
		w.endArray().key("meta").beginObject().kv("count", (long long)engine.outputs.size()).kv("complete", true).endObject();
		break;
	case ENGINE_OUTPUTS:
		w.key("outputs").beginArray();
		writeOutputs(w, engine.outputs);
		w.endArray();
		break;
	case ENGINE_ALL:
	{
		// the devices as a status page lists them: one list for each thing said of them
		const auto each = [&](const char *key, std::string DeviceFacts::*field) {
			w.key(key).beginArray();
			for (const auto &d : engine.devices)
				w.val(d.*field);
			w.endArray();
		};
		w.kv("sharing", engine.sharing).kv("sharing_uuid", engine.sharing_uuid).kv("engine_running", engine_running);
		each("product", &DeviceFacts::product);
		each("vendor", &DeviceFacts::vendor);
		each("serial", &DeviceFacts::serial);
		each("model", &DeviceFacts::model);
		each("sample_rate", &DeviceFacts::sample_rate);
		each("device_label", &DeviceFacts::label);
		w.kv("received", (unsigned long long)bytes.received()).kv("run_time", std::to_string((long long)(std::time(nullptr) - born)))
			.kv("os", Util::Helper::getOS()).kv("hardware", Util::Helper::getHardware());
		w.key("outputs").beginArray();
		writeOutputs(w, engine.outputs);
		w.endArray();
		break;
	}
	}
}

void Site::start()
{
	std::lock_guard<std::mutex> lock(site_mtx);
	started = true;
	launch();
}

// under site_mtx: the heartbeat runs once started and there is a tracker to keep
void Site::launch()
{
	if (!started || heartbeat.joinable() || !tracker())
		return;
	{
		std::lock_guard<std::mutex> lock(mtx);
		running = true;
	}
	heartbeat = std::thread(&Site::loop, this);
}

void Site::stop()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		running = false;
	}
	cv.notify_all();
	if (heartbeat.joinable())
		heartbeat.join();

	std::lock_guard<std::mutex> lock(site_mtx);
#ifdef HASDELIVERY
	if (webhooks)
		webhooks->stop();
	mqtt_state.reset();
#endif
#ifdef HASAPI
	servers.clear();
#endif
	backup();
}

void Site::configure(const SiteConfig &cfg)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	config = cfg;
#ifdef HASAPI
	// a viewer that shows a vessel's last message needs it kept
	TrackingConfig tracking = cfg.tracking;
	for (const auto &spec : cfg.servers)
		tracking.msg_save |= spec.viewer.msg;
	if (cfg.wantsTracker() && !tracker_)
	{
		tracker_.reset(new Tracker());
		tracker_->configure(tracking);
		if (cfg.catalogue)
			tracker_->useCatalogue(cfg.catalogue);
		else
			tracker_->usePlaces(cfg.places);
		backup_file = backup_path = cfg.backup_file;
		if (!cfg.backup_file.empty())
		{
			Tracker::Restored r = tracker_->restore(cfg.backup_file);
			// A viewer backup of an earlier version is read once and never
			// written over, so that version still finds it: backups go beside it
			if (r == Tracker::FOREIGN)
			{
				backup_path = cfg.backup_file + ".tracking";
				r = tracker_->restore(backup_path);
				if (r == Tracker::MISSING && tracker_->importViewerBackup(cfg.backup_file))
				{
					Info() << "Tracker: imported the viewer's statistics and ships from " << cfg.backup_file << "; backups go to " << backup_path;
					r = Tracker::RESTORED;
				}
			}
			if (r == Tracker::RESTORED)
				Info() << "Tracker: restored from " << backup_path;
			else
				Info() << "Tracker: no backup restored from " << backup_path;
		}
		last_backup = std::time(nullptr);
		tracker_->setStations(stationsSnapshot());
		launch();
	}
	else if (tracker_)
	{
		tracker_->configure(tracking);
		if (cfg.catalogue)
			tracker_->useCatalogue(cfg.catalogue);
		else
			tracker_->usePlaces(cfg.places);
		// the backup file is read when the tracker is made: another one waits for a restart
		tracker_->setRestartRequired("Backup file (service restart)", cfg.backup_file != backup_file);
	}

	// A server on the same port and address takes the new settings in place, so
	// its connections and a mount stay; the others close before the new ones
	// open, so a server that moves to another address finds its port free
	std::vector<std::unique_ptr<Services::Server>> kept(cfg.servers.size());
	for (std::size_t i = 0; i < cfg.servers.size(); i++)
	{
		const ServerSpec &spec = cfg.servers[i];
		auto it = std::find_if(servers.begin(), servers.end(), [&](const std::unique_ptr<Services::Server> &s) {
			return s && s->spec().port == spec.port && s->spec().bind == spec.bind;
		});
		if (it == servers.end())
			continue;
		if ((*it)->spec() != spec)
			(*it)->reconfigure(spec);
		kept[i] = std::move(*it);
	}
	servers.clear();
	for (std::size_t i = 0; i < cfg.servers.size(); i++)
	{
		if (kept[i])
		{
			servers.push_back(std::move(kept[i]));
			continue;
		}
		std::unique_ptr<Services::Server> s(new Services::Server(cfg.servers[i], *tracker_, [this] { return publicStations(); },
																 [this](JSON::Writer &w, EnginePart part) { writeEngine(w, part); },
																 [this] { return systemFacts(); }));
		if (s->open())
			servers.push_back(std::move(s));
	}
#endif
#ifdef HASDELIVERY
	// made when first asked for, kept after: a webhook that goes keeps the others' places
	if (tracker_ && (!cfg.webhooks.empty() || webhooks))
	{
		if (!webhooks)
			webhooks.reset(new Webhooks(*tracker_));
		webhooks->configure(cfg.webhooks);
	}
	if (tracker_ && (cfg.mqtt_state.any() || mqtt_state))
	{
		if (!mqtt_state)
			mqtt_state.reset(new MqttState(*tracker_, [this] { return systemFacts(); }, [this] { return publicStations(); }));
		mqtt_state->configure(cfg.mqtt_state);
	}
#endif
}

#ifdef HASAPI
// the ports the servers listen on: a server whose port was taken is not among them
std::vector<int> Site::serverPorts()
{
	std::lock_guard<std::mutex> lock(site_mtx);
	std::vector<int> ports;
	for (auto &s : servers)
		ports.push_back(s->spec().port);
	return ports;
}

// where a request made on this machine reaches the server on a port: its own
// address, or this machine's when it listens on every one
bool Site::serverAddress(int port, std::string &host)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	for (auto &s : servers)
		if (s->spec().port == port)
		{
			const std::string &bind = s->spec().bind;
			host = bind.empty() || bind == "0.0.0.0" || bind == "::" ? "127.0.0.1" : bind;
			return true;
		}
	return false;
}

Services::Server *Site::server(int port)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	for (auto &s : servers)
		if (s->spec().port == port)
			return s.get();
	return nullptr;
}
#endif

void Site::refreshPlaces()
{
#ifdef HASAPI
	std::lock_guard<std::mutex> lock(site_mtx);
	if (tracker_)
		tracker_->refreshPlaces();
#endif
}

// under site_mtx; a backup that starts failing is an event, once
void Site::backup()
{
#ifdef HASAPI
	if (!tracker_ || backup_path.empty())
		return;
	const bool ok = tracker_->save(backup_path);
	if (!ok)
		Error() << "Tracker: backup to " << backup_path << " failed";
	if (!ok && !backup_failing)
		systemEventLocked(Tracking::Level::URGENT, "backup_failed", "The backup to " + backup_path + " failed");
	backup_failing = !ok;
#endif
}

void Site::beat(std::time_t now)
{
#ifdef HASAPI
	std::lock_guard<std::mutex> lock(site_mtx);
	if (!tracker_)
		return;
	if (now - last_tick >= 60)
	{
		tracker_->tick(now);
		last_tick = now;
	}
	if (config.backup_minutes > 0 && now - last_backup >= config.backup_minutes * 60)
	{
		backup();
		last_backup = now;
	}
#ifdef HASDELIVERY
	if (mqtt_state)
		mqtt_state->poll(now);
#endif
#else
	(void)now;
#endif
}

void Site::loop()
{
	std::unique_lock<std::mutex> lock(mtx);
	while (running)
	{
		cv.wait_for(lock, std::chrono::seconds(1), [this] { return !running; });
		if (!running)
			break;
		lock.unlock();
		beat(std::time(nullptr));
		lock.lock();
	}
}
