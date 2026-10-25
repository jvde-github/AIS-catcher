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

#include "Kml.h"

namespace Services
{
	namespace
	{
		struct SpriteRect
		{
			int x, y, w, h;
		};

		// Offsets into icons.png. Indexed by ShippingClass; vessel rows sit at y=68 at
		// rest and SPRITE_UNDERWAY_DY lower when moving.
		const SpriteRect SPRITES[] = {
			{120, 68, 20, 20}, // CLASS_OTHER
			{120, 68, 20, 20}, // CLASS_UNKNOWN
			{0, 68, 20, 20},   // CLASS_CARGO
			{140, 68, 20, 20}, // CLASS_B
			{40, 68, 20, 20},  // CLASS_PASSENGER
			{60, 68, 20, 20},  // CLASS_SPECIAL
			{80, 68, 20, 20},  // CLASS_TANKER
			{100, 68, 20, 20}, // CLASS_HIGHSPEED
			{140, 68, 20, 20}, // CLASS_FISHING
			{0, 25, 25, 25},   // CLASS_PLANE
			{0, 0, 25, 25},    // CLASS_HELICOPTER
			{20, 50, 20, 20},  // CLASS_STATION
			{0, 50, 20, 20},   // CLASS_ATON
			{40, 50, 20, 20},  // CLASS_SARTEPIRB
		};

		static_assert(sizeof(SPRITES) / sizeof(SPRITES[0]) == CLASS_SARTEPIRB + 1,
					  "SPRITES out of sync with ShippingClass");

		const int SPRITE_UNDERWAY_DY = 20;

		void appendSprite(std::string &out, const Ship &ship)
		{
			if (ship.shipclass < 0 || ship.shipclass > CLASS_SARTEPIRB)
				return;

			SpriteRect s = SPRITES[ship.shipclass];
			if (ship.shipclass <= CLASS_FISHING && ship.speed != SPEED_UNDEFINED && ship.speed > 0.5)
				s.y += SPRITE_UNDERWAY_DY;

			out += "<x>" + std::to_string(s.x) + "</x><y>" + std::to_string(s.y) + "</y><w>" + std::to_string(s.w) + "</w><h>" +
				   std::to_string(s.h) + "</h>";
		}

		void appendXMLEscaped(std::string &out, const std::string &s)
		{
			for (char c : s)
				switch (c)
				{
				case '&':
					out += "&amp;";
					break;
				case '<':
					out += "&lt;";
					break;
				case '>':
					out += "&gt;";
					break;
				default:
					out += c;
				}
		}

		// a ship as a placemark with its icon, turned to its course
		void shipKML(std::string &out, const Ship &ship)
		{
			if (!isValidCoord(ship.lat, ship.lon))
				return;

			std::string shipNameStr(ship.shipname);

			const std::string name = !shipNameStr.empty() ? shipNameStr : std::to_string(ship.mmsi);
			const std::string styleId = "style" + std::to_string(ship.mmsi);
			const std::string coordinates = std::to_string(ship.lon) + "," + std::to_string(ship.lat) + ",0";

			out += "<Style id=\"" + styleId + "\"><IconStyle><scale>1</scale><heading>" + std::to_string(ship.cog) +
				   "</heading><Icon><href>/icons.png</href>";
			appendSprite(out, ship);
			out += "</Icon></IconStyle></Style><Placemark><name>";
			appendXMLEscaped(out, name);
			out += "</name><styleUrl>#" + styleId + "</styleUrl><Point><coordinates>" + coordinates + "</coordinates></Point></Placemark>";
		}
	}

	int Kml::scope(const Request &request) const
	{
		return request.path == "/kml" ? (int)ApiKey::DATA : NOT_MINE;
	}

	bool Kml::handle(const Request &request, Response &response)
	{
		if (scope(request) == NOT_MINE)
			return false;
		if (request.method != "GET")
		{
			response = Response::problem(405, "Method not allowed", "The ships are read here, not written.");
			response.header = "Allow: GET, HEAD";
			return true;
		}
		const std::time_t now = std::time(nullptr);
		DB &db = tracker.ships();
		std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns = \"http://www.opengis.net/kml/2.2\"><Document>";
		db.forEachRecent(now, now - db.timeHistory(), [&](int, const Ship &ship, long int) {
			if (!view.any() || view.passes(ship, now))
				shipKML(out, ship);
		});
		out += "</Document></kml>";
		response.type = "application/vnd.google-earth.kml+xml";
		response.body = std::move(out);
		response.cors = true;
		return true;
	}
}
