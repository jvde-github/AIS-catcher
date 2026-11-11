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

#include "Config.h"

#include "CSV.h"
#include "File.h"
#include "Helper.h"
#include "JSON/Parser.h"
#include "Logger.h"
#include "Network.h"
#include "PostgreSQL.h"
#include "SiteConfig.h"
#include "SQLite.h"
#include "Stations.h"
#ifdef HASNMEA2000
#include "N2KStream.h"
#endif

const char *const Config::MODEL_DEPRECATED = "Receiver: \"model\" is deprecated and will be removed, use \"engines\": [ { \"type\": ... } ] instead";
const char *const Config::RECEIVERS_NOT_ARRAY = "Receiver settings need to be an \"array\" of \"objects\" in config file.";

namespace
{
	int engineType(const std::string &s)
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
		if (t.find_first_not_of("0123456789") == std::string::npos)
			return Util::Parse::Integer(t, 0, AIS::MODEL_MAX);

		throw std::runtime_error("engine type must be auto, v1_base, v1_high, v2_base or a model number");
	}

	// skip: a key the caller already consumed; told: the settings in the order given
	void applySettings(const JSON::Value &m, Setting &s, int skip = -1, std::string *told = nullptr)
	{
		for (const JSON::Member &p : m.getObject().getMembers())
			if (p.Key() != AIS::KEY_SETTING_ACTIVE && p.Key() != skip)
			{
				const std::string value = JSON::text(p.Get());
				if (told)
					*told += AIS::settingName(p.Key()) + "=" + value + ";";
				s.SetKey((AIS::Keys)p.Key(), value);
			}
	}

	IO::OutputMessage &add(Outputs &into, IO::OutputMessage *o, const std::string &kind)
	{
		into.list.push_back(std::shared_ptr<IO::OutputMessage>(o));
		into.told.push_back(kind + ":");
		return *o;
	}

	template <typename T>
	void readList(const JSON::Member &m, const std::string &label, Outputs &into, const char *default_timeout = nullptr)
	{
		if (!m.Get().isArray())
			throw std::runtime_error(label + " settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : m.Get().getArray())
		{
			if (!Config::isActiveObject(v))
				continue;

			IO::OutputMessage &o = add(into, new T(), label);
			if (default_timeout)
				o.SetKey(AIS::KEY_SETTING_TIMEOUT, default_timeout);
			applySettings(v, o, -1, &into.told.back());
		}
	}

	// the "db" array names its backend with a "type" field
	void readDatabases(const JSON::Member &m, Outputs &into)
	{
		if (!m.Get().isArray())
			throw std::runtime_error("Database settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : m.Get().getArray())
		{
			if (!Config::isActiveObject(v))
				continue;

			// "type" must be read ahead of the settings pass
			std::string type = "postgres";
			for (const JSON::Member &p : v.getObject().getMembers())
				if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
				{
					type = p.Get().to_string();
					Util::Convert::toLower(type);
				}

			IO::OutputMessage &o = add(into, Config::newDatabaseOutput(type), type);
			applySettings(v, o, AIS::KEY_SETTING_MODEL_TYPE, &into.told.back());
		}
	}

	// "engines": [ { "type": "v2_base", "settings": { "afc_wide": "off" } } ]
	void readEngine(Receiver &r, const JSON::Value &v)
	{
		if (!Config::isActiveObject(v))
			return;

		int type = -1;
		const JSON::Value *settings = nullptr;
		std::vector<std::string> zones;

		for (const JSON::Member &p : v.getObject().getMembers())
		{
			if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
				type = engineType(p.Get().to_string());
			else if (p.Key() == AIS::KEY_SETTING_SETTINGS)
			{
				if (!p.Get().isObject())
					throw std::runtime_error("engine: \"settings\" must be an object");
				settings = &p.Get();
			}
			else if (p.Key() == AIS::KEY_SETTING_ZONE)
			{
				if (!p.Get().isArray())
					throw std::runtime_error("engine: \"zone\" must be an array of strings");
				for (const auto &z : p.Get().getArray())
					zones.push_back(z.to_string());
			}
			else if (p.Key() != AIS::KEY_SETTING_ACTIVE)
				throw std::runtime_error("engine: \"" + AIS::settingName(p.Key()) + "\" must be inside the engine's \"settings\" block");
		}

		AIS::Model &model = r.addModel(type, zones);

		if (settings)
			applySettings(*settings, model);
	}

	// a member as the document has it, to tell whether two documents say the same
	std::string said(const JSON::Member &m)
	{
		JSON::JSON one;
		one.Add(m.Key(), m.Get());
		std::string text;
		JSON::Serializer(JSON_DICT_SETTING).stringify(one, text);
		return text;
	}

	// a main-section key the engine itself is made from: a change of it takes a new engine
	bool isEngineValue(int key)
	{
		return key == AIS::KEY_SETTING_OWN_MMSI || key == AIS::KEY_SETTING_SCREEN || key == AIS::KEY_SETTING_TIMEOUT ||
			   key == AIS::KEY_SETTING_TIMEOUT_NOMSG || key == AIS::KEY_SETTING_VERBOSE_TIME;
	}

	bool isReceiverValue(int key)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_VERBOSE:
		case AIS::KEY_SETTING_META:
		case AIS::KEY_SETTING_LAT:
		case AIS::KEY_SETTING_LON:
		case AIS::KEY_SETTING_DESCRIPTION:
		case AIS::KEY_SETTING_DESC:
		case AIS::KEY_SETTING_STATION:
		case AIS::KEY_SETTING_SAMPLE_RATE:
		case AIS::KEY_SETTING_BANDWIDTH:
		case AIS::KEY_SETTING_FREQOFFSET:
		case AIS::KEY_SETTING_TIMING:
			return true;
		default:
			return false;
		}
	}
}

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

bool Config::readOutput(const JSON::Member &m, Outputs &into)
{
	switch (m.Key())
	{
	case AIS::KEY_SETTING_UDP:
		readList<IO::UDPStreamer>(m, "UDP", into);
		break;
	case AIS::KEY_SETTING_TCP:
		readList<IO::TCPClientStreamer>(m, "TCP", into);
		break;
	case AIS::KEY_SETTING_MQTT:
		readList<IO::MQTTStreamer>(m, "MQTT", into);
		break;
	case AIS::KEY_SETTING_TCP_LISTENER:
		readList<IO::TCPlistenerStreamer>(m, "TCP Listener", into, "0");
		break;
	case AIS::KEY_SETTING_HTTP:
		readList<IO::HTTPStreamer>(m, "HTTP", into);
		for (const auto &o : into.list)
			into.http |= dynamic_cast<IO::HTTPStreamer *>(o.get()) != nullptr;
		break;
	case AIS::KEY_SETTING_DB:
		readDatabases(m, into);
		break;
	case AIS::KEY_SETTING_FILE_OUTPUT:
		readList<IO::FileOutput>(m, "File output", into);
		break;
	case AIS::KEY_SETTING_NMEA2000_OUTPUT:
#ifdef HASNMEA2000
		readList<IO::N2KStreamer>(m, "NMEA2000 output", into);
		break;
#else
		throw std::runtime_error("NMEA2000 support not compiled in.");
#endif
	default:
		return false;
	}
	return true;
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

std::string Config::folderOf(const std::string &file)
{
	const size_t at = file.find_last_of("/\\");
	return at == std::string::npos ? "" : file.substr(0, at);
}

std::string Config::modelName(int model)
{
	switch (model)
	{
	case AIS::MODEL_V1_BASE:
		return "v1_base";
	case AIS::MODEL_V1_HIGH:
		return "v1_high";
	case AIS::MODEL_V2_BASE:
		return "v2_base";
	default:
		return std::to_string(model);
	}
}

void Config::readReceivers(const JSON::Member &m, bool &filled)
{
	if (!m.Get().isArray())
		throw std::runtime_error(RECEIVERS_NOT_ARRAY);

	int n = 0;
	for (const auto &v : m.Get().getArray())
	{
		n++;
		if (!isActiveObject(v))
			continue;
		if (filled)
			engine.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
		filled = true;
		Receiver &r = *engine.receivers.back();
		prefixed("receiver " + std::to_string(n), [&] { applyReceiver(r, v.getObject().getMembers()); });
		for (const JSON::Member &p : v.getObject().getMembers())
			if (p.Key() != AIS::KEY_SETTING_ZONE)
				r.told += said(p);
	}
}

void Config::receiverInput(const std::vector<JSON::Member> &members, std::string &serial, std::string &input)
{
	for (const auto &m : members)
		if (m.Key() == AIS::KEY_SETTING_SERIAL)
			serial = m.Get().to_string();
		else if (m.Key() == AIS::KEY_SETTING_INPUT)
			input = m.Get().to_string();
}

bool Config::isReceiverKey(int key)
{
	switch (key)
	{
	case AIS::KEY_SETTING_SERIAL:
	case AIS::KEY_SETTING_INPUT:
	case AIS::KEY_SETTING_OWN_MMSI:
	case AIS::KEY_SETTING_MODEL:
	case AIS::KEY_SETTING_RECEIVER:
		return true;
	default:
		return isReceiverValue(key) || DeviceManager::isDeviceKey((AIS::Keys)key);
	}
}

void Config::applyReceiver(Receiver &r, const std::vector<JSON::Member> &members)
{
	// the input first, wherever it stands among the keys
	std::string serial, input;
	receiverInput(members, serial, input);
	if (serial.empty() && input.empty())
		throw std::runtime_error("needs \"input\", its kind of device, or a \"serial\".");
	if (!serial.empty())
		r.SetKey(AIS::KEY_SETTING_SERIAL, serial);
	if (!input.empty())
		r.SetKey(AIS::KEY_SETTING_INPUT, input);

	for (const auto &m : members)
	{
		switch (m.Key())
		{
		case AIS::KEY_SETTING_ENGINES:
		case AIS::KEY_SETTING_NMEA_CHANNEL:
		case AIS::KEY_SETTING_SERIAL:
		case AIS::KEY_SETTING_INPUT:
		case AIS::KEY_SETTING_ACTIVE:
			break;
		case AIS::KEY_SETTING_ZONE:
			if (!m.Get().isArray())
				throw std::runtime_error("\"zone\" must be an array of strings");
			for (const auto &v : m.Get().getArray())
				r.addZone(v.to_string());
			break;
		case AIS::KEY_SETTING_CHANNEL:
		case AIS::KEY_SETTING_SENSITIVITY_HIGH:
			r.SetKey((AIS::Keys)m.Key(), m.Get().to_string());
			break;
		default:
		{
			if (isReceiverValue(m.Key()))
			{
				r.SetKey((AIS::Keys)m.Key(), m.Get().to_string());
				break;
			}
			Setting *device = r.devices().settingForKey((AIS::Keys)m.Key());
			if (!device)
			{
				Warning() << "Config file: field \"" << AIS::settingName(m.Key()) << "\" is ignored in a receiver object.";
				break;
			}
			if (isActiveObject(m.Get()))
				applySettings(m.Get(), *device);
			break;
		}
		}
	}

	// the NMEA designation follows the channel choice, in whichever order the
	// two keys were written; engines are applied once the input format is known
	for (const auto &m : members)
	{
		if (m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL)
		{
			r.SetKey(AIS::KEY_SETTING_NMEA_CHANNEL, m.Get().to_string());
		}
		else if (m.Key() == AIS::KEY_SETTING_ENGINES)
		{
			if (!m.Get().isArray())
				throw std::runtime_error("\"engines\" must be an array, e.g. [ { \"type\": \"v2_base\", \"settings\": { ... } } ]");
			for (const auto &v : m.Get().getArray())
				readEngine(r, v);
		}
	}
}

void Config::resolveFiles(JSON::JSON &section, const std::string &folder, JSON::Pool &pool)
{
	const auto resolve = [&](JSON::Value &v, bool backup) {
		const std::string path = backup ? SiteReader::backupFile(v.getString(), folder) : Util::Helper::resolvePath(v.getString(), folder);
		if (path != v.getString())
			v.setString(pool.addString(path));
	};
	for (JSON::Member &m : section.getMembers())
	{
		JSON::Value &v = m.Get();
		if (v.isString() && SiteReader::isPath(m.Key()))
			resolve(v, m.Key() == AIS::KEY_SETTING_FILE);
		else if (m.Key() == AIS::KEY_SETTING_TRACKING && v.isObject())
			resolveFiles(v.getObject(), folder, pool);
		else if (m.Key() == AIS::KEY_SETTING_TLS && v.isObject())
			for (JSON::Member &f : v.getObject().getMembers())
				if (f.Get().isString())
					resolve(f.Get(), false);
	}
}

namespace
{
	// "model": "v1_base" or "model": { settings } as an entry of "engines"
	JSON::Value engineOf(const JSON::Value &model, JSON::Pool &pool)
	{
		JSON::JSON *engine = pool.addObject();
		if (model.isString())
			engine->Add(AIS::KEY_SETTING_MODEL_TYPE, model);
		else
		{
			JSON::JSON *settings = pool.addObject();
			for (const JSON::Member &p : model.getObject().getMembers())
				if (p.Key() != AIS::KEY_SETTING_ACTIVE)
					settings->Add(p.Key(), p.Get());
			engine->Add(AIS::KEY_SETTING_SETTINGS, settings);
		}
		JSON::Value engines(pool.addArray());
		engines.getArray().push_back(JSON::Value(engine));
		return engines;
	}
}

bool Config::normalise(JSON::Document &doc, const std::string &folder, std::vector<std::string> &notes, bool alone)
{
	std::vector<JSON::Member> &root = doc.root.getMembers();
	for (const JSON::Member &m : root)
		if (m.Key() == AIS::KEY_SETTING_RECEIVER && !m.Get().isArray())
			throw std::runtime_error(RECEIVERS_NOT_ARRAY);

	// the main section's receiver, as an entry
	std::vector<JSON::Member> main;
	for (auto it = root.begin(); it != root.end();)
		if (isReceiverKey(it->Key()) && it->Key() != AIS::KEY_SETTING_RECEIVER && it->Key() != AIS::KEY_SETTING_OWN_MMSI)
		{
			main.push_back(*it);
			it = root.erase(it);
		}
		else
			++it;
	bool unplaced = false;
	if (!main.empty())
	{
		std::string serial, input;
		receiverInput(main, serial, input);
		const bool named = !serial.empty() || !input.empty();
		std::vector<JSON::Value> &entries = doc.root.array(AIS::KEY_SETTING_RECEIVER, doc.pool);
		// without an input of its own it is the first receiver's, ahead of what that says itself
		JSON::Value *first = nullptr;
		if (!named && alone)
			for (JSON::Value &e : entries)
				if (e.isObject() && isActiveObject(e))
				{
					first = &e;
					break;
				}
		if (first)
			first->getObject().getMembers().insert(first->getObject().getMembers().begin(), main.begin(), main.end());
		else
		{
			JSON::JSON *entry = doc.pool.addObject();
			if (!named && alone)
				entry->Add(AIS::KEY_SETTING_INPUT, "none", doc.pool);
			for (const JSON::Member &m : main)
				entry->Add(m.Key(), m.Get());
			entries.insert(entries.begin(), JSON::Value(entry));
			unplaced = !named && !alone;
		}
	}

	std::vector<JSON::Value> own_mmsi;
	for (JSON::Member &list : root)
	{
		if (list.Key() != AIS::KEY_SETTING_RECEIVER)
			continue;
		for (JSON::Value &e : list.Get().getArray())
		{
			if (!e.isObject() || !isActiveObject(e))
				continue;
			std::vector<JSON::Member> &members = e.getObject().getMembers();
			for (auto it = members.begin(); it != members.end();)
			{
				if (it->Key() == AIS::KEY_SETTING_OWN_MMSI)
				{
					own_mmsi.push_back(it->Get());
					it = members.erase(it);
					continue;
				}
				if (it->Key() == AIS::KEY_SETTING_MODEL)
				{
					notes.push_back(MODEL_DEPRECATED);
					if (!it->Get().isString() && !isActiveObject(it->Get()))
					{
						it = members.erase(it);
						continue;
					}
					*it = JSON::Member(AIS::KEY_SETTING_ENGINES, engineOf(it->Get(), doc.pool));
				}
				++it;
			}
		}
	}
	for (const JSON::Value &v : own_mmsi)
		doc.root.Add(AIS::KEY_SETTING_OWN_MMSI, v);

	for (JSON::Member &m : root)
	{
		JSON::Value &v = m.Get();
		if (m.Key() == AIS::KEY_SETTING_RECEIVER)
			continue;
		if (v.isObject() && (m.Key() == AIS::KEY_SETTING_TRACKING || m.Key() == AIS::KEY_SETTING_SERVER))
			resolveFiles(v.getObject(), folder, doc.pool);
		else if (v.isObject() && m.Key() == AIS::KEY_SETTING_CONTROL)
		{
			// managed mode's old viewer section
			JSON::Value *viewer = v.getObject().getValue(AIS::KEY_SETTING_VIEWER);
			if (viewer && viewer->isObject())
				resolveFiles(viewer->getObject(), folder, doc.pool);
		}
		else if (v.isArray())
			for (JSON::Value &item : v.getArray())
				if (item.isObject())
					resolveFiles(item.getObject(), folder, doc.pool);
	}
	return unplaced;
}

void Config::readSharing(const std::vector<JSON::Member> &members, bool managed, Outputs &into)
{

	bool xchange = false;
	std::string uuid;
	std::vector<std::string> zones;

	for (const JSON::Member &m : members)
	{
		if (m.Key() == AIS::KEY_SETTING_SHARING)
		{
			xchange = Util::Parse::Switch(m.Get().to_string());
			into.sharing_defined = true;
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

	if (into.sharing_defined && !xchange && !uuid.empty() && !Util::Helper::isUUID(uuid))
	{
		Warning() << "Config file: \"sharing_key\" is not a UUID and sharing is off: ignored.";
		uuid.clear();
	}
	if ((!xchange && (uuid.empty() || (managed && into.sharing_defined))) || into.feed)
		return;

	into.feed = new IO::HubStreamer();
	add(into, into.feed, "feed");
	if (!uuid.empty())
		into.feed->SetKey(AIS::KEY_SETTING_UUID, uuid);
	into.feed->zones = zones;
	// sharing is opt-in: a feed nobody asked for by name carries what the radios hear
	into.radios_only = managed ? uuid.empty() : !into.sharing_defined;
	into.told.back() += uuid + (into.radios_only ? ";radios;" : ";");
	for (const auto &z : zones)
		into.told.back() += z + ",";
}

void Config::preamble(const std::vector<JSON::Member> &members)
{
	std::string config;
	int version = 0;

	for (const auto &m : members)
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

	// rejected before applyReceiver runs, so nothing is applied from an invalid config
	for (const auto &m : members)
		if (m.Key() == AIS::KEY_SETTING_CHANNEL || m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL || m.Key() == AIS::KEY_SETTING_ENGINES)
			throw std::runtime_error("Config file: \"" + AIS::settingName(m.Key()) + "\" is not allowed in the main section, set it inside a receiver object.");
}

void Config::set(const std::string &str, const Origins *from)
{
	JSON::Parser parser(JSON_DICT_SETTING);
	JSON::Document doc = parser.parse(str);
	set(doc, from);
}

void Config::set(JSON::Document &doc, const Origins *from)
{
	preamble(doc.getMembers());
	std::vector<std::string> notes;
	normalise(doc, folder, notes);
	for (const auto &n : notes)
		Warning() << n;

	bool filled = false;
	readSharing(doc.getMembers(), managed, engine.outputs);

	// the receiver array, before the outputs apply tags
	for (const auto &p : doc.getMembers())
	{
		if (p.Key() == AIS::KEY_SETTING_RECEIVER)
			readReceivers(p, filled);
	}

	for (const auto &m : doc.getMembers())
	{
		if (readOutput(m, engine.outputs))
			continue;
		if (isEngineValue(m.Key()))
			engine.told += said(m);
		switch (m.Key())
		{
		case AIS::KEY_SETTING_OWN_MMSI:
			engine.own_mmsi = m.Get().getInt();
			break;
		case AIS::KEY_SETTING_RECEIVER:
		case AIS::KEY_SETTING_CONFIG:
		case AIS::KEY_SETTING_VERSION:
		case AIS::KEY_SETTING_SHARING:
		case AIS::KEY_SETTING_SHARING_KEY:
		case AIS::KEY_SETTING_SHARING_ZONE:
			break;
		case AIS::KEY_SETTING_SERVER:
#ifdef HASWEBVIEWER
			break;
#else
			throw std::runtime_error("Config file: \"server\" needs a build with the web viewer (cmake -DWEBVIEWER=ON).");
#endif
		// managed mode's own sections
		case AIS::KEY_SETTING_CONTROL:
		case AIS::KEY_SETTING_ENGINE:
			break;
		case AIS::KEY_SETTING_SCREEN:
			// the message format, or an object of screen settings as -o takes them
			if (m.Get().isObject())
				applySettings(m.Get(), engine.screen);
			else
				engine.screen.setScreen(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_TIMEOUT:
			if (m.Get().isBool())
			{
				if (!m.Get().getBool())
					engine.timeout = 0;
				else
					throw std::runtime_error("Config file: timeout must be false or a number between 1 and 3600.");
			}
			else
			{
				engine.timeout = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
			}
			break;
		case AIS::KEY_SETTING_TIMEOUT_NOMSG:
			engine.timeout_nomsg = Util::Parse::Switch(m.Get().to_string());
			break;
		case AIS::KEY_SETTING_STATIONS:
			SiteReader::stations(m.Get(), stations);
			break;
#ifdef HASAPI
		// the site's, read below
		case AIS::KEY_SETTING_TRACKING:
		case AIS::KEY_SETTING_SERVICES:
			break;
#else
		case AIS::KEY_SETTING_TRACKING:
		case AIS::KEY_SETTING_SERVICES:
			throw std::runtime_error("Config file: \"tracking\" and \"services\" need a build with the API (cmake -DAPI=ON).");
#endif
#ifdef HASDELIVERY
		case AIS::KEY_SETTING_WEBHOOKS:
		case AIS::KEY_SETTING_MQTT_STATE:
			break;
#else
		case AIS::KEY_SETTING_WEBHOOKS:
		case AIS::KEY_SETTING_MQTT_STATE:
			throw std::runtime_error("Config file: \"webhooks\" and \"mqtt_state\" need a build with them (cmake -DDELIVERY=ON).");
#endif
		case AIS::KEY_SETTING_VERBOSE_TIME:
			engine.screen.verboseUpdateTime = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
			break;
		default:
			// the parser refuses a name it does not know: the key is one of the map's
			throw std::runtime_error("Config file: field \"" + AIS::settingName(m.Key()) + "\" in main section is not allowed.");
		}
	}

	// the site's sections
#ifdef HASAPI
	if (managed)
		SiteReader::managedSite(doc.root, control_port, viewer_port, bind, site, station_defaults, stations);
	else
		SiteReader::site(doc.root, site, station_defaults, stations, from ? &from->servers : nullptr);
#endif
}

void Config::finish()
{
	// a file without "stations": a receiver's number is a station of its own
	if (stations.ordered().empty())
		for (const auto &r : engine.receivers)
			if (r->getStation() != 0 && !stations.find(r->getStation()))
			{
				StationRegistry::Station s;
				s.id = r->getStation();
				s.name = "Station " + std::to_string(s.id);
				stations.define(s);
			}
	Stations::validate(stations, engine.receivers);
}
