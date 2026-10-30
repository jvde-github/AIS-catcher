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
	eachTracker([&](Tracker &t) { t.setStations(resolved); });
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

bool Site::resetEngine(const EngineFacts &facts, bool running)
{
	std::lock_guard<std::mutex> lock(engine_mtx);
	const bool was = engine_running;
	engine = facts;
	engine_running = running;
	if (running)
		session_start = std::time(nullptr);
#ifdef HASAPI
	receiver_states.clear();
	receiver_counts.clear();
#endif
	return was;
}

void Site::attachEngine(const EngineFacts &facts)
{
	resetEngine(facts, true);
#ifdef HASAPI
	systemEvent(Tracking::Level::ROUTINE, "engine_started", "The engine started");
#endif
}

void Site::detachEngine()
{
#ifdef HASAPI
	if (resetEngine(EngineFacts(), false))
		systemEvent(Tracking::Level::NOTICE, "engine_stopped", "The engine stopped");
	// the trackers the configuration dropped while this engine fed them
	std::lock_guard<std::mutex> lock(site_mtx);
	dropRetired();
#else
	resetEngine(EngineFacts(), false);
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
	if (!anyTracker())
		return;
	EventRing::Entry e;
	e.kind = Tracking::Kind::SYSTEM;
	e.level = level;
	e.time = e.first = std::time(nullptr);
	e.label = what;
	e.text = text;
	e.station = station;
	e.receiver = receiver;
	eachTracker([&](Tracker &t) { t.journal().append(e); });
}
#endif

#ifdef HASAPI
// a station as the events name it: by its name, or "Station <id>"
static std::string stationName(const StationRegistry &registry, int id)
{
	const StationRegistry::Station *s = registry.find(id);
	if (s && s->name.compare(0, 8, "Station ") == 0)
		return s->name;
	return "Station " + (s && !s->name.empty() ? s->name : std::to_string(id));
}

// What the beat tells of the stations of this installation's receivers: one
// that has heard nothing for STATION_QUIET_S while the engine runs, as the
// health warns of it, and that it hears again; and, looked at once a minute and
// told once a day, a day of exceptional range. Under site_mtx.
void Site::watchStations(std::time_t now, bool daily)
{
	const SystemFacts f = systemFacts();
	std::set<int> mine;
	for (const auto &r : f.receivers)
		mine.insert(r.station);
	const StationRegistry registry = stationsSnapshot();
	std::set<int> quiet = tellQuiet(now, f, mine, registry);
	setStationStates(now, f, mine, quiet, registry);
	quiet_stations.swap(quiet);
	if (daily)
		tellPropagation(now, mine, registry);
}

std::set<int> Site::tellQuiet(std::time_t now, const SystemFacts &f, const std::set<int> &mine, const StationRegistry &registry)
{
	std::set<int> quiet;
	if (f.running && now - f.session > STATION_QUIET_S)
		for (int id : mine)
		{
			const std::time_t heard = tracker_->lastHeard(id);
			if (now - (heard ? heard : f.session) > STATION_QUIET_S)
				quiet.insert(id);
		}
	for (int id : quiet)
		if (!quiet_stations.count(id))
			systemEventLocked(Tracking::Level::NOTICE, "station_quiet", stationName(registry, id) + " has heard nothing for " + std::to_string(STATION_QUIET_S / 60) + " minutes", id);
	if (f.running)
		for (int id : quiet_stations)
			if (!quiet.count(id) && mine.count(id))
				systemEventLocked(Tracking::Level::ROUTINE, "station_heard", stationName(registry, id) + " hears again", id);
	return quiet;
}

// the same judgement for the map: a station of this installation is down when every
// receiver naming it is lost, quiet as the event above or while the engine is stopped,
// receiving otherwise; a station heard through a feed is quiet after the same silence;
// one the installation cannot tell of stays unknown. A row goes out when its state changes.
void Site::setStationStates(std::time_t now, const SystemFacts &f, const std::set<int> &mine, const std::set<int> &quiet, const StationRegistry &registry)
{
	std::map<int, int> states;
	std::map<int, int> receivers, lost;
	for (const auto &r : f.receivers)
	{
		receivers[r.station]++;
		if (r.lost)
			lost[r.station]++;
	}
	registry.forEach([&](const StationRegistry::Station &s) {
		if (mine.count(s.id))
			states[s.id] = lost[s.id] && lost[s.id] == receivers[s.id] ? StationRegistry::STATE_DOWN
						   : !f.running || quiet.count(s.id)	   ? StationRegistry::STATE_QUIET
																   : StationRegistry::STATE_RECEIVING;
		else if (const std::time_t heard = tracker_->lastHeard(s.id))
			states[s.id] = now - heard > STATION_QUIET_S ? StationRegistry::STATE_QUIET : StationRegistry::STATE_RECEIVING;
	});
	eachTracker([&](Tracker &t) { t.setStationStates(states); });
}

void Site::tellPropagation(std::time_t now, const std::set<int> &mine, const StationRegistry &registry)
{
	const std::time_t today = now - now % 86400;
	for (int id : mine)
	{
		if (far_told[id] == today)
			continue;
		std::vector<Figures::Propagation> days;
		const auto look = [&](Figures &fig) { days = fig.propagation(); };
		if (!tracker_->withStation(id, look) && mine.size() == 1)
			tracker_->withAll(look);
		if (days.empty() || days.back().day != today)
			continue;
		far_told[id] = today;
		// the journal is anyone's to read: the distances of a position kept home stay out of it
		const StationRegistry::Station *s = registry.find(id);
		systemEventLocked(Tracking::Level::NOTICE, "propagation",
						  s && s->share_loc ? stationName(registry, id) + " hears far today: " + std::to_string((int)(days.back().max_nm + 0.5f)) + " NM, against " +
												  std::to_string((int)(days.back().usual_nm + 0.5f)) + " NM on a usual day"
											: stationName(registry, id) + " hears far today",
						  id);
	}
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

// under site_mtx: the heartbeat runs once there is a tracker to keep
void Site::launch()
{
	if (heartbeat.joinable() || !anyTracker())
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
	backup_minutes = cfg.backup_minutes;
#ifdef HASAPI
	configureTracker(cfg);
	configureServers(cfg);
#endif
#ifdef HASDELIVERY
	configureDeliveries(cfg);
#endif
#ifdef __GLIBC__
	// glibc keeps what the replaced servers, trackers and places freed
	malloc_trim(0);
#endif
}

#ifdef HASAPI
// under site_mtx: the site's tracker, made when first asked for, and its settings
void Site::configureTracker(const SiteConfig &cfg)
{
	// a viewer that shows a vessel's last message needs it kept
	TrackingConfig tracking = cfg.tracking;
	for (const auto &spec : cfg.servers)
		if (!spec.tracking.own)
			tracking.msg_save |= spec.viewer.msg;
	const bool made = cfg.wantsTracker() && !tracker_;
	if (made)
	{
		tracker_.reset(new Tracker());
		tracker_->journal() >> journal_ping;
	}
	if (!tracker_)
		return;
	tracker_->configure(tracking);
	if (cfg.catalogue)
		tracker_->useCatalogue(cfg.catalogue);
	else
		tracker_->usePlaces(cfg.places);
	if (made)
	{
		backup_file = cfg.backup_file;
		restoreInto();
		last_backup = std::time(nullptr);
		tracker_->setStations(stationsSnapshot());
	}
	else
		// the backup file is read when the tracker is made: another one waits for a restart
		tracker_->setRestartRequired("Backup file (service restart)", cfg.backup_file != backup_file);
}

// under site_mtx: the servers and the trackers of their own
void Site::configureServers(const SiteConfig &cfg)
{
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
		dropRetired();
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
}
#endif

#ifdef HASDELIVERY
// under site_mtx: the webhooks and the MQTT state, made when first asked for and kept after
void Site::configureDeliveries(const SiteConfig &cfg)
{
	// a webhook that goes keeps the others' places
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
	events_wanted = mqtt_state && cfg.mqtt_state.any();
}
#endif

#ifdef HASAPI
// `to` as a whole copy of `from`, unless `to` is there already: a copy made on an
// earlier start is the one of before the upgrade
static bool keepCopy(const std::string &from, const std::string &to)
{
	if (std::ifstream(to, std::ios::binary))
		return true;
	const std::string tmp = to + ".tmp";
	if (!Util::Helper::createTemp(tmp, to))
		return false;
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

// under site_mtx: the site's tracker's backup is read once, when it is made,
// from the file the configuration named; backups go there from then on
void Site::restoreInto()
{
	backing_up = !backup_file.empty();
	if (backup_file.empty())
		return;
	Tracker::Restored r = tracker_->restore(backup_file);
	// A backup of an earlier version is copied once, beside it, for that version
	// to be given back; then it is imported, and written over in this version's
	// format from the next backup on. Without the copy it is left as it is.
	if (r == Tracker::FOREIGN)
	{
		const std::string kept = backup_file + ".before-upgrade";
		if (!keepCopy(backup_file, kept))
		{
			Error() << "Tracker: " << backup_file << " is a backup of an earlier version and could not be copied to " << kept
					<< "; it is left as it is and nothing is backed up until it can be";
			backing_up = false;
			return;
		}
		if (tracker_->importViewerBackup(backup_file))
		{
			Info() << "Tracker: copied the backup of an earlier version to " << kept << ", which is not written again, and imported its statistics and ships";
			r = Tracker::RESTORED;
		}
	}
	if (r == Tracker::RESTORED)
		Info() << "Tracker: restored from " << backup_file;
	else
		Info() << "Tracker: no backup restored from " << backup_file;
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

void Site::dropRetired()
{
	own.erase(std::remove_if(own.begin(), own.end(), [](const std::unique_ptr<Own> &x) { return x->retired; }), own.end());
}

// under site_mtx: a server's own tracker, behind its filter; the reader gave
// its settings what the viewer needs kept
std::unique_ptr<Site::Own> Site::makeOwn(const ServerSpec &spec)
{
	std::unique_ptr<Own> o(new Own());
	o->port = spec.port;
	o->spec = spec.tracking;
	o->tracker.reset(new Tracker());
	o->tracker->configure(spec.tracking.tracking);
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

std::vector<std::pair<int, long long>> Site::serverTLS()
{
	std::lock_guard<std::mutex> lock(site_mtx);
	std::vector<std::pair<int, long long>> on;
	for (auto &s : servers)
		if (s->hasTLS())
			on.emplace_back(s->spec().port, s->tlsUntil());
	return on;
}

Services::Server *Site::serverOn(int port)
{
	for (auto &s : servers)
		if (s->spec().port == port)
			return s.get();
	return nullptr;
}

// where a request made on this machine reaches the server on a port: its own
// address, or this machine's when it listens on every one
bool Site::serverAddress(int port, std::string &host)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	const Services::Server *s = serverOn(port);
	if (!s)
		return false;
	const std::string &bind = s->spec().bind;
	host = bind.empty() || bind == "0.0.0.0" || bind == "::" ? "127.0.0.1" : bind;
	return true;
}

Services::Server *Site::server(int port)
{
	std::lock_guard<std::mutex> lock(site_mtx);
	return serverOn(port);
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
	if (!tracker_ || !backing_up)
		return;
	const bool ok = tracker_->save(backup_file);
	if (!ok)
		Error() << "Tracker: backup to " << backup_file << " failed";
	if (!ok && !backup_failing)
		systemEventLocked(Tracking::Level::URGENT, "backup_failed", "The backup to " + backup_file + " failed");
	backup_failing = !ok;
#endif
}

int Site::beat(std::time_t now)
{
	int wait = 5;
#ifdef HASAPI
	std::lock_guard<std::mutex> lock(site_mtx);
	if (!anyTracker())
		return wait;
	const bool minute = now - last_tick >= 60;
	if (minute)
	{
		eachTracker([&](Tracker &t) { t.tick(now); });
		last_tick = now;
	}
	if (tracker_)
		watchStations(now, minute);
	if (tracker_ && backup_minutes > 0 && now - last_backup >= backup_minutes * 60)
	{
		backup();
		last_backup = now;
	}
#ifdef HASDELIVERY
	if (mqtt_state)
	{
		mqtt_state->poll(now);
		// a connection under way is stepped every second
		if (mqtt_state->busy())
			wait = 1;
	}
#endif
#else
	(void)now;
#endif
	return wait;
}

// every few seconds, every second while the MQTT state connects or sends, and
// at once when poked
void Site::loop()
{
	std::unique_lock<std::mutex> lock(mtx);
	int wait = 1;
	while (running)
	{
		cv.wait_for(lock, std::chrono::seconds(wait), [this] { return !running || poked; });
		poked = false;
		if (!running)
			break;
		lock.unlock();
		try
		{
			wait = beat(std::time(nullptr));
		}
		catch (const std::exception &e)
		{
			Error() << "Site: " << e.what();
			wait = 1;
		}
		lock.lock();
	}
}

void Site::poke()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		poked = true;
	}
	cv.notify_all();
}
