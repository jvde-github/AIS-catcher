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


#include "Tracker.h"

#include <cmath>
#include "Logger.h"

namespace
{
	const uint32_t TRACKER_MAGIC = 0x54524b52; // "TRKR"
	const uint32_t TRACKER_VERSION = 2; // 2 adds the stations' last fixes
}

Tracker::Tracker()
{
	router.tracker = this;
	intake.tracker = gps_intake.tracker = adsb_intake.tracker = this;
	db >> router;
	db.events >> events;
	// called with the ship table locked: the figures' lock is never taken the other way round
	db.setStationHeard([this](int id) {
		std::lock_guard<std::mutex> lock(figures_mtx);
		auto it = last_heard.find(id);
		return it == last_heard.end() ? (std::time_t)0 : it->second;
	});
	db.setup();
}

namespace
{
	const char *const STORAGE_RESTART = "Tracking storage (service restart)";

	// the settings that size the table
	bool sameStorage(const TrackingConfig &a, const TrackingConfig &b)
	{
		return a.server_mode == b.server_mode && a.track_memory == b.track_memory && a.max_ships == b.max_ships;
	}
}

void Tracker::configure(const TrackingConfig &next)
{
	{
		std::lock_guard<std::mutex> lock(config_mtx);
		const bool first = !configured;
		TrackingConfig applied = next;
		// the table keeps the size it was made with: another size waits for a restart, and is
		// reported for as long as the configuration asks it
		if (!first)
		{
			applied.server_mode = cfg.server_mode;
			applied.track_memory = cfg.track_memory;
			applied.max_ships = cfg.max_ships;
		}
		const bool waits = !sameStorage(applied, next);
		restart_required.erase(std::remove(restart_required.begin(), restart_required.end(), STORAGE_RESTART), restart_required.end());
		if (waits)
			restart_required.push_back(STORAGE_RESTART);
		cfg = applied;
		configured = true;

		const auto live = [&] {
			db.setConfigPosition(applied.lat, applied.lon, applied.use_gps);
			db.setShareLatLon(applied.latlon_share);
			db.setMsgSave(applied.msg_save);
			db.setOwnMMSI(applied.own_mmsi);
			db.setTimeHistory(applied.time_history);
			db.setTrackTime(applied.track_time);
			db.setExpireFields(applied.expire_fields);
		};
		if (first)
		{
			live();
			db.setServerMode(applied.server_mode);
			db.setTrackMemory(applied.track_memory);
			db.setMaxShips(applied.max_ships);
			db.setup();
		}
		else
			db.configure(live);
	}

	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		fixed_at.lat = next.fixed ? next.lat : LAT_UNDEFINED;
		fixed_at.lon = next.fixed ? next.lon : LON_UNDEFINED;
	}
	std::lock_guard<std::mutex> lock(figures_mtx);
	cutoff = next.cutoff > 0 ? next.cutoff : LONG_RANGE_CUTOFF_DEFAULT;
	figures.setCutoff(cutoff);
	for (auto &s : station_figures)
		s.second->setCutoff(cutoff);
}

void Tracker::setRestartRequired(const std::string &what, bool pending)
{
	std::lock_guard<std::mutex> lock(config_mtx);
	auto it = std::find(restart_required.begin(), restart_required.end(), what);
	if (pending && it == restart_required.end())
		restart_required.push_back(what);
	else if (!pending && it != restart_required.end())
		restart_required.erase(it);
}

// what the ship table took, behind its filter: the figures, then the taps
void Tracker::Router::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	{
		std::lock_guard<std::mutex> lock(tracker->figures_mtx);
		tracker->figures.Receive(data, len, tag);
		// a message of no station counts in the figures of everything only
		const int id = tag.station_id;
		if (id >= 0)
		{
			tracker->last_heard[id] = std::time(nullptr);
			auto it = tracker->station_figures.find(id);
			if (it == tracker->station_figures.end() && tracker->station_figures.size() < MAX_STATIONS)
			{
				it = tracker->station_figures.emplace(id, std::unique_ptr<Figures>(new Figures())).first;
				it->second->setCutoff(tracker->cutoff);
			}
			if (it != tracker->station_figures.end())
				it->second->Receive(data, len, tag);
		}
	}
	std::lock_guard<std::mutex> lock(tracker->taps_mtx);
	for (auto *tap : tracker->taps)
		tap->Receive(data, len, tag);
}

void Tracker::usePlaces(const std::string &directory)
{
	std::shared_ptr<PlaceCatalogue> next;
	{
		std::lock_guard<std::mutex> lock(catalogue_mtx);
		if (directory == places_dir)
			return;
		places_dir = directory;
	}
	// read outside the lock: a large catalogue takes a while
	if (!directory.empty())
		next = std::make_shared<PlaceCatalogue>(directory);
	{
		std::lock_guard<std::mutex> lock(catalogue_mtx);
		catalogue = next;
	}
	setPlaces(next ? next->snapshot() : nullptr);
}

void Tracker::useCatalogue(std::shared_ptr<PlaceCatalogue> shared)
{
	{
		std::lock_guard<std::mutex> lock(catalogue_mtx);
		if (shared == catalogue && places_dir.empty())
			return;
		places_dir.clear();
		catalogue = shared;
	}
	setPlaces(shared ? shared->snapshot() : nullptr);
}

// the ship table judges against these places, and the journal names a crossing's place from them
void Tracker::setPlaces(std::shared_ptr<const PlaceIndex> index)
{
	events.setPlaces(index);
	db.setPlaces(index);
}

void Tracker::refreshPlaces()
{
	// the DB compares editions: an unchanged catalogue costs nothing
	if (const std::shared_ptr<PlaceCatalogue> c = placeCatalogue())
		setPlaces(c->snapshot());
}

const char *Tracker::sourceName(Position::Source s)
{
	switch (s)
	{
	case Position::CONFIGURED: return "configured";
	case Position::GPS: return "gps";
	case Position::VESSEL: return "vessel";
	default: return nullptr;
	}
}

void Tracker::Intake::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	// a copy: the receiver's tag goes on to other readers as it came
	TAG t = tag;
	tracker->place((const AIS::Message *)data[0].binary, t);
	tracker->db.Receive(data, len, t);

	// a position report moves a station riding its vessel
	const AIS::Message *msg = (const AIS::Message *)data[0].binary;
	if (!msg || !isValidCoord(t.lat, t.lon))
		return;
	switch (msg->type())
	{
	case 1: case 2: case 3: case 9: case 18: case 19: case 27:
		tracker->follow(msg->mmsi(), t.lat, t.lon);
	}
}

void Tracker::GpsIntake::Receive(const AIS::GPS *data, int len, TAG &tag)
{
	if (!len || !tracker->moveByGPS(tag.station_id, data[0].getLat(), data[0].getLon()))
		tracker->db.Receive(data, len, tag);
}

void Tracker::AdsbIntake::Receive(const Plane::ADSB *data, int len, TAG &tag)
{
	TAG t = tag;
	tracker->place(nullptr, t);
	tracker->plane_db.Receive(data, len, t);
}

void Tracker::place(const AIS::Message *msg, TAG &tag)
{
	std::lock_guard<std::mutex> lock(stations_mtx);
	// a message from a station the configuration lists, as a feed from a hub carries them, counts there
	if (msg && msg->getStation() >= 0 && msg->getStation() != tag.station_id && stations.count(msg->getStation()))
		tag.station_id = msg->getStation();
	if (isValidCoord(fixed_at.lat, fixed_at.lon))
	{
		tag.station_lat = fixed_at.lat;
		tag.station_lon = fixed_at.lon;
		return;
	}
	auto it = stations.find(tag.station_id);
	if (it != stations.end() && isValidCoord(it->second.now.lat, it->second.now.lon))
	{
		tag.station_lat = it->second.now.lat;
		tag.station_lon = it->second.now.lon;
	}
}

// A fix moves the stations that follow GPS and take it: the one its input
// names, and any that no input names, which take GPS from every input.
bool Tracker::moveByGPS(int id, FLOAT32 lat, FLOAT32 lon)
{
	std::vector<int> moved;
	bool known = false;
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		known = stations.count(id) > 0;
		if (!isValidCoord(lat, lon))
			return known;
		const std::time_t now = std::time(nullptr);
		for (auto &s : stations)
		{
			if (!s.second.followsGPS() || (s.first != id && s.second.inputs > 0))
				continue;
			Position &p = s.second.now;
			p.lat = lat;
			p.lon = lon;
			p.source = Position::GPS;
			p.time = now;
			moved.push_back(s.first);
		}
	}
	if (!moved.empty())
		publish(moved);
	return known || !moved.empty();
}

void Tracker::follow(uint32_t mmsi, FLOAT32 lat, FLOAT32 lon)
{
	int id = 0;
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		if (riders.empty())
			return;
		auto r = riders.find(mmsi);
		if (r == riders.end())
			return;
		id = r->second;
		Position &p = stations[id].now;
		p.lat = lat;
		p.lon = lon;
		p.source = Position::VESSEL;
		p.time = std::time(nullptr);
	}
	publish({id});
}

// The map's stations where they are now. A moving station is told again
// only once it has moved some fifty metres, so a GPS fix a second does not
// turn every poll of the map into a row per station.
void Tracker::publish(std::vector<int> moved)
{
	std::vector<StationRegistry::Station> out;
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		for (int id : moved)
		{
			const StationRegistry::Station *s = shown.find(id);
			auto it = stations.find(id);
			if (!s || it == stations.end())
				continue;
			const Position &p = it->second.now;
			if (std::fabs(s->lat - p.lat) < 0.0005f && std::fabs(s->lon - p.lon) < 0.0005f)
				continue;
			StationRegistry::Station copy = *s;
			copy.lat = p.lat;
			copy.lon = p.lon;
			shown.define(copy);
			out.push_back(copy);
		}
	}
	for (const auto &s : out)
		db.setStation(s);
}

void Tracker::setStations(const StationRegistry &registry)
{
	std::vector<StationRegistry::Station> list;
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		applyStations(registry);
		// the map shows each where it is now
		shown = StationRegistry();
		for (const auto &s : registry.ordered())
		{
			// a station that keeps its position home is not on the map at all
			if (!s.share_loc)
				continue;
			StationRegistry::Station copy = s;
			auto it = stations.find(s.id);
			if (it != stations.end() && isValidCoord(it->second.now.lat, it->second.now.lon))
			{
				copy.lat = it->second.now.lat;
				copy.lon = it->second.now.lon;
			}
			copy.online = true;
			shown.define(copy);
			list.push_back(copy);
		}
	}
	db.setStations(list);
	remeasure();
}

void Tracker::applyStations(const StationRegistry &registry)
{
	std::map<int, Station> next;
	riders.clear();
	registry.forEach([&](const StationRegistry::Station &s) {
		Station &n = next[s.id];
		n.lat = s.lat;
		n.lon = s.lon;
		n.mmsi = s.mmsi;
		n.gps = s.gps;
		n.share_loc = s.share_loc;
		n.inputs = s.inputs;
		if (s.mmsi)
			riders[s.mmsi] = s.id;
		// a fix outlives a new configuration while its source still applies
		auto old = stations.find(s.id);
		if (old != stations.end() && ((old->second.now.source == Position::GPS && n.followsGPS()) ||
									  (old->second.now.source == Position::VESSEL && s.mmsi && s.mmsi == old->second.mmsi)))
			n.now = old->second.now;
		else if (isValidCoord(s.lat, s.lon))
		{
			n.now.lat = s.lat;
			n.now.lon = s.lon;
			n.now.source = Position::CONFIGURED;
		}
	});
	stations.swap(next);
	registry_set = true;
}

// what a restored ship, or one heard before a move, is measured from: where
// the station that last heard it is now
void Tracker::remeasure()
{
	std::map<int, std::pair<FLOAT32, FLOAT32>> at;
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		for (const auto &s : stations)
			if (isValidCoord(s.second.now.lat, s.second.now.lon))
				at[s.first] = std::make_pair(s.second.now.lat, s.second.now.lon);
	}
	if (at.empty())
		return;
	db.measureFrom([&](const Ship &ship, float &lat, float &lon) {
		if (!ship.heard[0].last)
			return false;
		auto it = at.find(ship.heard[0].station);
		if (it == at.end())
			return false;
		lat = it->second.first;
		lon = it->second.second;
		return true;
	});
}

Tracker::Position Tracker::stationPosition(int id)
{
	std::lock_guard<std::mutex> lock(stations_mtx);
	auto it = stations.find(id);
	return it == stations.end() || !it->second.share_loc ? Position() : it->second.now;
}

Tracker::Position Tracker::stationPositionPrivate(int id)
{
	std::lock_guard<std::mutex> lock(stations_mtx);
	auto it = stations.find(id);
	return it == stations.end() ? Position() : it->second.now;
}


bool Tracker::save(const std::string &file)
{
	const std::string tmp = file + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f)
			return false;
		bool ok;
		{
			std::lock_guard<std::mutex> lock(figures_mtx);
			const uint32_t n = (uint32_t)station_figures.size();
			ok = f.write((const char *)&TRACKER_MAGIC, sizeof(TRACKER_MAGIC)) &&
				 f.write((const char *)&TRACKER_VERSION, sizeof(TRACKER_VERSION)) &&
				 figures.save(f) && f.write((const char *)&n, sizeof(n));
			for (auto &s : station_figures)
			{
				if (!ok)
					break;
				const int32_t id = s.first;
				ok = f.write((const char *)&id, sizeof(id)) && s.second->save(f);
			}
		}
		ok = ok && db.Save(f) && events.save(f);
		if (ok)
		{
			std::lock_guard<std::mutex> lock(stations_mtx);
			uint32_t fixes = 0;
			for (const auto &s : stations)
				fixes += s.second.now.source == Position::GPS || s.second.now.source == Position::VESSEL;
			ok = (bool)f.write((const char *)&fixes, sizeof(fixes));
			for (const auto &s : stations)
			{
				const Position &p = s.second.now;
				if (!ok || (p.source != Position::GPS && p.source != Position::VESSEL))
					continue;
				const int32_t id = s.first;
				const uint32_t mmsi = s.second.mmsi;
				const uint8_t source = p.source;
				const int64_t time = p.time;
				ok = f.write((const char *)&id, sizeof(id)) && f.write((const char *)&p.lat, sizeof(p.lat)) &&
					 f.write((const char *)&p.lon, sizeof(p.lon)) && f.write((const char *)&source, sizeof(source)) &&
					 f.write((const char *)&time, sizeof(time)) && f.write((const char *)&mmsi, sizeof(mmsi));
			}
		}
		if (!ok)
			return false;
	}
	return std::rename(tmp.c_str(), file.c_str()) == 0;
}

Tracker::Restored Tracker::restore(const std::string &file)
{
	std::ifstream f(file, std::ios::binary);
	if (!f)
		return MISSING;
	uint32_t magic = 0, version = 0, n = 0;
	if (!f.read((char *)&magic, sizeof(magic)) || magic != TRACKER_MAGIC)
		return FOREIGN;
	if (!f.read((char *)&version, sizeof(version)) || version < 1 || version > TRACKER_VERSION)
	{
		Warning() << "Tracker: " << file << " is a backup of a later version";
		return DAMAGED;
	}
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		if (!figures.load(f) || !f.read((char *)&n, sizeof(n)))
			return DAMAGED;
		station_figures.clear();
		for (uint32_t i = 0; i < n; i++)
		{
			int32_t id = 0;
			std::unique_ptr<Figures> s(new Figures());
			s->setCutoff(cutoff);
			if (!f.read((char *)&id, sizeof(id)) || !s->load(f))
				return DAMAGED;
			// -1 is no station: its figures are not kept apart
			if (id >= 0 && station_figures.size() < MAX_STATIONS)
				station_figures[id] = std::move(s);
		}
	}
	if (!db.Load(f))
	{
		Warning() << "Tracker: ships could not be restored from " << file;
		return RESTORED;
	}
	if (!events.load(f))
	{
		Warning() << "Tracker: events could not be restored from " << file;
		return RESTORED;
	}
	uint32_t fixes = 0;
	if (version < 2 || !f.read((char *)&fixes, sizeof(fixes)))
		return RESTORED;
	std::lock_guard<std::mutex> lock(stations_mtx);
	for (uint32_t i = 0; i < fixes; i++)
	{
		int32_t id = 0;
		uint32_t mmsi = 0;
		uint8_t source = 0;
		int64_t time = 0;
		Position p;
		if (!f.read((char *)&id, sizeof(id)) || !f.read((char *)&p.lat, sizeof(p.lat)) || !f.read((char *)&p.lon, sizeof(p.lon)) ||
			!f.read((char *)&source, sizeof(source)) || !f.read((char *)&time, sizeof(time)) || !f.read((char *)&mmsi, sizeof(mmsi)))
			break;
		if ((source != Position::GPS && source != Position::VESSEL) || !isValidCoord(p.lat, p.lon))
			continue;
		p.source = (Position::Source)source;
		p.time = (std::time_t)time;
		// before the registry is known the fix waits in its entry; after, only where it still applies
		auto it = stations.find(id);
		if (it == stations.end())
		{
			if (registry_set)
				continue;
			it = stations.emplace(id, Station()).first;
			it->second.mmsi = mmsi;
		}
		else if (!((source == Position::GPS && it->second.followsGPS()) || (source == Position::VESSEL && it->second.mmsi == mmsi)))
			continue;
		it->second.now = p;
	}
	return RESTORED;
}

bool Tracker::importViewerBackup(const std::string &file)
{
	std::ifstream f(file, std::ios::binary);
	if (!f)
		return false;
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		// a file that breaks off leaves no half-read figures behind
		if (!figures.load(f))
		{
			figures.clear();
			return false;
		}
	}
	if (f.peek() != EOF && !db.Load(f))
		Warning() << "Tracker: ships of the viewer's backup could not be read";
	return true;
}
