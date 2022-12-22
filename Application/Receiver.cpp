/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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
#include "JSON/StringBuilder.h"

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
		r.Output(_UDP[i].getSource()) >> _UDP[i];
		_UDP[i].Start();
	}
}

IO::UDP& OutputUDP::add() {

	_UDP.push_back(IO::UDP());
	return _UDP.back();
}

IO::UDP& OutputUDP::add(const std::string& host, const std::string& port) {
	_UDP.push_back(IO::UDP());

	_UDP.back().Set("HOST", host).Set("PORT", port);
	_UDP.back().setSource(0);

	return _UDP.back();
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

//-----------------------------------
// simple ship database

void Ships::setup(float lt, float ln) {

	ships.resize(N);
	std::memset(ships.data(), 0, N * sizeof(List));

	lat = lt;
	lon = ln;

	first = N - 1;
	last = 0;
	count = 0;
	// set up linked list
	for (int i = 0; i < N; i++) {
		ships[i].next = i - 1;
		ships[i].prev = i + 1;
	}
	ships[N - 1].prev = -1;
}


// https://www.movable-type.co.uk/scripts/latlong.html
float Ships::distance(float lat_a, float lon_a, float lat_b, float lon_b) {

	float lat_a_scaled = lat_a * PI / 180;
	float lat_b_scaled = lat_b * PI / 180;
	float lat_delta = (lat_b - lat_a) * PI / 180;
	float lon_delta = (lon_b - lon_a) * PI / 180;

	float a = sin(lat_delta / 2) * sin(lat_delta / 2) + cos(lat_a_scaled) * cos(lat_b_scaled) * sin(lon_delta / 2) * sin(lon_delta / 2);
	float c = 2 * atan2(sqrt(a), sqrt(1 - a));

	return 6372797.56085 * c / 1852;
}

bool Ships::isValidCoord(float lat, float lon) {
	return !(lat == 0 && lon == 0) && lat != 91 && lon != 181;
}

std::string Ships::getJSON(bool full) {
	const std::string null_str = "null";
	std::string str;

	content = "{\"count\":" + std::to_string(count);
	content += ",\"ships\":[";

	std::time_t tm = time(nullptr);
	int ptr = first;

	delim = "";
	while (ptr != -1) {
		if (ships[ptr].ship.mmsi != 0) {
			long int delta_time = (long int)tm - (long int)ships[ptr].ship.last_signal;
			if (full || delta_time > TIME_HISTORY) break;

			content += delim + "{\"mmsi\":" + std::to_string(ships[ptr].ship.mmsi) + ",";
			if (isValidCoord(ships[ptr].ship.lat, ships[ptr].ship.lon)) {
				content += "\"lat\":" + std::to_string(ships[ptr].ship.lat) + ",";
				content += "\"lon\":" + std::to_string(ships[ptr].ship.lon) + ",";

				if (isValidCoord(lat, lon)) {
					float d = distance(ships[ptr].ship.lat, ships[ptr].ship.lon, lat, lon);
					content += "\"distance\":" + std::to_string(d) + ",";
				}
				else
					content += "\"distance\": null,";
			}
			else {
				content += "\"lat\":" + null_str + ",";
				content += "\"lon\":" + null_str + ",";
				content += "\"distance\": null,";
			}

			content += "\"mmsi_type\":" + std::to_string(ships[ptr].ship.mmsi_type) + ",";
			content += "\"level\":" + std::to_string(ships[ptr].ship.level) + ",";
			content += "\"count\":" + std::to_string(ships[ptr].ship.count) + ",";
			content += "\"ppm\":" + std::to_string(ships[ptr].ship.ppm) + ",";

			content += "\"heading\":" + ((ships[ptr].ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.heading)) + ",";
			content += "\"cog\":" + ((ships[ptr].ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.cog)) + ",";
			content += "\"speed\":" + ((ships[ptr].ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ships[ptr].ship.speed)) + ",";
			content += "\"shiptype\":" + std::to_string(ships[ptr].ship.shiptype) + ",";
			content += "\"msg_type\":" + std::to_string(ships[ptr].ship.msg_type) + ",";
			content += "\"country\":\"" + std::string(ships[ptr].ship.country_code) + "\",";
			content += "\"status\":" + std::to_string(ships[ptr].ship.status) + ",";

			content += "\"callsign\":";
			str = std::string(ships[ptr].ship.callsign);
			JSON::StringBuilder::stringify(str, content);

			content += ",\"shipname\":";
			str = std::string(ships[ptr].ship.shipname)  + (ships[ptr].ship.virtual_aid ? std::string(" [V]") : std::string(""));
			JSON::StringBuilder::stringify(str, content);

			content += ",\"destination\":";
			str = std::string(ships[ptr].ship.destination);
			JSON::StringBuilder::stringify(str, content);


			content += ",\"last_signal\":" + std::to_string(delta_time) + "}";
			delim = ",";
		}
		ptr = ships[ptr].next;
	}
	content += "],\"error\":false}";
	return content;
}

void Ships::Receive(const JSON::JSON* data, int len, TAG& tag) {

	const AIS::Message* msg = (AIS::Message*)data[0].binary;
	uint32_t mmsi = msg->mmsi();

	int ptr = first, cnt = count;
	while (ptr != -1 && --cnt >= 0) {
		if (ships[ptr].ship.mmsi == mmsi) break;
		ptr = ships[ptr].next;
	}

	if (ptr == -1 || cnt < 0) {
		ptr = last;
		count = MIN(count + 1, N);
		std::memset(&ships[ptr].ship, 0, sizeof(Detail));
		ships[ptr].ship.lat = -1;
		ships[ptr].ship.lat = LAT_UNDEFINED;
		ships[ptr].ship.lon = LON_UNDEFINED;
		ships[ptr].ship.heading = HEADING_UNDEFINED;
		ships[ptr].ship.cog = COG_UNDEFINED;
		ships[ptr].ship.status = STATUS_UNDEFINED;
		ships[ptr].ship.speed = SPEED_UNDEFINED;
	}

	if (ptr != first) {
		// remove ptr out of the linked list
		if (ships[ptr].next != -1)
			ships[ships[ptr].next].prev = ships[ptr].prev;
		else
			last = ships[ptr].prev;
		ships[ships[ptr].prev].next = ships[ptr].next;

		// new ship is first in list
		ships[ptr].next = first;
		ships[ptr].prev = -1;

		ships[first].prev = ptr;
		first = ptr;
	}

	ships[ptr].ship.mmsi = msg->mmsi();
	ships[ptr].ship.count++;
	ships[ptr].ship.last_signal = msg->getRxTimeUnix();
	ships[ptr].ship.ppm = tag.ppm;
	ships[ptr].ship.level = tag.level;

	// type derived from MMSI
	if ((mmsi >= 200000000 && mmsi < 800000000) || (mmsi >= 20000000 && mmsi < 80000000) || ((mmsi >= 982000000 && mmsi < 988000000)))
		ships[ptr].ship.mmsi_type = (msg->type() == 18 || msg->type() == 19 || msg->type() == 24) ? MMSI_TYPE_CLASS_B : MMSI_TYPE_CLASS_A; // Class A & Class B
	else if ((mmsi >= 2000000 && mmsi < 8000000))
		ships[ptr].ship.mmsi_type = MMSI_TYPE_BASESTATION; // Base Station
	else if ((mmsi >= 111000000 && mmsi < 111999999) || (mmsi >= 11100000 && mmsi < 11199999))
		ships[ptr].ship.mmsi_type = MMSI_TYPE_SAR; // helicopter
	else if (mmsi >= 97000000 && mmsi < 98000000)
		ships[ptr].ship.mmsi_type = MMSI_TYPE_SARTEPIRB; //  AIS SART/EPIRB/man-overboard
	else if (mmsi >= 992000000 && mmsi < 998000000)
		ships[ptr].ship.mmsi_type = MMSI_TYPE_ATON; //  AtoN
	else
		ships[ptr].ship.mmsi_type = MMSI_TYPE_OTHER; // anything else

	for (const auto& p : data[0].getProperties()) {
		switch (p.Key()) {
		case AIS::KEY_LAT:
			ships[ptr].ship.lat = p.Get().getFloat();
			break;
		case AIS::KEY_LON:
			ships[ptr].ship.lon = p.Get().getFloat();
			break;
		case AIS::KEY_SHIPTYPE:
			ships[ptr].ship.shiptype = p.Get().getInt();
			break;
		case AIS::KEY_HEADING:
			ships[ptr].ship.heading = p.Get().getInt();
			break;
		case AIS::KEY_COURSE:
			ships[ptr].ship.cog = p.Get().getFloat();
			break;
		case AIS::KEY_SPEED:
			if (msg->type() == 9 && p.Get().getInt() != 1023)
				ships[ptr].ship.speed = p.Get().getInt();
			else if (p.Get().getFloat() != 102.3)
				ships[ptr].ship.speed = p.Get().getFloat();
			break;
		case AIS::KEY_STATUS:
			ships[ptr].ship.status = p.Get().getInt();
			break;
		case AIS::KEY_VIRTUAL_AID:
			ships[ptr].ship.virtual_aid = p.Get().getBool();
			break;
		case AIS::KEY_NAME:
		case AIS::KEY_SHIPNAME:
			std::strncpy(ships[ptr].ship.shipname, p.Get().getString().c_str(), 20);
			break;
		case AIS::KEY_CALLSIGN:
			std::strncpy(ships[ptr].ship.callsign, p.Get().getString().c_str(), 7);
			break;
		case AIS::KEY_COUNTRY_CODE:
			std::strncpy(ships[ptr].ship.country_code, p.Get().getString().c_str(), 2);
			break;
		case AIS::KEY_DESTINATION:
			std::strncpy(ships[ptr].ship.destination, p.Get().getString().c_str(), 20);
			break;
		}
	}
}

void OutputServer::Counter::Receive(const AIS::Message* msg, int len, TAG& tag) {
	count[0]++;

	if (msg->type() >= 1 && msg->type() <= 27)
		count[msg->type()]++;

	if (msg->getChannel() == 'A') ChA++;
	if (msg->getChannel() == 'B') ChB++;
}

void OutputServer::setup(Receiver& r) {

	ships.setup(lat, lon);

	sample_rate = std::to_string(r.device->getSampleRate() / 1000) + "K/S";
	product = r.device->getProduct();
	vendor = r.device->getVendor();
	serial = r.device->getSerial();
	model = r.Model(0)->getName();

	// connect all the statistical counters
	r.Output(0) >> counter;
	r.OutputJSON(0) >> ships;
	r.Output(0) >> hist_hour;
	r.Output(0) >> hist_minute;
	r.Output(0) >> hist_second;

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
}

#include "HTML/HTML.cpp"
#include "HTML/favicon.cpp"

void OutputServer::Request(SOCKET s, const std::string& r) {

	if (r == "/") {
		Response(s, "text/html", (char*)index_html_gz, index_html_gz_len, true);
	}
	if (r == "/favicon.ico") {
		Response(s, "text/html", (char*)favicon_ico_gzip, favicon_ico_gzip_len, true);
	}
	else if (r == "/stat.json") {

		std::string content = "{\"total\":" + std::to_string(counter.count[0]) + ",";
		for (int i = 1; i <= 27; i++) {
			content += "\"msg" + std::to_string(i) + "\":" + std::to_string(counter.count[i]) + ",";
		}

		content += "\"station\":" + station + ",";
		content += "\"station_link\":" + station_link + ",";
		content += "\"sample_rate\":\"" + sample_rate + "\",";
		content += "\"msg_rate\":" + std::to_string(hist_second.average()) + ",";
		content += "\"last_minute\":" + std::to_string(hist_minute.last()) + ",";
		content += "\"last_hour\":" + std::to_string(hist_hour.last()) + ",";
		content += "\"vessel_count\":" + std::to_string(ships.getCount()) + ",";
		content += "\"vessel_max\":" + std::to_string(ships.getMaxCount()) + ",";
		content += "\"product\":\"" + product + "\",";
		content += "\"vendor\":\"" + vendor + "\",";
		content += "\"serial\":\"" + serial + "\",";
		content += "\"model\":\"" + model + "\",";
		content += "\"run_time\":\"" + Util::Convert::toDeltaTimeStr((long int)time(0) - (long int)time_start) + "\",";

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

		content += "\"received\":\"" + std::to_string(d1) + "." + std::to_string(d2) + unit + "\",";
		content += "\"msgA\":" + std::to_string(counter.ChA) + ",\"msgB\":" + std::to_string(counter.ChB) + "}";

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
	else if (r == "/history_full.json") {

		std::string content = "{";
		content += "\"second\":";
		hist_second.addJSONarray(content);
		content += ",\"minute\":";
		hist_minute.addJSONarray(content);
		content += ",\"hour\":";
		hist_hour.addJSONarray(content);
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
	else
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);

	return *this;
}
