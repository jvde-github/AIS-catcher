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


#include "ApiV1.h"
#include "JournalJSON.h"
#include "Routes.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <tuple>

#include "Geodesy.h"
#include "Keys.h"
#include "Parser.h"
#ifdef HASPORTS
#include "libport.h"
#endif

namespace Services
{
	namespace
	{
		const long long LIST_MAX = 1000;

		bool number(const std::string &s, long long &out)
		{
			if (s.empty() || s.size() > 10 || s.find_first_not_of("0123456789") != std::string::npos)
				return false;
			out = std::atoll(s.c_str());
			return true;
		}

		// a station and where it is now, as far as the site lets it be told:
		// configured, by GPS on its input, or riding its vessel
		void writeStation(JSON::Writer &w, const StationRegistry::Station &s, const Tracker::Position &at)
		{
			w.kv("id", s.id).kv("name", s.name);
			if (isValidCoord(at.lat, at.lon))
				w.kv("lat", at.lat).kv("lon", at.lon).kv("position_source", Tracker::sourceName(at.source));
			else
				w.kv_null("lat").kv_null("lon").kv_null("position_source");
			if (at.time)
				w.kv("position_time", isoTime(at.time));
			else
				w.kv_null("position_time");
			if (s.alt != ALTITUDE_UNDEFINED)
				w.kv("alt_m", s.alt);
			else
				w.kv_null("alt_m");
			if (s.mmsi)
				w.kv("mmsi", (long long)s.mmsi);
			else
				w.kv_null("mmsi");
		}

		const char *const CLASSES[] = {"other", "unknown", "cargo", "class_b", "passenger", "special", "tanker",
										"high_speed", "fishing", "plane", "helicopter", "base_station", "aton", "sart_epirb"};
		const char *const MMSI_TYPES[] = {"other", "class_a", "class_b", "base_station", "sar_aircraft", "sart_epirb", "aton"};
		const char *const NAV_STATUS[] = {"under_way_engine", "at_anchor", "not_under_command", "restricted_manoeuvrability",
										  "constrained_by_draught", "moored", "aground", "fishing", "under_way_sailing",
										  "reserved_hsc", "reserved_wig", "towing_astern", "pushing_ahead", "reserved",
										  "ais_sart_active"};
		const char *const BINARY_KINDS[] = {"text", "persons", "meteo", "aton", "signal", "area", "lock", "safety"};

		template <typename T>
		void kvOr(JSON::Writer &w, const char *key, T v, bool known)
		{
			if (known)
				w.kv(key, v);
			else
				w.kv_null(key);
		}

		// a two-bit field as the ship keeps it: 0 unknown, 1 one way, 2 the other
		void triState(JSON::Writer &w, const char *key, int v, bool when2)
		{
			if (v == 1 || v == 2)
				w.kv(key, (v == 2) == when2);
			else
				w.kv_null(key);
		}

		// What a vessel is now, in full, as the viewer and a client both read it.
		// `badge` is the binary-message badge, `aboard` a station riding it, -1 for none.
		void writeVessel(JSON::Writer &w, const Ship &s, std::time_t now, uint16_t badge = 0, int aboard = -1,
						 const std::function<void(JSON::Writer &)> &extra = nullptr)
		{
			w.beginObject().kv("mmsi", (long long)s.mmsi).kv("name", s.shipname).kv("callsign", s.callsign);
			kvOr(w, "imo", s.IMO, s.IMO != IMO_UNDEFINED);
			w.kv("eni", s.vin).kv("destination", s.destination);
			w.kv("class", s.shipclass >= 0 && s.shipclass < (int)(sizeof(CLASSES) / sizeof(*CLASSES)) ? CLASSES[s.shipclass] : "other");
			kvOr(w, "ship_type", s.shiptype, s.shiptype > 0);
			w.kv("mmsi_type", s.mmsi_type >= 0 && s.mmsi_type < (int)(sizeof(MMSI_TYPES) / sizeof(*MMSI_TYPES)) ? MMSI_TYPES[s.mmsi_type] : "other");
			kvOr(w, "country", s.country_code, s.country_code[0] != 0);

			const bool placed = isValidCoord(s.lat, s.lon);
			kvOr(w, "lat", s.lat, placed);
			kvOr(w, "lon", s.lon, placed);
			kvOr(w, "distance_nm", s.distance, placed && s.distance != DISTANCE_UNDEFINED);
			kvOr(w, "bearing_deg", s.angle, placed && s.distance != DISTANCE_UNDEFINED && s.angle != ANGLE_UNDEFINED);
			kvOr(w, "speed_kn", s.speed, s.speed != SPEED_UNDEFINED);
			kvOr(w, "course_deg", s.cog, s.cog != COG_UNDEFINED);
			kvOr(w, "heading_deg", s.heading, s.heading != HEADING_UNDEFINED);
			kvOr(w, "nav_status", s.status >= 0 && s.status < 15 ? NAV_STATUS[s.status] : "", s.status >= 0 && s.status < 15);

			kvOr(w, "to_bow_m", s.to_bow, s.to_bow != DIMENSION_UNDEFINED);
			kvOr(w, "to_stern_m", s.to_stern, s.to_stern != DIMENSION_UNDEFINED);
			kvOr(w, "to_port_m", s.to_port, s.to_port != DIMENSION_UNDEFINED);
			kvOr(w, "to_starboard_m", s.to_starboard, s.to_starboard != DIMENSION_UNDEFINED);
			kvOr(w, "draught_m", s.draught, s.draught != DRAUGHT_UNDEFINED);
			if (s.month != ETA_MONTH_UNDEFINED || s.day != ETA_DAY_UNDEFINED || s.hour != ETA_HOUR_UNDEFINED || s.minute != ETA_MINUTE_UNDEFINED)
			{
				w.key("eta").beginObject();
				kvOr(w, "month", (int)s.month, s.month != ETA_MONTH_UNDEFINED);
				kvOr(w, "day", (int)s.day, s.day != ETA_DAY_UNDEFINED);
				kvOr(w, "hour", (int)s.hour, s.hour != ETA_HOUR_UNDEFINED);
				kvOr(w, "minute", (int)s.minute, s.minute != ETA_MINUTE_UNDEFINED);
				w.endObject();
			}
			else
				w.kv_null("eta");

			const int validated = s.getValidated();
			w.kv("validated", validated == 1 ? "yes" : validated == 2 ? "no" : "pending").kv("repeat", s.getRepeat())
				.kv("approximate", (bool)s.getApproximate()).kv("virtual_aid", (bool)s.getVirtualAid());
			w.key("channels").beginArray();
			for (int c = 0; c < 4; c++)
				if (s.getChannels() & (1 << c))
					w.val(std::string(1, (char)('A' + c)));
			w.endArray();
			// what the transponder says of itself; null where it has not said
			w.key("transponder").beginObject();
			if (s.getCSUnit() == 1 || s.getCSUnit() == 2)
				w.kv("unit", s.getCSUnit() == 2 ? "carrier_sense" : "sotdma");
			else
				w.kv_null("unit");
			triState(w, "raim", s.getRAIM(), true);
			triState(w, "dte_ready", s.getDTE(), false);
			triState(w, "assigned", s.getAssigned(), true);
			triState(w, "display", s.getDisplay(), true);
			triState(w, "dsc", s.getDSC(), true);
			triState(w, "whole_band", s.getBand(), true);
			triState(w, "message_22", s.getMsg22(), true);
			triState(w, "off_position", s.getOffPosition(), true);
			triState(w, "special_manoeuvre", s.getManeuver(), true);
			w.endObject();
			w.kv("vendor_id", s.vendorid);
			kvOr(w, "model", s.unit_model, s.unit_model >= 0);
			kvOr(w, "serial", s.unit_serial, s.unit_serial >= 0);

			kvOr(w, "level_db", s.level, s.level != LEVEL_UNDEFINED);
			kvOr(w, "ppm", s.ppm, s.ppm != PPM_UNDEFINED);
			kvOr(w, "altitude_m", s.altitude, s.altitude != ALT_UNDEFINED);
			kvOr(w, "received_stations", s.received_stations, s.received_stations != RECEIVED_STATIONS_UNDEFINED);
			w.kv("messages", s.count).key("message_types").beginArray();
			for (int t = 0; t < 32; t++)
				if (s.msg_type & (1 << t))
					w.val(t);
			w.endArray();

			if (badge & 15)
			{
				const unsigned bucket = (badge >> 7) & 3;
				w.key("binary_messages").beginObject().kv("count", (int)(badge & 15))
					.kv("kind", BINARY_KINDS[(badge >> 4) & 7]).kv("acknowledgement", (badge & 512) != 0)
					.kv("newest", bucket == 0 ? "under_15_min" : bucket == 1 ? "under_30_min" : "older").endObject();
			}
			else
				w.kv_null("binary_messages");
			kvOr(w, "station_aboard", aboard, aboard >= 0);

			w.kv("last_seen", isoTime(s.last_signal)).kv("age_s", (long long)(now > s.last_signal ? now - s.last_signal : 0));
			w.key("stations").beginArray();
			for (const auto &h : s.heard)
			{
				if (!h.last)
					break;
				w.beginObject().kv("id", h.station).kv("last_heard", isoTime((std::time_t)h.last)).endObject();
			}
			w.endArray();
			if (extra)
				extra(w);
			w.endObject();
		}

		// a reported change, in the units and names of the vessel itself
		void writeChange(JSON::Writer &w, const StaticStore::Entry &e)
		{
			static const char *const FIELDS[] = {"", "draught_m", "nav_status", "name", "callsign", "destination", "eta"};
			if (e.field < 1 || e.field > 6)
				return;
			w.beginObject().kv("time", isoTime(e.time)).kv("field", FIELDS[e.field]);
			if (e.field == StaticStore::DRAUGHT)
				w.kv("from", e.from / 10.0).kv("to", e.to / 10.0);
			else if (e.field == StaticStore::STATUS)
			{
				kvOr(w, "from", e.from < 15 ? NAV_STATUS[e.from] : "", e.from < 15);
				kvOr(w, "to", e.to < 15 ? NAV_STATUS[e.to] : "", e.to < 15);
			}
			else if (e.field == StaticStore::ETA)
			{
				const uint8_t *v = (const uint8_t *)e.value;
				w.kv_null("from").key("to").beginObject();
				kvOr(w, "month", (int)v[0], v[0] != ETA_MONTH_UNDEFINED);
				kvOr(w, "day", (int)v[1], v[1] != ETA_DAY_UNDEFINED);
				kvOr(w, "hour", (int)v[2], v[2] != ETA_HOUR_UNDEFINED);
				kvOr(w, "minute", (int)v[3], v[3] != ETA_MINUTE_UNDEFINED);
				w.endObject();
			}
			else
				w.kv_null("from").kv("to", e.value);
			w.kv("initial", (e.flags & StaticStore::INITIAL) != 0).endObject();
		}

		void writePlaceRef(JSON::Writer &w, const PlaceIndex::Entry &e)
		{
			const PlaceMetadata &m = *e.metadata;
			w.key("place").beginObject().kv("uuid", m.uuid).kv("name", m.name).kv("type", m.type());
			kvOr(w, "unlocode", m.code, !m.code.empty());
			kvOr(w, "country", m.country, !m.country.empty());
			w.endObject();
		}

		void writePlace(JSON::Writer &w, const PlaceIndex &index, const PlaceIndex::Entry &e)
		{
			const PlaceMetadata &m = *e.metadata;
			w.kv("uuid", m.uuid).kv("name", m.name).kv("type", m.type());
			if (!m.category.empty())
				w.kv("category", m.category);
			else
				w.kv_null("category");
			if (!m.code.empty())
				w.kv("unlocode", m.code);
			else
				w.kv_null("unlocode");
			if (!m.country.empty())
				w.kv("country", m.country);
			else
				w.kv_null("country");
			const PlaceIndex::Entry *parent = e.parent != UINT32_MAX ? index.find(e.parent) : nullptr;
			if (parent)
				w.kv("part_of", parent->metadata->uuid);
			else
				w.kv_null("part_of");
			w.kv("lat", e.lat).kv("lon", e.lon).kv("has_outline", !e.polygons->empty());
		}

		// a stay as the visit tracker keeps it; a time nobody saw is a bound, and says so
		void writeVisit(JSON::Writer &w, const VisitTracker::Visit &v, const PlaceIndex::Entry &e)
		{
			w.kv("entered", isoTime(v.entered)).kv("entered_observed", v.entryTime() != 0);
			if (v.inside())
				w.kv_null("left").kv_null("left_observed");
			else
				w.kv("left", isoTime(v.exited)).kv("left_observed", v.exitTime() != 0);
			w.kv("inside", v.inside()).kv("call", v.call(e.metadata->requiresStop())).kv("stopped_min", (int)v.idle);
		}

		// An aircraft as ADS-B gives it: feet, knots, feet a minute, and the
		// standard spellings of its emitter category and squawk.
		void writeAircraft(JSON::Writer &w, const Plane::ADSB &p, std::time_t heard, std::time_t now)
		{
			char icao[8], code[8];
			snprintf(icao, sizeof(icao), "%06X", p.hexident & 0xFFFFFF);
			w.beginObject().kv("icao", icao);
			kvOr(w, "callsign", p.callsign, p.callsign[0] != 0 && p.callsign[0] != ' ');
			kvOr(w, "country", std::string(p.country_code, 2), p.country_code[0] != ' ' && p.country_code[0] != 0);
			const bool placed = isValidCoord(p.lat, p.lon);
			kvOr(w, "lat", p.lat, placed);
			kvOr(w, "lon", p.lon, placed);
			kvOr(w, "distance_nm", p.distance, placed && p.distance != DISTANCE_UNDEFINED);
			kvOr(w, "bearing_deg", p.angle, placed && p.distance != DISTANCE_UNDEFINED && p.angle != ANGLE_UNDEFINED);
			kvOr(w, "altitude_ft", p.altitude, p.altitude != ALTITUDE_UNDEFINED);
			kvOr(w, "ground_speed_kn", p.speed, p.speed != SPEED_UNDEFINED);
			kvOr(w, "track_deg", p.heading, p.heading != HEADING_UNDEFINED);
			kvOr(w, "vertical_rate_fpm", p.vertrate, p.vertrate != VERT_RATE_UNDEFINED);
			snprintf(code, sizeof(code), "%04d", p.squawk);
			kvOr(w, "squawk", code, p.squawk != SQUAWK_UNDEFINED && p.squawk >= 0 && p.squawk <= 7777);
			if (p.airborne == 0 || p.airborne == 1)
				w.kv("on_ground", p.airborne == 0);
			else
				w.kv_null("on_ground");
			// type code 4 to 1 is set A to D, the subtype the number
			const int tc = p.category / 10, st = p.category % 10;
			if (p.category != CATEGORY_UNDEFINED && tc >= 1 && tc <= 4)
			{
				snprintf(code, sizeof(code), "%c%d", "DCBA"[tc - 1], st);
				w.kv("category", code);
			}
			else
				w.kv_null("category");
			kvOr(w, "level_db", p.signalLevel, p.signalLevel != LEVEL_UNDEFINED);
			w.kv("messages", (long long)p.nMessages).kv("last_seen", isoTime(heard)).kv("age_s", (long long)(now > heard ? now - heard : 0));
			w.endObject();
		}

		// A ship's visits in reading order: the stays under way first, the berth
		// before the terminal before the port before the water and the smaller
		// first within a kind, as the ship card names them; then the past ones,
		// the newest first.
		struct VisitRow
		{
			bool inside;
			int rank;
			double size;
			uint32_t id, stamp;
			std::string json;
		};
		void sortForReading(std::vector<VisitRow> &rows)
		{
			std::sort(rows.begin(), rows.end(), [](const VisitRow &a, const VisitRow &b) {
				if (a.inside != b.inside)
					return a.inside;
				if (a.inside)
					return std::make_tuple(a.rank, a.size, a.id) < std::make_tuple(b.rank, b.size, b.id);
				return a.stamp > b.stamp;
			});
		}
		VisitRow visitRow(const VisitTracker::Visit &v, const PlaceIndex::Entry &e)
		{
			VisitRow r{v.inside(), e.metadata->rank(), e.size, e.id, v.entered ? v.entered : v.exited, std::string()};
			JSON::Writer w(r.json);
			w.beginObject();
			writePlaceRef(w, e);
			writeVisit(w, v, e);
			w.endObject();
			w.finish();
			return r;
		}

		// The viewer's rules for a place's lists. A place where a stay needs a stop
		// lists as inside only the vessels that have lain still in it: a ship
		// steaming through, an aid or a base station is not news there.
		bool insideNow(const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &e, std::time_t now)
		{
			if (!v.inside() || now - s.last_signal > DB::VISIT_SILENT)
				return false;
			return !e.metadata->requiresStop() || (s.shipclass < CLASS_PLANE && v.idle > 0);
		}
		// a stay that is over and was never a call is not history
		bool visitShown(const VisitTracker::Visit &v, const PlaceIndex::Entry &e)
		{
			return v.shown(e.metadata->requiresStop());
		}
	}

	bool ApiV1::shows(uint32_t mmsi)
	{
		if (!filter.any())
			return true;
		const std::time_t now = std::time(nullptr);
		bool passes = false;
		tracker.ships().withShip(mmsi, [&](const Ship &s, int) { passes = filter.passes(s, now); });
		return passes;
	}

	int ApiV1::scope(const Request &request) const
	{
		const std::string base = "/api/v1";
		const std::string &p = request.path;
		if (p.compare(0, base.size(), base) != 0 || (p.size() > base.size() && p[base.size()] != '/'))
			return NOT_MINE;
		// the system service's, when the server offers it
		if (p == "/api/v1/health" || p == "/api/v1/system" || p.compare(0, 15, "/api/v1/system/") == 0)
			return NOT_MINE;
		return ApiKey::DATA;
	}

	bool ApiV1::handle(const Request &request, Response &response)
	{
		const std::string base = "/api/v1";
		if (scope(request) == NOT_MINE)
			return false;
		if (request.method != "GET")
		{
			response = Response::problem(405, "Method not allowed", "This part of the API only reads.");
			return true;
		}

		std::vector<std::string> parts;
		std::string rest = request.path.substr(base.size());
		std::size_t pos = 0;
		while (pos < rest.size())
		{
			std::size_t next = rest.find('/', pos + 1);
			std::string part = rest.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
			if (!part.empty())
				parts.push_back(part);
			if (next == std::string::npos)
				break;
			pos = next;
		}

		long long n = 0;
		if (parts.empty())
			response = index();
		else if (parts[0] == "openapi.json" && parts.size() == 1)
			response = openapi();
		else if (parts[0] == "stations" && parts.size() == 1)
			response = stationList();
		else if (parts[0] == "stations" && parts.size() == 2 && number(parts[1], n))
			response = station((int)n);
		else if (parts[0] == "stations" && parts.size() == 3 && parts[2] == "statistics" && number(parts[1], n))
			response = statistics((int)n);
		else if (parts[0] == "statistics" && parts.size() == 1)
			response = statistics(-1);
		else if (parts[0] == "vessels" && parts.size() == 1)
			response = vessels(request);
		else if (parts[0] == "vessels" && parts.size() == 2 && number(parts[1], n))
			response = vessel((uint32_t)n);
		else if (parts[0] == "vessels" && parts.size() == 3 && parts[2] == "track" && number(parts[1], n))
			response = track((uint32_t)n, request);
		else if (parts[0] == "vessels" && parts.size() == 3 && parts[2] == "visits" && number(parts[1], n))
			response = vesselVisits((uint32_t)n);
		else if (parts[0] == "places" && parts.size() == 1)
			response = places(request);
		else if (parts[0] == "places" && parts.size() == 2)
			response = place(parts[1]);
		else if (parts[0] == "places" && parts.size() == 3 && parts[2] == "vessels")
			response = placeVessels(parts[1]);
		else if (parts[0] == "places" && parts.size() == 3 && parts[2] == "visits")
			response = placeVisits(parts[1], request);
		else if (parts[0] == "events" && parts.size() == 1)
			response = events(request);
		else if (parts[0] == "aircraft" && parts.size() == 1)
			response = aircraftList(request);
		else if (parts[0] == "aircraft" && parts.size() == 2 && parts[1].size() == 6 &&
				 parts[1].find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
			response = aircraft((uint32_t)std::strtoul(parts[1].c_str(), nullptr, 16));
		else
			response = Response::problem(404, "Not found", "No such resource: " + request.path);
		return true;
	}

	Response ApiV1::index()
	{
		std::string out;
		JSON::Writer w(out);
		w.beginObject().kv("name", "AIS-catcher").kv("api", "v1").key("resources").beginArray();
		for (const auto &r : routes())
			if (std::string(r.path) != "/api/v1" &&
				(std::find(services.begin(), services.end(), r.service) != services.end() || std::string(r.service) == "api"))
				w.val(r.path);
		w.endArray().kv("openapi", "/api/v1/openapi.json").kv("notice", "Positions are what ships broadcast; not for navigation.").endObject();
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::openapi()
	{
		std::vector<std::string> offered = services;
		if (std::find(offered.begin(), offered.end(), "api") == offered.end())
			offered.push_back("api");
		std::string out;
		JSON::Writer w(out);
		writeOpenAPI(w, offered);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::stationList()
	{
		const StationRegistry registry = stations();
		const std::time_t now = std::time(nullptr);
		std::map<int, int> vessels;
		tracker.ships().forEachRecent(now, now - Ship::STATION_WINDOW, [&](int, const Ship &s, long) {
			if (!filter.passes(s, now))
				return;
			s.forEachHeard([&](int station, std::time_t t) {
				if (t >= now - Ship::STATION_WINDOW)
					vessels[station]++;
			});
		});
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		registry.forEach([&](const StationRegistry::Station &s) {
			if (!filter.allowsStation(s.id))
				return;
			w.beginObject();
			writeStation(w, s, tracker.stationPosition(s.id));
			w.kv("vessels_last_hour", vessels.count(s.id) ? vessels[s.id] : 0);
			w.endObject();
			count++;
		});
		endList(w, count, true);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::station(int id)
	{
		if (!filter.allowsStation(id))
			return Response::problem(404, "Not found", "No station " + std::to_string(id) + ".");
		const StationRegistry registry = stations();
		const StationRegistry::Station *s = registry.find(id);
		if (!s)
			return Response::problem(404, "Not found", "No station " + std::to_string(id) + ".");
		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		writeStation(w, *s, tracker.stationPosition(s->id));
		w.endObject();
		w.finish();
		return Response::json(out);
	}

	// one station's figures, or with `station` -1 those of everything the
	// server shows: all of them, or its stations' added up
	Response ApiV1::statistics(int station)
	{
		if (station >= 0 && !filter.allowsStation(station))
			return Response::problem(404, "Not found", "Station " + std::to_string(station) + " has heard nothing yet.");
		std::vector<int> ids = station >= 0 ? std::vector<int>(1, station) : filter.stations;
		std::string label = ids.empty() ? "all" : "";
		for (int id : ids)
			label += (label.empty() ? "" : ",") + std::to_string(id);
		std::string out;
		JSON::Writer w(out);
		const auto write = [&](Figures &f) {
			w.beginObject().kv("station", label);
			f.writeCountersJSON(w);
			w.key("history");
			f.writeHistoryJSON(w);
			w.endObject();
		};
		if (ids.empty())
			tracker.withAll(write);
		else if (!tracker.withStations(ids, write))
			return Response::problem(404, "Not found", (ids.size() == 1 ? "Station " : "Stations ") + label + " ha" + (ids.size() == 1 ? "s" : "ve") + " heard nothing yet.");
		w.finish();
		return Response::json(out);
	}

	namespace
	{
		// What the API asks of a vessel beyond the viewer's filter terms: a
		// text in its name, call sign, MMSI or IMO; a box; a circle.
		struct Search
		{
			std::string q;
			bool box = false, near = false;
			float west = 0, south = 0, east = 0, north = 0, lat = 0, lon = 0, radius_nm = 10;

			bool any() const { return !q.empty() || box || near; }

			static bool floats(const std::string &v, std::vector<float> &out, std::size_t n)
			{
				out.clear();
				std::size_t pos = 0;
				while (pos <= v.size())
				{
					std::size_t comma = v.find(',', pos);
					const std::string part = v.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
					char *end = nullptr;
					const float f = std::strtof(part.c_str(), &end);
					if (part.empty() || !end || *end)
						return false;
					out.push_back(f);
					if (comma == std::string::npos)
						break;
					pos = comma + 1;
				}
				return out.size() == n;
			}

			// false and why, for a term that is not one of these or is malformed; true when consumed
			bool set(const std::string &key, const std::string &value, std::string &error, bool &mine)
			{
				std::vector<float> f;
				mine = true;
				if (key == "q")
				{
					q = value;
					std::transform(q.begin(), q.end(), q.begin(), ::tolower);
				}
				else if (key == "bbox")
				{
					if (!floats(value, f, 4) || f[1] < -90 || f[3] > 90 || f[1] > f[3] || f[0] < -180 || f[2] > 180)
						return error = "bbox takes west,south,east,north in degrees.", false;
					west = f[0], south = f[1], east = f[2], north = f[3], box = true;
				}
				else if (key == "near")
				{
					if (!floats(value, f, 2) || !isValidCoord(f[0], f[1]))
						return error = "near takes lat,lon in degrees.", false;
					lat = f[0], lon = f[1], near = true;
				}
				else if (key == "radius_nm")
				{
					if (!floats(value, f, 1) || f[0] <= 0 || f[0] > 1000)
						return error = "radius_nm takes nautical miles, more than 0 and up to 1000.", false;
					radius_nm = f[0];
				}
				else
					mine = false;
				return true;
			}

			static bool contains(const char *field, const std::string &needle)
			{
				std::string hay(field);
				std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
				return hay.find(needle) != std::string::npos;
			}

			bool passes(const Ship &s) const
			{
				if (!q.empty() && !contains(s.shipname, q) && !contains(s.callsign, q) &&
					std::to_string(s.mmsi).find(q) == std::string::npos && !(s.IMO > 0 && std::to_string(s.IMO) == q))
					return false;
				if ((box || near) && !isValidCoord(s.lat, s.lon))
					return false;
				if (box && (s.lat < south || s.lat > north || (west <= east ? (s.lon < west || s.lon > east) : (s.lon < west && s.lon > east))))
					return false;
				if (near)
				{
					float d = 0;
					int bearing = 0;
					Util::Geodesy::distanceBearing(lat, lon, s.lat, s.lon, d, bearing);
					if (!(d <= radius_nm))
						return false;
				}
				return true;
			}
		};
	}

	Response ApiV1::vessels(const Request &request)
	{
		const long long capacity = tracker.ships().capacity();
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, capacity))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(capacity) + ".");
		std::time_t since = 0;
		const std::string from = request.param("since");
		if (!from.empty() && !parseTime(from, since))
			return Response::problem(400, "Bad request", "since must be what a previous page gave as next.");
		// the viewer's filter terms, under its names; they narrow what the server shows
		ViewFilter query;
		Search search;
		std::string carried;
		for (const auto &p : request.params())
		{
			if (p.first == "since")
				continue;
			std::string error;
			bool searched = false;
			if (!search.set(p.first, p.second, error, searched))
				return Response::problem(400, "Bad request", error);
			if (p.first != "limit" && !searched)
			{
				if (!ViewFilter::isKey(p.first))
					return Response::problem(400, "Bad request", "\"" + p.first + "\" is not a parameter of this list.");
				try
				{
					query.set(p.first, p.second);
				}
				catch (const std::exception &e)
				{
					return Response::problem(400, "Bad request", std::string(e.what()) + ".");
				}
			}
			carried += "&" + p.first + "=" + urlEncode(p.second);
		}
		const bool narrowed = query.any();
		const std::time_t now = std::time(nullptr);
		DB &db = tracker.ships();

		// A cursor older than the last reset of the ship data, or none, gets the
		// whole picture, and says so: the client then drops what it holds.
		const bool full = since <= 0 || since <= db.fullRefreshAt();
		const std::time_t shown = now - db.timeHistory();
		const std::time_t cutoff = full ? shown : MAX(since, shown);
		const auto aboard = tracker.riding();

		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		bool complete = true;
		db.forEachRecent(now, cutoff, [&](int, const Ship &s, long) {
			if (!complete || !filter.passes(s, now) || (narrowed && !query.passes(s, now)) || (search.any() && !search.passes(s)))
				return;
			if ((long long)count >= limit)
			{
				complete = false;
				return;
			}
			auto r = aboard.find(s.mmsi);
			writeVessel(w, s, now, db.binaryBadgeHeld(s.mmsi, now), r == aboard.end() ? -1 : r->second);
			count++;
		});
		// what changes from now on: a second of overlap, as messages share a second
		const std::string next = "/api/v1/vessels?since=" + std::to_string((long long)now - 1) + carried;
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", complete)
			.kv("next", next).kv("full", full).kv("time", isoTime(now)).kv("expire_s", db.timeHistory()).endObject().endObject();
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::vessel(uint32_t mmsi)
	{
		const std::time_t now = std::time(nullptr);
		const auto aboard = tracker.riding();
		DB &db = tracker.ships();

		// the parts kept beside the ship's record, each read under its own lock
		std::vector<StaticStore::Entry> changes;
		db.forEachChangeOf(mmsi, [&](const StaticStore::Entry &e) { changes.push_back(e); });
		std::vector<VisitRow> visits;
		db.forEachVisitOf(mmsi, [&](const Ship &, const VisitTracker::Visit &v, const PlaceIndex::Entry &e) {
			if (visitShown(v, e))
				visits.push_back(visitRow(v, e));
		});
		sortForReading(visits);
		const std::string port = db.matchedPort(mmsi);
		const auto index = tracker.places();

		const auto extra = [&](JSON::Writer &w) {
			// the port the destination names, as the place catalogue or the port list knows it
			if (port.empty())
				w.kv_null("destination_port");
			else
			{
				w.key("destination_port").beginObject().kv("unlocode", port);
				const PlaceIndex::Entry *e = index ? index->findPort(port) : nullptr;
				if (e)
					w.kv("place", e->metadata->uuid).kv("name", e->metadata->name).kv("country", port.substr(0, 2)).kv("lat", e->lat).kv("lon", e->lon);
				else
				{
					bool known = false;
#ifdef HASPORTS
					libport::Info info;
					if (libport::lookup(port.c_str(), info))
					{
						known = true;
						w.kv_null("place").kv("name", info.name).kv("country", info.country);
						if (info.has_position)
							w.kv("lat", info.lat).kv("lon", info.lon);
						else
							w.kv_null("lat").kv_null("lon");
					}
#endif
					if (!known)
						w.kv_null("place").kv_null("name").kv("country", port.substr(0, 2)).kv_null("lat").kv_null("lon");
				}
				w.endObject();
			}
			w.key("changes").beginArray();
			for (const auto &c : changes)
				writeChange(w, c);
			w.endArray().key("visits").beginArray();
			for (const auto &v : visits)
				w.raw_val(v.json);
			w.endArray();
		};

		std::string out;
		bool found = false;
		db.withShip(mmsi, [&](const Ship &s, int) {
			if (!filter.passes(s, now))
				return;
			auto r = aboard.find(s.mmsi);
			JSON::Writer w(out);
			writeVessel(w, s, now, db.binaryBadgeHeld(s.mmsi, now), r == aboard.end() ? -1 : r->second, extra);
			w.finish();
			found = true;
		});
		if (!found)
			return Response::problem(404, "Not found", "No vessel " + std::to_string(mmsi) + " is known.");
		return Response::json(out);
	}

	Response ApiV1::track(uint32_t mmsi, const Request &request)
	{
		std::time_t since = 0;
		const std::string from = request.param("since");
		if (!from.empty() && !parseTime(from, since))
			return Response::problem(400, "Bad request", "since must be a UTC time such as 2026-10-19T14:00:00Z, or seconds since 1970.");
		const std::string format = request.param("format");
		if (!format.empty() && format != "json" && format != "geojson")
			return Response::problem(400, "Bad request", "format must be json or geojson.");
		if (!shows(mmsi))
			return Response::problem(404, "Not found", "No vessel " + std::to_string(mmsi) + " is known.");

		std::vector<PathStore::Point> points;
		if (!tracker.ships().forEachTrackPoint(mmsi, since, [&](const PathStore::Point &p) { points.push_back(p); }))
			return Response::problem(404, "Not found", "No vessel " + std::to_string(mmsi) + " is known.");
		std::reverse(points.begin(), points.end());

		std::string out;
		JSON::Writer w(out);
		if (format == "geojson")
		{
			w.beginObject().kv("type", "Feature").key("geometry").beginObject().kv("type", "LineString").key("coordinates").beginArray();
			for (const auto &p : points)
				w.beginArray().val(p.lon).val(p.lat).endArray();
			w.endArray().endObject().key("properties").beginObject().kv("mmsi", (long long)mmsi).key("times").beginArray();
			for (const auto &p : points)
				w.val(isoTime(p.time));
			w.endArray().endObject().endObject();
		}
		else
		{
			beginList(w);
			for (const auto &p : points)
			{
				w.beginObject().kv("time", isoTime(p.time)).kv("lat", p.lat).kv("lon", p.lon);
				// a ship lying still is one point that lasts
				if (p.dur)
					w.kv("until", isoTime(p.end()));
				kvOr(w, "speed_kn", p.sog / 10.0, p.sog != PathStore::NA);
				kvOr(w, "course_deg", p.cog / 10.0, p.cog != PathStore::NA);
				kvOr(w, "heading_deg", (int)p.hdg, p.hdg != PathStore::NA);
				w.endObject();
			}
			endList(w, points.size(), true);
		}
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::vesselVisits(uint32_t mmsi)
	{
		if (!shows(mmsi))
			return Response::problem(404, "Not found", "No vessel " + std::to_string(mmsi) + " is known.");
		// written under the lock into rows of their own: the place index may be replaced once it is released
		std::vector<VisitRow> rows;
		const bool known = tracker.ships().forEachVisitOf(mmsi, [&](const Ship &, const VisitTracker::Visit &v, const PlaceIndex::Entry &e) {
			if (visitShown(v, e))
				rows.push_back(visitRow(v, e));
		});
		if (!known)
			return Response::problem(404, "Not found", "No vessel " + std::to_string(mmsi) + " is known.");
		sortForReading(rows);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : rows)
			w.raw_val(r.json);
		endList(w, rows.size(), true);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::places(const Request &request)
	{
		long long limit = 0, after = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		if (!request.integer("after", after, -1, -1, 4294967294LL))
			return Response::problem(400, "Bad request", "after must be what a previous page gave as next.");
		const std::string type = request.param("type");
		PlaceKind kind = PlaceKind::Port;
		if (!type.empty() && !PlaceMetadata::parseKind(type, kind))
			return Response::problem(400, "Bad request", "type must be a place type, such as port, berth or guardzone.");
		// a text in the name or the UN/LOCODE
		std::string q = request.param("q");
		std::transform(q.begin(), q.end(), q.begin(), ::tolower);
		const auto matches = [&](const PlaceMetadata &m) {
			if (q.empty())
				return true;
			std::string name = m.name, code = m.code;
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			std::transform(code.begin(), code.end(), code.begin(), ::tolower);
			return name.find(q) != std::string::npos || code.find(q) != std::string::npos;
		};

		const auto index = tracker.places();
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		std::string next;
		if (index)
			for (const auto &e : index->entries)
			{
				if (!e.live() || (long long)e.id <= after || (!type.empty() && !e.metadata->is(kind)) || !matches(*e.metadata))
					continue;
				if ((long long)count >= limit)
				{
					next = "/api/v1/places?after=" + std::to_string(e.id - 1) + "&limit=" + std::to_string(limit) + (type.empty() ? "" : "&type=" + type) +
						   (q.empty() ? "" : "&q=" + urlEncode(q));
					break;
				}
				w.beginObject();
				writePlace(w, *index, e);
				w.endObject();
				count++;
			}
		endList(w, count, next.empty(), next);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::place(const std::string &uuid)
	{
		const auto index = tracker.places();
		const PlaceIndex::Entry *e = index ? index->resolve(uuid) : nullptr;
		if (!e)
			return Response::problem(404, "Not found", "No place " + uuid + ".");
		const std::time_t now = std::time(nullptr);
		int inside = 0;
		tracker.ships().forEachVisit([&](const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &at) {
			if (at.id == e->id && insideNow(s, v, at, now) && filter.passes(s, now))
				inside++;
		});

		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		writePlace(w, *index, *e);
		w.kv("vessels_inside", inside);
		// the outline as drawn, from the place's file; its other properties stay home
		w.key("outline");
		bool written = false;
		if (outlines && !e->polygons->empty())
			try
			{
				JSON::Parser parser(JSON_DICT_SETTING);
				parser.setPreserveUnknown(true);
				auto doc = parser.parse(*e->feature);
				if (const JSON::Value *g = doc.root[AIS::KEY_PLACE_GEOMETRY])
				{
					JSON::Serializer serializer(JSON_DICT_SETTING);
					serializer.setPool(&doc.pool);
					serializer.setExactFloats(true);
					serializer.stringifyValue(*g, w);
					written = true;
				}
			}
			catch (const std::exception &)
			{
			}
		if (!written)
			w.val_null();
		w.endObject();
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::placeVessels(const std::string &uuid)
	{
		const auto index = tracker.places();
		const PlaceIndex::Entry *e = index ? index->resolve(uuid) : nullptr;
		if (!e)
			return Response::problem(404, "Not found", "No place " + uuid + ".");
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		tracker.ships().forEachVisit([&](const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &at) {
			if (at.id != e->id || !insideNow(s, v, at, now) || !filter.passes(s, now))
				return;
			w.beginObject().kv("mmsi", (long long)s.mmsi).kv("name", s.shipname);
			writeVisit(w, v, at);
			w.endObject();
			count++;
		});
		endList(w, count, true);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::placeVisits(const std::string &uuid, const Request &request)
	{
		long long hours = 0, limit = 0;
		if (!request.integer("hours", hours, 24, 1, 30 * 24))
			return Response::problem(400, "Bad request", "hours must be between 1 and 720.");
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		const auto index = tracker.places();
		const PlaceIndex::Entry *e = index ? index->resolve(uuid) : nullptr;
		if (!e)
			return Response::problem(404, "Not found", "No place " + uuid + ".");
		const std::time_t now = std::time(nullptr), since = now - (std::time_t)hours * 3600;
		struct Row
		{
			uint32_t mmsi;
			std::string name;
			VisitTracker::Visit v;
		};
		std::vector<Row> rows;
		// the stays under way, and those that ended within the window
		tracker.ships().forEachVisit([&](const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &at) {
			if (at.id != e->id || !visitShown(v, at) || !filter.passes(s, now))
				return;
			if (!v.inside() && (std::time_t)MAX(v.entered, v.exited) < since)
				return;
			rows.push_back({s.mmsi, s.shipname, v});
		});
		const auto stamp = [](const Row &r) { return r.v.entered ? r.v.entered : r.v.exited; };
		std::sort(rows.begin(), rows.end(), [&](const Row &a, const Row &b) {
			return stamp(a) != stamp(b) ? stamp(a) > stamp(b) : a.mmsi < b.mmsi;
		});
		const bool complete = (long long)rows.size() <= limit;
		if (!complete)
			rows.resize((std::size_t)limit);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : rows)
		{
			w.beginObject().kv("mmsi", (long long)r.mmsi).kv("name", r.name);
			writeVisit(w, r.v, *e);
			w.endObject();
		}
		endList(w, rows.size(), complete);
		w.finish();
		return Response::json(out);
	}

	// Aircraft carry no station: a server that shows only some stations shows none
	bool ApiV1::aircraftShown(Response &refusal)
	{
		if (filter.stations.empty())
			return true;
		refusal = Response::problem(404, "Not found", "Aircraft are not kept by station, and this server shows only some stations.");
		return false;
	}

	Response ApiV1::aircraftList(const Request &request)
	{
		Response refusal;
		if (!aircraftShown(refusal))
			return refusal;
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, 100000))
			return Response::problem(400, "Bad request", "limit must be between 1 and 100000.");
		std::time_t since = 0;
		const std::string from = request.param("since");
		if (!from.empty() && !parseTime(from, since))
			return Response::problem(400, "Bad request", "since must be what a previous page gave as next.");
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		bool complete = true;
		tracker.planes().forEachAircraft(since, [&](const Plane::ADSB &p, std::time_t heard) {
			if ((long long)count >= limit)
			{
				complete = false;
				return;
			}
			writeAircraft(w, p, heard, now);
			count++;
		});
		const std::string next = "/api/v1/aircraft?since=" + std::to_string((long long)now - 1) + "&limit=" + std::to_string(limit);
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", complete)
			.kv("next", next).kv("time", isoTime(now)).endObject().endObject();
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::aircraft(uint32_t icao)
	{
		Response refusal;
		if (!aircraftShown(refusal))
			return refusal;
		const std::time_t now = std::time(nullptr);
		std::string out;
		tracker.planes().forEachAircraft(0, [&](const Plane::ADSB &p, std::time_t heard) {
			if (out.empty() && (p.hexident & 0xFFFFFF) == icao)
			{
				JSON::Writer w(out);
				writeAircraft(w, p, heard, now);
				w.finish();
			}
		});
		if (out.empty())
			return Response::problem(404, "Not found", "No aircraft is kept under that address.");
		return Response::json(out);
	}

	Response ApiV1::events(const Request &request)
	{
		long long after = 0, limit = 0;
		if (!request.integer("after", after, -1, 0, 9223372036854775807LL))
			return Response::problem(400, "Bad request", "after must be an event number.");
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
		// no cursor: the newest page, and the cursor to follow from there
		if (after < 0)
			after = (long long)tracker.journal().before((std::size_t)limit);
		std::vector<Journal::Entry> entries;
		if (!tracker.journal().since((uint64_t)after, (std::size_t)limit, entries))
			return Response::problem(410, "Gone", "Events after " + std::to_string(after) + " are no longer kept; start again without after.");
		std::string out;
		JSON::Writer w(out);
		w.beginObject().key("data").beginArray();
		std::size_t count = 0;
		for (const auto &e : entries)
		{
			// the installation's own events are the system's to tell; an event of a
			// ship the server does not show is passed over but still moves the cursor
			if (e.event.kind == Tracking::Kind::SYSTEM || !shows(e.event.from))
				continue;
			count++;
			writeEvent(w, e);
		}
		const uint64_t last = entries.empty() ? (uint64_t)after : entries.back().seq;
		w.endArray().key("meta").beginObject().kv("count", (long long)count)
			.kv("complete", last >= tracker.journal().newest())
			.kv("next", "/api/v1/events?after=" + std::to_string(last))
			.kv("epoch", (long long)tracker.journal().epoch()).endObject().endObject();
		w.finish();
		return Response::json(out);
	}
}
