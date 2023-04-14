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

	setSettingsFromJSON(pd, _server);
	_receiver.setTags("DTM");
	_server.active() = true;
}

void Config::setHTTPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("HTTP settings need to be an \"array\" of \"objects\". in config file");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		auto& h = _http.add(AIS::KeyMap, JSON_DICT_FULL);
		setSettingsFromJSON(v, *h);
		_receiver.setTags("DT");
	}
}

void Config::setUDPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("UDP settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		IO::UDP& udp = _udp.add();
		setSettingsFromJSON(v, udp);
	}
}

void Config::setTCPfromJSON(const JSON::Property& pd) {

	if (!pd.Get().isArray())
		throw std::runtime_error("TCP settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto& v : pd.Get().getArray()) {
		if (!isActiveObject(v)) continue;

		IO::TCP& tcp = _tcp.add();
		setSettingsFromJSON(v, tcp);
	}
}

void Config::setModelfromJSON(const JSON::Property& p) {

	if (!isActiveObject(p.Get())) return;

	_receiver.addModel(2);
	setSettingsFromJSON(p.Get(), *_receiver.Model(_receiver.Count() - 1));
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

	// temporary check, back and forth
	if (false) {

		JSON::StringBuilder builder(&AIS::KeyMap, JSON_DICT_SETTING);
		std::cerr << "Config input : " << builder.stringify(*json) << std::endl;
	}

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
		case AIS::KEY_SETTING_SERIAL:
			serial = p.Get().to_string();
			break;
		case AIS::KEY_SETTING_INPUT:
			input = p.Get().to_string();
			break;
		}
	}

	if (version < 1 || version > 1 || config != "aiscatcher")
		throw std::runtime_error("version and/or format of config file not supported (required version <=1)");

	_receiver.Serial() = serial;
	if (!input.empty()) {
		if (!Util::Parse::DeviceType(input, _receiver.InputType())) {
			throw std::runtime_error("\"" + input + "\" is unknown input type in config file");
		}
	}

	// pass 2
	for (const auto& p : props) {
		switch (p.Key()) {
		case AIS::KEY_SETTING_MODEL:
			setModelfromJSON(p);
			break;
		case AIS::KEY_SETTING_META:
			_receiver.setTags(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_RTLSDR:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.RTLSDR());
			break;
		case AIS::KEY_SETTING_RTLTCP:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.RTLTCP());
			break;
		case AIS::KEY_SETTING_AIRSPY:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.AIRSPY());
			break;
		case AIS::KEY_SETTING_AIRSPYHF:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.AIRSPYHF());
			break;
		case AIS::KEY_SETTING_SDRPLAY:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.SDRPLAY());
			break;
		case AIS::KEY_SETTING_WAVFILE:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.WAV());
			break;
		case AIS::KEY_SETTING_HACKRF:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.HACKRF());
			break;
		case AIS::KEY_SETTING_UDPSERVER:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.UDP());
			break;
		case AIS::KEY_SETTING_SOAPYSDR:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.SOAPYSDR());
			break;
		case AIS::KEY_SETTING_FILE:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.RAW());
			break;
		case AIS::KEY_SETTING_ZMQ:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.ZMQ());
			break;
		case AIS::KEY_SETTING_SPYSERVER:
			if (!isActiveObject(p.Get())) continue;
			setSettingsFromJSON(p.Get(), _receiver.SpyServer());
			break;
		case AIS::KEY_SETTING_UDP:
			setUDPfromJSON(p);
			break;
		case AIS::KEY_SETTING_TCP:
			setTCPfromJSON(p);
			break;
		case AIS::KEY_SETTING_SERVER:
			if (!isActiveObject(p.Get())) continue;
			setServerfromJSON(p.Get());
			break;
		case AIS::KEY_SETTING_HTTP:
			setHTTPfromJSON(p);
			break;
		case AIS::KEY_SETTING_SCREEN:
			_screen.setScreen(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE:
			_receiver.verbose = Util::Parse::Switch(p.Get().to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE_TIME:
			_screen.verboseUpdateTime = Util::Parse::Integer(p.Get().to_string(), 1, 300);
			break;
			// fields that are already processed for completeness
		case AIS::KEY_SETTING_CONFIG:
		case AIS::KEY_SETTING_VERSION:
		case AIS::KEY_SETTING_SERIAL:
		case AIS::KEY_SETTING_INPUT:
			break;
		default:
			throw std::runtime_error("Config file: field \"" + AIS::KeyMap[p.Key()][JSON_DICT_SETTING] + "\" in main section is not allowed.");
			break;
		}
	}
}
