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
#include "Logger.h"
#include "PostgreSQL.h"
#include "SQLite.h"
#include "CSV.h"

IO::OutputMessage *Config::newDatabaseOutput(const std::string &type)
{
	if (type == "postgres" || type == "postgresql" || type == "psql")
		return new IO::PostgreSQL();
	if (type == "sqlite")
		return new IO::SQLite();
	if (type == "csv")
		return new IO::CSV();

	throw std::runtime_error("Database type \"" + type + "\" is not one of postgres, sqlite, csv.");
}

void Config::addDatabaseOutputsFromJSON(const JSON::Member &m)
{
	if (!m.Get().isArray())
		throw std::runtime_error("Database settings need to be an \"array\" of \"objects\" in config file.");

	for (const auto &v : m.Get().getArray())
	{
		if (!isActiveObject(v))
			continue;

		// "type" must be read ahead of the settings pass
		std::string type = "postgres";
		for (const JSON::Member &p : v.getObject().getMembers())
			if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
			{
				type = p.Get().to_string();
				Util::Convert::toLower(type);
			}

		_engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(newDatabaseOutput(type)));
		setSettingsFromJSON(v, *_engine.msg.back(), AIS::KEY_SETTING_MODEL_TYPE);
	}
}

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

void Config::setSettingsFromJSON(const JSON::Value &m, Setting &s, int skip)
{

	for (const JSON::Member &p : m.getObject().getMembers())
	{
		if (p.Key() != AIS::KEY_SETTING_ACTIVE && p.Key() != skip)
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
void Config::setManagedViewerfromJSON(const JSON::Value &m)
{
	if (!managed_viewer || !m.isObject())
		return;

	const JSON::Value *v = m.getObject()[AIS::KEY_SETTING_VIEWER];
	if (v && v->isObject())
		setSettingsFromJSON(*v, *managed_viewer);
}

void Config::readManagedViewer(const std::string &file_config)
{
	if (!managed_viewer)
		return;

	try
	{
		JSON::Parser parser(JSON_DICT_SETTING);
		JSON::Document doc = parser.parse(Util::Helper::readFile(file_config));

		const JSON::Value *ctrl = doc.root[AIS::KEY_SETTING_CONTROL];
		if (ctrl)
			setManagedViewerfromJSON(*ctrl);
	}
	catch (std::exception const &e)
	{
		Error() << "Control: viewer configuration failed: " << e.what();
	}
}

void Config::setServerfromJSON(const JSON::Value &m)
{
	auto add = [this](const JSON::Value &v)
	{
		if (!isActiveObject(v))
			return;

		_engine.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
		setSettingsFromJSON(v, *_engine.servers.back());
		_engine.servers.back()->setActive(true);
	};

	if (m.isArray())
		for (const auto &v : m.getArray())
			add(v);
	else
		add(m);
}
#endif

void Config::setModelfromJSON(const JSON::Member &m)
{
	if (!isActiveObject(m.Get()))
		return;

	Receiver &r = *_engine.receivers.back();

	r.addModel(-1);
	setSettingsFromJSON(m.Get(), *r.Model(r.Count() - 1));
}

int Config::engineType(const std::string &s)
{
	std::string t = s;
	Util::Convert::toUpper(t);

	if (t.empty() || t == "AUTO")
		return -1;
	if (t == "V1_BASE")
		return AIS::MODEL_V1_BASE;
	if (t == "V1_HIGH")
		return AIS::MODEL_V1_HIGH;
	if (t == "V2_BASE")
		return AIS::MODEL_V2_BASE;

	throw std::runtime_error("engine type must be auto, v1_base, v1_high or v2_base");
}

// "engines": [ { "type": "v2_base", "settings": { "afc_wide": "off" } } ]
void Config::setEnginefromJSON(const JSON::Value &v)
{
	if (!isActiveObject(v))
		return;

	Receiver &r = *_engine.receivers.back();

	int type = -1;
	const JSON::Value *settings = nullptr;
	std::vector<std::string> zones;

	for (const JSON::Member &p : v.getObject().getMembers())
	{
		if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
			type = engineType(p.Get().to_string());
		else if (p.Key() == AIS::KEY_SETTING_SETTINGS)
			settings = &p.Get();
		else if (p.Key() == AIS::KEY_SETTING_ZONE)
		{
			if (!p.Get().isArray())
				throw std::runtime_error("engine: \"zone\" must be an array of strings");
			for (const auto &z : p.Get().getArray())
				zones.push_back(z.to_string());
		}
		else if (p.Key() != AIS::KEY_SETTING_ACTIVE)
			throw std::runtime_error("engine: \"" + std::string(AIS::KeyMap[p.Key()][JSON_DICT_SETTING]) + "\" must be inside the engine's \"settings\" block");
	}

	r.addModel(type);
	r.engine_zones.resize(r.Count()); // index must line up with the model
	r.engine_zones.back() = zones;

	if (settings)
		setSettingsFromJSON(*settings, *r.Model(r.Count() - 1));
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
		_engine.newReceiver();

		if (!serial.empty())
			_engine.receivers.back()->SetKey(AIS::KEY_SETTING_SERIAL, serial);
		if (!input.empty())
			_engine.receivers.back()->SetKey(AIS::KEY_SETTING_INPUT, input);
	}

	// pass 2
	for (const auto &m : members)
	{
		switch (m.Key())
		{
		case AIS::KEY_SETTING_ENGINES:
		case AIS::KEY_SETTING_MODEL:
			break; // pass 3
		case AIS::KEY_SETTING_ZONE:
			if (!m.Get().isArray())
				throw std::runtime_error("\"zone\" must be an array of strings");
			for (const auto &v : m.Get().getArray())
				_engine.receivers.back()->zones.push_back(v.to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE:
		case AIS::KEY_SETTING_CHANNEL:
		case AIS::KEY_SETTING_META:
		case AIS::KEY_SETTING_SENSITIVITY_HIGH:
		case AIS::KEY_SETTING_LAT:
		case AIS::KEY_SETTING_LON:
			_engine.receivers.back()->SetKey((AIS::Keys)m.Key(), m.Get().to_string());
			break;
		case AIS::KEY_SETTING_OWN_MMSI:
			_engine.own_mmsi = m.Get().getInt();
			break;
		default:
		{
			Setting *device = _engine.receivers.back()->getDeviceManager().settingForKey((AIS::Keys)m.Key());
			if (device && isActiveObject(m.Get()))
				setSettingsFromJSON(m.Get(), *device);
			break;
		}
		}
	}

	// engines are applied once the input format is known
	for (const auto &m : members)
	{
		if (m.Key() == AIS::KEY_SETTING_ENGINES)
		{
			if (!m.Get().isArray())
				throw std::runtime_error("\"engines\" must be an array, e.g. [ { \"type\": \"v2_base\", \"settings\": { ... } } ]");
			for (const auto &v : m.Get().getArray())
				setEnginefromJSON(v);
		}
		else if (m.Key() == AIS::KEY_SETTING_MODEL)
		{
			Warning() << "Receiver: \"model\" is deprecated and will be removed, use \"engines\": [ { \"type\": ... } ] instead";
			if (m.Get().isString())
				_engine.receivers.back()->addModel(engineType(m.Get().to_string()));
			else
				setModelfromJSON(m);
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
			_engine.xshare_defined = true;
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

	// A sharing_key is an explicit intent to share, credited to that key, so it
	// enables the feed on its own -- otherwise the key was dropped and the station
	// still shared anonymously via the default-on path.
	if ((xchange || !uuid.empty()) && !_engine.comm_feed)
		_engine.createCommunityFeed();

	if (!uuid.empty() && _engine.comm_feed)
		_engine.comm_feed->SetKey(AIS::KEY_SETTING_UUID, uuid);

	if (!zones.empty() && _engine.comm_feed)
		_engine.comm_feed->zones = zones;
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

	// rejected before setReceiverfromJSON runs, so nothing is applied from an invalid config
	for (const auto &m : doc.getMembers())
		if (m.Key() == AIS::KEY_SETTING_CHANNEL || m.Key() == AIS::KEY_SETTING_ENGINES)
			throw std::runtime_error(std::string("Config file: \"") + AIS::KeyMap[m.Key()][JSON_DICT_SETTING] + "\" is not allowed in the main section, set it inside a receiver object.");

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
		case AIS::KEY_SETTING_DB:
			addDatabaseOutputsFromJSON(m);
			break;
		case AIS::KEY_SETTING_RECEIVER: // handled in pass 1.5
			break;
		case AIS::KEY_SETTING_CONTROL: // managed mode (-E) fields, consumed by ControlCore
#ifdef HASWEBVIEWER
			if (!dry_run)
				setManagedViewerfromJSON(m.Get());
#endif
			break;
		case AIS::KEY_SETTING_ENGINE:
			break;
		case AIS::KEY_SETTING_SCREEN:
			_engine.screen.setScreen(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_TIMEOUT:
			if (m.Get().isBool()) {
				if (!m.Get().getBool())
					_engine.timeout = 0;
				else
					throw std::runtime_error("Config file: timeout must be false or a number between 1 and 3600.");
			}
			else {
				_engine.timeout = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
			}
			break;
		case AIS::KEY_SETTING_TIMEOUT_NOMSG:
			_engine.timeout_nomsg = Util::Parse::Switch(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_VERBOSE_TIME:
			_engine.screen.verboseUpdateTime = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
			break;
		default:
			if (m.Key() >= 0 && m.Key() < AIS::KEY_COUNT)
				throw std::runtime_error(std::string("Config file: field \"") + AIS::KeyMap[m.Key()][JSON_DICT_SETTING] + "\" in main section is not allowed.");
			else
				throw std::runtime_error("Config file: unknown field in main section is not allowed.");
		}
	}
}
