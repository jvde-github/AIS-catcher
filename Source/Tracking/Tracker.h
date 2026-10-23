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

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "DB.h"
#include "Figures.h"
#include "Journal.h"
#include "PlaceCatalogue.h"
#include "PlaneDB.h"
#include "StationRegistry.h"
#include "TrackingConfig.h"

// The picture one process keeps of the world it hears: one ship table, in
// which each ship knows the stations that heard it; the figures of everything
// heard and of each station; the planes; and the journal of events. Fed by the
// receivers of every engine session, read through the site's servers.
class Tracker
{
public:
	Tracker();

	// How ships are tracked. The first call sizes the table, before the first
	// message; a later one applies what changes while messages flow, and a
	// change of the table's size waits for a restart, as restartRequired says.
	void configure(const TrackingConfig &cfg);
	// a tracker of a server's own takes what passes a filter on the message stream
	void setFilter(const AIS::Filter &f) { db.setFilter(f); }
	// the settings in force
	TrackingConfig trackingConfig()
	{
		std::lock_guard<std::mutex> lock(config_mtx);
		return cfg;
	}
	// what the configuration asks that waits for a restart, as people read it
	std::vector<std::string> restartRequired()
	{
		std::lock_guard<std::mutex> lock(config_mtx);
		return restart_required;
	}
	// a setting of the site's beside these that waits for a restart, or none
	void setRestartRequired(const std::string &what, bool pending);

	// where receivers deliver; a message is measured from where its station
	// is at that moment, and a GPS fix moves the station it arrived on
	StreamIn<JSON::JSON> &json() { return intake; }
	StreamIn<AIS::GPS> &gps() { return gps_intake; }
	StreamIn<AIS::Control> &control() { return db; }
	StreamIn<Plane::ADSB> &adsb() { return adsb_intake; }

	// Where a station is: as configured, from GPS on its input, or riding a vessel
	struct Position
	{
		enum Source : uint8_t { NONE, CONFIGURED, GPS, VESSEL };
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		Source source = NONE;
		std::time_t time = 0; // of the last fix; 0 when configured
	};
	static const char *sourceName(Position::Source s);

	// Readers of every message as the tracker has it, after it is placed and
	// measured: a server's live stream. Removed before the reader goes.
	void addTap(StreamIn<JSON::JSON> *tap)
	{
		std::lock_guard<std::mutex> lock(taps_mtx);
		taps.push_back(tap);
	}
	void removeTap(StreamIn<JSON::JSON> *tap)
	{
		std::lock_guard<std::mutex> lock(taps_mtx);
		taps.erase(std::remove(taps.begin(), taps.end(), tap), taps.end());
	}

	// the place catalogue in `directory`, read now; empty drops it. Before a
	// backup is loaded, so the visits in it find their places
	void usePlaces(const std::string &directory);
	std::shared_ptr<const PlaceIndex> places() { return db.placeIndex(); }
	// a catalogue someone else edits, as managed mode's place editor does; a
	// new edition reaches the ships with refreshPlaces()
	void useCatalogue(std::shared_ptr<PlaceCatalogue> shared);
	void setPlaces(std::shared_ptr<const PlaceIndex> index);
	void refreshPlaces();
	// every place, as the viewer's places.json lists them
	std::string placesCollection()
	{
		const std::shared_ptr<PlaceCatalogue> c = placeCatalogue();
		return c ? c->collection() : "{\"place_version\":\"\",\"objects\":[]}";
	}
	// one place's file as written, by its number in the index of `version`
	std::string placeFeature(uint32_t number, const std::string &version)
	{
		const std::shared_ptr<PlaceCatalogue> c = placeCatalogue();
		return c ? c->feature(number, version) : "{}";
	}

	// the stations of the registry: positions, the vessels they ride, GPS on or off
	void setStations(const StationRegistry &registry);
	// where a station is now, for those who serve it: nothing for a station
	// that keeps its position home
	Position stationPosition(int id);
	// where a station is now whether it shares that or not: for its owner's
	// control panel only, never for a service
	Position stationPositionPrivate(int id);
	// the vessels stations ride, of the stations that share where they are: MMSI to station id
	std::map<uint32_t, int> riding()
	{
		std::lock_guard<std::mutex> lock(stations_mtx);
		std::map<uint32_t, int> out;
		for (const auto &r : riders)
			if (shown.find(r.second))
				out.insert(r);
		return out;
	}

	// A reader of the messages the ship table kept, as they leave it: after
	// its quality gate, before the figures. Connected before messages flow.
	void connectSink(StreamIn<JSON::JSON> &sink) { db >> sink; }

	DB &ships() { return db; }
	PlaneDB &planes() { return plane_db; }
	Journal &journal() { return events; }

	// messages a second, over the last minute, of everything heard
	float msgRate()
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		return figures.second.getAverage();
	}

	// when a station's last message came; 0 when none has
	std::time_t lastHeard(int id)
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		auto it = last_heard.find(id);
		return it == last_heard.end() ? (std::time_t)0 : it->second;
	}

	// The figures of everything heard, of one station, or of several as one;
	// f runs under the figures' lock. False when none of the stations has any
	// yet. Several stations add up: their counts sum, their extremes take the
	// extreme.
	template <typename F>
	void withAll(F f)
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		f(figures);
	}
	template <typename F>
	bool withStation(int id, F f)
	{
		return withStations(std::vector<int>(1, id), f);
	}
	template <typename F>
	bool withStations(const std::vector<int> &ids, F f)
	{
		std::lock_guard<std::mutex> lock(figures_mtx);
		std::vector<Figures *> parts;
		for (int id : ids)
		{
			auto it = station_figures.find(id);
			if (it != station_figures.end() && std::find(parts.begin(), parts.end(), it->second.get()) == parts.end())
				parts.push_back(it->second.get());
		}
		if (parts.empty())
			return false;
		if (parts.size() == 1)
		{
			f(*parts[0]);
			return true;
		}
		std::unique_ptr<Figures> sum(new Figures());
		sum->combine(parts);
		f(*sum);
		return true;
	}

	void tick(std::time_t now) { db.tick(now); }

	bool save(const std::string &file);
	// what a backup file turned out to be: read, not there, another program's, or damaged
	enum Restored
	{
		RESTORED,
		MISSING,
		FOREIGN,
		DAMAGED
	};
	Restored restore(const std::string &file);
	// a viewer backup of the version before the site's: its overall figures and its ships
	bool importViewerBackup(const std::string &file);

	// figures of their own for at most this many stations; a station past
	// them counts in the figures of everything only
	static const std::size_t MAX_STATIONS = 256;

private:
	struct Router : public StreamIn<JSON::JSON>
	{
		Tracker *tracker = nullptr;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
	} router;

	struct Intake : public StreamIn<JSON::JSON>
	{
		Tracker *tracker = nullptr;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
	} intake;

	struct GpsIntake : public StreamIn<AIS::GPS>
	{
		Tracker *tracker = nullptr;
		void Receive(const AIS::GPS *data, int len, TAG &tag) override;
	} gps_intake;

	struct AdsbIntake : public StreamIn<Plane::ADSB>
	{
		Tracker *tracker = nullptr;
		void Receive(const Plane::ADSB *data, int len, TAG &tag) override;
	} adsb_intake;

	struct Station
	{
		Position now;
		FLOAT32 lat = LAT_UNDEFINED, lon = LON_UNDEFINED; // configured
		uint32_t mmsi = 0;
		bool gps = true, share_loc = false;
		int inputs = 0;
		// one source at a time: the vessel it rides, else GPS when chosen, else where it was put
		bool followsGPS() const { return gps && !mmsi; }
	};

	// the tag a message is measured with: the station where it is now
	// the station the message is counted at: the one it names, when listed, else the input's; and where it was heard
	void place(const AIS::Message *msg, TAG &tag);
	// false when the fix belongs to no station, and the tracker's own position takes it
	bool moveByGPS(int id, FLOAT32 lat, FLOAT32 lon);
	void follow(uint32_t mmsi, FLOAT32 lat, FLOAT32 lon);
	void remeasure();
	void applyStations(const StationRegistry &registry); // under stations_mtx

	std::mutex config_mtx;
	TrackingConfig cfg;
	bool configured = false;
	std::vector<std::string> restart_required;

	// the catalogue is replaced by the panel while servers read it
	std::mutex catalogue_mtx;
	std::string places_dir;
	std::shared_ptr<PlaceCatalogue> catalogue;
	std::shared_ptr<PlaceCatalogue> placeCatalogue()
	{
		std::lock_guard<std::mutex> lock(catalogue_mtx);
		return catalogue;
	}

	std::mutex taps_mtx;
	std::vector<StreamIn<JSON::JSON> *> taps;

	// the stations as the map shows them: the registry's, where each is now
	StationRegistry shown;
	void publish(std::vector<int> moved);
	// when each station last delivered a message
	std::map<int, std::time_t> last_heard;

	std::mutex stations_mtx;
	Position fixed_at; // where every message is measured from, when the configuration fixes it
	std::map<int, Station> stations;
	std::map<uint32_t, int> riders;
	bool registry_set = false;

	DB db;
	PlaneDB plane_db;
	Journal events;

	std::mutex figures_mtx;
	Figures figures;
	std::map<int, std::unique_ptr<Figures>> station_figures;
	int cutoff = LONG_RANGE_CUTOFF_DEFAULT;
};
