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
#include "PlaceCatalogue.h"
#include "FIFO.h"
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
	station_table->set(resolved);
#ifdef HASAPI
	std::lock_guard<std::mutex> lock(site_mtx);
	eachTracker([&](Tracker &t) { t.setStations(resolved); });
#endif
}

bool Site::resetEngine(const EngineFacts &facts, bool running)
{
	std::lock_guard<std::mutex> lock(engine_mtx);
	const bool was = engine_running;
	engine = facts;
	engine_running = running;
	if (running && !was)
		session_start = std::time(nullptr);
#ifdef HASAPI
	receiver_states.clear();
	receiver_counts.clear();
#endif
	return was;
}

void Site::attachEngine(const EngineFacts &facts, EngineWiring &engine_wiring, Outputs engine_outputs)
{
	resetEngine(facts, true);
	const std::shared_ptr<Outputs> made = std::make_shared<Outputs>(std::move(engine_outputs));
	{
		std::lock_guard<std::mutex> lock(site_mtx);
		wiring = &engine_wiring;
		startOutputs(*made, made->list);
		{
			std::lock_guard<std::mutex> held(engine_mtx);
			outputs_ = made;
		}
		connect();
	}
#ifdef HASAPI
	systemEvent(Tracking::Level::ROUTINE, "engine_started", "The engine started");
#endif
}

void Site::detachEngine()
{
	// the receivers have stopped: the outputs stop here, outside the locks
	std::shared_ptr<Outputs> gone = std::make_shared<Outputs>();
	{
		std::lock_guard<std::mutex> lock(site_mtx);
		wiring = nullptr;
		std::lock_guard<std::mutex> held(engine_mtx);
		gone.swap(outputs_);
	}
#ifdef HASAPI
	if (resetEngine(EngineFacts(), false))
		systemEvent(Tracking::Level::NOTICE, "engine_stopped", "The engine stopped");
#else
	resetEngine(EngineFacts(), false);
#endif
}

// under site_mtx, with an engine running
void Site::startOutputs(const Outputs &o, const OutputList &fresh)
{
	std::map<std::string, int> nth;
	for (const auto &out : o.list)
	{
		const std::string label = out->getType() + " #" + std::to_string(++nth[out->getType()]);
		const uint64_t asked = out.get() == o.feed && o.radios_only ? wiring->radios() : out->groups_asked;
		out->takeGroups(wiring->route(out->zones, asked, out->getDescription().empty() ? label : out->getDescription()));
	}
	for (const auto &out : fresh)
		out->Start();
}

// under site_mtx
void Site::connect()
{
	if (!wiring)
		return;
	const std::shared_ptr<Outputs> o = outputs();
	std::lock_guard<std::mutex> held(delivery_mtx);
	wiring->unwire();
	for (const auto &out : o->list)
		wiring->wire(*out);
#ifdef HASAPI
	if (tracker_)
		wiring->feed(*tracker_, nullptr);
	for (auto &t : own)
		wiring->feed(*t->tracker, &t->spec);
#endif
}

void Site::rewire(const EngineFacts &facts)
{
	resetEngine(facts, true);
	std::lock_guard<std::mutex> lock(site_mtx);
	startOutputs(*outputs(), OutputList());
	connect();
}

void Site::setOutputs(Outputs next)
{
	const std::shared_ptr<Outputs> made = std::make_shared<Outputs>(std::move(next));
	// the ones no longer listed stop when this returns, outside the locks
	std::shared_ptr<Outputs> old;
	OutputList fresh;
	std::lock_guard<std::mutex> lock(site_mtx);
	if (!wiring)
		return;
	old = outputs();

	std::vector<bool> kept(old->list.size(), false);
	for (std::size_t i = 0; i < made->list.size(); i++)
	{
		std::size_t j = 0;
		while (j < old->list.size() && (kept[j] || old->told[j] != made->told[i]))
			j++;
		if (j == old->list.size())
		{
			fresh.push_back(made->list[i]);
			continue;
		}
		kept[j] = true;
		if (made->feed == made->list[i].get())
			made->feed = old->list[j].get();
		made->list[i] = old->list[j];
	}
	const std::size_t gone = std::count(kept.begin(), kept.end(), false);
	if (fresh.empty() && !gone)
		return;

	startOutputs(*made, fresh);
	{
		std::lock_guard<std::mutex> held(engine_mtx);
		outputs_ = made;
	}
	connect();
	Info() << "Outputs: " << fresh.size() << " started and " << gone << " stopped, as the configuration now has them";
}

void Site::pollOutputs()
{
	for (auto &o : outputs()->list)
		o->poll();
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
	f.devices = engine;
	f.receivers = receiver_states;
	f.sharing = outputs_->feed != nullptr;
	f.sharing_uuid = f.sharing && outputs_->feed->hasUUID();
	std::map<std::string, int> nth;
	for (const auto &o : outputs_->list)
	{
		OutputFacts of;
		of.type = o->getType();
		const std::string numbered = of.type + " #" + std::to_string(++nth[of.type]);
		of.label = o->getDescription().empty() ? numbered : o->getDescription();
		of.link = o->getLink();
		of.connection = o->hasConnection();
		const IO::OutputStats &s = o->getStats();
		of.messages = s.messages.get();
		of.bytes_out = s.bytes_out.get();
		of.bytes_in = s.bytes_in.get();
		of.connect_ok = s.connect_ok.get();
		of.connect_fail = s.connect_fail.get();
		of.reconnects = s.reconnects.get();
		of.connected = s.connected.get();
		of.dropped = s.dropped.get();
		f.outputs.push_back(of);
	}
	f.overruns = FIFO::overruns();
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
static std::string stationName(const StationRegistry &registry, int id)
{
	const StationRegistry::Station *s = registry.find(id);
	if (s && s->name.compare(0, 8, "Station ") == 0)
		return s->name;
	return "Station " + (s && !s->name.empty() ? s->name : std::to_string(id));
}

// under site_mtx; a day of exceptional range is looked for once a minute and
// told once a day
void Site::watchStations(std::time_t now, bool daily)
{
	const SystemFacts f = systemFacts();
	std::set<int> mine;
	for (const auto &r : f.receivers)
		mine.insert(r.station);
	const StationRegistry registry = stations();
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

// a station left out of the states stays unknown on the map
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
		// the journal is every server's to read: the distance only where none keeps the station home
		const StationRegistry::Station *s = registry.find(id);
		bool located = s != nullptr;
		for (const auto &server : servers)
			if (located && server && server->spec().showsStation(id))
				located = server->spec().locates(s->share_loc);
		systemEventLocked(Tracking::Level::NOTICE, "propagation",
						  located ? stationName(registry, id) + " hears far today: " + std::to_string((int)(days.back().max_nm + 0.5f)) + " NM, against " +
												  std::to_string((int)(days.back().usual_nm + 0.5f)) + " NM on a usual day"
											: stationName(registry, id) + " hears far today",
						  id);
	}
}
#endif

// under site_mtx
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
		if (stopped)
			return;
		stopped = true;
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
// under site_mtx
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
		tracker_->useStations(station_table);
		tracker_->journal() >> journal_ping;
	}
	if (!tracker_)
		return;
	tracker_->configure(tracking);
	tracker_->useCatalogue(catalogueFor(cfg));
	if (made)
	{
		backup_file = cfg.backup_file;
		restoreInto();
		last_backup = std::time(nullptr);
		tracker_->setStations(stations());
		connect();
	}
	else
		// the backup file is read when the tracker is made: another one waits for a restart
		tracker_->setRestartRequired("Backup file (service restart)", cfg.backup_file != backup_file);
}

// under site_mtx
void Site::configureServers(const SiteConfig &cfg)
{
	// A server's own tracker is known by what it keeps; one no server names goes before a new one is made.
	const auto reading = [&](const ServerSpec &spec) -> Own * {
		for (auto &o : own)
			if (spec.tracking.reads(o->spec))
				return o.get();
		return nullptr;
	};
	const auto trackerOf = [&](const ServerSpec &spec) -> Tracker * {
		if (!spec.tracking.own)
			return tracker_.get();
		Own *o = reading(spec);
		return o ? o->tracker.get() : nullptr;
	};

	// A server on the same port, address and tracker takes the new settings in place; the others close first.
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

	for (auto it = own.begin(); it != own.end();)
	{
		Own &o = **it;
		if (std::any_of(cfg.servers.begin(), cfg.servers.end(), [&](const ServerSpec &spec) { return spec.tracking.own && spec.tracking.reads(o.spec); }))
		{
			++it;
			continue;
		}
		// off the receivers before it is saved and goes
		const std::unique_ptr<Own> gone = std::move(*it);
		it = own.erase(it);
		connect();
		if (!gone->backup_file.empty() && !gone->tracker->save(gone->backup_file))
			Error() << "Server on port " << gone->port << ": backup to " << gone->backup_file << " failed";
	}
	std::vector<Own *> made;
	for (const auto &spec : cfg.servers)
	{
		if (!spec.tracking.own)
			continue;
		if (Own *o = reading(spec))
		{
			// a server with the filter of one just made, and no way of its own to keep the ships, reads that one
			if (std::find(made.begin(), made.end(), o) != made.end())
				Info() << "Server on port " << spec.port << ": the ships of port " << o->port;
			continue;
		}
		own.push_back(makeOwn(spec, cfg));
		made.push_back(own.back().get());
		connect();
	}
	launch();
	for (std::size_t i = 0; i < cfg.servers.size(); i++)
	{
		if (kept[i])
		{
			servers.push_back(std::move(kept[i]));
			continue;
		}
		std::unique_ptr<Services::Server> s(new Services::Server(cfg.servers[i], *trackerOf(cfg.servers[i]),
																 [this](const ServerSpec &) { return stations(); },
																 [this] { return systemFacts(); }));
		if (s->open())
			servers.push_back(std::move(s));
	}
}
#endif

#ifdef HASDELIVERY
// under site_mtx; once made, the webhooks and the MQTT state are kept
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
			mqtt_state.reset(new MqttState(*tracker_, [this] { return systemFacts(); }, [this] { return stations(); }));
		mqtt_state->configure(cfg.mqtt_state);
	}
	events_wanted = mqtt_state && cfg.mqtt_state.any();
}
#endif

#ifdef HASAPI
void Site::restoreInto()
{
	backing_up = !backup_file.empty() && tracker_->open(backup_file);
}

// under site_mtx
std::shared_ptr<PlaceCatalogue> Site::catalogueFor(const SiteConfig &cfg)
{
	if (cfg.catalogue)
		return cfg.catalogue;
	if (cfg.places != places_dir)
	{
		places_dir = cfg.places;
		places_catalogue = cfg.places.empty() ? nullptr : std::make_shared<PlaceCatalogue>(cfg.places);
	}
	return places_catalogue;
}

// under site_mtx; the config reader already gave its settings what the viewer needs kept
std::unique_ptr<Site::Own> Site::makeOwn(const ServerSpec &spec, const SiteConfig &cfg)
{
	std::unique_ptr<Own> o(new Own());
	o->port = spec.port;
	o->spec = spec.tracking;
	o->tracker.reset(new Tracker());
	o->tracker->useStations(station_table);
	o->tracker->configure(spec.tracking.tracking);
	AIS::Filter filter;
	for (const auto &term : spec.tracking.filter)
		filter.SetOptionKey((AIS::Keys)term.first, term.second);
	o->tracker->setFilter(filter);
	o->tracker->useCatalogue(catalogueFor(cfg));
	o->backup_file = spec.tracking.backup_file;
	if (!o->backup_file.empty() && !o->tracker->open(o->backup_file))
		o->backup_file.clear();
	std::string of;
	for (int id : spec.stations)
		of += (of.empty() ? std::string(spec.stations.size() > 1 ? ", stations " : ", station ") : ", ") + std::to_string(id);
	Info() << "Server on port " << spec.port << ": own ships" << of << (o->backup_file.empty() ? std::string() : ", kept in " + o->backup_file);
	o->tracker->setStations(stations());
	return o;
}

// a server whose port was taken is not among them
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

std::string Site::stationsLive()
{
	std::string out;
	JSON::Writer w(out);
	const std::time_t now = std::time(nullptr);
	Tracker *t = tracker();
	w.beginObject().key("stations").beginArray();
	stations().forEach([&](const StationRegistry::Station &s) {
		w.beginObject().kv("id", s.id);
		if (t)
		{
			const Tracker::Position at = t->stationPosition(s.id);
			const char *source = StationTable::sourceName(at.source);
			if (source)
				w.kv("source", source);
			if (isValidCoord(at.lat, at.lon))
				w.kv("lat", at.lat).kv("lon", at.lon);
			if (at.time)
				w.kv("fix_age_s", (long long)(now - at.time));
			const std::time_t heard = t->lastHeard(s.id);
			if (heard)
				w.kv("heard_age_s", (long long)(now - heard));
			if (s.mmsi)
				t->ships().withShip(s.mmsi, [&](const Ship &ship, int) {
					w.key("vessel").beginObject().kv("mmsi", (long long)s.mmsi).kv("name", std::string(ship.shipname)).kv("report_age_s", (long long)(now - ship.last_signal)).endObject();
				});
		}
		w.endObject();
	});
	w.endArray();
	if (const std::time_t saved = lastBackup())
		w.kv("backup_age_s", (long long)(now - saved));
	const SystemFacts facts = systemFacts();
	w.kv("running", facts.running).key("receivers").beginArray();
	for (const auto &r : facts.receivers)
	{
		w.beginObject().kv("index", r.index).kv("station", r.station).kv("input", r.input).kv("label", r.label).kv("state", r.status()).kv("messages", (unsigned long long)r.messages).kv("per_minute", (unsigned long long)r.per_minute);
		if (t)
		{
			const std::time_t heard = t->lastHeard(r.station);
			if (heard)
				w.kv("heard_age_s", (long long)(now - heard));
		}
		w.endObject();
	}
	w.endArray();
	w.endObject();
	w.finish();
	return out;
}

std::vector<std::pair<int, std::string>> Site::receiverStates()
{
	std::vector<std::pair<int, std::string>> states;
	for (const auto &r : systemFacts().receivers)
		states.emplace_back(r.index, r.status());
	return states;
}

Services::Server *Site::serverOn(int port)
{
	for (auto &s : servers)
		if (s->spec().port == port)
			return s.get();
	return nullptr;
}

// where a request made on this machine reaches the server on a port
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

// under site_mtx
void Site::backup()
{
#ifdef HASAPI
	if (tracker_ && backing_up)
	{
		const bool ok = tracker_->save(backup_file);
		if (!ok)
			Error() << "Tracker: backup to " << backup_file << " failed";
		if (!ok && !backup_failing)
			systemEventLocked(Tracking::Level::URGENT, "backup_failed", "The backup to " + backup_file + " failed");
		backup_failing = !ok;
	}
	for (auto &o : own)
		if (!o->backup_file.empty() && !o->tracker->save(o->backup_file))
			Error() << "Server on port " << o->port << ": backup to " << o->backup_file << " failed";
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
	if (backup_minutes > 0 && now - last_backup >= backup_minutes * 60)
	{
		backup();
		last_backup = now;
	}
#ifdef HASDELIVERY
	if (mqtt_state)
	{
		mqtt_state->poll(now);
		if (mqtt_state->busy())
			wait = 1;
	}
#endif
#else
	(void)now;
#endif
	return wait;
}

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
