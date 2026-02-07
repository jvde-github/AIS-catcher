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

#include "WebViewer.h"
#include "WebDB.h"
#include "Helper.h"
#include "JSON/JSONBuilder.h"

extern IO::OutputMessage *commm_feed;

// Common MIME type constants
static const std::string MIME_JSON = "application/json";
static const std::string MIME_TEXT = "application/text";
static const std::string MIME_JAVASCRIPT = "application/javascript";
static const std::string MIME_CSS = "text/css";
static const std::string MIME_PLAIN = "text/plain";
static const std::string MIME_MARKDOWN = "text/markdown";
static const std::string MIME_OCTET_STREAM = "application/octet-stream";
static const std::string MIME_KML = "application/vnd.google-earth.kml+xml";
static const std::string MIME_SVG = "image/svg+xml";
static const std::string MIME_PNG = "image/png";

extern IO::OutputMessage *commm_feed;

void WebViewer::sendError(IO::TCPServerConnection &c, const std::string &message, bool gzip)
{
	server.Response(c, MIME_JSON, "{\"error\":\"" + message + "\"}", use_zlib & gzip);
}

void WebViewer::initializeRoutes()
{
	// Register all routes in the table for O(1) lookup
	routeTable["/api/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIStats(c, gzip); };
	routeTable["/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIStats(c, gzip); };
	routeTable["/api/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIShips(c, gzip); };
	routeTable["/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIShips(c, gzip); };
	routeTable["/api/ships_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIShipsArray(c, gzip); };
	routeTable["/api/planes_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIPlanesArray(c, gzip); };
	routeTable["/sb"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleBinaryShips(c, gzip); };
	routeTable["/api/ships_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIShipsFull(c, gzip); };
	routeTable["/api/binmsgs.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIBinaryMessages(c, gzip); };
	routeTable["/custom/plugins.js"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handlePluginsJS(c, gzip); };
	routeTable["/custom/config.css"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleConfigCSS(c, gzip); };
	routeTable["/about.md"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAboutMD(c, gzip); };
	routeTable["/api/path.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIPath(c, args, gzip); };
	routeTable["/api/allpath.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIAllPath(c, gzip); };
	routeTable["/api/path.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIPathGeoJSON(c, args, gzip); };
	routeTable["/api/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIAllPathGeoJSON(c, gzip); };
	routeTable["/api/message"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIMessage(c, args, gzip); };
	routeTable["/api/decode"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIDecode(c, args, gzip); };
	routeTable["/api/vessel"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPIVessel(c, args, gzip); };
	routeTable["/api/history_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleHistoryFull(c, gzip); };

	// Conditional routes - handlers check their enable flags internally
	routeTable["/kml"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleKML(c, gzip); };
	routeTable["/metrics"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleMetrics(c, gzip); };
	routeTable["/api/sse"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPISSE(c, gzip); };
	routeTable["/api/signal"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPISignal(c, gzip); };
	routeTable["/api/log"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAPILog(c, gzip); };
	routeTable["/geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleGeoJSON(c, gzip); };
	routeTable["/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
	{ handleAllPathGeoJSON(c, gzip); };
}

void WebViewer::handleRequest(IO::TCPServerConnection &c, const std::string &response, bool gzip)
{
	std::string path, args;
	std::string::size_type pos = response.find('?');

	if (pos != std::string::npos)
	{
		path = response.substr(0, pos);
		args = response.substr(pos + 1);
	}
	else
	{
		path = response;
	}

	// Root redirect
	if (path == "/")
	{
		path = cdn.empty() ? "/index.html" : "/index_local.html";
	}

	// Try router table (O(1) lookup)
	auto it = routeTable.find(path);
	if (it != routeTable.end())
	{
		it->second(c, args, gzip);
		return;
	}

	// Handle prefix-based routes
	if (!cdn.empty() && path.find("/cdn/") == 0)
	{
		handleCDNFile(c, path, gzip);
	}
	else if (path.substr(0, 6) == "/tiles")
	{
		handleTiles(c, path, gzip);
	}
	// Default: static file handler
	else if (path.rfind("/", 0) == 0)
	{
		handleStaticFile(c, path.substr(1), gzip);
	}
}

void WebViewer::handleCDNFile(IO::TCPServerConnection &c, const std::string &path, bool gzip)
{
	try
	{
		if (path.find("..") != std::string::npos || path.find(".") == std::string::npos)
			throw std::runtime_error("Blocked, pattern not matching");

		std::string extension = path.substr(path.find_last_of('.') + 1);
		std::string contentType;

		if (extension == "svg")
			contentType = "image/svg+xml";
		else if (extension == "js")
			contentType = "application/javascript";
		else if (extension == "png")
			contentType = "image/png";
		else if (extension == "css")
			contentType = "text/css";
		else
			throw std::runtime_error("Blocked " + extension);

		std::string content = Util::Helper::readFile(cdn + path);
		server.Response(c, contentType, content, use_zlib & gzip);
	}
	catch (const std::exception &e)
	{
		Error() << "Server - error returning requested file (" << path << "): " << e.what();
		server.Response(c, "text/html", std::string(""), true);
	}
}

void WebViewer::handleAPIStats(IO::TCPServerConnection &c, bool gzip)
{
	std::string unit;
	const uint64_t GB = 1000000000;
	const uint64_t MB = 1000000;
	int norm = 0;

	if (raw_counter.received > GB)
	{
		norm = (raw_counter.received + (GB / 20)) / (GB / 10);
		unit = "GB";
	}
	else
	{
		norm = (raw_counter.received + (MB / 20)) / (MB / 10);
		unit = "MB";
	}

	int d1 = norm / 10;
	int d2 = norm % 10;
	std::string received_str = std::to_string(d1) + "." + std::to_string(d2) + unit;

	JSON::JSONBuilder json;
	json.start()
		.addRaw("total", counter.toJSON())
		.addRaw("session", counter_session.toJSON())
		.addRaw("last_day", hist_day.lastStatToJSON())
		.addRaw("last_hour", hist_hour.lastStatToJSON())
		.addRaw("last_minute", hist_minute.lastStatToJSON())
		.add("tcp_clients", server.getNumberOfClients())
		.add("sharing", commm_feed ? true : false);

	if (ships.getShareLatLon() && ships.getLat() != LAT_UNDEFINED && ships.getLon() != LON_UNDEFINED)
		json.add("sharing_link", "https://www.aiscatcher.org/?&zoom=10&lat=" + std::to_string(ships.getLat()) + "&lon=" + std::to_string(ships.getLon()));
	else
		json.add("sharing_link", "https://www.aiscatcher.org");

	json.add("station", station)
		.add("station_link", station_link)
		.add("sample_rate", sample_rate)
		.add("msg_rate", hist_second.getAverage())
		.add("vessel_count", ships.getCount())
		.add("vessel_max", ships.getMaxCount())
		.add("product", product)
		.add("vendor", vendor)
		.add("serial", serial)
		.add("model", model)
		.add("build_date", std::string(__DATE__))
		.add("build_version", std::string(VERSION))
		.add("build_describe", std::string(VERSION_DESCRIBE))
		.add("run_time", std::to_string((long int)time(nullptr) - (long int)time_start))
		.add("memory", Util::Helper::getMemoryConsumption())
		.add("os", os)
		.add("hardware", hardware)
		.add("received", received_str)
		.end();

	server.Response(c, "application/json", json.str(), use_zlib & gzip);
}

void WebViewer::handleAPIShips(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getJSON(), use_zlib & gzip);
}

void WebViewer::handleAPIPath(IO::TCPServerConnection &c, const std::string &args, bool gzip)
{
	std::stringstream ss(args);
	std::string mmsi_str;
	JSON::JSONBuilder json;
	json.start();
	int count = 0;
	const int MAX_MMSI_COUNT = 100;

	while (std::getline(ss, mmsi_str, ','))
	{
		if (++count > MAX_MMSI_COUNT)
		{
			Error() << "Server - path MMSI count exceeds limit: " << MAX_MMSI_COUNT;
			break;
		}

		try
		{
			int mmsi = std::stoi(mmsi_str);
			if (mmsi >= 1 && mmsi <= 999999999)
			{
				json.addRaw(std::to_string(mmsi), ships.getPathJSON(mmsi));
			}
		}
		catch (const std::invalid_argument &)
		{
			Error() << "Server - path MMSI invalid: " << mmsi_str;
		}
		catch (const std::out_of_range &)
		{
			Error() << "Server - path MMSI out of range: " << mmsi_str;
		}
	}
	json.end();
	server.Response(c, "application/json", json.str(), use_zlib & gzip);
}

void WebViewer::handleAPIPathGeoJSON(IO::TCPServerConnection &c, const std::string &args, bool gzip)
{
	std::stringstream ss(args);
	std::string mmsi_str;

	if (std::getline(ss, mmsi_str))
	{
		try
		{
			int mmsi = std::stoi(mmsi_str);
			if (mmsi >= 1 && mmsi <= 999999999)
			{
				server.Response(c, "application/json", ships.getPathGeoJSON(mmsi), use_zlib & gzip);
			}
			else
			{
				server.Response(c, "application/json", "{\"error\":\"Invalid MMSI range\"}", use_zlib & gzip);
			}
		}
		catch (const std::invalid_argument &)
		{
			Error() << "Server - path GeoJSON MMSI invalid: " << mmsi_str;
			server.Response(c, "application/json", "{\"error\":\"Invalid MMSI format\"}", use_zlib & gzip);
		}
		catch (const std::out_of_range &)
		{
			Error() << "Server - path GeoJSON MMSI out of range: " << mmsi_str;
			server.Response(c, "application/json", "{\"error\":\"MMSI out of range\"}", use_zlib & gzip);
		}
	}
	else
	{
		server.Response(c, "application/json", "{\"error\":\"No MMSI provided\"}", use_zlib & gzip);
	}
}

void WebViewer::handleAPIMessage(IO::TCPServerConnection &c, const std::string &args, bool gzip)
{
	int mmsi = -1;
	std::stringstream ss(args);
	if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
	{
		server.Response(c, "application/text", ships.getMessage(mmsi), use_zlib & gzip);
	}
	else
	{
		server.Response(c, "application/text", "Message not available");
	}
}

void WebViewer::handleAPIDecode(IO::TCPServerConnection &c, const std::string &args, bool gzip)
{
	if (!showdecoder)
	{
		server.Response(c, "application/json", std::string("{\"error\":\"Decoder is disabled\"}"), use_zlib & gzip);
		return;
	}

	try
	{
		if (args.empty() || args.size() > 1024)
			throw std::runtime_error("Input size limit exceeded");

		std::string result = decodeNMEAtoJSON(args, true);

		if (result == "[]")
		{
			server.Response(c, "application/json", std::string("{\"error\":\"No valid AIS messages decoded\"}"), use_zlib & gzip);
		}
		else
		{
			server.Response(c, "application/json", result, use_zlib & gzip);
		}
	}
	catch (const std::exception &e)
	{
		Error() << "Decoder error: " << e.what();
		server.Response(c, "application/json", std::string("{\"error\":\"Decoding failed\"}"), use_zlib & gzip);
	}
}

void WebViewer::handleAPIVessel(IO::TCPServerConnection &c, const std::string &args, bool gzip)
{
	std::stringstream ss(args);
	int mmsi;
	if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
	{
		server.Response(c, "application/text", ships.getShipJSON(mmsi), use_zlib & gzip);
	}
	else
	{
		server.Response(c, "application/text", "Vessel not available");
	}
}

void WebViewer::handleTiles(IO::TCPServerConnection &c, const std::string &path, bool gzip)
{
	int z, x, y;
	std::string layer;
	if (parseMBTilesURL(path, layer, z, x, y))
	{
		for (const auto &source : mapSources)
		{
			if (source->getLayerID() != layer)
				continue;

			if (source->isValidTile(z, x, y))
			{
				std::string contentType;
				const std::vector<unsigned char> &data = source->getTile(z, x, y, contentType);

				if (!data.empty())
				{
					server.Response(c, contentType, (char *)data.data(), data.size(), use_zlib & gzip, true);
					return;
				}
			}
		}
		server.Response(c, "text/plain", std::string("Tile not found"), false, true);
		return;
	}
	server.Response(c, "text/plain", std::string("Invalid Tile Request"), false, true);
}

void WebViewer::handleStaticFile(IO::TCPServerConnection &c, const std::string &filename, bool gzip)
{
	auto it = WebDB::files.find(filename);
	if (it != WebDB::files.end())
	{
		const WebDB::FileData &file = it->second;
		server.ResponseRaw(c, file.mime_type, (char *)file.data, file.size, true, std::string(file.mime_type) != "text/html");
	}
	else
	{
		Error() << "File not found: " << filename << "\n";
		server.Request(c, filename, false);
	}
}

void WebViewer::handleKML(IO::TCPServerConnection &c, bool gzip)
{
	if (!KML)
	{
		server.Response(c, "text/plain", std::string("Feature disabled"), false, true);
		return;
	}
	server.Response(c, "application/vnd.google-earth.kml+xml", ships.getKML(), use_zlib & gzip);
}

void WebViewer::handleMetrics(IO::TCPServerConnection &c, bool gzip)
{
	if (!supportPrometheus)
	{
		server.Response(c, "text/plain", std::string("Feature disabled"), false, true);
		return;
	}
	std::string content = dataPrometheus.toPrometheus();
	server.Response(c, "text/plain", content, use_zlib & gzip);
	dataPrometheus.Reset();
}

void WebViewer::handleAPIShipsArray(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getJSONcompact(), use_zlib & gzip);
}

void WebViewer::handleAPIPlanesArray(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", planes.getCompactArray(), use_zlib & gzip);
}

void WebViewer::handleBinaryShips(IO::TCPServerConnection &c, bool gzip)
{
	binary.clear();
	ships.getBinary(binary);
	server.Response(c, "application/octet-stream", binary.data(), binary.size(), use_zlib & gzip);
}

void WebViewer::handleAPIShipsFull(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getJSON(true), use_zlib & gzip);
}

void WebViewer::handleAPISSE(IO::TCPServerConnection &c, bool gzip)
{
	if (!realtime)
	{
		server.Response(c, "text/plain", std::string("Feature disabled"), false, true);
		return;
	}
	server.upgradeSSE(c, 1);
}

void WebViewer::handleAPISignal(IO::TCPServerConnection &c, bool gzip)
{
	if (!realtime)
	{
		server.Response(c, "text/plain", std::string("Feature disabled"), false, true);
		return;
	}
	server.upgradeSSE(c, 2);
}

void WebViewer::handleAPILog(IO::TCPServerConnection &c, bool gzip)
{
	if (!showlog)
	{
		server.Response(c, "text/plain", std::string("Feature disabled"), false, true);
		return;
	}
	IO::SSEConnection *s = server.upgradeSSE(c, 3);
	auto l = Logger::getInstance().getLastMessages(25);
	for (auto &m : l)
	{
		s->SendEvent("log", m.toJSON());
	}
}

void WebViewer::handleAPIBinaryMessages(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getBinaryMessagesJSON(), use_zlib & gzip);
}

void WebViewer::handlePluginsJS(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/javascript", params + plugins + plugin_code + "}\nserver_version = false;\naboutMDpresent = " + (aboutPresent ? "true" : "false") + ";\ncommunityFeed = " + (commm_feed ? "true" : "false") + ";\n", use_zlib & gzip);
}

void WebViewer::handleConfigCSS(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "text/css", stylesheets, use_zlib & gzip);
}

void WebViewer::handleAboutMD(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "text/markdown", about, use_zlib & gzip);
}

void WebViewer::handleAPIAllPath(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getAllPathJSON(), use_zlib & gzip);
}

void WebViewer::handleAPIAllPathGeoJSON(IO::TCPServerConnection &c, bool gzip)
{
	server.Response(c, "application/json", ships.getAllPathGeoJSON(), use_zlib & gzip);
}

void WebViewer::handleGeoJSON(IO::TCPServerConnection &c, bool gzip)
{
	if (!GeoJSON)
	{
		server.Response(c, "application/json", std::string("{\"error\":\"Feature disabled\"}"), false, true);
		return;
	}
	server.Response(c, "application/json", ships.getGeoJSON(), use_zlib & gzip);
}

void WebViewer::handleAllPathGeoJSON(IO::TCPServerConnection &c, bool gzip)
{
	if (!GeoJSON)
	{
		server.Response(c, "application/json", std::string("{\"error\":\"Feature disabled\"}"), false, true);
		return;
	}
	server.Response(c, "application/json", ships.getAllPathGeoJSON(), use_zlib & gzip);
}

void WebViewer::handleHistoryFull(IO::TCPServerConnection &c, bool gzip)
{
	JSON::JSONBuilder json;
	json.start()
		.addRaw("second", hist_second.toJSON())
		.addRaw("minute", hist_minute.toJSON())
		.addRaw("hour", hist_hour.toJSON())
		.addRaw("day", hist_day.toJSON())
		.end()
		.nl()
		.nl();
	server.Response(c, "application/json", json.str(), use_zlib & gzip);
}
