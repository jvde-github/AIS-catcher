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


#include "Ui.h"
#include "UiFormat.h"
#include "Parse.h"
#include "AIS-catcher.h"
#include "HTTPServer.h"
#include "Helper.h"
#include "Logger.h"
#include "SHA256.h"
#include "Streams.h"
#include "WebDB.h"

#include <climits>

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Services
{
	namespace
	{

		// replay comes in blocks of ten minutes, looking back at most a week
		const std::time_t REPLAY_BLOCK = 600;
		const long long MAX_REPLAY_LOOKBACK = 7 * 24 * 3600;
		bool validReplayBlock(long long block)
		{
			return block > 0 && block <= (long long)(std::time(nullptr) / REPLAY_BLOCK);
		}

		long long toInt(const std::string &s, long long def = 0)
		{
			char *end = nullptr;
			const long long v = std::strtoll(s.c_str(), &end, 10);
			return end == s.c_str() ? def : v;
		}

		int toMMSI(const std::string &s)
		{
			const long long m = toInt(s, -1);
			return m >= 1 && m <= 999999999 ? (int)m : -1;
		}

		// a bare query, as the viewer sends a vessel or a list of them: the part before the first '&'
		std::string bare(const std::string &query)
		{
			return query.substr(0, query.find('&'));
		}

		// the aircraft list as PlaneDB writes it, empty
		std::string noPlanes(const char *list)
		{
			return "{\"count\":0,\"time\":" + std::to_string((long long)std::time(nullptr)) + ",\"" + list + "\":[]}";
		}
	}

	Ui::Ui(const Context &context, std::function<int()> clients)
		: tracker(context.tracker), spec(context.spec), stations(context.stations), options(context.spec.viewer), view(context.spec.view),
		  engine(context.engine), clients(std::move(clients))
	{
		for (const auto &p : options.plugins)
			plugins.addPlugin(p);
		for (const auto &s : options.styles)
			plugins.addStyle(s);
		for (const auto &d : options.plugin_dirs)
			plugins.addDir(d);
		if (!options.about.empty())
			plugins.setAbout(options.about);
		openTiles(options.mbtiles, false, true);
		openTiles(options.mboverlay, true, true);
		openTiles(options.fstiles, false, false);
		openTiles(options.fsoverlay, true, false);
	}

	// Each setting a list of paths, comma separated as the command line has them;
	// a layer is named by a hash of its path, so its name stays across restarts
	void Ui::openTiles(const std::vector<std::string> &lists, bool overlay, bool mbtiles)
	{
		for (const auto &list : lists)
		{
			std::stringstream ss(list);
			std::string path;
			while (std::getline(ss, path, ','))
			{
				const std::size_t a = path.find_first_not_of(" \t"), b = path.find_last_not_of(" \t");
				if (a == std::string::npos)
					continue;
				path = path.substr(a, b - a + 1);
				std::shared_ptr<MapTiles> source;
#if HASSQLITE
				if (mbtiles)
					source = std::make_shared<MBTilesSupport>();
#endif
				if (!mbtiles)
					source = std::make_shared<FileSystemTiles>();
				if (!source)
					continue;
				if (!source->open(path))
				{
					Error() << "Failed to load " << (mbtiles ? "MBTiles" : "FileSystemTiles") << " from: " << path;
					continue;
				}
				source->setLayerID(Util::SHA256::hex(path + (overlay ? "|overlay" : "|base")).substr(0, 16));
				maps.push_back({source, overlay});
			}
		}
	}

	// "/tiles/<layer>/<z>/<x>/<y>"
	bool Ui::tile(const std::string &path, Response &response)
	{
		std::vector<std::string> segment;
		std::stringstream ss(path);
		std::string s;
		while (std::getline(ss, s, '/'))
			if (!s.empty())
				segment.push_back(s);
		const int z = segment.size() == 5 ? (int)toInt(segment[2], -1) : -1, x = segment.size() == 5 ? (int)toInt(segment[3], -1) : -1,
				  y = segment.size() == 5 ? (int)toInt(segment[4], -1) : -1;
		response.type = "text/plain";
		if (segment.size() != 5 || segment[0] != "tiles" || z < 0 || x < 0 || y < 0)
		{
			response.status = 400;
			response.body = "Invalid Tile Request";
			return true;
		}
		for (const auto &m : maps)
		{
			if (m.source->getLayerID() != segment[1])
				continue;
			std::string type;
			const std::vector<unsigned char> &data = m.source->getTile(z, x, y, type);
			if (data.empty())
				continue;
			response.type = type;
			response.raw = (const char *)data.data();
			response.raw_size = data.size();
			response.cache = true;
			return true;
		}
		response.status = 404;
		response.body = "Tile not found";
		return true;
	}

	// the viewer's pages are anyone's; its log is the system's, as /api/v1/system/log is
	int Ui::scope(const Request &request) const
	{
		const std::string &p = request.path;
		if (p == "/api/v1" || p.compare(0, 8, "/api/v1/") == 0)
			return NOT_MINE;
		return p == "/api/log" && options.log ? (int)ApiKey::SYSTEM : OPEN;
	}

	bool Ui::handle(const Request &request, Response &response)
	{
		// the decoder takes its NMEA as a body; everything else is read
		if ((request.method != "GET" && !(request.method == "POST" && request.path == "/api/decode")) || request.path.empty() ||
			request.path[0] != '/' || scope(request) == NOT_MINE)
			return false;

		// this server's settings, then the station owner's plugins
		if (request.path == "/custom/plugins.js")
		{
			response.type = "application/javascript";
			response.body = "window.__SERVER_CONFIG__ = " + pageConfig() + ";\n" + plugins.script();
			return true;
		}
		if (request.path == "/custom/config.css")
		{
			response.type = "text/css";
			response.body = plugins.getStylesheets();
			return true;
		}
		if (request.path == "/about.md")
		{
			response.type = "text/markdown";
			response.body = plugins.getAbout();
			return true;
		}
		if (request.path.compare(0, 7, "/tiles/") == 0)
			return tile(request.path, response);
		if (request.path.compare(0, 5, "/api/") == 0 || request.path == "/ships.json" || request.path == "/stat.json")
		{
			const bool answered = viewerData(request, response);
			response.cors = true;
			return answered;
		}

		auto it = WebDB::files.find(request.path == "/" ? "index.html" : request.path.substr(1));
		if (it == WebDB::files.end())
			return false;
		const WebDB::FileData &file = it->second;
		response.type = file.mime_type;
		response.raw = (const char *)file.data;
		response.raw_size = file.size;
		response.gzipped = true;
		// the page itself is read again on every visit, so a new version reaches the browser
		response.cache = response.type != "text/html";
		return true;
	}

	bool Ui::drawnStation(const std::vector<int> &focus, StationRegistry::Station &at)
	{
		// a tracker that measures from a position of its own draws that: the ship table does it
		if (tracker.trackingConfig().fixed)
			return false;
		const StationRegistry registry = stations();
		int only = -1, count = 0;
		registry.forEach([&](const StationRegistry::Station &s) {
			if (!view.allowsStation(s.id))
				return;
			only = s.id;
			count++;
		});
		const int id = focus.size() == 1 ? focus[0] : count == 1 ? only : -1;
		const StationRegistry::Station *s = id >= 0 ? registry.find(id) : nullptr;
		if (!s || !s->share_loc)
			return false;
		const Tracker::Position now = tracker.stationPosition(id);
		at = *s;
		at.lat = now.lat;
		at.lon = now.lon;
		return true;
	}

	std::vector<int> Ui::figuresOf(const std::vector<int> &focus) const
	{
		return focus.empty() ? view.stations : focus;
	}

	// the stations the page looks through, among the server's; none for all of them
	std::vector<int> Ui::focusOf(const Request &request)
	{
		std::vector<int> ids;
		const std::string asked = request.param("stations");
		if (asked.empty())
			return ids;
		const StationRegistry registry = stations();
		std::vector<std::string> parts;
		Util::Parse::Split(asked, ',', parts);
		for (const auto &p : parts)
		{
			const int id = (int)toInt(p, -1);
			if (id >= 0 && view.allowsStation(id) && registry.find(id) && std::find(ids.begin(), ids.end(), id) == ids.end())
				ids.push_back(id);
		}
		return ids;
	}

	DB::Keep Ui::keepFor(const std::vector<int> &focus)
	{
		const bool scoped = view.any();
		if (focus.empty() && !scoped)
			return DB::Keep();
		const std::time_t now = std::time(nullptr), from = now - tracker.ships().timeHistory();
		const ViewFilter within = view;
		return [focus, from, now, scoped, within](const Ship &s) {
			if (scoped && !within.passes(s, now))
				return false;
			if (focus.empty())
				return true;
			for (int id : focus)
				if (s.heardSince(id, from))
					return true;
			return false;
		};
	}

	// The viewer's own endpoints, through the station the page has in focus
	bool Ui::viewerData(const Request &request, Response &response)
	{
		const std::string &p = request.path;
		DB &db = tracker.ships();
		const auto since = [&]() { return (std::time_t)toInt(request.param("since")); };
		const std::vector<int> focus = focusOf(request);
		const DB::Keep keep = keepFor(focus);
		// how far back the view lets tracks go
		const std::time_t oldest = view.trackFloor(std::time(nullptr));
		// one vessel, when the focus shows it
		const auto shows = [&](int mmsi) {
			bool ok = !keep;
			if (keep)
				db.withShip((uint32_t)mmsi, [&](const Ship &s, int) { ok = keep(s); });
			return ok;
		};

		if (p == "/api/config.json")
			response = Response::json(pageConfig());
		else if (p == "/api/ships_array.json")
		{
			StationRegistry::Station at;
			const bool drawn = drawnStation(focus, at);
			response = Response::json(UiFormat::shipsCompact(db, false, since(), configVersion(), keep, drawn ? &at : nullptr, true));
		}
		else if (p == "/api/ships_table.json")
			response = Response::json(UiFormat::shipsTable(db, since(), keep));
		else if (p == "/api/planes_array.json")
			// aircraft are not kept by station: a server of some stations has none
			response = Response::json(view.stations.empty() ? UiFormat::planesCompact(tracker.planes(), since()) : noPlanes("values"));
		else if (p == "/api/ship.json" || p == "/api/vessel")
		{
			const int mmsi = toMMSI(p == "/api/vessel" ? bare(request.query) : request.param("mmsi"));
			const std::string vessel = mmsi > 0 && shows(mmsi) ? UiFormat::ship(db, mmsi) : std::string("{}");
			response = Response::json(mmsi < 0 ? "{\"error\":\"Invalid MMSI\"}" : vessel == "{}" ? "{\"error\":\"Vessel not found\"}" : vessel);
		}
		else if (p == "/api/changes.json")
		{
			const int mmsi = toMMSI(bare(request.query));
			response = Response::json(mmsi < 0 ? std::string("{\"error\":\"Invalid MMSI\"}") : shows(mmsi) ? UiFormat::changes(db, mmsi) : std::string("[]"));
		}
		else if (p == "/api/path.json")
		{
			// up to a hundred vessels, joined by commas
			std::string out;
			JSON::Writer w(out);
			w.beginObject();
			std::stringstream ss(bare(request.query));
			std::string item;
			for (int n = 0; std::getline(ss, item, ',') && n < 100; n++)
			{
				const int mmsi = toMMSI(item);
				if (mmsi > 0 && shows(mmsi))
					w.key((unsigned)mmsi).raw_val(UiFormat::path(db, mmsi, oldest));
			}
			w.endObject();
			w.finish();
			response = Response::json(out);
		}
		else if (p == "/api/allpath.json")
			response = Response::json(since() > 0 ? UiFormat::allPathsSince(db, MAX(since(), oldest), keep) : UiFormat::allPaths(db, keep, oldest));
		else if (p == "/api/mapobjects.json")
		{
			// the stations the server shows; its other objects are every vessel's broadcasts
			std::function<bool(int)> shown;
			if (!view.stations.empty())
				shown = [this](int id) { return view.allowsStation(id); };
			response = Response::json(UiFormat::mapObjects(db, (uint64_t)toInt(request.param("since")), true, shown));
		}
		else if (p == "/api/object.json")
		{
			const std::string key = request.param("key");
			const bool station = !key.empty() && key[0] == 's';
			response = Response::json(station && !view.allowsStation(std::atoi(key.c_str() + 1)) ? std::string("{}") : UiFormat::object(db, key));
		}
		else if (p == "/api/binmsgs.json")
		{
			const uint32_t mmsi = (uint32_t)toInt(request.param("mmsi"));
			response = Response::json(mmsi && !shows((int)mmsi) ? std::string("{\"messages\":[]}")
													 : UiFormat::binaryMessages(db, since(), std::strtoull(request.param("marker").c_str(), nullptr, 16), mmsi));
		}
		else if (p == "/api/events.json")
		{
			// `before` pages back through what the rings hold; `since` is the live feed
			const std::string before = request.param("before");
			const int level = (int)toInt(request.param("level"));
			if (!before.empty())
			{
				const long long limit = toInt(request.param("limit"));
				response = Response::json(UiFormat::eventHistory(db, std::strtoull(before.c_str(), nullptr, 10), level, limit > 0 ? (int)limit : 25, keep));
			}
			else
				response = Response::json(UiFormat::events(db, std::strtoull(request.param("since").c_str(), nullptr, 10), level, keep));
		}
		else if (p == "/api/history_full.json")
		{
			std::string out;
			JSON::Writer w(out);
			const auto write = [&](Figures &f) { UiFormat::history(w, f); };
			const std::vector<int> of = figuresOf(focus);
			if (of.empty())
				tracker.withAll(write);
			else if (!tracker.withStations(of, write))
				w.raw("{}");
			w.finish();
			response = Response::json(out);
		}
		else if (p == "/api/sharing_state.json")
		{
			// the community icon's poll: the engine's part of the stat page, with the settings version
			std::string out;
			JSON::Writer w(out);
			w.beginObject().kv("config_version", configVersion());
			if (engine)
			{
				engine(w, ENGINE_SHARING);
				engine(w, ENGINE_RUNNING);
			}
			w.endObject();
			w.finish();
			response = Response::json(out);
		}
		else if (p == "/api/stat.json" || p == "/stat.json")
			response = Response::json(statJSON(focus, keep));
		else if (p == "/api/ships.json" || p == "/ships.json" || p == "/api/ships_full.json")
		{
			StationRegistry::Station at;
			const bool drawn = drawnStation(std::vector<int>(), at);
			response = Response::json(UiFormat::ships(db, p == "/api/ships_full.json", drawn ? &at : nullptr, keep));
		}
		else if (p == "/api/output_stats.json")
		{
			std::string out;
			JSON::Writer w(out);
			w.beginObject().kv("tcp_clients", clients ? clients() : 0);
			if (engine)
				engine(w, ENGINE_OUTPUTS);
			else
				w.key("outputs").beginArray().endArray();
			w.endObject();
			w.finish();
			response = Response::json(out);
		}
		else if (p == "/api/planes.json")
			response = Response::json(view.stations.empty() ? UiFormat::planes(tracker.planes()) : noPlanes("planes"));
		else if (p == "/api/places.json")
			response = Response::json(tracker.placesCollection());
		else if (p == "/api/ships_port.json")
		{
			std::string code = request.param("code");
			bool valid = !code.empty() && code.size() <= 8;
			for (char &c : code)
			{
				if (c >= 'a' && c <= 'z')
					c -= 'a' - 'A';
				valid &= (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			}
			response = Response::json(valid ? UiFormat::placeShips(db, UINT32_MAX, "", code, "expected", 24, 0, 10, keep)
								  : std::string("{\"error\":\"code must be 1-8 letters or digits\"}"));
		}
		else if ((p == "/api/sse" || p == "/api/signal") && options.realtime)
			response.stream = 1u << (p == "/api/sse" ? IO::SSE::NMEA : IO::SSE::SIGNAL);
		else if (p == "/api/log" && options.log)
		{
			response.stream = 1u << IO::SSE::VIEWER_LOG;
			response.stream_topic = "log";
			response.stream_backlog = [] { return Logger::getInstance().getBacklogJSON(INT_MAX); };
		}
		else if (p == "/api/decode" && options.decoder)
		{
			// a POST sends the NMEA as its body, a GET as its query
			const std::string text = request.method == "POST" && !request.body.empty() ? request.body : request.query;
			if (text.empty() || text.size() > 1024)
				response = Response::json("{\"error\":\"Input size limit exceeded\"}");
			else
				try
				{
					// NMEA holds neither '%' nor '+': the text reads the same whether the page encoded it or not
					const std::string result = decodeNMEA(urlDecode(text));
					response = Response::json(result == "[]" ? std::string("{\"error\":\"No valid AIS messages decoded\"}") : result);
				}
				catch (const std::exception &e)
				{
					Error() << "Decoder error: " << e.what();
					response = Response::json("{\"error\":\"Decoding failed\"}");
				}
		}
		else if (p == "/api/message")
		{
			// the vessel's last message, decoded from the NMEA kept for it, as one
			// object; none is kept without msg
			const int mmsi = toMMSI(bare(request.query));
			const std::string msg = options.msg && mmsi > 0 && shows(mmsi) ? UiFormat::message(db, mmsi) : std::string();
			const std::string decoded = msg.empty() ? std::string() : decodeMessage(msg);
			response = Response::json(mmsi < 0 ? std::string("{\"error\":\"Invalid MMSI\"}")
									 : decoded.empty() ? std::string("{\"error\":\"Message not found\"}") : decoded);
		}
		else if (p == "/api/replay_info.json" && options.replay)
			response = Response::json(UiFormat::replayInfo(db, REPLAY_BLOCK, oldest));
		else if (p == "/api/replay_ships.json" && options.replay)
			response = Response::json(UiFormat::replayShips(db, (std::time_t)toInt(request.param("since")), (std::time_t)toInt(request.param("lookback")), keep, oldest));
		else if (p == "/api/replay.json" && options.replay)
		{
			const long long block = toInt(request.param("block"));
			if (!validReplayBlock(block))
				response = Response::json("{}\n\n");
			else
			{
				const std::time_t from = (std::time_t)(block * REPLAY_BLOCK);
				long long lookback = toInt(request.param("lookback"));
				if (lookback < 0 || lookback > MAX_REPLAY_LOOKBACK)
					lookback = 0;
				response = Response::json(UiFormat::replay(db, from, from + REPLAY_BLOCK - 1, keep, (std::time_t)lookback, oldest));
				// a dwell inside the block can still grow for DWELL_GAP after it ends; past that only eviction changes it
				response.cache = (block + 1) * REPLAY_BLOCK + (long long)PathStore::DWELL_GAP <= (long long)std::time(nullptr);
			}
		}
		else if (p == "/api/place.json" && view.place_geometry)
		{
			// a missing or malformed id is no place, not place 0
			const std::string id = request.param("id");
			long long n = 0;
			const unsigned long long number = Util::Parse::Digits(id, n, 10) ? (unsigned long long)n : UINT32_MAX;
			response = Response::json(number < UINT32_MAX ? tracker.placeFeature((uint32_t)number, request.param("version")) : "{}");
		}
		else if (p == "/api/ships_place.json")
		{
			const std::string id = request.param("id"), hours = request.param("hours"), offset = request.param("offset"),
							  limit = request.param("limit");
			const auto digits = [](const std::string &v, std::size_t most) {
				long long n = 0;
				return Util::Parse::Digits(v, n, most);
			};
			std::string code = request.param("code");
			bool valid = (id.empty() || (digits(id, 10) && std::strtoull(id.c_str(), nullptr, 10) < UINT32_MAX)) &&
						 (hours.empty() || digits(hours, 4)) && (offset.empty() || digits(offset, 8)) && (limit.empty() || digits(limit, 3)) &&
						 code.size() <= 8;
			for (char &c : code)
			{
				if (c >= 'a' && c <= 'z')
					c -= 'a' - 'A';
				valid &= (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			}
			response = Response::json(!valid ? std::string("{\"error\":\"Invalid place query\"}")
								   : UiFormat::placeShips(db, id.empty() ? UINT32_MAX : (uint32_t)std::strtoul(id.c_str(), nullptr, 10),
														  request.param("version"), code, request.param("tab").empty() ? "inside" : request.param("tab"),
														  hours.empty() ? 24 : (unsigned)std::strtoul(hours.c_str(), nullptr, 10),
														  offset.empty() ? 0 : (unsigned)std::strtoul(offset.c_str(), nullptr, 10),
														  limit.empty() ? 10 : (unsigned)std::strtoul(limit.c_str(), nullptr, 10), keep));
		}
		else if (p == "/api/nearby.json")
		{
			// `station` is the one asking, left out of the answer; none when not given
			const std::string lat = request.param("lat"), lon = request.param("lon");
			response = Response::json(lat.empty() || lon.empty() ? std::string("{\"error\":\"lat and lon are required\"}")
													   : UiFormat::nearby(db, (float)std::atof(lat.c_str()), (float)std::atof(lon.c_str()),
																		  (uint32_t)toInt(request.param("mmsi")), (int)toInt(request.param("station"), -1), keep));
		}
		else
			return false;
		return true;
	}

	// The stat page: the figures of what the page looks through, and what the
	// site knows of itself and of the running engine.
	std::string Ui::statJSON(const std::vector<int> &focus, const DB::Keep &keep)
	{
		DB &db = tracker.ships();
		int vessels = 0;
		if (keep)
			db.forEachRecent(std::time(nullptr), std::time(nullptr) - db.timeHistory(), [&](int, const Ship &s, long) { vessels += keep(s); });
		const TrackingConfig tracking = tracker.trackingConfig();
		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		const auto write = [&](Figures &f) { UiFormat::counters(w, f); };
		const std::vector<int> of = figuresOf(focus);
		if (of.empty())
			tracker.withAll(write);
		else if (!tracker.withStations(of, write))
		{
			// stations that have heard nothing yet count nothing
			std::unique_ptr<Figures> none(new Figures());
			UiFormat::counters(w, *none);
		}
		// the running engine's devices, outputs and community feed, as the site knows them
		w.kv("tcp_clients", clients ? clients() : 0);
		if (engine)
			engine(w, ENGINE_ALL);
		else
			w.kv("sharing", false).kv("sharing_uuid", false).kv("engine_running", false).key("outputs").beginArray().endArray();
		w.kv("sharing_link", "https://www.aiscatcher.org/livemap").kv("station", title()).kv("station_link", options.station_link)
			.kv("vessel_count", keep ? vessels : db.getCount()).kv("vessel_max", db.getMaxCount())
			.kv("build_date", __DATE__).kv("build_version", VERSION).kv("build_describe", VERSION_DESCRIBE)
			.kv("memory", (unsigned long long)Util::Helper::getMemoryConsumption())
			.kv("track_time", view.track_time > 0 && (tracking.track_time <= 0 || view.track_time < tracking.track_time) ? view.track_time : tracking.track_time)
			.kv("track_memory", tracking.track_memory > 0 ? tracking.track_memory : (tracking.server_mode ? 4096 : 1024));
		w.endObject();
		w.finish();
		return out;
	}

	// What the page reloads its settings by: a hash of them, for a managed viewer whose
	// settings the panel changes while it runs; any other viewer restarts with the engine,
	// so it gives none and the page never asks again
	std::string Ui::configVersion()
	{
		return options.managed ? Util::SHA256::hex(pageConfig(false)).substr(0, 16) : std::string();
	}

	std::string Ui::title()
	{
		if (!options.station.empty())
			return options.station;
		std::string only;
		int count = 0;
		stations().forEach([&](const StationRegistry::Station &s) {
			if (!view.allowsStation(s.id))
				return;
			only = s.name;
			count++;
		});
		return count == 1 ? only : std::string();
	}

	std::string Ui::pageConfig(bool versioned)
	{
		const std::string name = title();
		// the page's stations, in the order the configuration names them
		std::vector<StationRegistry::Station> picker;
		bool sharing = false;
		for (const auto &s : stations().ordered())
		{
			if (!view.allowsStation(s.id))
				continue;
			picker.push_back(s);
			sharing |= s.share_loc;
		}

		std::string out;
		JSON::Writer w(out);
		w.beginObject();
		if (versioned)
			w.kv("config_version", configVersion());
		w.kv("plugin_version", plugins.version())
			.kv("about_version", Util::SHA256::hex(plugins.getAbout())).key("maps").beginArray();
		for (const auto &m : maps)
			w.raw_val(m.source->json(m.overlay));
		w.endArray().key("restart_required").beginArray();
		for (const auto &r : tracker.restartRequired())
			w.val(r);
		w.endArray()
			.key("build").beginObject().kv("version", VERSION).kv("describe", VERSION_DESCRIBE).endObject()
			.kv("context", options.context).kv("station", name).kv("webcontrol_http", options.webcontrol_http)
			.key("features").beginObject().kv("share_location", sharing).kv("save_messages", options.msg).kv("replay", options.replay)
			.kv("realtime", options.realtime).kv("log", options.log).kv("decoder", options.decoder).kv("managed", options.managed)
			.kv("places", (bool)tracker.places()).kv("outlines", view.place_geometry).kv("geojson", spec.geojson).kv("kml", spec.kml).kv("about_md", plugins.isAboutPresent());
		// the community feed, as the running engine has it
		if (engine)
			engine(w, ENGINE_SHARING);
		else
			w.kv("sharing", false).kv("sharing_uuid", false);
		w.endObject()
			.key("stations").beginArray();
		// the stations the page may look through, by number and name
		for (const auto &s : picker)
			w.beginObject().kv("id", s.id).kv("name", s.name).endObject();
		w.endArray()
			.key("plugins").beginObject().key("loaded").beginArray();
		for (const auto &p : plugins.loaded())
			w.beginObject().kv("name", p.first).kv("version", p.second).endObject();
		w.endArray().key("errors").beginArray();
		for (const auto &e : plugins.errors())
			w.val(e);
		w.endArray().endObject()
			.endObject();
		w.finish();
		return out;
	}
}
