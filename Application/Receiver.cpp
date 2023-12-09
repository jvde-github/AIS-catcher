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

std::vector<Device::Description> Receiver::device_list;


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

	if (mode == "AB") {
		ChannelMode = AIS::Mode::AB;
		if (NMEA.empty()) NMEA = "AB";
	}
	else if (mode == "CD") {
		ChannelMode = AIS::Mode::CD;
		if (NMEA.empty()) NMEA = "CD";
	}
	else if (mode == "X") {
		ChannelMode = AIS::Mode::X;
		if (NMEA.empty()) {
			NMEA = "XX";
		}
		else if (NMEA.length() == 1) {
			NMEA += NMEA;
		}
	}
	else
		throw std::runtime_error("channel mode needs to be AB, CD or X");

	if (NMEA.length() != 2)
		throw std::runtime_error("invalid NMEA channel designation: " + NMEA);

	for (int i = 0; i < NMEA.length(); i++)
		if (!std::isalpha(NMEA[i]) && !std::isdigit(NMEA[i]) && NMEA[i] != '?')
			throw std::runtime_error("invalid NMEA channel designation: " + std::string(1, NMEA[i]));

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
	case Type::SERIALPORT:
		return &_SerialPort;
	case Type::UDP:
		return &_UDP;
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

	if (sample_rate) device->setSampleRate(sample_rate);
	if (ppm) device->Set("FREQOFFSET", std::to_string(ppm));
	if (bandwidth) device->Set("BW", std::to_string(bandwidth));

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

void Receiver::setupModel(int& group) {
	// if nothing defined, create one model
	if (!models.size())
		addModel((device->getFormat() == Format::TXT) ? 5 : 2);

	// ensure some basic compatibility between model and device
	for (const auto& m : models) {
		if ((m->getClass() == AIS::ModelClass::TXT && device->getFormat() != Format::TXT) ||
			(m->getClass() != AIS::ModelClass::TXT && device->getFormat() == Format::TXT))
			throw std::runtime_error("Decoding model and input format not consistent.");
	}

	// build the decoder models
	for (auto& m : models) {
		m->setDesignation(ChannelNMEA);
		m->setMode(ChannelMode);
		m->buildModel(ChannelNMEA[0], ChannelNMEA[1], device->getSampleRate(), timing, device);
	}

	// set up JSON output channels, still unconnected
	jsonais.clear();
	jsonais.resize(models.size());

	// assign the output of each individual model to a seperate group
	assert(group + models.size() < 32);

	for (int i = 0; i < models.size(); i++) {
		uint32_t mask = 1 << group;
		jsonais[i].out.setGroupOut(mask);
		models[i]->Output().out.setGroupOut(mask);
		group++;
	}
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
			std::cerr << "Model #" + std::to_string(i) << " -> (Src: " << std::to_string(Util::Helper::lsb(models[i]->Output().out.getGroupOut()) + 1)
					  << ", Grp: " + std::to_string(models[i]->Output().out.getGroupOut()) + "): [" + models[i]->getName() + "] " + models[i]->Get() + "\n";
	}
}

void Receiver::stop() {
	if (device)
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

void OutputScreen::connect(Receiver& r) {

	if (level == OutputLevel::NMEA || level == OutputLevel::JSON_NMEA || level == OutputLevel::FULL) {
		for (int j = 0; j < r.Count(); j++) {
			if (r.Output(j).canConnect(((StreamIn<AIS::Message>)msg2screen).getGroupsIn()))
				r.Output(j).Connect((StreamIn<AIS::Message>*)&msg2screen);

			if (r.OutputGPS(j).canConnect(((StreamIn<AIS::GPS>)msg2screen).getGroupsIn()))
				r.OutputGPS(j).Connect((StreamIn<AIS::GPS>*)&msg2screen);
		}

		msg2screen.setDetail(level);
	}
	else if (level == OutputLevel::JSON_SPARSE || level == OutputLevel::JSON_FULL) {
		for (int j = 0; j < r.Count(); j++) {
			if (r.OutputJSON(j).canConnect(((StreamIn<JSON::JSON>)json2screen).getGroupsIn()))
				r.OutputJSON(j).Connect((StreamIn<JSON::JSON>*)&json2screen);

			if (r.OutputGPS(j).canConnect(((StreamIn<AIS::GPS>)json2screen).getGroupsIn()))
				r.OutputGPS(j).Connect((StreamIn<AIS::GPS>*)&json2screen);
		}

		if (level == OutputLevel::JSON_SPARSE) json2screen.setMap(JSON_DICT_SPARSE);
	}
}

void OutputScreen::start() {}
//-----------------------------------
// set up screen counters

void OutputStatistics::connect(Receiver& r) {

	if (r.verbose) {
		statistics.resize(r.Count());

		for (int i = 0; i < r.Count(); i++)
			r.Output(i) >> statistics[i];
	}
}

void OutputStatistics::start() {}

//-----------------------------------
// set up UDP

void OutputUDP::connect(Receiver& r) {
	// Create and connect output to UDP stream
	for (int i = 0; i < _UDP.size(); i++) {
		for (int j = 0; j < r.Count(); j++) {
			StreamIn<AIS::Message>* um = (StreamIn<AIS::Message>*)&*_UDP[i];
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.Output(j).Connect(um);

			StreamIn<AIS::GPS>* ug = (StreamIn<AIS::GPS>*)&*_UDP[i];
			if (r.OutputGPS(j).canConnect(ug->getGroupsIn()))
				r.OutputGPS(j).Connect(ug);
		}
	}
}

void OutputUDP::start() {
	// Create and connect output to UDP stream
	for (int i = 0; i < _UDP.size(); i++) {
		_UDP[i]->Start();
	}
}

IO::UDPStreamer& OutputUDP::add() {

	_UDP.push_back(std::unique_ptr<IO::UDPStreamer>(new IO::UDPStreamer()));
	return *_UDP.back();
}

IO::UDPStreamer& OutputUDP::add(const std::string& host, const std::string& port) {
	_UDP.push_back(std::unique_ptr<IO::UDPStreamer>(new IO::UDPStreamer()));

	_UDP.back()->Set("HOST", host).Set("PORT", port);

	return *_UDP.back();
}

//-----------------------------------
// set up TCP

void OutputTCP::connect(Receiver& r) {
	for (int i = 0; i < _TCP.size(); i++) {
		for (int j = 0; j < r.Count(); j++) {
			StreamIn<AIS::Message>* tm = (StreamIn<AIS::Message>*)&*_TCP[i];
			if (r.Output(j).canConnect(tm->getGroupsIn()))
				r.Output(j).Connect(tm);

			StreamIn<AIS::GPS>* tg = (StreamIn<AIS::GPS>*)&*_TCP[i];
			if (r.OutputGPS(j).canConnect(tg->getGroupsIn()))
				r.OutputGPS(j).Connect(tg);
		}
	}
}

void OutputTCP::start() {
	for (int i = 0; i < _TCP.size(); i++) {
		_TCP[i]->Start();
	}
}

IO::TCPClientStreamer& OutputTCP::add() {

	_TCP.push_back(std::unique_ptr<IO::TCPClientStreamer>(new IO::TCPClientStreamer()));
	return *_TCP.back();
}

IO::TCPClientStreamer& OutputTCP::add(const std::string& host, const std::string& port) {
	_TCP.push_back(std::unique_ptr<IO::TCPClientStreamer>(new IO::TCPClientStreamer()));

	_TCP.back()->Set("HOST", host).Set("PORT", port);

	return *_TCP.back();
}

//-----------------------------------
// set up DBMS

void OutputDBMS::connect(Receiver& r) {
	for (int i = 0; i < _PSQL.size(); i++) {
		for (int j = 0; j < r.Count(); j++)
			if (r.Output(j).canConnect(_PSQL[i]->getGroupsIn()))
				r.OutputJSON(j) >> *_PSQL[i];
	}
}

void OutputDBMS::start() {
	for (int i = 0; i < _PSQL.size(); i++) {
		_PSQL[i]->setup();
	}
}

IO::PostgreSQL& OutputDBMS::add() {

	_PSQL.push_back(std::unique_ptr<IO::PostgreSQL>(new IO::PostgreSQL()));
	return *_PSQL.back();
}


std::unique_ptr<IO::HTTPStreamer>& OutputHTTP::add(const std::vector<std::vector<std::string>>& km, int dict) {
	_http.push_back(std::unique_ptr<IO::HTTPStreamer>(new IO::HTTPStreamer(&km, dict)));
	return _http.back();
}

// set up client thread to periodically submit msgs over HTTP
// we will have a json decoder for every model

void OutputHTTP::connect(Receiver& r) {

	for (auto& h : _http) {
		h->Set("MODEL", "N/A").Set("MODEL_SETTING", "N/A");
		h->Set("DEVICE_SETTING", "N/A").Set("PRODUCT", "N/A");
		h->Set("VENDOR", "N/A").Set("SERIAL", "N/A");

		/*
				for (int i = 0; i < r.Count(); i++)
					if (r.Output(i).canConnect(h->getGroupsIn()))
						r.OutputJSON(i) >> *h;
		*/
		for (int i = 0; i < r.Count(); i++) {
			StreamIn<JSON::JSON>* tm = (StreamIn<JSON::JSON>*)&*h;
			if (r.Output(i).canConnect(tm->getGroupsIn()))
				r.OutputJSON(i).Connect(tm);

			StreamIn<AIS::GPS>* tg = (StreamIn<AIS::GPS>*)&*h;
			if (r.OutputGPS(i).canConnect(tg->getGroupsIn()))
				r.OutputGPS(i).Connect(tg);
		}
	}
}

void OutputHTTP::start() {

	for (auto& h : _http) {
		h->Start();
	}
}

// --------------------------
void OutputTCPlistener::connect(Receiver& r) {

	for (auto& h : _listener) {

		for (int j = 0; j < r.Count(); j++) {

			StreamIn<AIS::Message>* tm = (StreamIn<AIS::Message>*)&*h;
			if (r.Output(j).canConnect(tm->getGroupsIn()))
				r.Output(j).Connect(tm);

			StreamIn<AIS::GPS>* tg = (StreamIn<AIS::GPS>*)&*h;
			if (r.OutputGPS(j).canConnect(tg->getGroupsIn()))
				r.OutputGPS(j).Connect(tg);
		}
	}
}

void OutputTCPlistener::start() {

	for (auto& h : _listener) {
		h->Start();
	}
}

IO::TCPlistenerStreamer& OutputTCPlistener::add(const std::string& port) {
	_listener.push_back(std::unique_ptr<IO::TCPlistenerStreamer>(new IO::TCPlistenerStreamer()));
	_listener.back()->Set("PORT", port).Set("TIMEOUT", "0");

	return *_listener.back();
}
