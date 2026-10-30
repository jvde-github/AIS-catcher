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

namespace {
// an engine's "type": auto (-1), v1_base, v1_high, v2_base or a model number
int engineType(const std::string &s) {
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
  // the decoding model by number, as -m takes it
  if (t.find_first_not_of("0123456789") == std::string::npos)
    return Util::Parse::Integer(t, 0, AIS::MODEL_MAX);

  throw std::runtime_error("engine type must be auto, v1_base, v1_high, v2_base or a model number");
}

// skip: one extra key the caller already consumed, e.g. the "type" selector
void applySettings(const JSON::Value &m, Setting &s, int skip = -1) {
  for (const JSON::Member &p : m.getObject().getMembers())
    if (p.Key() != AIS::KEY_SETTING_ACTIVE && p.Key() != skip)
      s.SetKey((AIS::Keys)p.Key(), JSON::text(p.Get()));
}

void readModel(Receiver &r, const JSON::Value &v) {
  if (!Config::isActiveObject(v))
    return;

  applySettings(v, r.addModel(-1));
}

// "engines": [ { "type": "v2_base", "settings": { "afc_wide": "off" } } ]
void readEngine(Receiver &r, const JSON::Value &v) {
  if (!Config::isActiveObject(v))
    return;

  int type = -1;
  const JSON::Value *settings = nullptr;
  std::vector<std::string> zones;

  for (const JSON::Member &p : v.getObject().getMembers()) {
    if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
      type = engineType(p.Get().to_string());
    else if (p.Key() == AIS::KEY_SETTING_SETTINGS) {
      if (!p.Get().isObject())
        throw std::runtime_error("engine: \"settings\" must be an object");
      settings = &p.Get();
    }
    else if (p.Key() == AIS::KEY_SETTING_ZONE) {
      if (!p.Get().isArray())
        throw std::runtime_error("engine: \"zone\" must be an array of strings");
      for (const auto &z : p.Get().getArray())
        zones.push_back(z.to_string());
    } else if (p.Key() != AIS::KEY_SETTING_ACTIVE)
      throw std::runtime_error("engine: \"" + AIS::settingName(p.Key()) + "\" must be inside the engine's \"settings\" block");
  }

  AIS::Model &model = r.addModel(type, zones);

  if (settings)
    applySettings(*settings, model);
}

// a key that is a receiver's plain value, as "lat" or "sample_rate"
bool isReceiverValue(int key) {
  switch (key) {
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
} // namespace

IO::OutputMessage *Config::newDatabaseOutput(const std::string &type) {
  if (type == "postgres" || type == "postgresql" || type == "psql")
    return new IO::PostgreSQL();
  if (type == "sqlite")
    return new IO::SQLite();
  if (type == "csv")
    return new IO::CSV();

  throw std::runtime_error("Database type \"" + type + "\" is not one of postgres, sqlite, csv.");
}

template <typename T>
void Config::readOutputs(const JSON::Member &m, const std::string &label, const char *default_timeout) {
  if (!m.Get().isArray())
    throw std::runtime_error(label + " settings need to be an \"array\" of \"objects\" in config file.");

  for (const auto &v : m.Get().getArray()) {
    if (!isActiveObject(v))
      continue;

    engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(new T()));
    IO::OutputMessage &o = *engine.msg.back();
    if (default_timeout)
      o.SetKey(AIS::KEY_SETTING_TIMEOUT, default_timeout);
    applySettings(v, o);
  }
}

void Config::readDatabases(const JSON::Member &m) {
  if (!m.Get().isArray())
    throw std::runtime_error("Database settings need to be an \"array\" of \"objects\" in config file.");

  for (const auto &v : m.Get().getArray()) {
    if (!isActiveObject(v))
      continue;

    // "type" must be read ahead of the settings pass
    std::string type = "postgres";
    for (const JSON::Member &p : v.getObject().getMembers())
      if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE) {
        type = p.Get().to_string();
        Util::Convert::toLower(type);
      }

    engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(newDatabaseOutput(type)));
    applySettings(v, *engine.msg.back(), AIS::KEY_SETTING_MODEL_TYPE);
  }
}

bool Config::isActiveObject(const JSON::Value &m) {

  if (!m.isObject())
    throw std::runtime_error("expected JSON \"object\"");

  for (const JSON::Member &p : m.getObject().getMembers()) {
    if (p.Key() == AIS::KEY_SETTING_ACTIVE) {
      return Util::Parse::Switch(p.Get().to_string());
    }
  }
  return true;
}

std::string Config::base_dir;

void Config::setConfigBaseDir(const std::string &file_config) {
  const size_t at = file_config.find_last_of("/\\");
  base_dir = at == std::string::npos ? "" : file_config.substr(0, at);
}

std::string Config::modelName(int model) {
  switch (model) {
  case AIS::MODEL_V1_BASE: return "v1_base";
  case AIS::MODEL_V1_HIGH: return "v1_high";
  case AIS::MODEL_V2_BASE: return "v2_base";
  default: return std::to_string(model);
  }
}

void Config::readReceivers(const JSON::Member &m, bool &filled) {
  if (!m.Get().isArray())
    throw std::runtime_error(RECEIVERS_NOT_ARRAY);

  int n = 0;
  for (const auto &v : m.Get().getArray()) {
    n++;
    if (!isActiveObject(v))
      continue;
    if (filled)
      engine.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
    filled = true;
    prefixed("receiver " + std::to_string(n), [&] { readReceiver(v.getObject().getMembers(), false, *engine.receivers.back()); });
  }
}

void Config::receiverInput(const std::vector<JSON::Member> &members, std::string &serial, std::string &input) {
  for (const auto &m : members)
    if (m.Key() == AIS::KEY_SETTING_SERIAL)
      serial = m.Get().to_string();
    else if (m.Key() == AIS::KEY_SETTING_INPUT)
      input = m.Get().to_string();
}

bool Config::isReceiverKey(int key) {
  switch (key) {
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

bool Config::readReceiver(const std::vector<JSON::Member> &members, bool unspecAllowed, Receiver &r) {
  for (const auto &m : members)
    if (m.Key() == AIS::KEY_SETTING_ACTIVE && !Util::Parse::Switch(m.Get().to_string()))
      return false;

  applyReceiver(r, members, unspecAllowed, &engine.own_mmsi);

  std::string serial, input;
  receiverInput(members, serial, input);
  return !serial.empty() || !input.empty();
}

void Config::applyReceiver(Receiver &r, const std::vector<JSON::Member> &members, bool unspecAllowed, int *own_mmsi) {
  // the input first, wherever it stands among the keys
  std::string serial, input;
  receiverInput(members, serial, input);
  if (!unspecAllowed && serial.empty() && input.empty())
    throw std::runtime_error("needs \"input\", its kind of device, or a \"serial\".");
  if (!serial.empty())
    r.SetKey(AIS::KEY_SETTING_SERIAL, serial);
  if (!input.empty())
    r.SetKey(AIS::KEY_SETTING_INPUT, input);

  // then its values, zones and device blocks
  for (const auto &m : members) {
    switch (m.Key()) {
    case AIS::KEY_SETTING_ENGINES:
    case AIS::KEY_SETTING_MODEL:
    case AIS::KEY_SETTING_NMEA_CHANNEL:
      break; // below, once the input is known
    case AIS::KEY_SETTING_SERIAL:
    case AIS::KEY_SETTING_INPUT:
    case AIS::KEY_SETTING_ACTIVE:
      break; // read above
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
    case AIS::KEY_SETTING_OWN_MMSI:
      if (own_mmsi)
        *own_mmsi = m.Get().getInt();
      break;
    default: {
      if (isReceiverValue(m.Key())) {
        r.SetKey((AIS::Keys)m.Key(), m.Get().to_string());
        break;
      }
      Setting *device = r.devices().settingForKey((AIS::Keys)m.Key());
      if (!device) {
        // the main section's other keys are the site's and the outputs', read
        // by the caller; only a receiver object can refuse one
        if (!unspecAllowed)
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
  for (const auto &m : members) {
    if (m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL) {
      r.SetKey(AIS::KEY_SETTING_NMEA_CHANNEL, m.Get().to_string());
    } else if (m.Key() == AIS::KEY_SETTING_ENGINES) {
      if (!m.Get().isArray())
        throw std::runtime_error("\"engines\" must be an array, e.g. [ { \"type\": \"v2_base\", \"settings\": { ... } } ]");
      for (const auto &v : m.Get().getArray())
        readEngine(r, v);
    } else if (m.Key() == AIS::KEY_SETTING_MODEL) {
      Warning() << MODEL_DEPRECATED;
      if (m.Get().isString())
        r.addModel(engineType(m.Get().to_string()));
      else
        readModel(r, m.Get());
    }
  }
}

void Config::read(const std::string &file_config) {
  setConfigBaseDir(file_config);
  set(Util::Helper::readFile(file_config));
}

void Config::readSharing(const std::vector<JSON::Member> &members) {

  bool xchange = false;
  std::string uuid;
  std::vector<std::string> zones;

  for (const JSON::Member &m : members) {
    if (m.Key() == AIS::KEY_SETTING_SHARING) {
      xchange = Util::Parse::Switch(m.Get().to_string());
      engine.xshare_defined = true;
    } else if (m.Key() == AIS::KEY_SETTING_SHARING_KEY)
      uuid = m.Get().to_string();
    else if (m.Key() == AIS::KEY_SETTING_SHARING_ZONE) {
      if (!m.Get().isArray())
        throw std::runtime_error("\"sharing_zone\" must be an array of strings");
      for (const auto &v : m.Get().getArray())
        zones.push_back(v.to_string());
    }
  }

  // a key of no use with sharing off stops nothing
  if (engine.xshare_defined && !xchange && !uuid.empty() && !Util::Helper::isUUID(uuid)) {
    Warning() << "Config file: \"sharing_key\" is not a UUID and sharing is off: ignored.";
    uuid.clear();
  }
  if ((xchange || !uuid.empty()) && !engine.comm_feed)
    engine.createCommunityFeed();

  if (!uuid.empty() && engine.comm_feed)
    engine.comm_feed->SetKey(AIS::KEY_SETTING_UUID, uuid);

  if (!zones.empty() && engine.comm_feed)
    engine.comm_feed->zones = zones;
}

void Config::preamble(const std::vector<JSON::Member> &members) {
  std::string config;
  int version = 0;

  for (const auto &m : members) {
    switch (m.Key()) {
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

void Config::set(const std::string &str, const Origins *from) {
  JSON::Parser parser(JSON_DICT_SETTING);
  JSON::Document doc = parser.parse(str);

  preamble(doc.getMembers());

  // the engine's first receiver is there from the start: the main section's keys
  // fill it, and so does the first input the "receiver" array names unless the
  // main section named one; every input after that is a receiver of its own
  bool filled = readReceiver(doc.getMembers(), true, *engine.receivers.front());
  readSharing(doc.getMembers());

  // every receiver of the array, before the outputs apply tags
  for (const auto &p : doc.getMembers()) {
    if (p.Key() == AIS::KEY_SETTING_RECEIVER)
      readReceivers(p, filled);
  }

  // the rest: the receivers' keys are applied above
  for (const auto &m : doc.getMembers()) {
    if (isReceiverKey(m.Key()))
      continue;
    switch (m.Key()) {
    case AIS::KEY_SETTING_CONFIG:
    case AIS::KEY_SETTING_VERSION:
    case AIS::KEY_SETTING_SHARING:
    case AIS::KEY_SETTING_SHARING_KEY:
    case AIS::KEY_SETTING_SHARING_ZONE:
      break;
    case AIS::KEY_SETTING_UDP:
      readOutputs<IO::UDPStreamer>(m, "UDP");
      break;
    case AIS::KEY_SETTING_TCP:
      readOutputs<IO::TCPClientStreamer>(m, "TCP");
      break;
    case AIS::KEY_SETTING_MQTT:
      readOutputs<IO::MQTTStreamer>(m, "MQTT");
      break;
    case AIS::KEY_SETTING_TCP_LISTENER:
      readOutputs<IO::TCPlistenerStreamer>(m, "TCP Listener", "0");
      break;
    case AIS::KEY_SETTING_SERVER: // the site's, read below
#ifdef HASWEBVIEWER
      break;
#else
      throw std::runtime_error("Config file: \"server\" needs a build with the web viewer (cmake -DWEBVIEWER=ON).");
#endif
    case AIS::KEY_SETTING_HTTP:
      readOutputs<IO::HTTPStreamer>(m, "HTTP");
      break;
    case AIS::KEY_SETTING_DB:
      readDatabases(m);
      break;
    // managed mode's: its viewer section is read with the site
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
    case AIS::KEY_SETTING_FILE_OUTPUT:
      readOutputs<IO::FileOutput>(m, "File output");
      break;
    case AIS::KEY_SETTING_NMEA2000_OUTPUT:
#ifdef HASNMEA2000
      readOutputs<IO::N2KStreamer>(m, "NMEA2000 output");
      break;
#else
      throw std::runtime_error("NMEA2000 support not compiled in.");
#endif
    case AIS::KEY_SETTING_TIMEOUT:
      if (m.Get().isBool()) {
        if (!m.Get().getBool())
          engine.timeout = 0;
        else
          throw std::runtime_error("Config file: timeout must be false or a number between 1 and 3600.");
      } else {
        engine.timeout = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
      }
      break;
    case AIS::KEY_SETTING_TIMEOUT_NOMSG:
      engine.timeout_nomsg = Util::Parse::Switch(m.Get().to_string());
      break;
    case AIS::KEY_SETTING_STATIONS:
      SiteReader::stations(m.Get(), engine.stations);
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

  // the site's sections, with the files they name resolved against the file's folder
#ifdef HASAPI
  if (managed)
    SiteReader::managedSite(doc.root, base_dir, control_port, viewer_port, bind, engine.site_config, engine.station_defaults, engine.stations);
  else
    SiteReader::site(doc.root, base_dir, engine.site_config, engine.station_defaults, engine.stations, from ? &from->servers : nullptr);
#endif
}

void Config::finish() {
  // a file without "stations" from before there were any: a receiver's number is a station of its own
  if (engine.stations.ordered().empty())
    for (const auto &r : engine.receivers)
      if (r->getStation() != 0 && !engine.stations.find(r->getStation())) {
        StationRegistry::Station s;
        s.id = r->getStation();
        s.name = "Station " + std::to_string(s.id);
        engine.stations.define(s);
      }
  Stations::validate(engine.stations, engine.receivers);
}
