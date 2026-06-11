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

#include "AIS-catcher.h"
#include "Config.h"
#include "Helper.h"

bool Config::isActiveObject(const JSON::Value &m)
{

	if (!m.isObject())
		throw std::runtime_error("expected JSON \"object\"");

	for (const JSON::Member &p : m.getObject().getMembers())
	{
		if (p.Key() == AIS::KEY_SETTING_ACTIVE)
		{
			return Util::Parse::Switch(p.Get().to_string());
		}
	}
	return true;
}

void Config::setSettingsFromJSON(const JSON::Value &m, Setting &s)
{

	for (const JSON::Member &p : m.getObject().getMembers())
	{
		if (p.Key() != AIS::KEY_SETTING_ACTIVE)
		{
			if (p.Get().isArray())
			{
				std::string joined;
				for (const auto &v : p.Get().getArray())
					joined += (joined.empty() ? "" : ",") + v.to_string();
				s.SetKey((AIS::Keys)p.Key(), joined);
			}
			else if (p.Get().isArrayString())
			{
				std::string joined;
				for (const auto &v : p.Get().getStringArray())
					joined += (joined.empty() ? "" : ",") + v;
				s.SetKey((AIS::Keys)p.Key(), joined);
			}
			else
			{
				s.SetKey((AIS::Keys)p.Key(), p.Get().to_string());
			}
		}
	}
}

#ifdef HASWEBVIEWER
void Config::setServerfromJSON(const JSON::Value &m)
{

	if (m.isArray())
	{
		for (const auto &v : m.getArray())
		{
			if (!isActiveObject(v))
				continue;
			_state.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
			setSettingsFromJSON(v, *_state.servers.back());
			_state.servers.back()->active() = true;
		}
	}
	else
	{
		if (!isActiveObject(m))
			return;

		_state.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
		setSettingsFromJSON(m, *_state.servers.back());
		_state.servers.back()->active() = true;
	}
}
#endif

Setting *Config::getDeviceSetting(AIS::Keys key)
{
	DeviceManager &dm = _state.receivers.back()->getDeviceManager();

	switch (key)
	{
	case AIS::KEY_SETTING_RTLSDR: return &dm.RTLSDR();
	case AIS::KEY_SETTING_RTLTCP: return &dm.RTLTCP();
	case AIS::KEY_SETTING_AIRSPY: return &dm.AIRSPY();
	case AIS::KEY_SETTING_AIRSPYHF: return &dm.AIRSPYHF();
	case AIS::KEY_SETTING_SDRPLAY: return &dm.SDRPLAY();
	case AIS::KEY_SETTING_WAVFILE: return &dm.WAV();
	case AIS::KEY_SETTING_SERIALPORT: return &dm.SerialPort();
	case AIS::KEY_SETTING_HACKRF: return &dm.HACKRF();
	case AIS::KEY_SETTING_HYDRASDR: return &dm.HYDRASDR();
	case AIS::KEY_SETTING_UDPSERVER: return &dm.UDP();
	case AIS::KEY_SETTING_SOAPYSDR: return &dm.SOAPYSDR();
	case AIS::KEY_SETTING_NMEA2000: return &dm.N2KSCAN();
	case AIS::KEY_SETTING_FILE: return &dm.RAW();
	case AIS::KEY_SETTING_ZMQ: return &dm.ZMQ();
	case AIS::KEY_SETTING_SPYSERVER: return &dm.SpyServer();
	default: return nullptr;
	}
}

void Config::setModelfromJSON(const JSON::Member &m)
{
	if (!isActiveObject(m.Get()))
		return;

	_state.receivers.back()->addModel(_state.receivers.back()->getDeviceManager().isTXTformatSet() ? 5 : 2);
	setSettingsFromJSON(m.Get(), *_state.receivers.back()->Model(_state.receivers.back()->Count() - 1));
}

void Config::setReceiverFromArray(const JSON::Member &m)
{
	if (!m.Get().isArray())
		throw std::runtime_error("Receiver settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto &v : m.Get().getArray())
	{
		if (!isActiveObject(v))
			continue;

		setReceiverfromJSON(v.getObject().getMembers(), false);
	}
}

void Config::setReceiverfromJSON(const std::vector<JSON::Member> &members, bool unspecAllowed)
{
	std::string serial, input;
	// pass 1
	for (const auto &m : members)
	{
		switch (m.Key())
		{
		case AIS::KEY_SETTING_SERIAL:
			serial = m.Get().to_string();
			break;
		case AIS::KEY_SETTING_INPUT:
			input = m.Get().to_string();
			break;
		case AIS::KEY_SETTING_ACTIVE:
			if (!Util::Parse::Switch(m.Get().to_string()))
				return;
			break;
		}
	}

	if (!unspecAllowed)
	{
		if (serial.empty() && input.empty())
			throw std::runtime_error("receiver needs to have a serial or input specified in Config.");
	}

	if ((!serial.empty() || !input.empty()))
	{
		_state.newReceiver();

		if (!serial.empty())
			_state.receivers.back()->SetKey(AIS::KEY_SETTING_SERIAL, serial);
		if (!input.empty())
			_state.receivers.back()->SetKey(AIS::KEY_SETTING_INPUT, input);
	}

	// pass 2
	for (const auto &m : members)
	{
		switch (m.Key())
		{
		case AIS::KEY_SETTING_ZONE:
			if (!m.Get().isArray())
				throw std::runtime_error("\"zone\" must be an array of strings");
			for (const auto &v : m.Get().getArray())
				_state.receivers.back()->zones.push_back(v.to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE:
		case AIS::KEY_SETTING_CHANNEL:
		case AIS::KEY_SETTING_META:
		case AIS::KEY_SETTING_SENSITIVITY_HIGH:
			_state.receivers.back()->SetKey((AIS::Keys)m.Key(), m.Get().to_string());
			break;
		case AIS::KEY_SETTING_MODEL:
			setModelfromJSON(m);
			break;
		case AIS::KEY_SETTING_OWN_MMSI:
			_state.own_mmsi = m.Get().getInt();
			break;
		default:
		{
			Setting *device = getDeviceSetting((AIS::Keys)m.Key());
			if (device && isActiveObject(m.Get()))
				setSettingsFromJSON(m.Get(), *device);
			break;
		}
		}
	}
}

void Config::read(const std::string &file_config)
{
	if (!file_config.empty())
	{
		set(Util::Helper::readFile(file_config));
	}
}

void Config::setSharing(const std::vector<JSON::Member> &members)
{

	bool xchange = false;
	std::string uuid;
	std::vector<std::string> zones;

	for (const JSON::Member &m : members)
	{
		if (m.Key() == AIS::KEY_SETTING_SHARING)
		{
			xchange = Util::Parse::Switch(m.Get().to_string());
			_state.xshare_defined = true;
		}
		else if (m.Key() == AIS::KEY_SETTING_SHARING_KEY)
			uuid = m.Get().to_string();
		else if (m.Key() == AIS::KEY_SETTING_SHARING_ZONE)
		{
			if (!m.Get().isArray())
				throw std::runtime_error("\"sharing_zone\" must be an array of strings");
			for (const auto &v : m.Get().getArray())
				zones.push_back(v.to_string());
		}
	}

	if (xchange && !_state.comm_feed)
		_state.createCommunityFeed();

	if (!uuid.empty() && _state.comm_feed)
		_state.comm_feed->SetKey(AIS::KEY_SETTING_UUID, uuid);

	if (!zones.empty() && _state.comm_feed)
		_state.comm_feed->zones = zones;
}

void Config::set(const std::string &str)
{
	std::string config;
	int version = 0;

	JSON::Parser parser(JSON_DICT_SETTING);
	JSON::Document doc = parser.parse(str);

	// pass 1
	for (const auto &m : doc.getMembers())
	{
		switch (m.Key())
		{
		case AIS::KEY_SETTING_CONFIG:
			config = m.Get().to_string();
			break;
		case AIS::KEY_SETTING_VERSION:
			version = Util::Parse::Integer(m.Get().to_string());
			break;
		}
	}

	if (version > 1 || config != "aiscatcher")
		throw std::runtime_error("version and/or format of config file not supported (required version <=1)");

	setReceiverfromJSON(doc.getMembers(), true);
	setSharing(doc.getMembers());

	// pass 2 - instantiate all receivers from the array before outputs apply tags
	for (const auto &p : doc.getMembers())
	{
		if (p.Key() == AIS::KEY_SETTING_RECEIVER)
			setReceiverFromArray(p);
	}

	// pass 3
	for (const auto &m : doc.getMembers())
	{
		switch (m.Key())
		{
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
		case AIS::KEY_SETTING_OWN_MMSI:
		case AIS::KEY_SETTING_SDRPLAY:
		case AIS::KEY_SETTING_NMEA2000:
		case AIS::KEY_SETTING_WAVFILE:
		case AIS::KEY_SETTING_HACKRF:
		case AIS::KEY_SETTING_HYDRASDR:
		case AIS::KEY_SETTING_UDPSERVER:
		case AIS::KEY_SETTING_SOAPYSDR:
		case AIS::KEY_SETTING_FILE:
		case AIS::KEY_SETTING_ZMQ:
		case AIS::KEY_SETTING_SPYSERVER:
		case AIS::KEY_SETTING_SHARING:
		case AIS::KEY_SETTING_SHARING_KEY:
		case AIS::KEY_SETTING_SHARING_ZONE:
			break;
		case AIS::KEY_SETTING_UDP:
			addOutputsFromJSON<IO::UDPStreamer>(m, "UDP");
			break;
		case AIS::KEY_SETTING_TCP:
			addOutputsFromJSON<IO::TCPClientStreamer>(m, "TCP");
			break;
		case AIS::KEY_SETTING_MQTT:
			addOutputsFromJSON<IO::MQTTStreamer>(m, "MQTT");
			break;
		case AIS::KEY_SETTING_TCP_LISTENER:
			addOutputsFromJSON<IO::TCPlistenerStreamer>(m, "TCP Listener", "0");
			break;
		case AIS::KEY_SETTING_SERVER:
#ifdef HASWEBVIEWER
			setServerfromJSON(m.Get());
#else
			Warning() << "Config 'server' key ignored: WebViewer not compiled in.";
#endif
			break;
		case AIS::KEY_SETTING_HTTP:
			addOutputsFromJSON<IO::HTTPStreamer>(m, "HTTP");
			break;
		case AIS::KEY_SETTING_RECEIVER: // handled in pass 1.5
			break;
		case AIS::KEY_SETTING_SCREEN:
			_state.screen.setScreen(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_TIMEOUT:
			if (m.Get().isBool()) {
				if (!m.Get().getBool())
					_state.timeout = 0;
				else
					throw std::runtime_error("Config file: timeout must be false or a number between 1 and 3600.");
			}
			else {
				_state.timeout = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
			}
			break;
		case AIS::KEY_SETTING_TIMEOUT_NOMSG:
			_state.timeout_nomsg = Util::Parse::Switch(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE_TIME:
			_state.screen.verboseUpdateTime = Util::Parse::Integer(m.Get().to_string(), 1, 300);
			break;
		default:
			if (m.Key() >= 0 && m.Key() < AIS::KEY_COUNT)
				throw std::runtime_error(std::string("Config file: field \"") + AIS::KeyMap[m.Key()][JSON_DICT_SETTING] + "\" in main section is not allowed.");
			else
				throw std::runtime_error("Config file: unknown field in main section is not allowed.");
		}
	}
}
