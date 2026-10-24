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
#include "Convert.h"
#include "Parse.h"
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
		// a station and where it is now, as far as the site lets it be told:
		// configured, by GPS on its input, or riding its vessel
		void writeStation(JSON::Writer &w, const StationRegistry::Station &s, const Tracker::Position &at, int vessels)
		{
			w.kv("id", s.id);
			textOrNull(w, "name", s.name);
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
			w.kv("vessels_last_hour", vessels);
		}

		int classOf(const Ship &s) { return s.shipclass >= 0 && s.shipclass < N_VESSEL_CLASSES ? s.shipclass : 0; }
		int validatedOf(const Ship &s) { return s.getValidated() == 1 || s.getValidated() == 2 ? s.getValidated() : 0; }

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
			w.beginObject().kv("mmsi", (long long)s.mmsi);
			textOrNull(w, "name", s.shipname);
			textOrNull(w, "callsign", s.callsign);
			w.kv_or_null("imo", s.IMO, s.IMO != IMO_UNDEFINED);
			textOrNull(w, "eni", s.vin);
			textOrNull(w, "destination", s.destination);
			w.kv("class", VESSEL_CLASSES[classOf(s)]);
			w.kv_or_null("ship_type", s.shiptype, s.shiptype > 0);
			w.kv("mmsi_type", s.mmsi_type >= 0 && s.mmsi_type < N_MMSI_TYPES ? MMSI_TYPES[s.mmsi_type] : "other");
			w.kv_or_null("country", s.country_code, s.country_code[0] != 0);

			const bool placed = isValidCoord(s.lat, s.lon);
			w.kv_or_null("lat", s.lat, placed);
			w.kv_or_null("lon", s.lon, placed);
			w.kv_or_null("distance_nm", s.distance, placed && s.distance != DISTANCE_UNDEFINED);
			w.kv_or_null("bearing_deg", s.angle, placed && s.distance != DISTANCE_UNDEFINED && s.angle != ANGLE_UNDEFINED);
			w.kv_or_null("speed_kn", s.speed, s.speed != SPEED_UNDEFINED);
			w.kv_or_null("course_deg", s.cog, s.cog != COG_UNDEFINED);
			w.kv_or_null("heading_deg", s.heading, s.heading != HEADING_UNDEFINED);
			w.kv_or_null("nav_status", s.status >= 0 && s.status < N_NAV_STATUSES ? NAV_STATUSES[s.status] : "", s.status >= 0 && s.status < N_NAV_STATUSES);

			w.kv_or_null("to_bow_m", s.to_bow, s.to_bow != DIMENSION_UNDEFINED);
			w.kv_or_null("to_stern_m", s.to_stern, s.to_stern != DIMENSION_UNDEFINED);
			w.kv_or_null("to_port_m", s.to_port, s.to_port != DIMENSION_UNDEFINED);
			w.kv_or_null("to_starboard_m", s.to_starboard, s.to_starboard != DIMENSION_UNDEFINED);
			w.kv_or_null("draught_m", s.draught, s.draught != DRAUGHT_UNDEFINED);
			if (s.month != ETA_MONTH_UNDEFINED || s.day != ETA_DAY_UNDEFINED || s.hour != ETA_HOUR_UNDEFINED || s.minute != ETA_MINUTE_UNDEFINED)
			{
				w.key("eta").beginObject();
				w.kv_or_null("month", (int)s.month, s.month != ETA_MONTH_UNDEFINED);
				w.kv_or_null("day", (int)s.day, s.day != ETA_DAY_UNDEFINED);
				w.kv_or_null("hour", (int)s.hour, s.hour != ETA_HOUR_UNDEFINED);
				w.kv_or_null("minute", (int)s.minute, s.minute != ETA_MINUTE_UNDEFINED);
				w.endObject();
			}
			else
				w.kv_null("eta");

			w.kv("validated", VALIDATED[validatedOf(s)]).kv("repeat", s.getRepeat())
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
			textOrNull(w, "vendor_id", s.vendorid);
			w.kv_or_null("model", s.unit_model, s.unit_model >= 0);
			w.kv_or_null("serial", s.unit_serial, s.unit_serial >= 0);

			w.kv_or_null("level_db", s.level, s.level != LEVEL_UNDEFINED);
			w.kv_or_null("ppm", s.ppm, s.ppm != PPM_UNDEFINED);
			w.kv_or_null("altitude_m", s.altitude, s.altitude != ALT_UNDEFINED);
			w.kv_or_null("received_stations", s.received_stations, s.received_stations != RECEIVED_STATIONS_UNDEFINED);
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
			w.kv_or_null("station_aboard", aboard, aboard >= 0);

			w.kv("last_seen", isoTimeText(s.last_signal).text).kv("age_s", (long long)(now > s.last_signal ? now - s.last_signal : 0));
			w.key("stations").beginArray();
			for (const auto &h : s.heard)
			{
				if (!h.last)
					break;
				w.beginObject().kv("id", h.station).kv("last_heard", isoTimeText((std::time_t)h.last).text).endObject();
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
				w.kv_or_null("from", e.from < N_NAV_STATUSES ? NAV_STATUSES[e.from] : "", e.from < N_NAV_STATUSES);
				w.kv_or_null("to", e.to < N_NAV_STATUSES ? NAV_STATUSES[e.to] : "", e.to < N_NAV_STATUSES);
			}
			else if (e.field == StaticStore::ETA)
			{
				const uint8_t *v = (const uint8_t *)e.value;
				w.kv_null("from").key("to").beginObject();
				w.kv_or_null("month", (int)v[0], v[0] != ETA_MONTH_UNDEFINED);
				w.kv_or_null("day", (int)v[1], v[1] != ETA_DAY_UNDEFINED);
				w.kv_or_null("hour", (int)v[2], v[2] != ETA_HOUR_UNDEFINED);
				w.kv_or_null("minute", (int)v[3], v[3] != ETA_MINUTE_UNDEFINED);
				w.endObject();
			}
			else
			{
				w.kv_null("from");
				textOrNull(w, "to", e.value);
			}
			w.kv("initial", (e.flags & StaticStore::INITIAL) != 0).endObject();
		}

		void writePlaceRef(JSON::Writer &w, const PlaceIndex::Entry &e)
		{
			const PlaceMetadata &m = *e.metadata;
			w.key("place").beginObject().kv("uuid", m.uuid).kv("name", m.name).kv("type", m.type());
			w.kv_or_null("unlocode", m.code, !m.code.empty());
			w.kv_or_null("country", m.country, !m.country.empty());
			w.endObject();
		}

		void writePlace(JSON::Writer &w, const PlaceIndex &index, const PlaceIndex::Entry &e)
		{
			const PlaceMetadata &m = *e.metadata;
			w.kv("uuid", m.uuid).kv("name", m.name).kv("type", m.type());
			textOrNull(w, "category", m.category);
			textOrNull(w, "unlocode", m.code);
			textOrNull(w, "country", m.country);
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
			w.kv_or_null("callsign", p.callsign, p.callsign[0] != 0 && p.callsign[0] != ' ');
			w.kv_or_null("country", std::string(p.country_code, 2), p.country_code[0] != ' ' && p.country_code[0] != 0);
			const bool placed = isValidCoord(p.lat, p.lon);
			w.kv_or_null("lat", p.lat, placed);
			w.kv_or_null("lon", p.lon, placed);
			w.kv_or_null("distance_nm", p.distance, placed && p.distance != DISTANCE_UNDEFINED);
			w.kv_or_null("bearing_deg", p.angle, placed && p.distance != DISTANCE_UNDEFINED && p.angle != ANGLE_UNDEFINED);
			w.kv_or_null("altitude_ft", p.altitude, p.altitude != ALTITUDE_UNDEFINED);
			w.kv_or_null("ground_speed_kn", p.speed, p.speed != SPEED_UNDEFINED);
			w.kv_or_null("track_deg", p.heading, p.heading != HEADING_UNDEFINED);
			w.kv_or_null("vertical_rate_fpm", p.vertrate, p.vertrate != VERT_RATE_UNDEFINED);
			snprintf(code, sizeof(code), "%04d", p.squawk);
			w.kv_or_null("squawk", code, p.squawk != SQUAWK_UNDEFINED && p.squawk >= 0 && p.squawk <= 7777);
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
			w.kv_or_null("level_db", p.signalLevel, p.signalLevel != LEVEL_UNDEFINED);
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

		// the usual vessels an hour, by UTC weekday and hour; null for an hour never heard in
		void writeWeek(JSON::Writer &w, const Figures &f)
		{
			static const char *const DAYS[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
			w.key("week").beginObject();
			for (int d = 0; d < 7; d++)
			{
				w.key(DAYS[d]).beginArray();
				for (int h = 0; h < 24; h++)
					if (f.week_hours[d][h])
						w.val((float)f.week_vessels[d][h] / f.week_hours[d][h]);
					else
						w.val_null();
				w.endArray();
			}
			w.endObject();
		}

		void writeLevels(JSON::Writer &w, const char *key, const Figures::Levels &l)
		{
			w.key(key).beginArray();
			for (int b = 0; b < Figures::LEVEL_BANDS; b++)
			{
				w.beginObject().kv("from_nm", b * Figures::LEVEL_BAND_NM);
				if (b + 1 < Figures::LEVEL_BANDS)
					w.kv("to_nm", (b + 1) * Figures::LEVEL_BAND_NM);
				else
					w.kv_null("to_nm");
				if (l.n[b])
					w.kv("level_db", (float)(l.sum[b] / l.n[b]));
				else
					w.kv_null("level_db");
				w.kv("messages", (long long)l.n[b]).endObject();
			}
			w.endArray();
		}

		// The figures as /api/v1 names them, as members of an object the caller
		// opened. `vessels` is counted only by the slots of a history.
		void writeFigures(JSON::Writer &w, const MessageStatistics::Values &v, bool empty, bool vessels)
		{
			const int c = v.count - v.exclude;
			const bool has_level = !empty && c > 0 && v.level_min <= v.level_max;

			w.kv("messages", empty ? 0 : v.count);
			if (vessels)
				w.kv("vessels", empty ? 0 : v.vessels);
			else
				w.kv_null("vessels");
			w.key("errors").beginObject();
			const char *names[] = {"flagged", "undersized", "oversized", "checksum"};
			for (int i = 0; i < 4; i++)
				w.kv(names[i], empty ? 0 : v.errors[i]);
			w.endObject();
			if (has_level)
				w.kv("level_min_db", v.level_min).kv("level_max_db", v.level_max).kv("ppm", v.ppm / c);
			else
				w.kv_null("level_min_db").kv_null("level_max_db").kv_null("ppm");
			if (!empty && v.distance > 0)
				w.kv("max_distance_nm", v.distance);
			else
				w.kv_null("max_distance_nm");
			w.kv("active_minutes", empty ? 0 : v.active_minutes);
			w.key("furthest").beginArray();
			for (int i = 0; i < MessageStatistics::FURTHEST && !empty; i++)
				if (v.furthest[i].mmsi)
				{
					w.beginObject().kv("mmsi", (long long)v.furthest[i].mmsi).kv("distance_nm", v.furthest[i].nm);
					if (v.furthest[i].bearing >= 0 && v.furthest[i].bearing < 360)
						w.kv("bearing_deg", v.furthest[i].bearing);
					else
						w.kv_null("bearing_deg");
					w.kv("time", isoTime((std::time_t)v.furthest[i].time)).endObject();
				}
			w.endArray();

			w.key("channels").beginObject();
			for (int i = 0; i < 4; i++)
				w.kv(std::string(1, (char)('A' + i)), empty ? 0 : v.channel[i]);
			w.endObject();

			w.key("message_types").beginObject();
			for (int i = 0; i < 28; i++)
				if (!empty && v.msg[i])
					w.kv(std::to_string(i + 1), v.msg[i]);
			w.endObject();

			w.key("range_nm").beginObject().kv("sector_deg", 360 / MessageStatistics::RADAR_BUCKETS);
			w.key("class_a").beginArray();
			for (int i = 0; i < MessageStatistics::RADAR_BUCKETS; i++)
				w.val(empty ? 0.0f : v.radarA[i]);
			w.endArray().key("class_b").beginArray();
			for (int i = 0; i < MessageStatistics::RADAR_BUCKETS; i++)
				w.val(empty ? 0.0f : v.radarB[i]);
			w.endArray().endObject();
		}

		// a period's figures as an object of its own: a counter, or a history's last whole slot
		template <typename C>
		void writeTotal(JSON::Writer &w, const char *key, C &counter)
		{
			w.key(key).beginObject();
			writeFigures(w, counter.values(), false, false);
			w.endObject();
		}
		template <typename H>
		void writeLast(JSON::Writer &w, const char *key, H &history)
		{
			w.key(key).beginObject();
			history.withLastSlot([&](MessageStatistics &s, bool empty) { writeFigures(w, s.values(), empty, true); });
			w.endObject();
		}

		// one history of figures, a slot an object, oldest first
		template <typename H>
		void writeSlots(JSON::Writer &w, const char *key, H &history)
		{
			w.key(key).beginArray();
			history.forEachSlot([&](long int start, MessageStatistics &s, bool empty) {
				w.beginObject().kv("time", isoTime((std::time_t)start));
				writeFigures(w, s.values(), empty, true);
				w.endObject();
			});
			w.endArray();
		}
	}

	bool ApiV1::shows(uint32_t mmsi)
	{
		if (!view.any())
			return true;
		const std::time_t now = std::time(nullptr);
		bool passes = false;
		tracker.ships().withShip(mmsi, [&](const Ship &s, int) { passes = view.passes(s, now); });
		return passes;
	}

	std::map<int, int> ApiV1::heardLately()
	{
		const std::time_t now = std::time(nullptr);
		std::map<int, int> vessels;
		tracker.ships().forEachRecent(now, now - Ship::STATION_WINDOW, [&](int, const Ship &s, long) {
			if (!view.passes(s, now))
				return;
			s.forEachHeard([&](int station, std::time_t t) {
				if (t >= now - Ship::STATION_WINDOW)
					vessels[station]++;
			});
		});
		return vessels;
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
			response.header = "Allow: GET, HEAD";
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

		// the route as the table writes it, and how it is answered
		const char *path = nullptr;
		std::function<Response()> answer;
		const auto at = [&](const char *p, std::function<Response()> f) {
			path = p;
			answer = std::move(f);
		};
		long long n = 0;
		if (parts.empty())
			at("/api/v1", [&] { return index(); });
		else if (parts[0] == "openapi.json" && parts.size() == 1)
			at("/api/v1/openapi.json", [&] { return openapi(); });
		else if (parts[0] == "stations" && parts.size() == 1)
			at("/api/v1/stations", [&] { return stationList(); });
		else if (parts[0] == "stations" && parts.size() == 2 && Util::Parse::Digits(parts[1], n, 10))
			at("/api/v1/stations/{id}", [&] { return station((int)n); });
		else if (parts[0] == "stations" && parts.size() == 3 && parts[2] == "statistics" && Util::Parse::Digits(parts[1], n, 10))
			at("/api/v1/stations/{id}/statistics", [&] { return statistics((int)n); });
		else if (parts[0] == "statistics" && parts.size() == 1)
			at("/api/v1/statistics", [&] { return statistics(-1); });
		else if (parts[0] == "vessels" && parts.size() == 1)
			at("/api/v1/vessels", [&] { return vessels(request); });
		else if (parts[0] == "vessels" && parts.size() == 2 && Util::Parse::Digits(parts[1], n, 10))
			at("/api/v1/vessels/{mmsi}", [&] { return vessel((uint32_t)n); });
		else if (parts[0] == "vessels" && parts.size() == 3 && parts[2] == "track" && Util::Parse::Digits(parts[1], n, 10))
			at("/api/v1/vessels/{mmsi}/track", [&] { return track((uint32_t)n, request); });
		else if (parts[0] == "vessels" && parts.size() == 3 && parts[2] == "visits" && Util::Parse::Digits(parts[1], n, 10))
			at("/api/v1/vessels/{mmsi}/visits", [&] { return vesselVisits((uint32_t)n); });
		else if (parts[0] == "places" && parts.size() == 1)
			at("/api/v1/places", [&] { return places(request); });
		else if (parts[0] == "places" && parts.size() == 2)
			at("/api/v1/places/{uuid}", [&] { return place(parts[1]); });
		else if (parts[0] == "places" && parts.size() == 3 && parts[2] == "vessels")
			at("/api/v1/places/{uuid}/vessels", [&] { return placeVessels(parts[1], request); });
		else if (parts[0] == "places" && parts.size() == 3 && parts[2] == "visits")
			at("/api/v1/places/{uuid}/visits", [&] { return placeVisits(parts[1], request); });
		else if (parts[0] == "events" && parts.size() == 1)
			at("/api/v1/events", [&] { return events(request); });
		else if (parts[0] == "aircraft" && parts.size() == 1)
			at("/api/v1/aircraft", [&] { return aircraftList(request); });
		else if (parts[0] == "aircraft" && parts.size() == 2 && parts[1].size() == 6 &&
				 parts[1].find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
			at("/api/v1/aircraft/{icao}", [&] { return aircraft((uint32_t)std::strtoul(parts[1].c_str(), nullptr, 16)); });

		if (!answer)
		{
			response = Response::problem(404, "Not found", "No such resource: " + request.path);
			return true;
		}
		const Route *r = route(path);
		for (const auto &p : request.params())
			if (!r || !r->takes(p.first))
			{
				response = Response::problem(400, "Bad request", "\"" + p.first + "\" is not a parameter of " + path + ".");
				return true;
			}
		response = answer();
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
		std::map<int, int> vessels = heardLately();
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		registry.forEach([&](const StationRegistry::Station &s) {
			if (!view.allowsStation(s.id))
				return;
			w.beginObject();
			writeStation(w, s, tracker.stationPosition(s.id), vessels[s.id]);
			w.endObject();
			count++;
		});
		endList(w, count);
		w.finish();
		return Response::json(out);
	}

	Response ApiV1::station(int id)
	{
		if (!view.allowsStation(id))
			return Response::problem(404, "Not found", "No station " + std::to_string(id) + ".");
		const StationRegistry registry = stations();
		const StationRegistry::Station *s = registry.find(id);
		if (!s)
			return Response::problem(404, "Not found", "No station " + std::to_string(id) + ".");
		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		writeStation(w, *s, tracker.stationPosition(s->id), heardLately()[s->id]);
		w.endObject();
		w.finish();
		return Response::json(out);
	}

	// one station's figures, or with `station` -1 those of everything the
	// server shows: all of them, or its stations' added up
	Response ApiV1::statistics(int station)
	{
		if (station >= 0 && !view.allowsStation(station))
			return Response::problem(404, "Not found", "Station " + std::to_string(station) + " has heard nothing yet.");
		const std::vector<int> ids = station >= 0 ? std::vector<int>(1, station) : view.stations;
		const std::time_t now = std::time(nullptr);
		std::string out;
		JSON::Writer w(out);
		const auto write = [&](Figures &f) {
			w.beginObject();
			if (ids.empty())
				w.kv_null("stations");
			else
			{
				w.key("stations").beginArray();
				for (int id : ids)
					w.val(id);
				w.endArray();
			}
			w.kv("time", isoTime(now)).kv("messages_per_second", f.second.getAverage());
			writeTotal(w, "total", f.total);
			writeTotal(w, "session", f.session);
			writeLast(w, "last_minute", f.minute);
			writeLast(w, "last_hour", f.hour);
			writeLast(w, "last_day", f.day);
			w.key("history").beginObject();
			writeSlots(w, "second", f.second);
			writeSlots(w, "minute", f.minute);
			writeSlots(w, "hour", f.hour);
			writeSlots(w, "day", f.day);
			w.endObject();
			writeWeek(w, f);
			w.key("levels_by_distance").beginObject().kv("band_nm", Figures::LEVEL_BAND_NM);
			writeLevels(w, "total", f.levels_total);
			writeLevels(w, "session", f.levels_session);
			w.endObject().key("propagation").beginArray();
			for (const auto &d : f.propagation())
				w.beginObject().kv("day", isoTime(d.day)).kv("max_distance_nm", d.max_nm).kv("usual_nm", d.usual_nm).endObject();
			w.endArray().endObject();
		};
		if (ids.empty())
			tracker.withAll(write);
		else if (!tracker.withStations(ids, write))
		{
			std::string label;
			for (int id : ids)
				label += (label.empty() ? "" : ",") + std::to_string(id);
			return Response::problem(404, "Not found", (ids.size() == 1 ? "Station " : "Stations ") + label + " ha" + (ids.size() == 1 ? "s" : "ve") + " heard nothing yet.");
		}
		w.finish();
		return Response::json(out);
	}

	namespace
	{
		// What /api/v1/vessels asks of a vessel. A term is named after the field
		// it tests and takes the values that field shows; several values
		// separated by commas match any of them, and min_ and max_ bound a number.
		struct VesselQuery
		{
			std::string q;
			bool box = false, circle = false;
			float west = 0, south = 0, east = 0, north = 0, lat = 0, lon = 0, radius_nm = 10;
			std::vector<long long> mmsi, stations, classes, statuses, validated, repeat;
			bool has_min_speed = false, has_max_speed = false, has_min_distance = false, has_max_distance = false;
			float min_speed = 0, max_speed = 0, min_distance = 0, max_distance = 0;
			long long max_age = -1;

			static std::vector<std::string> split(const std::string &v)
			{
				std::vector<std::string> out;
				std::size_t pos = 0;
				while (pos <= v.size())
				{
					const std::size_t comma = v.find(',', pos);
					out.push_back(v.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
					if (comma == std::string::npos)
						break;
					pos = comma + 1;
				}
				return out;
			}

			static bool floats(const std::string &v, std::vector<float> &out, std::size_t n)
			{
				out.clear();
				for (const auto &part : split(v))
				{
					char *end = nullptr;
					const float f = std::strtof(part.c_str(), &end);
					if (part.empty() || !end || *end)
						return false;
					out.push_back(f);
				}
				return out.size() == n;
			}

			static bool real(const std::string &v, float &out)
			{
				std::vector<float> f;
				if (!floats(v, f, 1) || !(f[0] >= 0))
					return false;
				out = f[0];
				return true;
			}

			static bool wholes(const std::string &v, long long max, std::vector<long long> &out)
			{
				for (const auto &part : split(v))
				{
					long long x = 0;
					if (!Util::Parse::Digits(part, x, 10) || x > max)
						return false;
					out.push_back(x);
				}
				return true;
			}

			// names of a list, as their places in it
			static bool names(const std::string &v, const char *const *list, int n, std::vector<long long> &out)
			{
				for (const auto &part : split(v))
				{
					const int i = indexOf(list, n, part);
					if (i < 0)
						return false;
					out.push_back(i);
				}
				return true;
			}

			// 1 taken, 0 not a term of this list, -1 malformed with `error` saying why
			int set(const std::string &key, const std::string &value, std::string &error)
			{
				std::vector<float> f;
				if (key == "q")
					q = Util::Convert::toLowerCopy(value);
				else if (key == "bbox")
				{
					if (!floats(value, f, 4) || f[1] < -90 || f[3] > 90 || f[1] > f[3] || f[0] < -180 || f[2] > 180)
						error = "bbox takes west,south,east,north in degrees.";
					else
						west = f[0], south = f[1], east = f[2], north = f[3], box = true;
				}
				else if (key == "near")
				{
					if (!floats(value, f, 2) || !isValidCoord(f[0], f[1]))
						error = "near takes lat,lon in degrees.";
					else
						lat = f[0], lon = f[1], circle = true;
				}
				else if (key == "radius_nm")
				{
					if (!real(value, radius_nm) || radius_nm <= 0 || radius_nm > 1000)
						error = "radius_nm takes nautical miles, more than 0 and up to 1000.";
				}
				else if (key == "mmsi")
				{
					if (!wholes(value, 999999999, mmsi))
						error = "mmsi takes MMSIs separated by commas.";
				}
				else if (key == "stations")
				{
					if (!wholes(value, 2147483647, stations))
						error = "stations takes station numbers separated by commas.";
				}
				else if (key == "class")
				{
					if (!names(value, VESSEL_CLASSES, N_VESSEL_CLASSES, classes))
						error = "class takes " + listed(VESSEL_CLASSES, N_VESSEL_CLASSES) + ", separated by commas.";
				}
				else if (key == "nav_status")
				{
					if (!names(value, NAV_STATUSES, N_NAV_STATUSES, statuses))
						error = "nav_status takes " + listed(NAV_STATUSES, N_NAV_STATUSES) + ", separated by commas.";
				}
				else if (key == "validated")
				{
					if (!names(value, VALIDATED, 3, validated))
						error = "validated takes yes, no or pending, separated by commas.";
				}
				else if (key == "repeat")
				{
					if (!wholes(value, 3, repeat))
						error = "repeat takes 0 to 3, separated by commas.";
				}
				else if (key == "min_speed_kn" || key == "max_speed_kn" || key == "min_distance_nm" || key == "max_distance_nm")
				{
					const bool speed = key.find("speed") != std::string::npos, min = key[1] == 'i';
					float &v = speed ? (min ? min_speed : max_speed) : (min ? min_distance : max_distance);
					if (!real(value, v))
						error = key + " takes a number from 0.";
					else
						(speed ? (min ? has_min_speed : has_max_speed) : (min ? has_min_distance : has_max_distance)) = true;
				}
				else if (key == "max_age_s")
				{
					if (!Util::Parse::Digits(value, max_age, 10))
						error = "max_age_s takes whole seconds.";
				}
				else
					return 0;
				return error.empty() ? 1 : -1;
			}

			static bool contains(const char *field, const std::string &needle)
			{
				return Util::Convert::toLowerCopy(field).find(needle) != std::string::npos;
			}

			static bool has(const std::vector<long long> &list, long long v)
			{
				return std::find(list.begin(), list.end(), v) != list.end();
			}

			bool passes(const Ship &s, std::time_t now) const
			{
				if (!q.empty() && !contains(s.shipname, q) && !contains(s.callsign, q) &&
					std::to_string(s.mmsi).find(q) == std::string::npos && !(s.IMO > 0 && std::to_string(s.IMO) == q))
					return false;
				const bool placed = isValidCoord(s.lat, s.lon);
				if ((box || circle) && !placed)
					return false;
				if (box && (s.lat < south || s.lat > north || (west <= east ? (s.lon < west || s.lon > east) : (s.lon < west && s.lon > east))))
					return false;
				if (circle)
				{
					float d = 0;
					int bearing = 0;
					Util::Geodesy::distanceBearing(lat, lon, s.lat, s.lon, d, bearing);
					if (!(d <= radius_nm))
						return false;
				}
				if (!mmsi.empty() && !has(mmsi, s.mmsi))
					return false;
				if (!stations.empty())
				{
					bool heard = false;
					s.forEachHeard([&](int station, std::time_t t) { heard |= t >= now - Ship::STATION_WINDOW && has(stations, station); });
					if (!heard)
						return false;
				}
				if (!classes.empty() && !has(classes, classOf(s)))
					return false;
				if (!statuses.empty() && !(s.status >= 0 && s.status < N_NAV_STATUSES && has(statuses, s.status)))
					return false;
				if (!validated.empty() && !has(validated, validatedOf(s)))
					return false;
				if (!repeat.empty() && !has(repeat, s.getRepeat()))
					return false;
				const bool speed = s.speed != SPEED_UNDEFINED;
				if ((has_min_speed && !(speed && s.speed >= min_speed)) || (has_max_speed && !(speed && s.speed <= max_speed)))
					return false;
				const bool distance = placed && s.distance != DISTANCE_UNDEFINED;
				if ((has_min_distance && !(distance && s.distance >= min_distance)) || (has_max_distance && !(distance && s.distance <= max_distance)))
					return false;
				if (max_age >= 0 && now - s.last_signal > (std::time_t)max_age)
					return false;
				return true;
			}
		};

		// a row of a list kept newest first: when it was heard, and what it is
		struct Heard
		{
			std::time_t time;
			uint32_t id;
			bool operator<(const Heard &o) const { return time != o.time ? time > o.time : id < o.id; }
		};
	}

	// A listing pages from the newest heard to the oldest; a vessel heard
	// again meanwhile has left its place in it and comes with `poll`, which
	// asks for what changed since the listing began. A cursor with only a
	// time is such a poll.
	Response ApiV1::vessels(const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		VesselQuery query;
		std::string carried = "&limit=" + std::to_string(limit);
		for (const auto &p : request.params())
		{
			if (p.first == "limit" || p.first == "cursor")
				continue;
			std::string error;
			const int taken = query.set(p.first, p.second, error);
			if (taken < 0)
				return Response::problem(400, "Bad request", error);
			if (taken == 0)
				return Response::problem(400, "Bad request", "\"" + p.first + "\" is not a parameter of /api/v1/vessels.");
			carried += "&" + p.first + "=" + urlEncode(p.second);
		}

		DB &db = tracker.ships();
		const std::time_t now = std::time(nullptr), shown = now - db.timeHistory();
		std::time_t start = now, floor = shown;
		bool full = true;
		Heard after{0, 0};
		bool paging = false;
		const std::string from = request.param("cursor");
		std::vector<long long> c;
		if (parseCursor(from, "v", 1, c))
		{
			// a poll: a client holding ship data older than the last reset starts over
			full = (std::time_t)c[0] <= db.fullRefreshAt();
			floor = full ? shown : MAX((std::time_t)c[0], shown);
		}
		else if (parseCursor(from, "v", 4, c))
		{
			full = false, paging = true;
			start = (std::time_t)c[0], floor = MAX((std::time_t)c[1], shown);
			after = Heard{(std::time_t)c[2], (uint32_t)c[3]};
		}
		else if (!from.empty())
			return badCursor();

		// the rows of this page under one lock, written under the next: a ship
		// gone between the two is left out, one changed is written as it is now
		std::vector<Heard> rows;
		db.forEachRecent(now, floor, [&](int, const Ship &s, long) {
			const Heard h{s.last_signal, s.mmsi};
			if ((paging && !(after < h)) || !view.passes(s, now) || !query.passes(s, now))
				return;
			rows.push_back(h);
		});
		std::sort(rows.begin(), rows.end());
		const bool more = (long long)rows.size() > limit;
		if (more)
			rows.resize((std::size_t)limit);
		std::vector<uint32_t> mmsi;
		for (const auto &h : rows)
			mmsi.push_back(h.id);

		const auto aboard = tracker.riding();
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		db.withShips(mmsi.data(), mmsi.size(), [&](std::size_t, const Ship *s) {
			if (!s || !view.passes(*s, now) || !query.passes(*s, now))
				return;
			auto r = aboard.find(s->mmsi);
			writeVessel(w, *s, now, db.binaryBadgeHeld(s->mmsi, now), r == aboard.end() ? -1 : r->second);
			count++;
		});
		const std::string next = more ? "/api/v1/vessels?cursor=" + cursor("v", {(long long)start, (long long)floor, (long long)rows.back().time, (long long)rows.back().id}) + carried : "";
		beginMeta(w, count, next);
		// a second of overlap, as messages share a second
		w.kv("poll", "/api/v1/vessels?cursor=" + cursor("v", {(long long)start - 1}) + carried)
			.kv("full", full).kv("time", isoTime(now)).kv("expire_s", db.timeHistory()).endObject().endObject();
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
			if (v.shown(e.metadata->requiresStop()))
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
			if (!view.passes(s, now))
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
		since = std::max(since, view.trackFloor(std::time(nullptr)));
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
			w.finish();
			Response r = Response::json(out);
			r.type = "application/geo+json";
			return r;
		}
		beginList(w);
		for (const auto &p : points)
		{
			w.beginObject().kv("time", isoTime(p.time)).kv("lat", p.lat).kv("lon", p.lon);
			// a ship lying still is one point that lasts
			if (p.dur)
				w.kv("until", isoTime(p.end()));
			w.kv_or_null("speed_kn", p.sog / 10.0, p.sog != PathStore::NA);
			w.kv_or_null("course_deg", p.cog / 10.0, p.cog != PathStore::NA);
			w.kv_or_null("heading_deg", (int)p.hdg, p.hdg != PathStore::NA);
			w.endObject();
		}
		endList(w, points.size());
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
			if (v.shown(e.metadata->requiresStop()))
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
		endList(w, rows.size());
		w.finish();
		return Response::json(out);
	}

	// by name, then by id; the cursor is the id of the last place given
	Response ApiV1::places(const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		const std::string type = request.param("type");
		PlaceKind kind = PlaceKind::Port;
		if (!type.empty() && !PlaceMetadata::parseKind(type, kind))
			return Response::problem(400, "Bad request", "type must be a place type, such as port, berth or guardzone.");
		// a text in the name or the UN/LOCODE
		const std::string q = Util::Convert::toLowerCopy(request.param("q"));
		const auto index = tracker.places();

		typedef std::pair<std::string, const PlaceIndex::Entry *> Row;
		const auto key = [](const Row &r) { return std::make_pair(r.first, r.second->metadata->uuid); };
		std::pair<std::string, std::string> after;
		const std::string from = request.param("cursor");
		if (!from.empty())
		{
			if (from.compare(0, 2, "p_") != 0)
				return badCursor();
			const PlaceIndex::Entry *last = index ? index->resolve(from.substr(2)) : nullptr;
			if (!last)
				return gone("The place this cursor ends at is no longer in the catalogue.");
			after = key(Row(Util::Convert::toLowerCopy(last->metadata->name), last));
		}

		std::vector<Row> rows;
		if (index)
			for (const auto &e : index->entries)
			{
				if (!e.live() || (!type.empty() && !e.metadata->is(kind)))
					continue;
				Row r(Util::Convert::toLowerCopy(e.metadata->name), &e);
				if (!q.empty() && r.first.find(q) == std::string::npos && Util::Convert::toLowerCopy(e.metadata->code).find(q) == std::string::npos)
					continue;
				if (!from.empty() && !(after < key(r)))
					continue;
				rows.push_back(r);
			}
		std::sort(rows.begin(), rows.end(), [&](const Row &a, const Row &b) { return key(a) < key(b); });
		const bool more = (long long)rows.size() > limit;
		if (more)
			rows.resize((std::size_t)limit);

		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : rows)
		{
			w.beginObject();
			writePlace(w, *index, *r.second);
			w.endObject();
		}
		std::string next;
		if (more)
			next = "/api/v1/places?cursor=p_" + rows.back().second->metadata->uuid + "&limit=" + std::to_string(limit) +
				   (type.empty() ? "" : "&type=" + urlEncode(type)) + (q.empty() ? "" : "&q=" + urlEncode(q));
		endList(w, rows.size(), next);
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
			if (at.id == e->id && DB::insideNow(s, v, at.metadata->requiresStop(), now) && view.passes(s, now))
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
		std::string feature;
		if (view.place_geometry && !e->polygons->empty() && e->readFeature(feature))
			try
			{
				JSON::Parser parser(JSON_DICT_SETTING);
				parser.setPreserveUnknown(true);
				auto doc = parser.parse(feature);
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

	// by MMSI; the cursor is the last MMSI given
	Response ApiV1::placeVessels(const std::string &uuid, const Request &request)
	{
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		std::vector<long long> c;
		const std::string from = request.param("cursor");
		if (!from.empty() && !parseCursor(from, "in", 1, c))
			return badCursor();
		const long long after = c.empty() ? -1 : c[0];
		const auto index = tracker.places();
		const PlaceIndex::Entry *e = index ? index->resolve(uuid) : nullptr;
		if (!e)
			return Response::problem(404, "Not found", "No place " + uuid + ".");
		const std::time_t now = std::time(nullptr);
		std::vector<std::pair<uint32_t, std::string>> rows;
		tracker.ships().forEachVisit([&](const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &at) {
			if (at.id != e->id || (long long)s.mmsi <= after || !DB::insideNow(s, v, at.metadata->requiresStop(), now) || !view.passes(s, now))
				return;
			std::string row;
			JSON::Writer w(row);
			w.beginObject().kv("mmsi", (long long)s.mmsi);
			textOrNull(w, "name", s.shipname);
			writeVisit(w, v, at);
			w.endObject();
			w.finish();
			rows.push_back(std::make_pair(s.mmsi, row));
		});
		std::sort(rows.begin(), rows.end());
		const bool more = (long long)rows.size() > limit;
		if (more)
			rows.resize((std::size_t)limit);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : rows)
			w.raw_val(r.second);
		const std::string next = more ? "/api/v1/places/" + urlEncode(uuid) + "/vessels?cursor=" + cursor("in", {(long long)rows.back().first}) + "&limit=" + std::to_string(limit) : "";
		endList(w, rows.size(), next);
		w.finish();
		return Response::json(out);
	}

	// newest first; the cursor is when the last visit given began, and its vessel
	Response ApiV1::placeVisits(const std::string &uuid, const Request &request)
	{
		long long hours = 0, limit = 0;
		if (!request.integer("hours", hours, 24, 1, 30 * 24))
			return Response::problem(400, "Bad request", "hours must be between 1 and 720.");
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		std::vector<long long> c;
		const std::string from = request.param("cursor");
		if (!from.empty() && !parseCursor(from, "vis", 2, c))
			return badCursor();
		const Heard after = c.empty() ? Heard{0, 0} : Heard{(std::time_t)c[0], (uint32_t)c[1]};
		const auto index = tracker.places();
		const PlaceIndex::Entry *e = index ? index->resolve(uuid) : nullptr;
		if (!e)
			return Response::problem(404, "Not found", "No place " + uuid + ".");
		const std::time_t now = std::time(nullptr), since = now - (std::time_t)hours * 3600;
		struct Row
		{
			Heard key;
			std::string name;
			VisitTracker::Visit v;
		};
		std::vector<Row> rows;
		// the stays under way, and those that ended within the window
		tracker.ships().forEachVisit([&](const Ship &s, const VisitTracker::Visit &v, const PlaceIndex::Entry &at) {
			if (at.id != e->id || !v.shown(at.metadata->requiresStop()) || !view.passes(s, now))
				return;
			if (!v.inside() && (std::time_t)MAX(v.entered, v.exited) < since)
				return;
			const Heard key{(std::time_t)(v.entered ? v.entered : v.exited), s.mmsi};
			if (!c.empty() && !(after < key))
				return;
			rows.push_back({key, s.shipname, v});
		});
		std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.key < b.key; });
		const bool more = (long long)rows.size() > limit;
		if (more)
			rows.resize((std::size_t)limit);
		std::string out;
		JSON::Writer w(out);
		beginList(w);
		for (const auto &r : rows)
		{
			w.beginObject().kv("mmsi", (long long)r.key.id);
			textOrNull(w, "name", r.name);
			writeVisit(w, r.v, *e);
			w.endObject();
		}
		const std::string next = more ? "/api/v1/places/" + urlEncode(uuid) + "/visits?cursor=" + cursor("vis", {(long long)rows.back().key.time, (long long)rows.back().key.id}) +
											"&hours=" + std::to_string(hours) + "&limit=" + std::to_string(limit)
									  : "";
		endList(w, rows.size(), next);
		w.finish();
		return Response::json(out);
	}

	// Aircraft carry no station: a server that shows only some stations shows none
	bool ApiV1::aircraftShown(Response &refusal)
	{
		if (view.stations.empty())
			return true;
		refusal = Response::problem(404, "Not found", "Aircraft are not kept by station, and this server shows only some stations.");
		return false;
	}

	// paged and polled as the vessels are
	Response ApiV1::aircraftList(const Request &request)
	{
		Response refusal;
		if (!aircraftShown(refusal))
			return refusal;
		long long limit = 0;
		if (!request.integer("limit", limit, 100, 1, LIST_MAX))
			return badLimit();
		const std::time_t now = std::time(nullptr);
		std::time_t start = now, floor = 0;
		Heard after{0, 0};
		bool paging = false;
		const std::string from = request.param("cursor");
		std::vector<long long> c;
		if (parseCursor(from, "a", 1, c))
			floor = (std::time_t)c[0];
		else if (parseCursor(from, "a", 4, c))
		{
			paging = true;
			start = (std::time_t)c[0], floor = (std::time_t)c[1];
			after = Heard{(std::time_t)c[2], (uint32_t)c[3]};
		}
		else if (!from.empty())
			return badCursor();

		std::vector<Heard> rows;
		tracker.planes().forEachAircraft(floor, [&](const Plane::ADSB &p, std::time_t heard) {
			const Heard h{heard, (uint32_t)(p.hexident & 0xFFFFFF)};
			if (!paging || after < h)
				rows.push_back(h);
		});
		std::sort(rows.begin(), rows.end());
		const bool more = (long long)rows.size() > limit;
		if (more)
			rows.resize((std::size_t)limit);
		std::map<uint32_t, std::size_t> place;
		for (std::size_t i = 0; i < rows.size(); i++)
			place[rows[i].id] = i;
		std::vector<std::string> written(rows.size());
		tracker.planes().forEachAircraft(floor, [&](const Plane::ADSB &p, std::time_t heard) {
			auto it = place.find((uint32_t)(p.hexident & 0xFFFFFF));
			if (it == place.end())
				return;
			JSON::Writer w(written[it->second]);
			writeAircraft(w, p, heard, now);
			w.finish();
		});

		std::string out;
		JSON::Writer w(out);
		beginList(w);
		std::size_t count = 0;
		for (const auto &row : written)
			if (!row.empty())
			{
				w.raw_val(row);
				count++;
			}
		const std::string tail = "&limit=" + std::to_string(limit);
		const std::string next = more ? "/api/v1/aircraft?cursor=" + cursor("a", {(long long)start, (long long)floor, (long long)rows.back().time, (long long)rows.back().id}) + tail : "";
		beginMeta(w, count, next);
		w.kv("poll", "/api/v1/aircraft?cursor=" + cursor("a", {(long long)start - 1}) + tail).kv("time", isoTime(now)).endObject().endObject();
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

	// Without a cursor the newest page. Every event's id is a cursor, so a
	// client that got one from a webhook reads on from there.
	Response ApiV1::events(const Request &request)
	{
		// the installation's own events are the system's to tell, and a ship's
		// only when the server shows the ship
		return pageEvents(request, tracker.journal(), "/api/v1/events", [this](const Journal::Entry &e) {
			return e.event.kind != Tracking::Kind::SYSTEM && shows(e.event.from);
		});
	}
}
