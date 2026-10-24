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
#include <cstdio>
#ifdef __GLIBC__
#include <malloc.h>
#endif
#include <fstream>
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
	for (auto &o : own)
		o->tracker->setStations(resolved);
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
	// the trackers the configuration dropped while this engine fed them
	std::lock_guard<std::mutex> lock(site_mtx);
	own.erase(std::remove_if(own.begin(), own.end(), [](const std::unique_ptr<Own> &x) { return x->retired; }), own.end());
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
	if (!tracker_ && own.empty())
		return;
	EventRing::Entry e;
	e.kind = Tracking::Kind::SYSTEM;
	e.level = level;
	e.time = e.first = std::time(nullptr);
	e.label = what;
	e.text = text;
	e.station = station;
	e.receiver = receiver;
	if (tracker_)
		tracker_->journal().append(e);
	for (auto &o : own)
		o->tracker->journal().append(e);
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
		w.endArray().key("meta").beginObject().kv("count", (long long)engine.outputs.size()).kv("complete", true).kv_null("next").endObject();
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
	if (!started || heartbeat.joinable() || !anyTracker())
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
		if (!spec.tracking.own)
			tracking.msg_save |= spec.viewer.msg;
	if (cfg.wantsTracker() && !tracker_)
	{
		tracker_.reset(new Tracker());
		tracker_->configure(tracking);
		if (cfg.catalogue)
			tracker_->useCatalogue(cfg.catalogue);
		else
			tracker_->usePlaces(cfg.places);
		backup_file = cfg.backup_file;
		restoreInto(*tracker_, cfg.backup_file, backup_path);
		last_backup = std::time(nullptr);
		tracker_->setStations(stationsSnapshot());
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

	// The servers' own trackers, one a port. One stays while its server keeps the
	// same settings; while an engine feeds them, one with changed settings stays
	// too and waits for the next engine, as a new one does, since this engine was
	// wired at its start. A tracker that goes is destroyed once the servers
	// reading it have closed, and before any new one is made, so two never
	// share the memory.
	const bool fed = engineAttached();
	for (auto &o : own)
		o->retired = true;
	std::vector<const ServerSpec *> wanted;
	for (const auto &spec : cfg.servers)
	{
		if (!spec.tracking.own)
			continue;
		Own *o = ownFor(spec);
		if (o && (fed || o->spec == spec.tracking))
		{
			o->retired = false;
			o->tracker->setRestartRequired("Own ships (service restart)", o->spec != spec.tracking);
			continue;
		}
		wanted.push_back(&spec);
	}

	// A server on the same port and address, reading the same tracker, takes the
	// new settings in place, so its connections and a mount stay; the others close
	// before the new ones open, so a server that moves to another address finds
	// its port free and a tracker that goes has no reader left
	const auto trackerOf = [&](const ServerSpec &spec) -> Tracker * {
		if (!spec.tracking.own)
			return tracker_.get();
		Own *o = ownFor(spec);
		return o && !o->retired ? o->tracker.get() : nullptr;
	};
	std::vector<std::unique_ptr<Services::Server>> kept(cfg.servers.size());
	for (std::size_t i = 0; i < cfg.servers.size(); i++)
	{
		const ServerSpec &spec = cfg.servers[i];
		auto it = std::find_if(servers.begin(), servers.end(), [&](const std::unique_ptr<Services::Server> &s) {
			return s && s->spec().port == spec.port && s->spec().bind == spec.bind && &s->reads() == trackerOf(spec);
		});
		if (it == servers.end())
			continue;
		if ((*it)->spec() != spec)
			(*it)->reconfigure(spec);
		kept[i] = std::move(*it);
	}
	servers.clear();
	if (!fed)
		own.erase(std::remove_if(own.begin(), own.end(), [](const std::unique_ptr<Own> &x) { return x->retired; }), own.end());
	for (const ServerSpec *spec : wanted)
	{
		own.push_back(makeOwn(*spec));
		if (fed)
			own.back()->tracker->setRestartRequired("Own ships (service restart)", true);
	}
	launch();
	for (std::size_t i = 0; i < cfg.servers.size(); i++)
	{
		if (kept[i])
		{
			servers.push_back(std::move(kept[i]));
			continue;
		}
		std::unique_ptr<Services::Server> s(new Services::Server(cfg.servers[i], *trackerOf(cfg.servers[i]), [this] { return publicStations(); },
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
#ifdef __GLIBC__
	// glibc keeps what the replaced servers, trackers and places freed
	malloc_trim(0);
#endif
}

#ifdef HASAPI
// `to` as a whole copy of `from`, unless `to` is there already: a copy made on an
// earlier start is the one of before the upgrade
static bool keepCopy(const std::string &from, const std::string &to)
{
	if (std::ifstream(to, std::ios::binary))
		return true;
	const std::string tmp = to + ".tmp";
	{
		std::ifstream in(from, std::ios::binary);
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!in || !out || !(out << in.rdbuf()) || !out.flush())
		{
			std::remove(tmp.c_str());
			return false;
		}
	}
	if (std::rename(tmp.c_str(), to.c_str()) != 0)
	{
		std::remove(tmp.c_str());
		return false;
	}
	return true;
}

// under site_mtx: a tracker's backup is read once, when it is made
void Site::restoreInto(Tracker &t, const std::string &file, std::string &path)
{
	path = file;
	if (file.empty())
		return;
	Tracker::Restored r = t.restore(file);
	// A backup of an earlier version is copied once, beside it, for that version
	// to be given back; then it is imported, and written over in this version's
	// format from the next backup on. Without the copy it is left as it is.
	if (r == Tracker::FOREIGN)
	{
		const std::string kept = file + ".before-upgrade";
		if (!keepCopy(file, kept))
		{
			Error() << "Tracker: " << file << " is a backup of an earlier version and could not be copied to " << kept
					<< "; it is left as it is and nothing is backed up until it can be";
			path.clear();
			return;
		}
		if (t.importViewerBackup(file))
		{
			Info() << "Tracker: copied the backup of an earlier version to " << kept << ", which is not written again, and imported its statistics and ships";
			r = Tracker::RESTORED;
		}
	}
	if (r == Tracker::RESTORED)
		Info() << "Tracker: restored from " << path;
	else
		Info() << "Tracker: no backup restored from " << path;
}

bool Site::engineAttached()
{
	std::lock_guard<std::mutex> lock(engine_mtx);
	return engine_running;
}

// under site_mtx
Site::Own *Site::ownFor(const ServerSpec &spec)
{
	for (auto &o : own)
		if (o->port == spec.port)
			return o.get();
	return nullptr;
}

// under site_mtx: a server's own tracker, behind its filter
std::unique_ptr<Site::Own> Site::makeOwn(const ServerSpec &spec)
{
	std::unique_ptr<Own> o(new Own());
	o->port = spec.port;
	o->spec = spec.tracking;
	o->tracker.reset(new Tracker());
	TrackingConfig t = spec.tracking.tracking;
	t.msg_save |= spec.viewer.msg;
	o->tracker->configure(t);
	AIS::Filter filter;
	for (const auto &term : spec.tracking.filter)
		filter.SetOptionKey((AIS::Keys)term.first, term.second);
	o->tracker->setFilter(filter);
	Info() << "Server on port " << spec.port << ": ships of its own" << (filter.isOn() ? ", " + filter.Get() : std::string());
	o->tracker->setStations(stationsSnapshot());
	return o;
}

std::vector<Site::OwnTracker> Site::ownTrackers()
{
	std::lock_guard<std::mutex> lock(site_mtx);
	std::vector<OwnTracker> out;
	for (auto &o : own)
		out.push_back(OwnTracker{o->tracker.get(), o->spec});
	return out;
}

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

// under site_mtx: the site's tracker alone; a backup that starts failing is an event, once
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
	if (!tracker_ && own.empty())
		return;
	if (now - last_tick >= 60)
	{
		if (tracker_)
			tracker_->tick(now);
		for (auto &o : own)
			o->tracker->tick(now);
		last_tick = now;
	}
	if (tracker_ && config.backup_minutes > 0 && now - last_backup >= config.backup_minutes * 60)
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
