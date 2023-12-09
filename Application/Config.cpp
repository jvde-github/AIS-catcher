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

#include "Config.h"

bool Config::isActiveObject(const JSON::Value& pd) {

	if (!pd.isObject())
		throw std::runtime_error("expected JSON \"object\"");

	for (const JSON::Property& p : pd.getObject().getProperties()) {
		if (p.Key() == AIS::KEY_SETTING_ACTIVE) {
			return Util::Parse::Switch(p.Get().to_string());
		}
	}
	return true;
}

void Config::setSettingsFromJSON(const JSON::Value& pd, Setting& s) {

	for (const JSON::Property& p : pd.getObject().getProperties())
		if (p.Key() != AIS::KEY_SETTING_ACTIVE) {
			s.Set(AIS::KeyMap[p.Key()][JSON_DICT_SETTING], p.Get().to_string());
		}
}


void Config::setServerfromJSON(const JSON::Value& pd) {

	if (pd.isArray()) {
		for (const auto& v : pd.getArray()) {
			if (!isActiveObject(v)) continue;
			_server.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
			setSettingsFromJSON(v, *_server.back());
			_receivers.back()->setTags("DTM");
			_server.back()->active() = true;
		}
	}
	else {
		if (!isActiveObject(pd)) return;

		_server.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
		setSettingsFromJSON(pd, *_server.back());
		_receivers.back()->setTags("DTM");
		_server.back()->active() = true;
	}
}

void Config::setHTTPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("HTTP settings need to be an \"array\" of \"objects\". in config file");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		auto& h = _http.add(AIS::KeyMap, JSON_DICT_FULL);
		setSettingsFromJSON(v, *h);
		_receivers.back()->setTags("DT");
	}
}

void Config::setUDPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("UDP settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		IO::UDPStreamer& udp = _udp.add();
		setSettingsFromJSON(v, udp);
	}
}

void Config::setTCPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("TCP settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		IO::TCPClientStreamer& tcp = _tcp.add();
		setSettingsFromJSON(v, tcp);
	}
}

void Config::setTCPListenerfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("TCP Listener settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		setSettingsFromJSON(v, _tcp_server.add());
	}
}

void Config::setModelfromJSON(const JSON::Property& p) {

	if (!isActiveObject(p.Get())) return;

	_receivers.back()->addModel(2);
	setSettingsFromJSON(p.Get(), *_receivers.back()->Model(_receivers.back()->Count() - 1));
}

void Config::setReceiverFromArray(const JSON::Property& pd) {
	if (!pd.Get().isArray())
		throw std::runtime_error("Receiver settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		setReceiverfromJSON(v.getObject().getProperties(), false);
	}
}

void Config::setReceiverfromJSON(const std::vector<JSON::Property>& props, bool unspecAllowed) {
	std::string config, serial, input;
	// pass 1
	for (const auto& p : props) {
		switch (p.Key()) {
		case AIS::KEY_SETTING_SERIAL:
			serial = p.Get().to_string();
			break;
		case AIS::KEY_SETTING_INPUT:
			input = p.Get().to_string();
			break;
		case AIS::KEY_SETTING_ACTIVE:
			if (!Util::Parse::Switch(p.Get().to_string())) return;
			break;
		}
	}

	if (!unspecAllowed) {
		if (serial.empty() && input.empty())
			throw std::runtime_error("receiver needs to have a serial or input specified in Config.");
	}

	if ((!serial.empty() || !input.empty())) {
		if (++_nrec > 1)
			_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));

		_receivers.back()->Serial() = serial;
	}

	if (!input.empty()) {
		if (!Util::Parse::DeviceType(input, _receivers.back()->InputType())) {
			throw std::runtime_error("\"" + input + "\" is unknown input type in config file");
		}
	}

	// pass 2
	for (const auto& p : props) {
		switch (p.Key()) {
		case AIS::KEY_SETTING_VERBOSE:
			_receivers.back()->verbose = Util::Parse::Switch(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_MODEL:
			setModelfromJSON(p);
			break;
		case AIS::KEY_SETTING_META:
			_receivers.back()->setTags(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_RTLSDR:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->RTLSDR());
			break;
		case AIS::KEY_SETTING_RTLTCP:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->RTLTCP());
			break;
		case AIS::KEY_SETTING_AIRSPY:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->AIRSPY());
			break;
		case AIS::KEY_SETTING_AIRSPYHF:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->AIRSPYHF());
			break;
		case AIS::KEY_SETTING_SDRPLAY:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->SDRPLAY());
			break;
		case AIS::KEY_SETTING_WAVFILE:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->WAV());
			break;
		case AIS::KEY_SETTING_SERIALPORT:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->SerialPort());
			break;
		case AIS::KEY_SETTING_HACKRF:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->HACKRF());
			break;
		case AIS::KEY_SETTING_UDPSERVER:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->UDP());
			break;
		case AIS::KEY_SETTING_SOAPYSDR:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->SOAPYSDR());
			break;
		case AIS::KEY_SETTING_FILE:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->RAW());
			break;
		case AIS::KEY_SETTING_ZMQ:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->ZMQ());
			break;
		case AIS::KEY_SETTING_SPYSERVER:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receivers.back()->SpyServer());
			break;
		default:
			break;
		}
	}
}

void Config::read(std::string& file_config) {

	if (!file_config.empty()) {
		set(Util::Helper::readFile(file_config));
	}
}

void Config::set(const std::string& str) {
	std::string config, serial, input;
	int version = 0;

	JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
	std::shared_ptr<JSON::JSON> json = parser.parse(str);

	// loop over all properties
	const std::vector<JSON::Property>& props = json->getProperties();

	// pass 1
	for (const auto& p : props) {
		switch (p.Key()) {
		case AIS::KEY_SETTING_CONFIG:
			config = p.Get().to_string();
			break;
		case AIS::KEY_SETTING_VERSION:
			version = Util::Parse::Integer(p.Get().to_string());
			break;
		}
	}

	if (version < 1 || version > 1 || config != "aiscatcher")
		throw std::runtime_error("version and/or format of config file not supported (required version <=1)");

	setReceiverfromJSON(props, true);

	// pass 2
	for (const auto& p : props) {
		switch (p.Key()) {
		// fields that are already processed for completeness
		case AIS::KEY_SETTING_CONFIG:
		case AIS::KEY_SETTING_VERSION:
		case AIS::KEY_SETTING_VERBOSE:
		case AIS::KEY_SETTING_SERIAL:
		case AIS::KEY_SETTING_INPUT:
		case AIS::KEY_SETTING_MODEL:
		case AIS::KEY_SETTING_META:
		case AIS::KEY_SETTING_RTLSDR:
		case AIS::KEY_SETTING_RTLTCP:
		case AIS::KEY_SETTING_AIRSPY:
		case AIS::KEY_SETTING_AIRSPYHF:
		case AIS::KEY_SETTING_SERIALPORT:
		case AIS::KEY_SETTING_SDRPLAY:
		case AIS::KEY_SETTING_WAVFILE:
		case AIS::KEY_SETTING_HACKRF:
		case AIS::KEY_SETTING_UDPSERVER:
		case AIS::KEY_SETTING_SOAPYSDR:
		case AIS::KEY_SETTING_FILE:
		case AIS::KEY_SETTING_ZMQ:
		case AIS::KEY_SETTING_SPYSERVER:
			break;
		case AIS::KEY_SETTING_UDP:
			setUDPfromJSON(p);
			break;
		case AIS::KEY_SETTING_TCP:
			setTCPfromJSON(p);
			break;
		case AIS::KEY_SETTING_TCP_LISTENER:
			setTCPListenerfromJSON(p);
			break;
		case AIS::KEY_SETTING_SERVER:
			setServerfromJSON(p.Get());
			break;
		case AIS::KEY_SETTING_HTTP:
			setHTTPfromJSON(p);
			break;
		case AIS::KEY_SETTING_RECEIVER:
			setReceiverFromArray(p);
			break;
		case AIS::KEY_SETTING_SCREEN:
			_screen.setScreen(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE_TIME:
			_screen.verboseUpdateTime = Util::Parse::Integer(p.Get().to_string(), 1, 300);
			break;
		default:
			throw std::runtime_error("Config file: field \"" + AIS::KeyMap[p.Key()][JSON_DICT_SETTING] + "\" in main section is not allowed.");
			break;
		}
	}
}
