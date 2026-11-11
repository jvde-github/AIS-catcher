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

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include "Helper.h"
#include "Logger.h"

namespace
{
	const uint32_t TRACKER_MAGIC = 0x54524b52;
	const uint32_t TRACKER_VERSION = 3; // 2 adds the stations' last fixes, 3 each figures' week and levels
}

Tracker::Tracker()
{
	router.tracker = this;
	intake.tracker = gps_intake.tracker = adsb_intake.tracker = this;
	db >> router;
	db.events >> events;
	// called with the ship table locked: the figures' lock is never taken the other way round
	db.setStationHeard([this](int id) { return lastHeard(id); });
	db.setStationPlaced([this](int id, FLOAT32 lat, FLOAT32 lon) { table->place(id, lat, lon); });
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
			db.setMsgSave(applied.msg_save);
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
			// figures of its own for a listed station: a feed naming hundreds stays bounded
			if (it == tracker->station_figures.end() && tracker->listed.count(id) && tracker->station_figures.size() < MAX_STATIONS)
			{
				it = tracker->station_figures.emplace(id, std::unique_ptr<Figures>(new Figures())).first;
				it->second->setCutoff(tracker->cutoff);
			}
			if (it != tracker->station_figures.end())
				it->second->Receive(data, len, tag);
		}
	}
}

void Tracker::usePlaces(const std::string &directory)
{
	std::shared_ptr<PlaceCatalogue> next;
	{
		std::lock_guard<std::mutex> lock(catalogue_mtx);
		if (directory == places_dir && !(directory.empty() && catalogue))
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

void Tracker::Intake::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	// a copy: the receiver's tag goes on to other readers as it came
	TAG t = tag;
	const AIS::Message *msg = (const AIS::Message *)data[0].binary;
	tracker->place(msg, t);
	// the filter once: what passes reaches the table, then, with the position
	// and distance the table wrote to the tag, the readers of the stream
	const DB::Admission admission = tracker->db.admit(msg, t);
	if (admission == DB::DROP)
		return;
	if (admission == DB::COUNT)
		tracker->db.count(data, len, t);
	else
		tracker->db.Receive(data, len, t);
	{
		std::lock_guard<std::mutex> lock(tracker->taps_mtx);
		for (auto *tap : tracker->taps)
			tap->Receive(data, len, t);
	}

	// a position report moves a station riding its vessel
	if (!msg || !isValidCoord(t.lat, t.lon))
		return;
	switch (msg->type())
	{
	case 1: case 2: case 3: case 9: case 18: case 19: case 27:
		tracker->moved(tracker->table->follow(msg->mmsi(), t.lat, t.lon));
	}
}

void Tracker::GpsIntake::Receive(const AIS::GPS *data, int len, TAG &tag)
{
	if (!len)
		return;
	std::vector<int> ids;
	tracker->table->moveByGPS(tag.station_id, data[0].getLat(), data[0].getLon(), ids);
	for (int id : ids)
		tracker->moved(id);
}

void Tracker::AdsbIntake::Receive(const Plane::ADSB *data, int len, TAG &tag)
{
	TAG t = tag;
	tracker->place(nullptr, t);
	tracker->plane_db.Receive(data, len, t);
}

void Tracker::place(const AIS::Message *msg, TAG &tag)
{
	// a message from a station the configuration lists, as a feed from a hub carries them, counts
	// there; one a foreign feed names stays at its input's, so no station appears unasked
	const Position at = table->locate(msg ? msg->getStation() : -1, tag.station_id);
	if (isValidCoord(at.lat, at.lon))
	{
		tag.station_lat = at.lat;
		tag.station_lon = at.lon;
	}
}

void Tracker::moved(int id)
{
	if (id < 0)
		return;
	const Position at = table->at(id);
	db.moveStation(id, at.lat, at.lon);
}

void Tracker::setStations(const StationRegistry &registry)
{
	std::vector<StationRegistry::Station> list;
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		listed.clear();
		for (const auto &s : registry.ordered())
			listed.insert(s.id);
		for (auto it = station_figures.begin(); it != station_figures.end();)
			if (listed.count(it->first))
				++it;
			else
				it = station_figures.erase(it);
	}
	// the map shows each where it is now
	for (StationRegistry::Station s : registry.ordered())
	{
		const Position at = table->at(s.id);
		if (isValidCoord(at.lat, at.lon))
		{
			s.lat = at.lat;
			s.lon = at.lon;
		}
		s.online = true;
		list.push_back(s);
	}
	db.setStations(list);
	remeasure();
}

// what a restored ship, or one heard before a move, is measured from: where
// the station that last heard it is now. Any other has no distance. A server
// decides what of it to show.
void Tracker::remeasure()
{
	const std::map<int, std::pair<FLOAT32, FLOAT32>> at = table->placed();
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



bool Tracker::save(const std::string &file)
{
	const std::string tmp = file + ".tmp";
	if (!Util::Helper::createTemp(tmp, file))
		return false;
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
				 figures.save(f) && figures.saveProfile(f) && f.write((const char *)&n, sizeof(n));
			for (auto &s : station_figures)
			{
				if (!ok)
					break;
				const int32_t id = s.first;
				ok = f.write((const char *)&id, sizeof(id)) && s.second->save(f) && s.second->saveProfile(f);
			}
		}
		// the stations' last fixes ride in the backup
		if (!ok || !db.Save(f) || !events.save(f) || !table->save(f))
			return false;
	}
#ifdef _WIN32
	std::remove(file.c_str());
#endif
	return std::rename(tmp.c_str(), file.c_str()) == 0;
}

// an existing `to` is kept: a copy made on an earlier start is the one of
// before the upgrade
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

// why a backup that is not simply there yet cannot be read, nothing otherwise
static std::string backupProblem(const std::string &file)
{
	FILE *f = fopen(file.c_str(), "rb");
	if (f)
	{
		fclose(f);
		return "";
	}
	if (errno != ENOENT)
		return strerror(errno);
	const std::size_t slash = file.find_last_of("/\\");
	struct stat s;
	if (slash != std::string::npos && slash > 0 && stat(file.substr(0, slash).c_str(), &s) != 0)
		return "its folder cannot be seen: it does not exist, or the service's hardening hides it";
	return "";
}

bool Tracker::open(const std::string &file)
{
	Restored r = restore(file);
	// an earlier version's backup is copied beside it once, then imported and written in this version's format
	bool imported = false;
	if (r == FOREIGN)
	{
		if (!keepCopy(file, file + ".before-upgrade"))
		{
			Error() << "Tracker: " << file << " is of an earlier version and could not be copied beside it: left as it is, not backed up";
			return false;
		}
		imported = importViewerBackup(file);
		if (!imported)
		{
			Error() << "Tracker: cannot import the earlier version's backup " << file;
			return true;
		}
		r = RESTORED;
	}
	if (r == RESTORED)
		Info() << "Tracker: " << db.getCount() << " ships " << (imported ? "imported from " : "restored from ") << file << (imported ? ", of an earlier version, kept as .before-upgrade" : "");
	else
	{
		const std::string problem = backupProblem(file);
		if (problem.empty())
			Info() << "Tracker: no backup yet at " << file;
		else
			Error() << "Tracker: cannot read the backup " << file << ": " << problem;
	}
	return true;
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
		const auto damaged = [&]() {
			figures.clear();
			station_figures.clear();
			return DAMAGED;
		};
		if (!figures.load(f) || (version >= 3 && !figures.loadProfile(f)) || !f.read((char *)&n, sizeof(n)))
			return damaged();
		station_figures.clear();
		for (uint32_t i = 0; i < n; i++)
		{
			int32_t id = 0;
			std::unique_ptr<Figures> s(new Figures());
			s->setCutoff(cutoff);
			if (!f.read((char *)&id, sizeof(id)) || !s->load(f) || (version >= 3 && !s->loadProfile(f)))
				return damaged();
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
	if (version >= 2)
		table->load(f);
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
