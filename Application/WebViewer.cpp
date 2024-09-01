/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

bool communityFeed = false;

bool WebViewer::Save() {
	// std::cerr << "Server: writing statistics to file " << filename << std::endl;
	try {
		std::ofstream infile(filename, std::ios::binary);
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
		infile.close();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}
	return true;
}

void WebViewer::Clear() {
	counter.Clear();
	counter_session.Clear();

	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

bool WebViewer::Load() {

	if (filename.empty())
		return false;

	std::cerr << "Server: reading statistics from " << filename << std::endl;
	try {
		std::ifstream infile(filename, std::ios::binary);

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

		infile.close();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

void WebViewer::addPlugin(const std::string& arg) {
	int version = 0;
	std::string author, description;

	plugins += "console.log('plugin:" + arg + "');";
	plugins += "plugins += 'JS: " + arg + "\\n';";
	plugins += "\n\n//=============\n//" + arg + "\n\n";
	try {
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		std::shared_ptr<JSON::JSON> j = parser.parse(firstline);

		for (const auto& p : j->getProperties()) {
			switch (p.Key()) {
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
		std::cerr << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version << std::endl;
		if (version != 2)
			throw std::runtime_error("Version not supported, expected 2, got " + std::to_string(version));
		plugins += "try{" + s + "} catch (error) { showDialog(\"Error in Plugin " + arg + "\", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }";
	}
	catch (const std::exception& e) {

		plugins += "// FAILED\n";
		std::cerr << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what() << std::endl;
		plugins += "server_message += \"Plugin error (" + arg + ") " + std::string(e.what()) + "\\n\"\n";
	}
}

void WebViewer::BackupService() {

	try {
		std::cerr << "Server: starting backup service every " << backup_interval << " minutes." << std::endl;
		while (true) {
			std::unique_lock<std::mutex> lock(m);

			if (cv.wait_for(lock, std::chrono::minutes(backup_interval), [&] { return !run; })) {
				break;
			}

			// std::cerr << "Server: initiate backup." << std::endl;
			if (!Save())
				std::cerr << "Server: failed to write backup." << std::endl;
		}
	}
	catch (std::exception& e) {
		std::cerr << "WebClient BackupService: " << e.what() << std::endl;
		std::terminate();
	}

	std::cerr << "Server: stopping backup service." << std::endl;
}

void WebViewer::connect(Receiver& r) {

	bool rec_details = false;
	const std::string newline = "<br>";
	for (int j = 0; j < r.Count(); j++)
		if (r.Output(j).canConnect(groups_in)) {
			if (!rec_details) {

				sample_rate += r.device->getRateDescription() + "<br>";

				JSON::StringBuilder::stringify(r.device->getProduct(), product, false);
				JSON::StringBuilder::stringify(r.device->getVendor().empty() ? "-" : r.device->getVendor(), vendor, false);
				JSON::StringBuilder::stringify(r.device->getSerial().empty() ? "-" : r.device->getSerial(), serial, false);

				product += newline;
				vendor += newline;
				serial += newline;

				rec_details = true;
			}
			model += r.Model(j)->getName() + newline;

			r.OutputJSON(j).Connect((StreamIn<JSON::JSON>*)&ships);
			r.OutputGPS(j).Connect((StreamIn<AIS::GPS>*)&ships);

			*r.device >> raw_counter;
		}

	/*
	if (!rec_details)
		std::cerr << "Web Client: not connected to the output of any model." << std::endl;
	*/
}

void WebViewer::connect(AIS::Model& m, Connection<JSON::JSON>& json, Device::Device& device) {

	if (m.Output().out.canConnect(groups_in)) {

		json.Connect((StreamIn<JSON::JSON>*)&ships);
		device >> raw_counter;

		sample_rate = device.getRateDescription();
		setDeviceDescription(device.getProduct(), device.getVendor().empty() ? "-" : device.getVendor(), device.getSerial().empty() ? "-" : device.getSerial());
		model = m.getName();
	}
}

void WebViewer::Reset() {
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();

	raw_counter.Reset();
	ships.setup();
	time_start = time(nullptr);
}

void WebViewer::start() {
	ships.setup();

	if (filename.empty())
		Clear();
	else if (!Load()) {
		std::cerr << "Statistics: cannot read file." << std::endl;
		Clear();
	}

	ships >> sse_streamer;
	sse_streamer.setSSE(this);

	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;

	ships >> counter;
	ships >> counter_session;

	if (supportPrometheus)
		ships >> dataPrometheus;

	if (firstport && lastport) {
		for (port = firstport; port <= lastport; port++)
			if (HTTPServer::start(port))
				break;

		if (port > lastport)
			throw std::runtime_error("Cannot open port in range [" + std::to_string(firstport) + "," + std::to_string(lastport) + "]");

		std::cerr << "HTML Server running at port " << std::to_string(port) << std::endl;
	}
	else
		throw std::runtime_error("HTML server ports not specified");

	time_start = time(nullptr);

	run = true;

	if (backup_interval > 0) {
		if (filename.empty())
			throw std::runtime_error("Server: backup of statistics requested without providing filename.");

		backup_thread = std::thread(&WebViewer::BackupService, this);
		backup_thread.detach();
		thread_running = true;
	}
}

void WebViewer::stopThread() {
	if (thread_running) {
		std::unique_lock<std::mutex> lock(m);

		run = false;
		cv.notify_all();
		thread_running = false;
	}
}

void WebViewer::close() {

	stopThread();

	if (!filename.empty() && !Save()) {
		std::cerr << "Statistics: cannot write file." << std::endl;
	}
}

#include "HTML/index_html.cpp"
#include "HTML/index_local_html.cpp"
#include "HTML/script_js.cpp"
#include "HTML/style_css.cpp"
#include "HTML/favicon.cpp"

void WebViewer::Request(TCP::ServerConnection& c, const std::string& response, bool gzip) {

	std::string r;
	std::string a;

	std::string::size_type pos = response.find('?');

	if (pos != std::string::npos) {
		r = response.substr(0, pos);
		a = response.substr(pos + 1, response.length());
	}
	else {
		r = response;
	}

	if (r == "/") {
		if (cdn.empty())
			ResponseRaw(c, "text/html", (char*)index_html_gz, index_html_gz_len, true);
		else
			ResponseRaw(c, "text/html", (char*)index_local_html_gz, index_local_html_gz_len, true);
	}
	else if (r == "/script_" VERSION_URL_TAG ".js") {
		ResponseRaw(c, "application/javascript", (char*)script_js_gz, script_js_gz_len, true, true);
	}
	else if (r == "/style_" VERSION_URL_TAG ".css") {
		ResponseRaw(c, "text/css", (char*)style_css_gz, style_css_gz_len, true, true);
	}
	else if (!cdn.empty() && r.find("/cdn/") == 0) {
		try {

			if (r.find("..") != std::string::npos || r.find(".") == std::string::npos)
				throw std::runtime_error("Blocked, pattern not matching");

			std::string extension = r.substr(r.find_last_of('.') + 1);
			std::string contentType;

			if (extension == "svg") {
				contentType = "image/svg+xml";
			}
			else if (extension == "js") {
				contentType = "application/javascript";
			}
			else if (extension == "png") {
				contentType = "image/png";
			}
			else if (extension == "css") {
				contentType = "text/css";
			}
			else {
				throw std::runtime_error("Blocked " + extension);
			}

			std::string content = Util::Helper::readFile(cdn + r);
			Response(c, contentType, content, use_zlib & gzip);
		}
		catch (const std::exception& e) {
			std::cerr << "Server: error returning requested file (" << r << "): " << e.what() << std::endl;
			Response(c, "text/html", std::string(""), true);
		}
	}
	else if (r == "/favicon.ico") {
		ResponseRaw(c, "text/html", (char*)favicon_ico_gz, favicon_ico_gz_len, true);
	}
	else if (r == "/kml" && KML) {
		std::string content = ships.getKML();
		Response(c, "application/text", content, use_zlib);
	}
	else if (r == "/metrics") {
		if (supportPrometheus) {
			std::string content = dataPrometheus.toPrometheus();
			Response(c, "application/text", content, use_zlib);
			dataPrometheus.Reset();
		}
	}
	else if (r == "/stat.json") {

		std::string content;

		content += "{\"total\":" + counter.toJSON() + ",";
		content += "\"session\":" + counter_session.toJSON() + ",";
		content += "\"last_day\":" + hist_day.lastStatToJSON() + ",";
		content += "\"last_hour\":" + hist_hour.lastStatToJSON() + ",";
		content += "\"last_minute\":" + hist_minute.lastStatToJSON() + ",";
		content += "\"tcp_clients\":" + std::to_string(numberOfClients()) + ",";
		content += "\"sharing\":" + std::string(communityFeed ? "true" : "false") + ",";
		if (ships.getShareLatLon() && ships.getLat() != LAT_UNDEFINED && ships.getLon() != LON_UNDEFINED)
			content += "\"sharing_link\":\"https://aiscatcher.org/?&zoom=10&lat=" + std::to_string(ships.getLat()) + "&lon=" + std::to_string(ships.getLon()) + "\",";
		else
			content += "\"sharing_link\":\"https://aiscatcher.org\",";

		content += "\"station\":" + station + ",";
		content += "\"station_link\":" + station_link + ",";
		content += "\"sample_rate\":\"" + sample_rate + "\",";
		content += "\"msg_rate\":" + std::to_string(hist_second.getAverage()) + ",";
		content += "\"vessel_count\":" + std::to_string(ships.getCount()) + ",";
		content += "\"vessel_max\":" + std::to_string(ships.getMaxCount()) + ",";
		content += "\"product\":\"" + product + "\",";
		content += "\"vendor\":\"" + vendor + "\",";
		content += "\"serial\":\"" + serial + "\",";
		content += "\"model\":\"" + model + "\",";
		content += "\"build_date\":\"" + std::string(__DATE__) + "\",";
		content += "\"build_version\":\"" + std::string(VERSION) + "\",";
		content += "\"build_describe\":\"" + std::string(VERSION_DESCRIBE) + "\",";
		content += "\"run_time\":\"" + std::to_string((long int)time(nullptr) - (long int)time_start) + "\",";
		content += "\"memory\":" + std::to_string(Util::Helper::getMemoryConsumption()) + ",";
		content += "\"os\":" + os + ",";
		content += "\"hardware\":" + hardware + ",";

		std::string unit;
		const uint64_t GB = 1000000000;
		const uint64_t MB = 1000000;
		int norm = 0;

		if (raw_counter.received > GB) {
			norm = (raw_counter.received + (GB / 20)) / (GB / 10);
			unit = "GB";
		}
		else {
			norm = (raw_counter.received + (MB / 20)) / (MB / 10);
			unit = "MB";
		}

		int d1 = norm / 10;
		int d2 = norm % 10;

		content += "\"received\":\"" + std::to_string(d1) + "." + std::to_string(d2) + unit + "\"}";

		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/ships.json") {
		std::string content = ships.getJSON();
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/ships_array.json") {
		std::string content = ships.getJSONcompact();
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/sb") {
		binary.clear();
		ships.getBinary(binary);
		Response(c, "application/octet-stream", binary.data(), binary.size(), use_zlib & gzip);
	}
	else if (r == "/ships_full.json") {

		std::string content = ships.getJSON(true);
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/sse") {
		if (realtime)
			upgradeSSE(c, 1);
	}
	else if (r == "/signal") {
		if (realtime)
			upgradeSSE(c, 2);
	}
	else if (r == "/icons.png") {
		ResponseRaw(c, "image/png", (char*)icons_png_gz, icons_png_gz_len, true);
	}
	else if (r == "/config.js") {
		Response(c, "application/javascript", params + plugins + "\nserver_version = false;\ncommunityFeed = " + (communityFeed ? "true" : "false") + ";\n", use_zlib & gzip);
	}
	else if (r == "/config.css") {
		Response(c, "text/css", stylesheets, use_zlib & gzip);
	}
	else if (r == "/about.md") {
		Response(c, "text/markdown", about, use_zlib & gzip);
	}
	else if (r == "/path.json") {
		std::stringstream ss(a);
		std::string mmsi_str;
		std::string content = "{";

		while (std::getline(ss, mmsi_str, ',')) {
			try {
				int mmsi = std::stoi(mmsi_str);
				if (mmsi >= 1 && mmsi <= 999999999) {
					if (content.length() > 1)
						content += ",";
					content += "\"" + std::to_string(mmsi) + "\":" + ships.getPathJSON(mmsi);
				}
			}
			catch (const std::invalid_argument&) {
				std::cerr << "Server: path MMSI invalid: " << mmsi_str << std::endl;
			}
			catch (const std::out_of_range&) {
				std::cerr << "Server: path MMSI out of range: " << mmsi_str << std::endl;
			}
		}
		content += "}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/allpath.json") {

		std::string content = ships.getAllPathJSON();
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/geojson" && GeoJSON) {
		std::string content = ships.getGeoJSON();
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/message") {
		int mmsi = -1;
		std::stringstream ss(a);
		if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999) {
			std::string content = ships.getMessage(mmsi);
			Response(c, "application/text", content, use_zlib & gzip);
		}
		else
			Response(c, "application/text", "Message not availaible");
	}
	else if (r == "/vessel") {
		std::stringstream ss(a);
		int mmsi;
		if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999) {
			std::string content = ships.getShipJSON(mmsi);
			Response(c, "application/text", content, use_zlib & gzip);
		}
		else {
			Response(c, "application/text", "Vessel not available");
		}
	}
	else if (r == "/history_full.json") {

		std::string content = "{";
		content += "\"second\":";
		content += hist_second.toJSON();
		content += ",\"minute\":";
		content += hist_minute.toJSON();
		content += ",\"hour\":";
		content += hist_hour.toJSON();
		content += ",\"day\":";
		content += hist_day.toJSON();
		content += "}\n\n";

		Response(c, "application/json", content, use_zlib & gzip);
	}
	else
		HTTPServer::Request(c, r, false);
}

Setting& WebViewer::Set(std::string option, std::string arg) {
	Util::Convert::toUpper(option);

	if (option == "PORT") {
		port_set = true;
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535, option);
	}
	else if (option == "SERVER_MODE") {
		bool b = Util::Parse::Switch(arg);
		ships.setServerMode(b);
	}
	else if (option == "ZLIB") {
		use_zlib = Util::Parse::Switch(arg);
	}
	else if (option == "GROUPS_IN") {
		groups_in = Util::Parse::Integer(arg);
	}
	else if (option == "PORT_MIN") {
		port_set = true;
		firstport = Util::Parse::Integer(arg, 1, 65535, option);
		lastport = MAX(firstport, lastport);
	}
	else if (option == "PORT_MAX") {
		port_set = true;
		lastport = Util::Parse::Integer(arg, 1, 65535, option);
		firstport = MIN(firstport, lastport);
	}
	else if (option == "STATION") {
		station.clear();
		JSON::StringBuilder::stringify(arg, station);
	}
	else if (option == "STATION_LINK") {
		station_link.clear();
		JSON::StringBuilder::stringify(arg, station_link);
	}
	else if (option == "LAT") {
		ships.setLat(Util::Parse::Float(arg));
	}
	else if (option == "CUTOFF") {
		int cutoff = Util::Parse::Integer(arg, 0, 10000, option);
		hist_minute.setCutoff(cutoff);
		hist_second.setCutoff(cutoff);
		hist_hour.setCutoff(cutoff);
		hist_day.setCutoff(cutoff);
		dataPrometheus.setCutOff(cutoff);
		counter.setCutOff(cutoff);
		counter_session.setCutOff(cutoff);
	}
	else if (option == "SHARE_LOC") {
		bool b = Util::Parse::Switch(arg);
		ships.setShareLatLon(b);
		plugins += "param_share_loc=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "IP_BIND") {
		setIP(arg);
	}
	else if (option == "CONTEXT") {
		plugins += "context='" + arg + "';\n";
	}
	else if (option == "MESSAGE" || option == "MSG") {
		bool b = Util::Parse::Switch(arg);
		ships.setMsgSave(b);
		plugins += "message_save=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "LON") {
		ships.setLon(Util::Parse::Float(arg));
	}
	else if (option == "USE_GPS") {
		ships.setUseGPS(Util::Parse::Switch(arg));
	}
	else if (option == "KML") {
		KML = Util::Parse::Switch(arg);
	}
	else if (option == "GEOJSON") {
		GeoJSON = Util::Parse::Switch(arg);
	}
	else if (option == "OWN_MMSI") {
		ships.setOwnMMSI(Util::Parse::Integer(arg, 0, 999999999, option));
	}
	else if (option == "HISTORY") {
		ships.setTimeHistory(Util::Parse::Integer(arg, 5, 12 * 3600, option));
	}
	else if (option == "FILE") {
		filename = arg;
	}
	else if (option == "CDN") {
		cdn = arg;
		std::cerr << "Fetch Web Libraries locally at " << arg << std::endl;
	}
	else if (option == "BACKUP") {
		backup_interval = Util::Parse::Integer(arg, 5, 2 * 24 * 60, option);
	}
	else if (option == "REALTIME") {
		realtime = Util::Parse::Switch(arg);
		if (realtime)
			plugins += "realtime_enabled = true;\n";
		else
			plugins += "realtime_enabled = false;\n";
	}
	else if (option == "PLUGIN") {
		addPlugin(arg);
	}
	else if (option == "STYLE") {
		std::cerr << "Server: adding plugin (CSS): " << arg << std::endl;
		plugins += "console.log('css:" + arg + "');";
		plugins += "plugins += 'CSS: " + arg + "\\n';";

		stylesheets += "/* ================ */\n";
		stylesheets += "/* CSS plugin: " + arg + "*/\n";
		try {
			stylesheets += Util::Helper::readFile(arg) + "\n";
		}
		catch (const std::exception& e) {
			stylesheets += "/* FAILED */\r\n";
			std::cerr << "Server: style plugin error - " << e.what() << std::endl;
			plugins += "server_message += \"Plugin error: " + std::string(e.what()) + "\\n\"\n";
		}
	}
	else if (option == "PLUGIN_DIR") {
		const std::vector<std::string>& files_js = Util::Helper::getFilesWithExtension(arg, ".pjs");
		for (auto f : files_js)
			Set("PLUGIN", f);

		const std::vector<std::string>& files_ss = Util::Helper::getFilesWithExtension(arg, ".pss");
		for (auto f : files_ss)
			Set("STYLE", f);

		if (!files_ss.size() && !files_js.size())
			std::cerr << "Server: no plugin files found in directory." << std::endl;
	}
	else if (option == "ABOUT") {
		std::cerr << "Server: about context from " << arg << std::endl;
		plugins += "aboutMDpresent = true;";
		about = Util::Helper::readFile(arg);
	}
	else if (option == "PROME") {
		supportPrometheus = Util::Parse::Switch(arg);
	}
	else if (option == "REUSE_PORT") {
		setReusePort(Util::Parse::Switch(arg));
	}
	else if (!filter.SetOption(option, arg)) {
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);
	}
	else {
		ships.setFilterOption(option, arg);
		raw_counter.setFilterOption(option, arg);
	}

	return *this;
}
