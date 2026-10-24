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

#include <ctime>

#include "GeoJSON.h"

namespace Services
{
	namespace
	{
		// a ship as a Point, with what the viewer's table shows of it
		void writeShip(JSON::Writer &w, const Ship &ship, bool station_known)
		{
			w.beginObject()
				.kv("type", "Feature")
				.key("properties")
				.beginObject()
				.kv("mmsi", ship.mmsi);
			if (station_known && ship.distance != DISTANCE_UNDEFINED)
				w.kv("distance", ship.distance).kv("bearing", ship.angle);
			else
				w.kv_null("distance").kv_null("bearing");
			w.kv_unless("level", ship.level, LEVEL_UNDEFINED)
				.kv("count", ship.count)
				.kv_unless("ppm", ship.ppm, PPM_UNDEFINED)
				.kv("group_mask", ship.group_mask)
				.kv("approx", (bool)ship.getApproximate())
				.kv_unless("heading", ship.heading, HEADING_UNDEFINED)
				.kv_unless("cog", ship.cog, COG_UNDEFINED)
				.kv_unless("speed", ship.speed, SPEED_UNDEFINED)
				.kv_unless("to_bow", ship.to_bow, DIMENSION_UNDEFINED)
				.kv_unless("to_stern", ship.to_stern, DIMENSION_UNDEFINED)
				.kv_unless("to_starboard", ship.to_starboard, DIMENSION_UNDEFINED)
				.kv_unless("to_port", ship.to_port, DIMENSION_UNDEFINED)
				.kv("shiptype", ship.shiptype)
				.kv("mmsi_type", ship.mmsi_type)
				.kv("shipclass", ship.shipclass)
				.kv("validated", ship.getValidated())
				.kv("msg_type", ship.msg_type)
				.kv("channels", ship.getChannels())
				.kv("country", ship.country_code)
				.kv("status", ship.status)
				.kv("draught", ship.draught)
				.kv_unless("eta_month", (int)ship.month, ETA_MONTH_UNDEFINED)
				.kv_unless("eta_day", (int)ship.day, ETA_DAY_UNDEFINED)
				.kv_unless("eta_hour", (int)ship.hour, ETA_HOUR_UNDEFINED)
				.kv_unless("eta_minute", (int)ship.minute, ETA_MINUTE_UNDEFINED)
				.kv_unless("imo", ship.IMO, IMO_UNDEFINED)
				.kv("callsign", ship.callsign);

			if (ship.getVirtualAid())
				w.kv("shipname", ship.shipname, " [V]");
			else
				w.kv("shipname", ship.shipname);

			w.kv("destination", ship.destination)
				.kv("last_signal", ship.last_signal)
				.endObject()
				.key("geometry")
				.beginObject()
				.kv("type", "Point")
				.key("coordinates")
				.beginArray()
				.val(ship.lon)
				.val(ship.lat)
				.endArray()
				.endObject()
				.endObject();
		}

		// a ship's track as a LineString, newest point first, with when each point
		// began and ended; under the lock of the walk that hands over `ptr`
		void writeTrack(JSON::Writer &w, const DB &db, int ptr, const Ship &ship, std::time_t now, std::time_t oldest)
		{
			w.beginObject()
				.kv("type", "Feature")
				.key("geometry")
				.beginObject()
				.kv("type", "LineString")
				.key("coordinates")
				.beginArray();
			db.forEachTrackPointHeld(ptr, now, oldest, [&](const PathStore::Point &p) { w.beginArray().val(p.lon).val(p.lat).endArray(); });
			w.endArray()
				.endObject()
				.key("properties")
				.beginObject()
				.kv("mmsi", ship.mmsi)
				.key("timestamps_start")
				.beginArray();
			db.forEachTrackPointHeld(ptr, now, oldest, [&](const PathStore::Point &p) { w.val(p.time); });
			w.endArray().key("timestamps_end").beginArray();
			db.forEachTrackPointHeld(ptr, now, oldest, [&](const PathStore::Point &p) { w.val(p.end()); });
			w.endArray().endObject().endObject();
		}

		// the ships on the map, as long as the tracker keeps them there
		std::string ships(DB &db, const DB::Keep &keep)
		{
			const int history = db.timeHistory();
			const std::time_t now = std::time(nullptr);
			std::string out;
			{
				JSON::Writer w(out, 65536);
				w.beginObject().kv("type", "FeatureCollection").kv("time_span", history).key("features").beginArray();
				db.forEachRecent(now, now - history, [&](int, const Ship &ship, long int) {
					if (!keep || keep(ship))
						writeShip(w, ship, isValidCoord(db.getLat(), db.getLon()));
				});
				w.endArray().endObject();
			}
			return out;
		}

		// their tracks, back to `oldest` at most
		std::string tracks(DB &db, const DB::Keep &keep, std::time_t oldest)
		{
			const int history = db.timeHistory();
			const std::time_t now = std::time(nullptr);
			std::string out;
			{
				JSON::Writer w(out, 65536);
				w.beginObject().kv("type", "FeatureCollection").key("features").beginArray();
				db.forEachRecent(now, now - history, [&](int ptr, const Ship &ship, long int) {
					if (!keep || keep(ship))
						writeTrack(w, db, ptr, ship, now, oldest);
				});
				w.endArray().endObject().raw("\n\n");
			}
			return out;
		}
	}

	int GeoJSON::scope(const Request &request) const
	{
		return request.path == "/geojson" || request.path == "/allpath.geojson" ? (int)ApiKey::DATA : NOT_MINE;
	}

	bool GeoJSON::handle(const Request &request, Response &response)
	{
		if (scope(request) == NOT_MINE)
			return false;
		if (request.method != "GET")
		{
			response = Response::problem(405, "Method not allowed", "The ships are read here, not written.");
			response.header = "Allow: GET, HEAD";
			return true;
		}
		DB::Keep keep;
		if (view.any())
		{
			const ViewFilter within = view;
			keep = [within](const Ship &s) { return within.passes(s, std::time(nullptr)); };
		}
		DB &db = tracker.ships();
		response.type = "application/geo+json";
		response.body = request.path == "/geojson" ? ships(db, keep) : tracks(db, keep, view.trackFloor(std::time(nullptr)));
		response.cors = true;
		return true;
	}
}
