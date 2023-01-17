/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include "Receiver.h"
#include "Utilities.h"
#include "JSON/StringBuilder.h"

#include "Statistics.h"
//--------------------------------------------

void Receiver::refreshDevices(void) {
	device_list.clear();

	RTLSDR().getDeviceList(device_list);
	AIRSPYHF().getDeviceList(device_list);
	AIRSPY().getDeviceList(device_list);
	SDRPLAY().getDeviceList(device_list);
	HACKRF().getDeviceList(device_list);
	SOAPYSDR().getDeviceList(device_list);
}

void Receiver::setTags(const std::string& s) {
	for (char c : s) {
		switch (toupper(c)) {
		case 'M':
			tag.mode |= 4;
			break;
		case 'T':
			tag.mode |= 2;
			break;
		case 'D':
			tag.mode |= 1;
			break;
		default:
			throw std::runtime_error("illegal tag '" + std::string(1, c) + "' defined on command line [D / T / M]");
		}
	}
}

void Receiver::setChannel(std::string mode, std::string NMEA) {

	Util::Convert::toUpper(mode);
	Util::Convert::toUpper(NMEA);

	if (NMEA.length() != 2 && NMEA.length() != 0)
		throw std::runtime_error("invalid NMEA channel designation: " + NMEA);

	for (int i = 0; i < NMEA.length(); i++)
		if (!std::isalpha(NMEA[i]) && !std::isdigit(NMEA[i]) && NMEA[i] != '?')
			throw std::runtime_error("invalid NMEA channel designation: " + std::string(1, NMEA[i]));

	if (mode == "AB") {
		ChannelMode = AIS::Mode::AB;
		if (NMEA.empty()) NMEA = "AB";
	}
	else if (mode == "CD") {
		ChannelMode = AIS::Mode::CD;
		if (NMEA.empty()) NMEA = "CD";
	}
	else
		throw std::runtime_error("channel mode needs to be AB or CD");

	ChannelNMEA = NMEA;
}

// Set up Device
Device::Device* Receiver::getDeviceByType(Type type) {
	switch (type) {
	case Type::WAVFILE:
		return &_WAV;
	case Type::RAWFILE:
		return &_RAW;
	case Type::RTLTCP:
		return &_RTLTCP;
	case Type::SPYSERVER:
		return &_SpyServer;
#ifdef HASZMQ
	case Type::ZMQ:
		return &_ZMQ;
#endif
#ifdef HASAIRSPYHF
	case Type::AIRSPYHF:
		return &_AIRSPYHF;
#endif
#ifdef HASAIRSPY
	case Type::AIRSPY:
		return &_AIRSPY;
#endif
#ifdef HASSDRPLAY
	case Type::SDRPLAY:
		return &_SDRPLAY;
#endif
#ifdef HASRTLSDR
	case Type::RTLSDR:
		return &_RTLSDR;
#endif
#ifdef HASHACKRF
	case Type::HACKRF:
		return &_HACKRF;
#endif
#ifdef HASSOAPYSDR
	case Type::SOAPYSDR:
		return &_SOAPYSDR;
#endif
	default:
		return nullptr;
	}
}

void Receiver::setupDevice() {

	int idx = device_list.empty() ? -1 : 0;
	uint64_t handle = device_list.empty() ? 0 : device_list[0].getHandle();

	if (!serial.empty())
		std::cerr << "Searching for device with SN " << serial << "." << std::endl;

	if (!serial.empty() || type != Type::NONE) {
		idx = -1;
		for (int i = 0; i < device_list.size(); i++) {
			bool serial_match = device_list[i].getSerial() == serial && (type == Type::NONE || type == device_list[i].getType());
			bool type_match = serial.empty() && (type == device_list[i].getType());

			if (serial_match || type_match) {
				idx = i;
				handle = device_list[i].getHandle();
				break;
			}
		}
		if (idx == -1) {

			if (!serial.empty())
				throw std::runtime_error("Cannot find device with specified parameters");

			idx = 0;
			handle = 0;
		}
	}

	if (type == Type::NONE && device_list.size() == 0)
		throw std::runtime_error("No devices available");

	if (type != Type::NONE)
		device = getDeviceByType(type);
	else
		device = getDeviceByType(device_list[idx].getType());

	if (device == 0) throw std::runtime_error("cannot set up device");

	device->Open(handle);

	if (ChannelMode == AIS::Mode::AB)
		device->setFrequency((int)(162000000));
	else
		device->setFrequency((int)(156800000));

	device->setTag(tag);
}

// Set up model
std::unique_ptr<AIS::Model>& Receiver::addModel(int m) {

	switch (m) {
	case 0:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelStandard()));
		break;
	case 1:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelBase()));
		break;
	case 2:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelDefault()));
		break;
	case 3:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelDiscriminator()));
		break;
	case 4:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelChallenger()));
		break;
	case 5:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelNMEA()));
		break;
	default:
		throw std::runtime_error("Model not implemented in this version. Check in later.");
	}
	return models.back();
}

void Receiver::setupModel() {
	// if nothing defined, create one model
	if (!models.size())
		addModel((device->getFormat() == Format::TXT) ? 5 : 2);

	// build the decoder models
	for (auto& m : models)
		m->buildModel(ChannelNMEA[0], ChannelNMEA[1], device->getSampleRate(), timing, device);

	// ensure some basic compatibility between model and device
	for (const auto& m : models) {
		if ((m->getClass() == AIS::ModelClass::TXT && device->getFormat() != Format::TXT) ||
			(m->getClass() != AIS::ModelClass::TXT && device->getFormat() == Format::TXT))
			throw std::runtime_error("Decoding model and input format not consistent.");
	}

	// set up JSON output channels, still unconnected
	jsonais.clear();
	jsonais.resize(models.size());
}

void Receiver::play() {

	// connect the JSON output where and if needed
	for (int i = 0; i < jsonais.size(); i++) {
		if (jsonais[i].out.isConnected()) {
			models[i]->Output() >> jsonais[i];
		}
	}

	device->Play();

	if (verbose) {
		std::cerr << "Device    : " << device->getProduct() << std::endl;
		std::cerr << "Settings  : " << device->Get() << std::endl;
		for (int i = 0; i < models.size(); i++)
			std::cerr << "Model #" + std::to_string(i) + "  : [" + models[i]->getName() + "] " + models[i]->Get() + "\n";
	}
}

void Receiver::stop() {
	device->Stop();
}

//-----------------------------------
// set up screen output


void OutputScreen::setScreen(const std::string& str) {
	switch (Util::Parse::Integer(str, 0, 5)) {
	case 0:
		level = OutputLevel::NONE;
		break;
	case 1:
		level = OutputLevel::NMEA;
		break;
	case 2:
		level = OutputLevel::FULL;
		break;
	case 3:
		level = OutputLevel::JSON_NMEA;
		break;
	case 4:
		level = OutputLevel::JSON_SPARSE;
		break;
	case 5:
		level = OutputLevel::JSON_FULL;
		break;
	default:
		throw std::runtime_error("unknown option for screen output: " + str);
	}
}
void OutputScreen::setScreen(OutputLevel o) {
	level = o;
}

void OutputScreen::setup(Receiver& r) {

	if (level == OutputLevel::NMEA || level == OutputLevel::JSON_NMEA || level == OutputLevel::FULL) {
		r.Output(0) >> msg2screen;
		msg2screen.setDetail(level);
	}
	else if (level == OutputLevel::JSON_SPARSE || level == OutputLevel::JSON_FULL) {
		r.OutputJSON(0) >> json2screen;

		if (level == OutputLevel::JSON_SPARSE) json2screen.setMap(JSON_DICT_SPARSE);
	}
}

//-----------------------------------
// set up screen counters

void OutputStatistics::setup(Receiver& r) {

	if (r.verbose) {
		statistics.resize(r.Count());

		for (int i = 0; i < r.Count(); i++)
			r.Output(i) >> statistics[i];
	}
}

//-----------------------------------
// set up UDP

void OutputUDP::setup(Receiver& r) {
	// Create and connect output to UDP stream
	for (int i = 0; i < _UDP.size(); i++) {
		r.Output(_UDP[i]->getSource()) >> *_UDP[i];
		_UDP[i]->Start();
	}
}

IO::UDP& OutputUDP::add() {

	_UDP.push_back(std::unique_ptr<IO::UDP>(new IO::UDP()));
	return *_UDP.back();
}

IO::UDP& OutputUDP::add(const std::string& host, const std::string& port) {
	_UDP.push_back(std::unique_ptr<IO::UDP>(new IO::UDP()));

	_UDP.back()->Set("HOST", host).Set("PORT", port);
	_UDP.back()->setSource(0);

	return *_UDP.back();
}

std::unique_ptr<IO::HTTP>& OutputHTTP::add(const std::vector<std::vector<std::string>>& km, int dict) {
	_http.push_back(std::unique_ptr<IO::HTTP>(new IO::HTTP(&km, dict)));
	return _http.back();
}

// set up client thread to periodically submit msgs over HTTP
// we will have a json decoder for every model

void OutputHTTP::setup(Receiver& r) {

	for (auto& h : _http) {

		h->Set("MODEL", r.Model(h->getSource())->getName()).Set("MODEL_SETTING", r.Model(h->getSource())->Get());
		h->Set("DEVICE_SETTING", r.device->Get()).Set("PRODUCT", r.device->getProduct());
		h->Set("VENDOR", r.device->getVendor()).Set("SERIAL", r.device->getSerial());

		r.OutputJSON(h->getSource()) >> *h;
		h->Start();
	}
}

void OutputServer::Counter::Receive(const AIS::Message* msg, int len, TAG& tag) {
	stat.Add(msg[0], tag);
}

bool OutputServer::Save() {
	std::cerr << "Server: writing statistics to file " << filename << std::endl;
	try {
		std::ofstream infile(filename, std::ios::binary);
		if (!counter.Save(infile)) return false;
		if (!hist_second.Save(infile)) return false;
		if (!hist_minute.Save(infile)) return false;
		if (!hist_hour.Save(infile)) return false;
		if (!hist_day.Save(infile)) return false;
		infile.close();
	}
	catch (const std::exception& e) {
		// An exception occurred
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}
	return true;
}

void OutputServer::Clear() {
	counter.Clear();

	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

bool OutputServer::Load() {

	if (filename.empty()) return false;

	std::cerr << "Server: reading statistics from " << filename << std::endl;
	try {
		std::ifstream infile(filename, std::ios::binary);

		if (!counter.Load(infile)) return false;
		if (!hist_second.Load(infile)) return false;
		if (!hist_minute.Load(infile)) return false;
		if (!hist_hour.Load(infile)) return false;
		if (!hist_day.Load(infile)) return false;

		infile.close();
	}
	catch (const std::exception& e) {
		// An exception occurred
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

void OutputServer::BackupService() {

	std::cerr << "Server: starting backup service every " << backup_interval << " minutes." << std::endl;
	while (true) {
		std::unique_lock<std::mutex> lock(m);

		if (cv.wait_for(lock, std::chrono::minutes(backup_interval), [&] { return !run; })) {
			break;
		}

		std::cerr << "Server: initiate backup." << std::endl;
		if (!Save())
			std::cerr << "Server: failed to write backup." << std::endl;
	}

	std::cerr << "Server: stopping backup service." << std::endl;
}

void OutputServer::setup(Receiver& r) {

	ships.setup(lat, lon);

	if (filename.empty())
		Clear();
	else if (!Load()) {
		std::cerr << "Statistics: cannot read file." << std::endl;
		Clear();
	}

	sample_rate = std::to_string(r.device->getSampleRate() / 1000) + "K/S";
	product = r.device->getProduct();
	vendor = r.device->getVendor();
	serial = r.device->getSerial();
	model = r.Model(0)->getName();

	// connect all the statistical counters
	r.Output(0) >> counter;
	r.OutputJSON(0) >> ships;

	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;

	*r.device >> raw_counter;

	if (firstport && lastport) {
		for (port = firstport; port <= lastport; port++)
			if (start(port)) break;

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

		backup_thread = std::thread(&OutputServer::BackupService, this);
		backup_thread.detach();
	}
}

void OutputServer::close() {

	if (backup_interval > 0) {
		std::unique_lock<std::mutex> lock(m);

		run = false;
		cv.notify_all();
	}

	if (!filename.empty() && !Save()) {
		std::cerr << "Statistics: cannot write file." << std::endl;
	}
}
#include "HTML/HTML.cpp"
#include "HTML/favicon.cpp"

void OutputServer::Request(SOCKET s, const std::string& response) {

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
		Response(s, "text/html", (char*)index_html_gz, index_html_gz_len, true);
	}
	if (r == "/favicon.ico") {
		Response(s, "text/html", (char*)favicon_ico_gzip, favicon_ico_gzip_len, true);
	}
	else if (r == "/stat.json") {

		std::string content = "{\"stat\":" + counter.toJSON() + ",";
		content += "\"last_day\":" + hist_day.lastStatToJSON() + ",";
		content += "\"last_hour\":" + hist_hour.lastStatToJSON() + ",";
		content += "\"last_minute\":" + hist_minute.lastStatToJSON() + ",";

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
		content += "\"run_time\":\"" + std::to_string((long int)time(0) - (long int)time_start) + "\",";

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

		Response(s, "application/json", content);
	}
	else if (r == "/ships.json") {

		std::string content = ships.getJSON();
		Response(s, "application/json", content);
	}
	else if (r == "/ships_full.json") {

		std::string content = ships.getJSON(true);
		Response(s, "application/json", content);
	}
	else if (r == "/config.js") {
		Response(s, "application/javascript", plugins);
	}
	else if (r == "/config.css") {
		Response(s, "text/css", stylesheets);
	}
	else if (r == "/path.json") {
		int mmsi = -1;
		std::stringstream ss(a);
		ss >> mmsi;
		if (mmsi >= 1000000 && mmsi <= 999999999) {
			std::string content = ships.getPathJSON(mmsi);
			Response(s, "application/json", content);
		}
		else
			Response(s, "application/json", "[]");
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
		content += "}";
		Response(s, "application/json", content);
	}
	else
		Server::Request(s, r);
}

Setting& OutputServer::Set(std::string option, std::string arg) {
	Util::Convert::toUpper(option);

	if (option == "PORT") {
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535);
	}
	else if (option == "PORT_MIN") {
		firstport = Util::Parse::Integer(arg, 1, 65535);
		lastport = MAX(firstport, lastport);
	}
	else if (option == "PORT_MAX") {
		lastport = Util::Parse::Integer(arg, 1, 65535);
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
		lat = Util::Parse::Float(arg);
	}
	else if (option == "LON") {
		lon = Util::Parse::Float(arg);
	}
	else if (option == "HISTORY") {
		ships.setTimeHistory(Util::Parse::Integer(arg, 5, 3600));
	}
	else if (option == "FILE") {
		filename = arg;
	}
	else if (option == "BACKUP") {
		backup_interval = Util::Parse::Integer(arg, 5, 2 * 24 * 60);
	}
	else if (option == "PLUGIN") {
		std::cerr << "Server: adding plugin (JS): " << arg << std::endl;
		plugins += "console.log('plugin:" + arg + "');";
		plugins += "plugins += 'JS: " + arg + "\\n';";
		plugins += "\n\n//=============\n//" + arg + "\n\n";
		plugins += Util::Helper::readFile(arg);
	}
	else if (option == "STYLE") {
		std::cerr << "Server: adding plugin (CSS): " << arg << std::endl;
		plugins += "console.log('css:" + arg + "');";
		plugins += "plugins += 'CSS: " + arg + "\\n';";

		stylesheets += "/* ================ */\r\n";
		stylesheets += "/* CSS plugin: " + arg + "*/\r\n";
		stylesheets += Util::Helper::readFile(arg) + "\r\n";
	}
	else if (option == "PLUGIN_DIR") {
		const std::vector<std::string>& files_js = Util::Helper::getFilesWithExtension(arg, ".pjs");
		for (auto f : files_js) Set("PLUGIN", f);

		const std::vector<std::string>& files_ss = Util::Helper::getFilesWithExtension(arg, ".pss");
		for (auto f : files_ss) Set("STYLE", f);

		if (!files_ss.size() && !files_js.size()) std::cerr << "Server: no plugin files found in directory." << std::endl;
	}
	else if (option == "REUSE_PORT") {
		setReusePort(Util::Parse::Switch(arg));
	}
	else
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);

	return *this;
}
