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

#include "Stations.h"
#include "Logger.h"
#include "Site.h"

namespace Stations
{
	typedef std::vector<std::unique_ptr<Receiver>> Receivers;

	// how many receivers name each station: one no receiver names takes GPS from every input
	static void countInputs(StationRegistry &stations, const Receivers &receivers)
	{
		StationRegistry counted;
		for (auto s : stations.ordered())
		{
			s.inputs = 0;
			for (const auto &r : receivers)
				s.inputs += r->getStation() == s.id;
			counted.define(s);
		}
		stations = counted;
	}

	// The default station the configuration does not list, as a server section
	// (-N, "server", "services") describes it (STATION, LAT, LON, SHARE_LOC,
	// USE_GPS, OWN_MMSI): named after the one receiver standing on it when that
	// has a name of its own, else as the section says, else not at all; placed
	// where the first such receiver with a position of its own is, else where the
	// section puts it. It rides a vessel only when it is the only station (alone).
	static StationRegistry::Station defaultStation(const StationDefaults &defaults, const Receivers &receivers, bool alone)
	{
		StationRegistry::Station s;
		s.id = 0;
		s.share_loc = defaults.share_loc;
		s.gps = defaults.gps;
		s.mmsi = alone ? defaults.mmsi : 0;
		s.alt = defaults.alt;
		std::size_t unnamed = 0;
		for (const auto &r : receivers)
		{
			if (r->getStation() != 0)
				continue;
			if (++unnamed == 1)
				s.name = r->getDescription();
			else
				s.name.clear();
			if (!isValidCoord(s.lat, s.lon) && r->hasPosition())
			{
				s.lat = r->getLat();
				s.lon = r->getLon();
			}
		}
		if (s.name.empty())
			s.name = defaults.name;
		if (!isValidCoord(s.lat, s.lon) && defaults.placed())
		{
			s.lat = defaults.lat;
			s.lon = defaults.lon;
		}
		return s;
	}

	void resolve(StationRegistry &stations, const StationDefaults &defaults, const Receivers &receivers, Site *site)
	{
		bool configured = false;
		stations.forEach([&](const StationRegistry::Station &) { configured = true; });
		bool at_default = false;
		for (const auto &r : receivers)
			at_default |= r->getStation() == 0;
		if (at_default && !stations.find(0))
			stations.define(defaultStation(defaults, receivers, !configured));
		for (std::size_t k = 0; k < receivers.size(); k++)
		{
			Receiver &r = *receivers[k];
			const int id = r.getStation();
			// validate() saw to it that the configuration defines it
			const StationRegistry::Station *found = stations.find(id);
			if (!found)
				throw std::logic_error("station " + std::to_string(id) + " was not resolved");
			StationRegistry::Station s = *found;
			if (isValidCoord(s.lat, s.lon))
			{
				if (r.hasPosition() && (r.getLat() != s.lat || r.getLon() != s.lon))
					Warning() << "Receiver " << k + 1 << ": the position of station " << id
							  << " is used, not the receiver's own";
			}
			else if (r.hasPosition())
			{
				s.lat = r.getLat();
				s.lon = r.getLon();
				stations.define(s);
			}
			// once stations are configured, the receivers' messages say which station heard them
			r.placeAtStation(s.lat, s.lon, configured);
		}
		countInputs(stations, receivers);
		site->publishStations(stations);
		if (receivers.size() < 2 && !configured)
			return;
		for (std::size_t k = 0; k < receivers.size(); k++)
		{
			const Receiver &r = *receivers[k];
			const StationRegistry::Station *s = stations.find(r.getStation());
			Info() << "Receiver " << k + 1 << " (" << receiverLabel(receivers, k) << "): station "
				   << r.getStation() << " \"" << (s ? s->name : std::string()) << "\"" << (r.hasPosition() ? "" : ", no position");
		}
	}

	void refresh(StationRegistry &stations, const StationRegistry &configured, const Receivers &receivers, Site *site)
	{
		const auto named = [&](int id) {
			for (const auto &r : receivers)
				if (r->getStation() == id)
					return true;
			return false;
		};
		StationRegistry next;
		for (auto s : configured.ordered())
		{
			// a configured station without a position keeps the one its receiver gave it
			const StationRegistry::Station *had = stations.find(s.id);
			if (!isValidCoord(s.lat, s.lon) && had)
			{
				s.lat = had->lat;
				s.lon = had->lon;
			}
			next.define(s);
		}
		for (const auto &s : stations.ordered())
			if (!configured.find(s.id) && named(s.id))
				next.define(s);
		stations = next;
		countInputs(stations, receivers);
		if (site)
			site->publishStations(stations);
	}

	void validate(const StationRegistry &stations, const Receivers &receivers)
	{
		for (std::size_t k = 0; k < receivers.size(); k++)
		{
			const int id = receivers[k]->getStation();
			if (id != 0 && !stations.find(id))
				throw std::runtime_error("Receiver " + std::to_string(k + 1) + " names station " + std::to_string(id) + ", which \"stations\" does not define.");
		}
	}
}
