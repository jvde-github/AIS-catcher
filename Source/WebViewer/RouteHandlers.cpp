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

void WebViewer::initializeRoutes()
{
	// Register all routes in the table for O(1) lookup
	routeTable["/api/stat.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIStats(req); };
	routeTable["/stat.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIStats(req); };
	routeTable["/api/ships.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIShips(req); };
	routeTable["/ships.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIShips(req); };
	routeTable["/api/ships_array.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIShipsArray(req); };
	routeTable["/api/planes_array.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIPlanesArray(req); };
	routeTable["/sb"] = [this](IO::HTTPRequest &req)
	{ handleBinaryShips(req); };
	routeTable["/api/ships_full.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIShipsFull(req); };
	routeTable["/api/binmsgs.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIBinaryMessages(req); };
	routeTable["/custom/plugins.js"] = [this](IO::HTTPRequest &req)
	{ handlePluginsJS(req); };
	routeTable["/custom/config.css"] = [this](IO::HTTPRequest &req)
	{ handleConfigCSS(req); };
	routeTable["/about.md"] = [this](IO::HTTPRequest &req)
	{ handleAboutMD(req); };
	routeTable["/api/path.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIPath(req); };
	routeTable["/api/allpath.json"] = [this](IO::HTTPRequest &req)
	{ handleAPIAllPath(req); };
	routeTable["/api/path.geojson"] = [this](IO::HTTPRequest &req)
	{ handleAPIPathGeoJSON(req); };
	routeTable["/api/allpath.geojson"] = [this](IO::HTTPRequest &req)
	{ handleAPIAllPathGeoJSON(req); };
	routeTable["/api/message"] = [this](IO::HTTPRequest &req)
	{ handleAPIMessage(req); };
	routeTable["/api/decode"] = [this](IO::HTTPRequest &req)
	{ handleAPIDecode(req); };
	routeTable["/api/vessel"] = [this](IO::HTTPRequest &req)
	{ handleAPIVessel(req); };
	routeTable["/api/history_full.json"] = [this](IO::HTTPRequest &req)
	{ handleHistoryFull(req); };

	// Conditional routes - handlers check their enable flags internally
	routeTable["/kml"] = [this](IO::HTTPRequest &req)
	{ handleKML(req); };
	routeTable["/metrics"] = [this](IO::HTTPRequest &req)
	{ handleMetrics(req); };
	routeTable["/api/sse"] = [this](IO::HTTPRequest &req)
	{ handleAPISSE(req); };
	routeTable["/api/signal"] = [this](IO::HTTPRequest &req)
	{ handleAPISignal(req); };
	routeTable["/api/log"] = [this](IO::HTTPRequest &req)
	{ handleAPILog(req); };
	routeTable["/geojson"] = [this](IO::HTTPRequest &req)
	{ handleGeoJSON(req); };
	routeTable["/allpath.geojson"] = [this](IO::HTTPRequest &req)
	{ handleAllPathGeoJSON(req); };
}

void WebViewer::handleRequest(IO::HTTPRequest &req)
{
	// Root redirect
	if (req.path == "/")
	{
		req.path = cdn.empty() ? "/index.html" : "/index_local.html";
	}

	// Try router table
	auto it = routeTable.find(req.path);
	if (it != routeTable.end())
	{
		it->second(req);
		return;
	}

	// Handle prefix-based routes
	if (!cdn.empty() && req.path.find("/cdn/") == 0)
	{
		handleCDNFile(req);
	}
	else if (req.path.substr(0, 6) == "/tiles")
	{
		handleTiles(req);
	}
	// Default: static file handler
	else if (req.path.rfind("/", 0) == 0)
	{
		handleStaticFile(req);
	}
	else
	{
		// Malformed path
		server.ResponseBadRequest(req, "Invalid path format");
	}
}

void WebViewer::handleCDNFile(IO::HTTPRequest &req)
{
	try
	{
		if (req.path.find("..") != std::string::npos || req.path.find(".") == std::string::npos)
			throw std::runtime_error("Blocked, pattern not matching");

		std::string extension = req.path.substr(req.path.find_last_of('.') + 1);
		std::string contentType;

		if (extension == "svg")
			contentType = IO::MIME::SVG;
		else if (extension == "js")
			contentType = IO::MIME::JAVASCRIPT;
		else if (extension == "png")
			contentType = IO::MIME::PNG;
		else if (extension == "css")
			contentType = IO::MIME::CSS;
		else
			throw std::runtime_error("Blocked " + extension);

		std::string content = Util::Helper::readFile(cdn + req.path);
		server.Response(req, contentType.c_str(), content);
	}
	catch (const std::exception &e)
	{
		Error() << "Server - error returning requested file (" << req.path << "): " << e.what();
		server.Response(req, IO::MIME::HTML, "");
	}
}

void WebViewer::handleAPIStats(IO::HTTPRequest &req)
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

	server.Response(req, IO::MIME::JSON, json.str());
}

void WebViewer::handleAPIShips(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getJSON());
}

void WebViewer::handleAPIPath(IO::HTTPRequest &req)
{
	std::stringstream ss(req.args);
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
	server.Response(req, IO::MIME::JSON, json.str());
}

void WebViewer::handleAPIPathGeoJSON(IO::HTTPRequest &req)
{
	std::stringstream ss(req.args);
	std::string mmsi_str;

	if (std::getline(ss, mmsi_str))
	{
		try
		{
			int mmsi = std::stoi(mmsi_str);
			if (mmsi >= 1 && mmsi <= 999999999)
			{
				server.Response(req, IO::MIME::JSON, ships.getPathGeoJSON(mmsi));
			}
			else
			{
				server.ResponseBadRequest(req, "Invalid MMSI range");
			}
		}
		catch (const std::invalid_argument &)
		{
			Error() << "Server - path GeoJSON MMSI invalid: " << mmsi_str;
			server.ResponseBadRequest(req, "Invalid MMSI format");
		}
		catch (const std::out_of_range &)
		{
			Error() << "Server - path GeoJSON MMSI out of range: " << mmsi_str;
			server.ResponseBadRequest(req, "MMSI out of range");
		}
	}
	else
	{
		server.ResponseBadRequest(req, "No MMSI provided");
	}
}

void WebViewer::handleAPIMessage(IO::HTTPRequest &req)
{
	int mmsi = -1;
	std::stringstream ss(req.args);
	if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
	{
		server.Response(req, IO::MIME::TEXT, ships.getMessage(mmsi));
	}
	else
	{
		server.ResponseNotFound(req, "Message not available");
	}
}

void WebViewer::handleAPIDecode(IO::HTTPRequest &req)
{
	if (!showdecoder)
	{
		server.ResponseForbidden(req, "Decoder is disabled");
		return;
	}

	try
	{
		if (req.args.empty() || req.args.size() > 1024)
			throw std::runtime_error("Input size limit exceeded");

		std::string result = decodeNMEAtoJSON(req.args, true);

		if (result == "[]")
		{
			server.ResponseError(req, "No valid AIS messages decoded");
		}
		else
		{
			server.Response(req, IO::MIME::JSON, result);
		}
	}
	catch (const std::exception &e)
	{
		Error() << "Decoder error: " << e.what();
		server.ResponseError(req, "Decoding failed");
	}
}

void WebViewer::handleAPIVessel(IO::HTTPRequest &req)
{
	std::stringstream ss(req.args);
	int mmsi;
	if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
	{
		server.Response(req, IO::MIME::TEXT, ships.getShipJSON(mmsi));
	}
	else
	{
		server.ResponseNotFound(req, "Vessel not available");
	}
}

void WebViewer::handleTiles(IO::HTTPRequest &req)
{
	int z, x, y;
	std::string layer;
	if (parseMBTilesURL(req.path, layer, z, x, y))
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
					server.Response(req, contentType.c_str(), (char *)data.data(), data.size(), true);
					return;
				}
			}
		}
		server.ResponseNotFound(req, "Tile not found");
		return;
	}
	server.ResponseBadRequest(req, "Invalid tile request");
}

void WebViewer::handleStaticFile(IO::HTTPRequest &req)
{
	// Extract filename from path (remove leading slash)
	std::string filename = req.path.substr(1);
	auto it = WebDB::files.find(filename);
	if (it != WebDB::files.end())
	{
		const WebDB::FileData &file = it->second;
		// Static files are pre-compressed and require gzip support
		if (!req.accept_gzip)
		{
			Error() << "Client requested static file without gzip support: " << filename;
			server.ResponseError(req, "This server serves pre-compressed content. Please use a client that supports gzip encoding.");
			return;
		}
		server.ResponseRaw(req, file.mime_type, (char *)file.data, file.size, true, std::string(file.mime_type) != "text/html");
	}
	else
	{
		Error() << "File not found: " << filename;
		server.ResponseNotFound(req, "File not found");
	}
}

void WebViewer::handleKML(IO::HTTPRequest &req)
{
	if (!KML)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	server.Response(req, IO::MIME::KML, ships.getKML());
}

void WebViewer::handleMetrics(IO::HTTPRequest &req)
{
	if (!supportPrometheus)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	std::string content = dataPrometheus.toPrometheus();
	server.Response(req, IO::MIME::PLAIN, content);
	dataPrometheus.Reset();
}

void WebViewer::handleAPIShipsArray(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getJSONcompact());
}

void WebViewer::handleAPIPlanesArray(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, planes.getCompactArray());
}

void WebViewer::handleBinaryShips(IO::HTTPRequest &req)
{
	binary.clear();
	ships.getBinary(binary);
	server.Response(req, IO::MIME::OCTET_STREAM, binary.data(), binary.size());
}

void WebViewer::handleAPIShipsFull(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getJSON(true));
}

void WebViewer::handleAPISSE(IO::HTTPRequest &req)
{
	if (!realtime)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	server.upgradeSSE(req.connection, 1);
}

void WebViewer::handleAPISignal(IO::HTTPRequest &req)
{
	if (!realtime)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	server.upgradeSSE(req.connection, 2);
}

void WebViewer::handleAPILog(IO::HTTPRequest &req)
{
	if (!showlog)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	IO::SSEConnection *s = server.upgradeSSE(req.connection, 3);
	auto l = Logger::getInstance().getLastMessages(25);
	for (auto &m : l)
	{
		s->SendEvent("log", m.toJSON());
	}
}

void WebViewer::handleAPIBinaryMessages(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getBinaryMessagesJSON());
}

void WebViewer::handlePluginsJS(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JAVASCRIPT, params + plugins + plugin_code + "}\nserver_version = false;\naboutMDpresent = " + (aboutPresent ? "true" : "false") + ";\ncommunityFeed = " + (commm_feed ? "true" : "false") + ";\n");
}

void WebViewer::handleConfigCSS(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::CSS, stylesheets);
}

void WebViewer::handleAboutMD(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::MARKDOWN, about);
}

void WebViewer::handleAPIAllPath(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getAllPathJSON());
}

void WebViewer::handleAPIAllPathGeoJSON(IO::HTTPRequest &req)
{
	server.Response(req, IO::MIME::JSON, ships.getAllPathGeoJSON());
}

void WebViewer::handleGeoJSON(IO::HTTPRequest &req)
{
	if (!GeoJSON)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	server.Response(req, IO::MIME::JSON, ships.getGeoJSON());
}

void WebViewer::handleAllPathGeoJSON(IO::HTTPRequest &req)
{
	if (!GeoJSON)
	{
		server.ResponseForbidden(req, "Feature disabled");
		return;
	}
	server.Response(req, IO::MIME::JSON, ships.getAllPathGeoJSON());
}

void WebViewer::handleHistoryFull(IO::HTTPRequest &req)
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
	server.Response(req, IO::MIME::JSON, json.str());
}
