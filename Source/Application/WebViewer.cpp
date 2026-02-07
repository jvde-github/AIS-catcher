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
#include "NMEA.h"
#include "JSONAIS.h"
#include "Helper.h"
#include "JSON/JSONBuilder.h"

IO::OutputMessage *commm_feed = nullptr;

void SSEStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	if (server)
	{
		AIS::Message *m = (AIS::Message *)data[0].binary;
		std::time_t now = std::time(nullptr);
		JSON::JSONBuilder json;

		if (!m->NMEA.empty())
		{
			json.start()
				.add("mmsi", m->mmsi())
				.add("timestamp", (long)now)
				.add("channel", m->getChannel())
				.add("type", m->type())
				.add("shipname", tag.shipname)
				.key("nmea")
				.startArray();

			for (const auto &s : m->NMEA)
			{
				std::string nmea = s;

				int end = nmea.rfind(',');
				if (end == std::string::npos)
					continue;

				int start = nmea.rfind(',', end - 1);
				if (start == std::string::npos)
					continue;

				int len = end - start - 1;

				if (len == 0)
					continue;

				if (obfuscate)
				{
					for (int i = 0; i < 3; i++)
					{
						idx = (idx + 1) % len;
						nmea[MIN(start + 1 + idx, nmea.length() - 1)] = '*';
					}
				}

				json.value(nmea);
			}

			json.endArray()
				.end();

			server->sendSSE(1, "nmea", json.str());
		}
		if (tag.lat != 0 && tag.lon != 0)
		{
			json.clear();
			json.start()
				.add("mmsi", m->mmsi())
				.add("channel", m->getChannel())
				.add("lat", tag.lat)
				.add("lon", tag.lon)
				.end();

			server->sendSSE(2, "nmea", json.str());
		}
	}
}

WebViewer::WebViewer()
{
	params = "build_string = '" + std::string(VERSION_DESCRIBE) + "';\nbuild_version = '" + std::string(VERSION) + "';\ncontext='settings';\n\n";
	plugin_code = "\n\nfunction loadPlugins() {\n";
	plugins = "let plugins = '';\nlet server_message = '';\n";
	os = Util::Helper::getOS();
	hardware = Util::Helper::getHardware();
	
	initializeRoutes();
}

std::string WebViewer::decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced)
{
	// Decoder class with full pipeline: NMEA -> Message -> JSON -> Array
	class NMEADecoder : public StreamIn<JSON::JSON>
	{
	public:
		AIS::NMEA nmea_decoder;
		AIS::JSONAIS json_converter;
		JSON::StringBuilder *builder;
		std::string result;
		bool first;
		size_t message_count;
		const size_t MAX_OUTPUT_SIZE;

		NMEADecoder(JSON::StringBuilder *b) : builder(b), first(true), message_count(0), MAX_OUTPUT_SIZE(1024 * 1024)
		{
			nmea_decoder >> json_converter;
			json_converter.out.Connect(this);

			result = "[";
			result.reserve(4096);
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag) override
		{
			for (int i = 0; i < len; i++)
			{
				if (result.size() > MAX_OUTPUT_SIZE)
					throw std::runtime_error("Output size limit exceeded");

				if (!first)
					result += ",";

				first = false;
				builder->stringify(data[i], result);
				message_count++;
			}
		}

		std::string decode(const std::string &nmea_input)
		{
			RAW raw = {Format::TXT, (void *)nmea_input.c_str(), (int)nmea_input.length()};
			TAG tag;
			nmea_decoder.Receive(&raw, 1, tag);
			result += "]";
			return result;
		}
	};

	JSON::StringBuilder builder(&AIS::KeyMap, JSON_DICT_FULL);
	builder.setStringifyEnhanced(enhanced);
	NMEADecoder decoder(&builder);
	return decoder.decode(nmea_input);
}

std::vector<std::string> WebViewer::parsePath(const std::string &url)
{
	std::vector<std::string> path;
	std::string pattern = "/";

	auto pos = url.find(pattern);
	if (pos != 0)
		return path;

	std::string remainder = url.substr(pattern.length());
	std::stringstream ss(remainder);
	std::string segment;

	while (std::getline(ss, segment, '/'))
	{
		if (!segment.empty())
		{
			path.push_back(segment);
		}
	}

	return path;
}

bool WebViewer::parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y)
{
	std::string pattern = "/tiles/";

	layerID.clear();

	std::string segment;
	std::vector<std::string> segments = parsePath(url);

	if (segments.size() != 5 && segments[0] != "tiles")
		return false;

	try
	{
		layerID = segments[1];
		z = std::stoi(segments[2]);
		x = std::stoi(segments[3]);
		y = std::stoi(segments[4]);

		return true;
	}
	catch (...)
	{
		return false;
	}
}

void WebViewer::addMBTilesSource(const std::string &filepath, bool overlay)
{
#if HASSQLITE
	auto source = std::make_shared<MBTilesSupport>();
	if (source->open(filepath))
	{
		mapSources.push_back(source);
		plugin_code += source->generatePluginCode(overlay);
	}
	else
	{
		Error() << "Failed to load MBTiles from: " << filepath;
	}
#endif
}

void WebViewer::addFileSystemTilesSource(const std::string &directoryPath, bool overlay)
{
	auto source = std::make_shared<FileSystemTiles>();
	if (source->open(directoryPath))
	{
		mapSources.push_back(source);
		plugin_code += source->generatePluginCode(overlay);
	}
	else
	{
		Error() << "Failed to load FileSystemTiles from: " << directoryPath;
	}
}

bool WebViewer::Save()
{
	try
	{
		std::ofstream infile(backup_filename, std::ios::binary);
		if (!counter.Save(infile))
			return false;
		if (!hist_second.Save(infile))
			return false;
		if (!hist_minute.Save(infile))
			return false;
		if (!hist_hour.Save(infile))
			return false;
		if (!hist_day.Save(infile))
			return false;

		if (!ships.Save(infile))
			return false;

		infile.close();
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}
	return true;
}

void WebViewer::Clear()
{
	counter.Clear();
	counter_session.Clear();

	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

bool WebViewer::Load()
{

	if (backup_filename.empty())
		return false;

	Info() << "Server: reading statistics from " << backup_filename;
	try
	{
		std::ifstream infile(backup_filename, std::ios::binary);

		if (!counter.Load(infile))
			return false;
		if (!hist_second.Load(infile))
			return false;
		if (!hist_minute.Load(infile))
			return false;
		if (!hist_hour.Load(infile))
			return false;
		if (!hist_day.Load(infile))
			return false;
		// Load ship database if available
		if (infile.peek() != EOF)
		{
			if (!ships.Load(infile))
				Warning() << "Server: Could not load ship database from backup";
		}

		infile.close();
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}

	return true;
}

void WebViewer::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;

	plugins += "console.log('plugin:" + arg + "');";
	plugins += "plugins += 'JS: " + arg + "\\n';";
	plugins += "\n\n//=============\n//" + arg + "\n\n";
	try
	{
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		std::shared_ptr<JSON::JSON> j = parser.parse(firstline);

		for (const auto &p : j->getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_VERSION:
				version = p.Get().getInt();
				break;
			case AIS::KEY_SETTING_AUTHOR:
				author = p.Get().getString();
				break;
			case AIS::KEY_SETTING_DESCRIPTION:
				description = p.Get().getString();
				break;
			default:
				throw std::runtime_error("Unknown key in plugin JSON field");
				break;
			}
		}
		Info() << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version;
		if (version != 3)
			throw std::runtime_error("Version not supported, expected 3, got " + std::to_string(version));
		plugin_code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin " + arg + "\", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{

		plugins += "// FAILED\n";
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		plugins += "server_message += \"Plugin error (" + arg + ") " + std::string(e.what()) + "\\n\"\n";
	}
}

void WebViewer::BackupService()
{
	try
	{
		Info() << "Server: starting backup service every " << backup_interval << " minutes.";
		while (true)
		{
			std::unique_lock<std::mutex> lock(m);

			if (cv.wait_for(lock, std::chrono::minutes(backup_interval), [&]
							{ return !run; }))
			{
				break;
			}

			if (!Save())
				Error() << "Server failed to write backup.";
		}
	}
	catch (std::exception &e)
	{
		Error() << "WebClient BackupService: " << e.what();
		std::terminate();
	}

	Info() << "Server: stopping backup service.";
}

void WebViewer::initializeRoutes()
{
	// Register all routes in the table for O(1) lookup
	routeTable["/api/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIStats(c, gzip); };
	routeTable["/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIStats(c, gzip); };
	routeTable["/api/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIShips(c, gzip); };
	routeTable["/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIShips(c, gzip); };
	routeTable["/api/ships_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIShipsArray(c, gzip); };
	routeTable["/api/planes_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIPlanesArray(c, gzip); };
	routeTable["/sb"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleBinaryShips(c, gzip); };
	routeTable["/api/ships_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIShipsFull(c, gzip); };
	routeTable["/api/binmsgs.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIBinaryMessages(c, gzip); };
	routeTable["/custom/plugins.js"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handlePluginsJS(c, gzip); };
	routeTable["/custom/config.css"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleConfigCSS(c, gzip); };
	routeTable["/about.md"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAboutMD(c, gzip); };
	routeTable["/api/path.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIPath(c, args, gzip); };
	routeTable["/api/allpath.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIAllPath(c, gzip); };
	routeTable["/api/path.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIPathGeoJSON(c, args, gzip); };
	routeTable["/api/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIAllPathGeoJSON(c, gzip); };
	routeTable["/api/message"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIMessage(c, args, gzip); };
	routeTable["/api/decode"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIDecode(c, args, gzip); };
	routeTable["/api/vessel"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPIVessel(c, args, gzip); };
	routeTable["/api/history_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleHistoryFull(c, gzip); };
	
	// Conditional routes - handlers check their enable flags internally
	routeTable["/kml"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleKML(c, gzip); };
	routeTable["/metrics"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleMetrics(c, gzip); };
	routeTable["/api/sse"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPISSE(c, gzip); };
	routeTable["/api/signal"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPISignal(c, gzip); };
	routeTable["/api/log"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAPILog(c, gzip); };
	routeTable["/geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleGeoJSON(c, gzip); };
	routeTable["/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip) { handleAllPathGeoJSON(c, gzip); };
}

void WebViewer::connect(Receiver &r)
{
	bool rec_details = false;
	const std::string newline = "<br>";
	for (int j = 0; j < r.Count(); j++)
		if (r.Output(j).canConnect(groups_in))
		{
			if (!rec_details)
			{
				auto *device = r.getDeviceManager().getDevice();

				sample_rate += device->getRateDescription() + "<br>";

				product += device->getProduct() + newline;
				vendor += (device->getVendor().empty() ? "-" : device->getVendor()) + newline;
				serial += (device->getSerial().empty() ? "-" : device->getSerial()) + newline;

				rec_details = true;
			}
			model += r.Model(j)->getName() + newline;

			r.OutputJSON(j).Connect((StreamIn<JSON::JSON> *)&ships);
			r.OutputGPS(j).Connect((StreamIn<AIS::GPS> *)&ships);
			r.OutputADSB(j).Connect((StreamIn<Plane::ADSB> *)&planes);

			*r.getDeviceManager().getDevice() >> raw_counter;
		}
}

void WebViewer::connect(AIS::Model &m, Connection<JSON::JSON> &json, Device::Device &device)
{
	if (m.Output().out.canConnect(groups_in))
	{

		json.Connect((StreamIn<JSON::JSON> *)&ships);
		device >> raw_counter;

		sample_rate = device.getRateDescription();
		setDeviceDescription(device.getProduct(), device.getVendor().empty() ? "-" : device.getVendor(), device.getSerial().empty() ? "-" : device.getSerial());
		model = m.getName();
	}
}

void WebViewer::Reset()
{
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();

	raw_counter.Reset();
	ships.setup();
	time_start = time(nullptr);
}

void WebViewer::start()
{
	ships.setup();

	if (backup_filename.empty())
		Clear();
	else if (!Load())
	{
		Error() << "Statistics - cannot read file.";
		Clear();
	}

	if (realtime)
	{
		ships >> sse_streamer;
		sse_streamer.setSSE(&server);
	}

	if (showlog)
	{
		logger.setSSE(&server);
		logger.Start();
	}

	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;

	ships >> counter;
	ships >> counter_session;

	if (supportPrometheus)
		ships >> dataPrometheus;

	if (firstport && lastport)
	{
		for (port = firstport; port <= lastport; port++)
			if (server.start(port))
				break;

		if (port > lastport)
			throw std::runtime_error("Cannot open port in range [" + std::to_string(firstport) + "," + std::to_string(lastport) + "]");

		Info() << "HTML Server running at port " << std::to_string(port);
	}
	else
		throw std::runtime_error("HTML server ports not specified");

	// Set up request handler
	server.setRequestHandler([this](IO::TCPServerConnection &c, const std::string &r, bool gzip)
							 { handleRequest(c, r, gzip); });

	time_start = time(nullptr);

	run = true;

	if (backup_interval > 0)
	{
		if (backup_filename.empty())
		{
			Warning() << "Server: backup of statistics requested without providing backup filename.";
		}
		else
		{
			backup_thread = std::thread(&WebViewer::BackupService, this);
			thread_running = true;
		}
	}
}

void WebViewer::stopThread()
{
	if (thread_running)
	{
		{
			std::lock_guard<std::mutex> lock(m);
			run = false;
		}
		cv.notify_all();

		if (backup_thread.joinable())
			backup_thread.join();

		thread_running = false;
	}
}

void WebViewer::close()
{

	stopThread();

	if (!backup_filename.empty() && !Save())
	{
		Error() << "Statistics - cannot write file.";
	}
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

Setting &WebViewer::Set(std::string option, std::string arg)
{
	Util::Convert::toUpper(option);

	if (option == "PORT")
	{
		port_set = true;
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535, option);
	}
	else if (option == "SERVER_MODE")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setServerMode(b);
	}
	else if (option == "ZLIB")
	{
		use_zlib = Util::Parse::Switch(arg);
	}
	else if (option == "GROUPS_IN")
	{
		groups_in = Util::Parse::Integer(arg);
	}
	else if (option == "PORT_MIN")
	{
		port_set = true;
		firstport = Util::Parse::Integer(arg, 1, 65535, option);
		lastport = MAX(firstport, lastport);
	}
	else if (option == "PORT_MAX")
	{
		port_set = true;
		lastport = Util::Parse::Integer(arg, 1, 65535, option);
		firstport = MIN(firstport, lastport);
	}
	else if (option == "STATION")
	{
		station = arg;
	}
	else if (option == "STATION_LINK")
	{
		station_link = arg;
	}
	else if (option == "LAT")
	{
		ships.setLat(Util::Parse::Float(arg));
		planes.setLat(Util::Parse::Float(arg));
	}
	else if (option == "CUTOFF")
	{
		int cutoff = Util::Parse::Integer(arg, 0, 10000, option);
		hist_minute.setCutoff(cutoff);
		hist_second.setCutoff(cutoff);
		hist_hour.setCutoff(cutoff);
		hist_day.setCutoff(cutoff);
		dataPrometheus.setCutOff(cutoff);
		counter.setCutOff(cutoff);
		counter_session.setCutOff(cutoff);
	}
	else if (option == "SHARE_LOC")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setShareLatLon(b);
		plugins += "param_share_loc=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "IP_BIND")
	{
		server.setIP(arg);
	}
	else if (option == "CONTEXT")
	{
		plugins += "context='" + arg + "';\n";
	}
	else if (option == "MESSAGE" || option == "MSG")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setMsgSave(b);
		plugins += "message_save=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "LON")
	{
		ships.setLon(Util::Parse::Float(arg));
		planes.setLon(Util::Parse::Float(arg));
	}
	else if (option == "USE_GPS")
	{
		ships.setUseGPS(Util::Parse::Switch(arg));
	}
	else if (option == "KML")
	{
		KML = Util::Parse::Switch(arg);
	}
	else if (option == "GEOJSON")
	{
		GeoJSON = Util::Parse::Switch(arg);
	}
	else if (option == "OWN_MMSI")
	{
		ships.setOwnMMSI(Util::Parse::Integer(arg, 0, 999999999, option));
	}
	else if (option == "HISTORY")
	{
		ships.setTimeHistory(Util::Parse::Integer(arg, 5, 12 * 3600, option));
	}
	else if (option == "FILE")
	{
		backup_filename = arg;
	}
	else if (option == "CDN")
	{
		cdn = arg;
		Info() << "Fetch Web Libraries locally at " << arg;
	}
	else if (option == "MBTILES")
	{
		addMBTilesSource(arg, false);
	}
	else if (option == "MBOVERLAY")
	{
		addMBTilesSource(arg, true);
	}
	else if (option == "FSTILES")
	{
		addFileSystemTilesSource(arg, false);
	}
	else if (option == "FSOVERLAY")
	{
		addFileSystemTilesSource(arg, true);
	}
	else if (option == "BACKUP")
	{
		backup_interval = Util::Parse::Integer(arg, 5, 2 * 24 * 60, option);
	}
	else if (option == "REALTIME")
	{
		realtime = Util::Parse::Switch(arg);
		if (realtime)
			plugins += "realtime_enabled = true;\n";
		else
			plugins += "realtime_enabled = false;\n";
	}
	else if (option == "LOG")
	{
		showlog = Util::Parse::Switch(arg);
		if (showlog)
			plugins += "log_enabled = true;\n";
		else
			plugins += "log_enabled = false;\n";
	}
	else if (option == "DECODER")
	{
		showdecoder = Util::Parse::Switch(arg);
		if (showdecoder)
			plugins += "decoder_enabled = true;\n";
		else
			plugins += "decoder_enabled = false;\n";

		sse_streamer.setObfuscate(!showdecoder);
	}
	else if (option == "PLUGIN")
	{
		addPlugin(arg);
	}
	else if (option == "STYLE")
	{
		Info() << "Server: adding plugin (CSS): " << arg;
		plugins += "console.log('css:" + arg + "');";
		plugins += "plugins += 'CSS: " + arg + "\\n';";

		stylesheets += "/* ================ */\n";
		stylesheets += "/* CSS plugin: " + arg + "*/\n";
		try
		{
			stylesheets += Util::Helper::readFile(arg) + "\n";
		}
		catch (const std::exception &e)
		{
			stylesheets += "/* FAILED */\r\n";
			Warning() << "Server: style plugin error - " << e.what();
			plugins += "server_message += \"Plugin error: " + std::string(e.what()) + "\\n\"\n";
		}
	}
	else if (option == "PLUGIN_DIR")
	{
		const std::vector<std::string> &files_js = Util::Helper::getFilesWithExtension(arg, ".pjs");
		for (auto f : files_js)
			Set("PLUGIN", f);

		const std::vector<std::string> &files_ss = Util::Helper::getFilesWithExtension(arg, ".pss");
		for (auto f : files_ss)
			Set("STYLE", f);

		if (!files_ss.size() && !files_js.size())
			Info() << "Server: no plugin files found in directory.";
	}
	else if (option == "ABOUT")
	{
		Info() << "Server: about context from " << arg;
		about = Util::Helper::readFile(arg);
		aboutPresent = true;
	}
	else if (option == "PROME")
	{
		supportPrometheus = Util::Parse::Switch(arg);
	}
	else if (option == "REUSE_PORT")
	{
		server.setReusePort(Util::Parse::Switch(arg));
	}
	else if (!filter.SetOption(option, arg))
	{
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);
	}
	else
	{
		ships.setFilterOption(option, arg);
		raw_counter.setFilterOption(option, arg);
	}

	return *this;
}

// --- Route Handler Implementations ---

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

