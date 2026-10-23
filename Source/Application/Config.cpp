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
#include <sstream>
#include "AIS-catcher.h"
#include "CSV.h"
#include "Helper.h"
#include "Logger.h"
#include "PostgreSQL.h"
#include "SiteKeys.h"
#include "SQLite.h"
#ifdef HASAPI
#include "Event.h"
#include "Server.h"
#endif

IO::OutputMessage *Config::newDatabaseOutput(const std::string &type) {
  if (type == "postgres" || type == "postgresql" || type == "psql")
    return new IO::PostgreSQL();
  if (type == "sqlite")
    return new IO::SQLite();
  if (type == "csv")
    return new IO::CSV();

  throw std::runtime_error("Database type \"" + type +
                           "\" is not one of postgres, sqlite, csv.");
}

void Config::addDatabaseOutputsFromJSON(const JSON::Member &m) {
  if (!m.Get().isArray())
    throw std::runtime_error("Database settings need to be an \"array\" of "
                             "\"objects\" in config file.");

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

    _engine.msg.push_back(
        std::unique_ptr<IO::OutputMessage>(newDatabaseOutput(type)));
    setSettingsFromJSON(v, *_engine.msg.back(), AIS::KEY_SETTING_MODEL_TYPE);
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

// a value as a setting takes it: a list as text separated by commas
static std::string text(const JSON::Value &v) {
  std::string joined;
  if (v.isArray())
    for (const auto &item : v.getArray())
      joined += (joined.empty() ? "" : ",") + item.to_string();
  else if (v.isArrayString())
    for (const auto &item : v.getStringArray())
      joined += (joined.empty() ? "" : ",") + item;
  else
    joined = v.to_string();
  return joined;
}

static std::string keyName(int key) {
  return key >= 0 && key < AIS::KEY_COUNT ? AIS::KeyMap[key][JSON_DICT_SETTING].p : "?";
}

// the readers' errors, said of the file
template <typename F>
static void ofFile(F f) {
  try {
    f();
  } catch (const ConfigError &e) {
    throw std::runtime_error(std::string("Config file: ") + e.what());
  }
}

void Config::setSettingsFromJSON(const JSON::Value &m, Setting &s, int skip) {
  for (const JSON::Member &p : m.getObject().getMembers())
    if (p.Key() != AIS::KEY_SETTING_ACTIVE && p.Key() != skip)
      s.SetKey((AIS::Keys)p.Key(), text(p.Get()));
}

std::string Config::base_dir;

void Config::setConfigBaseDir(const std::string &file_config) {
  const size_t at = file_config.find_last_of("/\\");
  base_dir = at == std::string::npos ? "" : file_config.substr(0, at);
}

// "stations": [ { "id": 0, "name": "Mast", "lat": 52.1, "lon": 4.27, "alt": 20 } ]
// the "stations" block, into `into`; 0 is the default station, where every
// receiver stands that names none
void Config::stationsFromJSON(const JSON::Value &v, StationRegistry &into) {
  ofFile([&] {
    if (!v.isArray())
      throw ConfigError("\"stations\" must be an array of station objects.");
    for (const auto &item : v.getArray()) {
      if (!item.isObject())
        throw ConfigError("each entry of \"stations\" must be an object.");
      StationRegistry::Station s;
      // a configured station stays where it is put unless GPS is chosen for it
      s.gps = false;
      bool has_id = false;
      for (const auto &m : item.getObject().getMembers()) {
        const std::string value = m.Get().to_string();
        switch (m.Key()) {
        case AIS::KEY_SETTING_ID:
          s.id = Util::Parse::Integer(value, 0, 2147483647);
          has_id = true;
          break;
        case AIS::KEY_SETTING_NAME: s.name = value; break;
        case AIS::KEY_SETTING_LAT: s.lat = Util::Parse::Float(value, -90, 90); break;
        case AIS::KEY_SETTING_LON: s.lon = Util::Parse::Float(value, -180, 180); break;
        case AIS::KEY_SETTING_ALT: s.alt = Util::Parse::Float(value, -1000, 20000); break;
        case AIS::KEY_SETTING_MMSI: s.mmsi = (uint32_t)Util::Parse::Integer(value, 1, 999999999); break;
        case AIS::KEY_SETTING_GPS: s.gps = Util::Parse::Switch(value); break;
        case AIS::KEY_SETTING_SHARE_LOC: s.share_loc = Util::Parse::Switch(value); break;
        default:
          throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in a station.");
        }
      }
      if (!has_id)
        throw ConfigError("a station needs an \"id\".");
      if (into.find(s.id))
        throw ConfigError("station " + std::to_string(s.id) + " is defined twice.");
      if (isValidCoord(s.lat, s.lon) != (s.lat != LAT_UNDEFINED || s.lon != LON_UNDEFINED))
        throw ConfigError("station " + std::to_string(s.id) + " needs both \"lat\" and \"lon\", or neither.");
      if (s.name.empty())
        s.name = "Station " + std::to_string(s.id);
      into.define(s);
    }
  });
}

void Config::setStationsFromJSON(const JSON::Value &v) {
  stationsFromJSON(v, _engine.stations);
}

#ifdef HASAPI
// one value or a list of them; a text value is split at its commas
static void addEach(const JSON::Value &v, std::vector<std::string> &out) {
  if (v.isNull())
    return;
  std::vector<std::string> parts;
  Util::Parse::Split(text(v), ',', parts);
  for (const auto &p : parts)
    if (!p.empty())
      out.push_back(p);
}
#endif

#ifdef HASAPI
// "tracking": { "file": "/var/lib/aiscatcher/tracking.bin", "backup": 10, "history": 1800 }
void Config::trackingFromJSON(const JSON::Value &v, SiteConfig &site) {
  if (!v.isObject())
    throw ConfigError("\"tracking\" must be an object.");
  site.tracking_set = true;
  for (const auto &m : v.getObject().getMembers())
    if (!SiteKeys::tracking(m.Key(), text(m.Get()), site, base_dir))
      throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in \"tracking\".");
}

// "keys": [ { "name": "home-assistant", "sha256": "…", "scopes": ["data", "system"] } ]
static void keysFromJSON(const JSON::Value &v, std::vector<ApiKey> &out) {
  if (!v.isArray())
    throw ConfigError("\"keys\" must be an array of key objects.");
  for (const auto &item : v.getArray()) {
    if (!item.isObject())
      throw ConfigError("each entry of \"keys\" must be an object.");
    ApiKey k;
    bool scoped = false;
    for (const auto &m : item.getObject().getMembers()) {
      switch (m.Key()) {
      case AIS::KEY_SETTING_NAME: k.name = m.Get().to_string(); break;
      case AIS::KEY_SETTING_SHA256: k.sha256 = m.Get().to_string(); break;
      case AIS::KEY_SETTING_SCOPES: {
        std::vector<std::string> names;
        addEach(m.Get(), names);
        k.scopes = 0;
        scoped = true;
        for (const auto &n : names) {
          if (n == "data") k.scopes |= ApiKey::DATA;
          else if (n == "system") k.scopes |= ApiKey::SYSTEM;
          else throw ConfigError("key scope \"" + n + "\" is not data or system.");
        }
        break;
      }
      default:
        throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in a key.");
      }
    }
    Util::Convert::toLower(k.sha256);
    if (k.sha256.size() != 64 || k.sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw ConfigError("key \"" + k.name + "\" needs \"sha256\", the SHA-256 of the key in hex.");
    if (scoped && !k.scopes)
      throw ConfigError("key \"" + k.name + "\" has no scopes.");
    out.push_back(k);
  }
}

// A server, from whichever section writes one: -N, a "server" section, managed
// mode's old viewer section, or an entry of "services". The terms of the
// message filter, the zones and the groups make it a tracker of its own; the
// tracking keys go to that tracker when it has one, else to the site's.
struct Config::ServerEntry {
  ServerSpec spec;
  std::vector<std::pair<int, std::string>> tracking_terms, station_terms;
  std::vector<int> station_ids; // the stations the server shows, and describes when the file does not list them
  AIS::Filter probe; // tells a filter term from the rest, and whether the filter is switched on
  std::vector<std::string> dropped;
  bool network_default = false; // a -N or server section: every interface, unless it says otherwise
  bool bind_given = false;
};

// one key of a server; `json` is its value where the file has one, for the keys that take an object
bool Config::serverKey(ServerEntry &e, int key, const std::string &value, const JSON::Value *json) {
  ServerSpec &spec = e.spec;
  TrackerSpec &own = spec.tracking;
  switch (key) {
  case AIS::KEY_SETTING_PORT: spec.port = Util::Parse::Integer(value, 1, 65535); return true;
  case AIS::KEY_SETTING_IP_BIND:
  case AIS::KEY_SETTING_BIND: spec.bind = value; e.bind_given = true; return true;
  case AIS::KEY_SETTING_UI: spec.ui = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_API: spec.api = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_MCP: spec.mcp = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_SYSTEM: spec.system = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_GEOJSON: spec.geojson = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_PROME:
  case AIS::KEY_SETTING_METRICS: spec.metrics = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_PLACE_GEOMETRY: spec.place_geometry = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_PUBLIC: spec.open = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_MAIN: spec.main = Util::Parse::Switch(value); return true;
  case AIS::KEY_SETTING_DESCRIPTION: return true; // the panel's name for it
  case AIS::KEY_SETTING_STATION_ID: {
    std::vector<std::string> parts;
    Util::Parse::Split(value, ',', parts);
    for (const auto &p : parts)
      if (!p.empty())
        e.station_ids.push_back((int)Util::Parse::Integer(p, 0, 2147483647));
    return true;
  }
  case AIS::KEY_SETTING_KEYS:
    if (!json)
      throw ConfigError("\"keys\" is set in the configuration file.");
    keysFromJSON(*json, spec.keys);
    return true;
  case AIS::KEY_SETTING_VIEW:
    // which ships the server shows of what its tracker keeps
    if (!json)
      throw ConfigError("\"view\" is set in the configuration file.");
    viewFilterFromJSON(*json, spec.view);
    return true;
  case AIS::KEY_SETTING_FILTER:
    // the message filter is a switch with its terms beside it; the ships shown are the view's
    if (json && json->isObject())
      throw ConfigError("the ships a server shows are its \"view\"; \"filter\" is the switch of the message filter, with its terms beside it.");
    break;
  case AIS::KEY_SETTING_TRACKING: {
    // a tracker of its own: its filter, zones and groups, and what it keeps
    if (!json || !json->isObject())
      throw ConfigError("a server's \"tracking\" must be an object.");
    for (const auto &m : json->getObject().getMembers()) {
      const int k = m.Key();
      const std::string v = text(m.Get());
      if (k == AIS::KEY_SETTING_ZONE || k == AIS::KEY_SETTING_GROUPS_IN || SiteKeys::isTracking(k)) {
        serverKey(e, k, v, &m.Get());
        continue;
      }
      if (!e.probe.SetOptionKey((AIS::Keys)k, v))
        throw ConfigError("field \"" + keyName(k) + "\" is not allowed in a server's \"tracking\".");
      own.filter.emplace_back(k, v);
    }
    // a term in "tracking" is meant: the switch is on
    if (!e.probe.isOn() && !own.filter.empty()) {
      e.probe.SetOptionKey(AIS::KEY_SETTING_FILTER, "on");
      own.filter.emplace_back((int)AIS::KEY_SETTING_FILTER, "on");
    }
    return true;
  }
  case AIS::KEY_SETTING_ZONE:
    Util::Parse::Split(value, ',', own.zones);
    return true;
  case AIS::KEY_SETTING_GROUPS_IN:
    own.groups_in = (uint64_t)Util::Parse::Integer(value, 0, 0x7FFFFFFFFFFFFFFFLL);
    return true;
  // what the web viewer of before took and this server has no use for
  case AIS::KEY_SETTING_PORT_MIN:
  case AIS::KEY_SETTING_PORT_MAX:
  case AIS::KEY_SETTING_SPLIT:
  case AIS::KEY_SETTING_CDN:
  case AIS::KEY_SETTING_ZLIB:
  case AIS::KEY_SETTING_REUSE_PORT:
  case AIS::KEY_SETTING_STATS_ON_CLOSE:
  case AIS::KEY_SETTING_MSGS:
    e.dropped.push_back("\"" + keyName(key) + "\" has no effect and can go");
    return true;
  default:
    break;
  }
  // a viewer's option, a tracking key (the position is the station's too), a station key, a filter term
  bool taken = SiteKeys::viewer(key, value, spec.viewer);
  if (SiteKeys::isTracking(key)) {
    e.tracking_terms.emplace_back(key, value);
    taken = true;
  }
  StationDefaults probe;
  if (SiteKeys::station(key, value, probe)) {
    e.station_terms.emplace_back(key, value);
    taken = true;
  }
  if (!taken && e.probe.SetOptionKey((AIS::Keys)key, value)) {
    own.filter.emplace_back(key, value);
    taken = true;
  }
  return taken;
}

// the entry is complete: it keeps ships of its own when its filter is on or it
// picks receivers, else it reads the site's tracker and configures that
void Config::serverDone(ServerEntry &e, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry, const std::string &where) {
  ServerSpec &spec = e.spec;
  TrackerSpec &own = spec.tracking;
  // only a filter makes a tracker of its own: one that keeps messages out, or
  // picks the receivers that feed it; a view, a station and the tracking do not
  bool terms = false;
  for (const auto &t : own.filter)
    terms |= t.first != AIS::KEY_SETTING_FILTER;
  const bool routed = !own.zones.empty() || own.groups_in != GROUPS_ALL;
  if (e.probe.isOn() && !terms && !routed)
    e.dropped.push_back("a filter without terms lets every message through: the server reads the site's tracker");
  if (!e.probe.isOn() && terms)
    e.dropped.push_back("filter terms without \"filter\" on are not used");
  own.own = (e.probe.isOn() && terms) || routed;
  if (own.own && spec.main)
    throw ConfigError(where + " shows every ship the station hears: give the filter, zones or groups to another server.");
  StationDefaults at;
  for (const auto &t : e.station_terms)
    SiteKeys::station(t.first, t.second, at);
  if (!e.station_ids.empty()) {
    // the server shows these stations; what the section says of a station
    // describes the first of them, where "stations" does not list it
    spec.stations = e.station_ids;
    for (int id : e.station_ids)
      if (std::find(spec.view.stations.begin(), spec.view.stations.end(), id) == spec.view.stations.end())
        spec.view.stations.push_back(id);
    if (registry)
      for (std::size_t i = 0; i < e.station_ids.size(); i++) {
        const int id = e.station_ids[i];
        if (registry->find(id))
          continue;
        StationRegistry::Station s;
        s.id = id;
        s.name = i == 0 && !at.name.empty() ? at.name : "Station " + std::to_string(id);
        if (i == 0) {
          if (at.placed()) {
            s.lat = at.lat;
            s.lon = at.lon;
          }
          s.alt = at.alt;
          s.mmsi = at.mmsi;
          s.share_loc = at.share_loc;
        }
        // GPS moves it only when the section says so, as a listed station has it
        bool gps = false;
        for (const auto &t : e.station_terms)
          if (t.first == AIS::KEY_SETTING_USE_GPS)
            gps = Util::Parse::Switch(t.second);
        s.gps = gps;
        registry->define(s);
      }
  }
  if (own.own) {
    own.tracking.msg_save = spec.viewer.msg;
    // backups and places are the site's: one file, one catalogue
    for (const auto &t : e.tracking_terms)
      if (!SiteKeys::tracking(t.first, t.second, SiteKeys::TrackingTarget(own), base_dir))
        e.dropped.push_back("\"" + keyName(t.first) + "\" is the site's: a server with ships of its own keeps no backup and reads no places");
    // what the section says of the station is this tracker's, unless it names
    // the station: it measures from there, shares it or not, follows GPS or its own vessel
    if (e.station_ids.empty())
      for (const auto &t : e.station_terms)
        switch (t.first) {
        case AIS::KEY_SETTING_LAT:
        case AIS::KEY_SETTING_LON:
          own.tracking.fixed = at.placed();
          if (at.placed()) {
            own.tracking.lat = at.lat;
            own.tracking.lon = at.lon;
          }
          break;
        case AIS::KEY_SETTING_SHARE_LOC: own.tracking.latlon_share = at.share_loc; break;
        case AIS::KEY_SETTING_USE_GPS: own.tracking.use_gps = at.gps; break;
        case AIS::KEY_SETTING_OWN_MMSI: own.tracking.own_mmsi = at.mmsi; break;
        default: break; // its name and height are the station list's
        }
  } else {
    if (defaults && e.station_ids.empty())
      for (const auto &t : e.station_terms)
        SiteKeys::station(t.first, t.second, *defaults);
    if (site.tracking_set) {
      if (!e.tracking_terms.empty())
        Warning() << where << ": the configuration's \"tracking\" says how ships are kept; the server's tracking settings are not used";
    } else {
      const std::string file = site.backup_file;
      for (const auto &t : e.tracking_terms)
        SiteKeys::tracking(t.first, t.second, SiteKeys::TrackingTarget(site), base_dir);
      if (!file.empty() && site.backup_file != file)
        Warning() << where << ": the backup file is now " << site.backup_file << ", not " << file << ": one tracker, one file";
    }
    own.filter.clear();
  }
  for (const auto &d : e.dropped)
    Warning() << where << ": " << d;
  if (!spec.serves())
    throw ConfigError(where + " serves nothing: the map is switched off and nothing else is on.");
  // the map of before listens on every interface; the API, MCP and System only with keys or public
  if (e.network_default && !e.bind_given && (spec.api || spec.mcp || spec.system) && spec.keys.empty() && !spec.open) {
    spec.bind = "127.0.0.1";
    Info() << where << ": the API, MCP and System keep this server on this machine; give it keys, public on, or bind 0.0.0.0 to open it";
  }
  try {
    checkServerAccess(spec, where);
  } catch (const std::exception &ex) {
    throw ConfigError(ex.what());
  }
  for (const auto &other : site.servers)
    if (other.port == spec.port)
      throw ConfigError("two servers on port " + std::to_string(spec.port) + ".");
  site.servers.push_back(spec);
}

// Managed mode's viewer section, from before the main server was an entry of
// "services": a server with the map on the port beside the control panel, the
// tracking, and what the section says of the default station. The panel moves
// it into "services", "tracking" and "stations" when it saves.
void Config::managedViewer(const JSON::Value &viewer, int port, const std::string &bind, SiteConfig &site,
                           StationDefaults &defaults, StationRegistry *registry) {
  ServerEntry e;
  e.spec.port = port;
  e.spec.bind = bind;
  e.spec.main = true;
  e.spec.viewer.managed = true;
  if (viewer.isObject())
    for (const auto &m : viewer.getObject().getMembers())
      serverKey(e, m.Key(), text(m.Get()), &m.Get()); // what else the section holds is the panel's
  serverDone(e, site, &defaults, registry, "the main server");
}

// "server": [ { "port": 8100, "station": "Home", "lat": 52.1, "lon": 4.3, "filter": true, "allow_type": "1,2,3" } ]
// the web viewer as its section wrote it: a server on the site's tracker, or on one of its own behind its filter
void Config::serversFromJSON(const JSON::Value &v, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry) {
  const auto one = [&](const JSON::Value &entry) {
    if (!entry.isObject())
      throw ConfigError("each entry of \"server\" must be an object.");
    ServerEntry e;
    e.spec.bind = "0.0.0.0"; // the viewer of before listened on every interface
    e.network_default = true;
    bool active = true;
    for (const auto &m : entry.getObject().getMembers()) {
      if (m.Key() == AIS::KEY_SETTING_ACTIVE) {
        active = Util::Parse::Switch(m.Get().to_string());
        continue;
      }
      if (!serverKey(e, m.Key(), text(m.Get()), &m.Get()))
        throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in \"server\".");
    }
    if (!active)
      return;
    if (!e.spec.port)
      throw ConfigError("a \"server\" needs a \"port\".");
    serverDone(e, site, defaults, registry, "the web viewer on port " + std::to_string(e.spec.port));
  };
  if (v.isArray())
    for (const auto &entry : v.getArray())
      one(entry);
  else
    one(v);
}

// Managed mode's site as its configuration says now: the servers, the tracking
// and the webhooks and MQTT state as the file has them, and, where no entry of
// "services" is marked main, the viewer section of a file from before as the
// server beside the control panel, read into the same site so what it says of
// the tracking and the station counts like any server's
void Config::managedSite(const JSON::JSON &root, int control_port, int viewer_port, const std::string &bind,
                         SiteConfig &site, StationDefaults &defaults, StationRegistry *registry) {
  ofFile([&] {
    SiteConfig made;
    siteFromJSON(root, made, &defaults, registry, viewer_port, bind);
    for (const auto &spec : made.servers)
      if (!spec.main && (spec.port == control_port || spec.port == viewer_port))
        throw ConfigError("the server on port " + std::to_string(spec.port) + " takes the " +
                          (spec.port == control_port ? "control panel's" : "viewer's") + " port.");
    const auto main = std::find_if(made.servers.begin(), made.servers.end(), [](const ServerSpec &s) { return s.main; });
    if (main == made.servers.end()) {
      const JSON::Value *control = root[AIS::KEY_SETTING_CONTROL];
      const JSON::Value *viewer = control && control->isObject() ? control->getObject()[AIS::KEY_SETTING_VIEWER] : nullptr;
      managedViewer(viewer ? *viewer : JSON::Value(), viewer_port, bind, made, defaults, registry);
      // first, as the panel lists it
      std::rotate(made.servers.begin(), made.servers.end() - 1, made.servers.end());
    } else
      defaults.name = main->viewer.station;
    site = made;
  });
}

// the viewer's filter terms, as its settings save them: a list as an array,
// an unset bound as null
void Config::viewFilterFromJSON(const JSON::Value &v, ViewFilter &filter) {
  if (!v.isObject())
    throw ConfigError("a \"view\" must be an object.");
  for (const auto &m : v.getObject().getMembers()) {
    const std::string key = keyName(m.Key());
    if (!ViewFilter::isKey(key))
      throw ConfigError("\"" + key + "\" is not a term of a view.");
    const JSON::Value &value = m.Get();
    try {
      if (value.isArray())
        for (const auto &item : value.getArray())
          filter.set(key, item.to_string());
      else if (value.isArrayString())
        for (const auto &item : value.getStringArray())
          filter.set(key, item);
      else if (!value.isNull())
        filter.set(key, value.to_string());
    } catch (const std::exception &e) {
      throw ConfigError(std::string("view: ") + e.what());
    }
  }
}

#ifdef HASDELIVERY
// "webhooks": [ { "url": "https://…", "events": ["safety", "place_enter"], "secret": "whsec_…" } ]
void Config::webhooksFromJSON(const JSON::Value &v, SiteConfig &site) {
  if (!v.isArray())
    throw ConfigError("\"webhooks\" must be an array of webhook objects.");
  for (const auto &item : v.getArray()) {
    if (!item.isObject())
      throw ConfigError("each entry of \"webhooks\" must be an object.");
    WebhookSpec hook;
    bool active = true;
    for (const auto &m : item.getObject().getMembers()) {
      switch (m.Key()) {
      case AIS::KEY_SETTING_ACTIVE: active = Util::Parse::Switch(m.Get().to_string()); break;
      case AIS::KEY_SETTING_DESCRIPTION: break; // the panel's name for it
      case AIS::KEY_SETTING_URL: hook.url = m.Get().to_string(); break;
      case AIS::KEY_SETTING_SECRET: hook.secret = m.Get().to_string(); break;
      case AIS::KEY_SETTING_BATCH: hook.batch = Util::Parse::Switch(m.Get().to_string()); break;
      case AIS::KEY_SETTING_VIEW: viewFilterFromJSON(m.Get(), hook.view); break;
      case AIS::KEY_SETTING_FILTER:
        throw ConfigError("the ships a webhook reports on are its \"view\".");
      case AIS::KEY_SETTING_EVENTS:
        addEach(m.Get(), hook.events);
        for (const auto &e : hook.events)
          if (!Tracking::isJournalName(e)) {
            std::string kinds;
            for (const auto &n : Tracking::kindNames())
              if (n.journal)
                kinds += (kinds.empty() ? "" : ", ") + std::string(n.journal);
            throw ConfigError("webhook event \"" + e + "\" is not one of " + kinds + ".");
          }
        break;
      default:
        throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in a webhook.");
      }
    }
    if (!active)
      continue;
    if (hook.url.compare(0, 7, "http://") != 0 && hook.url.compare(0, 8, "https://") != 0)
      throw ConfigError("a webhook needs a \"url\" starting with http:// or https://.");
    site.webhooks.push_back(hook);
  }
}

// -W on the command line: written as the file would have it and read by the
// same code, so the two take the same keys and say the same when one is wrong
void Config::deliveryFromArgs(const std::string &url, const std::vector<std::pair<std::string, std::string>> &settings) {
  const auto starts = [&url](const char *scheme) { return url.compare(0, strlen(scheme), scheme) == 0; };
  const bool hook = starts("http://") || starts("https://");
  if (!hook && !starts("mqtt://") && !starts("mqtts://") && !starts("ws://") && !starts("wss://"))
    throw ConfigError("a url with http:// or https:// for a webhook, or mqtt://, mqtts://, ws:// or wss:// for the MQTT state.");
  std::string json;
  JSON::Writer w(json);
  w.beginObject().key(hook ? "webhooks" : "mqtt_state");
  if (hook)
    w.beginArray();
  w.beginObject().kv("url", url);
  for (const auto &setting : settings) {
    std::string name = setting.first;
    Util::Convert::toUpper(name);
    AIS::Keys key;
    if (!AIS::lookupSettingKey(name, key))
      throw ConfigError("unknown setting \"" + setting.first + "\".");
    w.kv(keyName(key), setting.second);
  }
  w.endObject();
  if (hook)
    w.endArray();
  w.endObject();
  w.finish();

  JSON::Parser parser(JSON_DICT_SETTING);
  auto doc = parser.parse(json);
  if (hook)
    webhooksFromJSON(*doc.root[AIS::KEY_SETTING_WEBHOOKS], _engine.site_config);
  else
    mqttStateFromJSON(*doc.root[AIS::KEY_SETTING_MQTT_STATE], _engine.site_config);
}

// "mqtt_state": { "url": "mqtt://broker:1883", "topic": "ais-catcher", "discovery": true, "watch": [244123456] }
void Config::mqttStateFromJSON(const JSON::Value &v, SiteConfig &site) {
  if (!v.isObject())
    throw ConfigError("\"mqtt_state\" must be an object.");
  MqttStateSpec spec;
  bool active = true;
  for (const auto &m : v.getObject().getMembers()) {
    const std::string value = m.Get().to_string();
    switch (m.Key()) {
    case AIS::KEY_SETTING_ACTIVE: active = Util::Parse::Switch(value); break;
    case AIS::KEY_SETTING_URL: spec.url = value; break;
    case AIS::KEY_SETTING_USERNAME: spec.username = value; break;
    case AIS::KEY_SETTING_PASSWORD: spec.password = value; break;
    case AIS::KEY_SETTING_CLIENT_ID: spec.client_id = value; break;
    case AIS::KEY_SETTING_TOPIC: spec.topic = value; break;
    case AIS::KEY_SETTING_DISCOVERY: spec.discovery = Util::Parse::Switch(value); break;
    case AIS::KEY_SETTING_WATCH: {
      std::vector<std::string> list;
      addEach(m.Get(), list);
      for (const auto &s : list)
        spec.watch.push_back((uint32_t)Util::Parse::Integer(s, 1, 999999999));
      break;
    }
    default:
      throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in \"mqtt_state\".");
    }
  }
  // without a broker, or switched off, there is none
  if (spec.url.empty() || !active)
    return;
  if (site.mqtt_state.any())
    throw ConfigError("the state goes to one MQTT broker, and one is given already.");
  while (!spec.topic.empty() && spec.topic.back() == '/')
    spec.topic.pop_back();
  if (spec.topic.empty() || spec.topic.find_first_of("#+") != std::string::npos)
    throw ConfigError("the \"topic\" of \"mqtt_state\" is a plain topic prefix, without # or +.");
  site.mqtt_state = spec;
}
#endif

// -N [port] [key value]... on the command line: a port starts a server, the
// keys that follow, on this -N or the next without a port, are its settings;
// the servers are made once every argument is in, so a file read after them
// has said how the site keeps ships
void Config::viewerFromArgs(const std::string &port, const std::vector<std::pair<std::string, std::string>> &settings) {
  // a port starts a server, unless the last -N has none yet: then that one takes it
  if (!port.empty()) {
    if (pending_servers.empty() || !pending_servers.back().port.empty())
      pending_servers.push_back(PendingServer{port, {}});
    else
      pending_servers.back().port = port;
  } else if (pending_servers.empty())
    pending_servers.push_back(PendingServer{std::string(), {}});
  for (const auto &setting : settings) {
    std::string name = setting.first;
    Util::Convert::toUpper(name);
    AIS::Keys key;
    if (!AIS::lookupSettingKey(name, key))
      throw ConfigError("-N " + pending_servers.back().port + ": unknown setting \"" + setting.first + "\".");
    pending_servers.back().args.emplace_back((int)key, setting.second);
  }
}

void Config::flushViewer() {
  for (const auto &p : pending_servers) {
    if (p.port.empty()) {
      Warning() << "-N: settings without a port are not used";
      continue;
    }
    try {
      ServerEntry e;
      e.spec.bind = "0.0.0.0"; // as -N always did: every interface, unless bind says otherwise
      e.network_default = true;
      e.spec.port = Util::Parse::Integer(p.port, 1, 65535);
      for (const auto &a : p.args)
        if (!serverKey(e, a.first, a.second, nullptr))
          throw ConfigError("\"" + keyName(a.first) + "\" is not a setting of -N.");
      serverDone(e, _engine.site_config, &_engine.station_defaults, &_engine.stations, "-N " + p.port);
    } catch (const std::exception &ex) {
      throw std::runtime_error("-N " + p.port + ": " + ex.what());
    }
  }
  pending_servers.clear();
}

// "services": [ { "port": 8100, "bind": "0.0.0.0", "api": true, "filter": { "stations": [3] },
//                 "tracking": { "allow_type": "1,2,3", "file": "class-a.bin" } } ]
// the map unless "ui" is false, the other services when switched on
void Config::servicesFromJSON(const JSON::Value &v, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry, int main_port, const std::string &main_bind) {
  if (!v.isArray())
    throw ConfigError("\"services\" must be an array of server objects.");
  for (const auto &item : v.getArray()) {
    if (!item.isObject())
      throw ConfigError("each entry of \"services\" must be an object.");
    ServerEntry e;
    ServerSpec &spec = e.spec;
    bool port_given = false, active = true;
    for (const auto &m : item.getObject().getMembers()) {
      if (m.Key() == AIS::KEY_SETTING_ACTIVE) {
        active = Util::Parse::Switch(text(m.Get()));
        continue;
      }
      if (m.Key() == AIS::KEY_SETTING_PORT)
        port_given = true;
      if (!serverKey(e, m.Key(), text(m.Get()), &m.Get()))
        throw ConfigError("field \"" + keyName(m.Key()) + "\" is not allowed in a server.");
    }
    // switched off: its settings are kept, and read so a mistake shows, but it opens no port
    // and makes no tracker; the main server always serves
    if (spec.main && !active)
      throw ConfigError("the main server always serves: \"active\" is for the other servers.");
    if (!active)
      continue;
    // the main server is the control panel's: its port and address, the map always on
    if (spec.main) {
      if (!main_port)
        throw ConfigError("\"main\" marks the server beside the control panel of managed mode (-E); give this server a \"port\" instead.");
      if (port_given)
        throw ConfigError("the main server listens on the control panel's port plus one: leave \"port\" out.");
      for (const auto &other : site.servers)
        if (other.main)
          throw ConfigError("two servers are marked \"main\".");
      spec.port = main_port;
      spec.bind = main_bind;
      spec.viewer.managed = true;
      spec.ui = true;
    } else if (!spec.port)
      throw ConfigError("a server needs a \"port\".");
    else if (main_port && spec.port == main_port)
      throw ConfigError("the server on port " + std::to_string(spec.port) + " takes the viewer's port.");
    serverDone(e, site, defaults, registry, spec.main ? "the main server" : "the server on port " + std::to_string(spec.port));
  }
}

// The parts of a configuration a site reads as they stand, without an engine:
// managed mode applies them as the panel saves them
void Config::siteFromJSON(const JSON::JSON &root, SiteConfig &site, StationDefaults *defaults, StationRegistry *registry, int main_port, const std::string &main_bind) {
  // the tracking first: a web viewer's tracking settings yield to it
  if (const JSON::Value *t = root[AIS::KEY_SETTING_TRACKING])
    trackingFromJSON(*t, site);
  for (const auto &m : root.getMembers()) {
    switch (m.Key()) {
    case AIS::KEY_SETTING_TRACKING: break;
    case AIS::KEY_SETTING_SERVICES: servicesFromJSON(m.Get(), site, defaults, registry, main_port, main_bind); break;
    case AIS::KEY_SETTING_SERVER: serversFromJSON(m.Get(), site, defaults, registry); break;
#ifdef HASDELIVERY
    case AIS::KEY_SETTING_WEBHOOKS: webhooksFromJSON(m.Get(), site); break;
    case AIS::KEY_SETTING_MQTT_STATE: mqttStateFromJSON(m.Get(), site); break;
#endif
    default: break;
    }
  }
}
#endif

void Config::setModelfromJSON(const JSON::Member &m) {
  if (!isActiveObject(m.Get()))
    return;

  Receiver &r = *_engine.receivers.back();

  r.addModel(-1);
  setSettingsFromJSON(m.Get(), *r.Model(r.Count() - 1));
}

int Config::engineType(const std::string &s) {
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

  throw std::runtime_error(
      "engine type must be auto, v1_base, v1_high or v2_base");
}

// "engines": [ { "type": "v2_base", "settings": { "afc_wide": "off" } } ]
void Config::setEnginefromJSON(const JSON::Value &v) {
  if (!isActiveObject(v))
    return;

  Receiver &r = *_engine.receivers.back();

  int type = -1;
  const JSON::Value *settings = nullptr;
  std::vector<std::string> zones;

  for (const JSON::Member &p : v.getObject().getMembers()) {
    if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
      type = engineType(p.Get().to_string());
    else if (p.Key() == AIS::KEY_SETTING_SETTINGS)
      settings = &p.Get();
    else if (p.Key() == AIS::KEY_SETTING_ZONE) {
      if (!p.Get().isArray())
        throw std::runtime_error(
            "engine: \"zone\" must be an array of strings");
      for (const auto &z : p.Get().getArray())
        zones.push_back(z.to_string());
    } else if (p.Key() != AIS::KEY_SETTING_ACTIVE)
      throw std::runtime_error(
          "engine: \"" + std::string(AIS::KeyMap[p.Key()][JSON_DICT_SETTING]) +
          "\" must be inside the engine's \"settings\" block");
  }

  r.addModel(type);
  r.engine_zones.resize(r.Count()); // index must line up with the model
  r.engine_zones.back() = zones;

  if (settings)
    setSettingsFromJSON(*settings, *r.Model(r.Count() - 1));
}

void Config::setReceiverFromArray(const JSON::Member &m) {
  if (!m.Get().isArray())
    throw std::runtime_error("Receiver settings need to be an \"array\" of "
                             "\"objects\" in config file.");

  for (const auto &v : m.Get().getArray()) {
    if (!isActiveObject(v))
      continue;

    setReceiverfromJSON(v.getObject().getMembers(), false);
  }
}

void Config::setReceiverfromJSON(const std::vector<JSON::Member> &members,
                                 bool unspecAllowed) {
  std::string serial, input;
  // pass 1
  for (const auto &m : members) {
    switch (m.Key()) {
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

  if (!unspecAllowed) {
    if (serial.empty() && input.empty())
      throw std::runtime_error(
          "receiver needs to have a serial or input specified in Config.");
  }

  if ((!serial.empty() || !input.empty())) {
    _engine.newReceiver();

    if (!serial.empty())
      _engine.receivers.back()->SetKey(AIS::KEY_SETTING_SERIAL, serial);
    if (!input.empty())
      _engine.receivers.back()->SetKey(AIS::KEY_SETTING_INPUT, input);
  }

  // pass 2
  for (const auto &m : members) {
    switch (m.Key()) {
    case AIS::KEY_SETTING_ENGINES:
    case AIS::KEY_SETTING_MODEL:
    case AIS::KEY_SETTING_NMEA_CHANNEL:
      break; // pass 3
    case AIS::KEY_SETTING_SERIAL:
    case AIS::KEY_SETTING_INPUT:
    case AIS::KEY_SETTING_ACTIVE:
      break; // consumed by pass 1
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
    case AIS::KEY_SETTING_DESCRIPTION:
    case AIS::KEY_SETTING_DESC:
    case AIS::KEY_SETTING_STATION:
      _engine.receivers.back()->SetKey((AIS::Keys)m.Key(), m.Get().to_string());
      break;
    case AIS::KEY_SETTING_OWN_MMSI:
      _engine.own_mmsi = m.Get().getInt();
      break;
    default: {
      Setting *device =
          _engine.receivers.back()->getDeviceManager().settingForKey(
              (AIS::Keys)m.Key());
      if (!device) {
        // The top-level call gets the whole document; those fields belong to
        // the later passes. Only an explicit receiver object can reject one.
        if (!unspecAllowed)
          throw std::runtime_error(std::string("Config file: field \"") +
                                   AIS::KeyMap[m.Key()][JSON_DICT_SETTING] +
                                   "\" is not allowed in a receiver object.");
        break;
      }
      if (isActiveObject(m.Get()))
        setSettingsFromJSON(m.Get(), *device);
      break;
    }
    }
  }

  // the NMEA designation follows the channel choice, in whichever order the
  // two keys were written; engines are applied once the input format is known
  for (const auto &m : members) {
    if (m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL) {
      _engine.receivers.back()->SetKey(AIS::KEY_SETTING_NMEA_CHANNEL,
                                       m.Get().to_string());
    } else if (m.Key() == AIS::KEY_SETTING_ENGINES) {
      if (!m.Get().isArray())
        throw std::runtime_error(
            "\"engines\" must be an array, e.g. [ { \"type\": \"v2_base\", "
            "\"settings\": { ... } } ]");
      for (const auto &v : m.Get().getArray())
        setEnginefromJSON(v);
    } else if (m.Key() == AIS::KEY_SETTING_MODEL) {
      Warning() << "Receiver: \"model\" is deprecated and will be removed, use "
                   "\"engines\": [ { \"type\": ... } ] instead";
      if (m.Get().isString())
        _engine.receivers.back()->addModel(engineType(m.Get().to_string()));
      else
        setModelfromJSON(m);
    }
  }
}

void Config::read(const std::string &file_config) {
  if (!file_config.empty()) {
    setConfigBaseDir(file_config);
    set(Util::Helper::readFile(file_config));
  }
}

void Config::setSharing(const std::vector<JSON::Member> &members) {

  bool xchange = false;
  std::string uuid;
  std::vector<std::string> zones;

  for (const JSON::Member &m : members) {
    if (m.Key() == AIS::KEY_SETTING_SHARING) {
      xchange = Util::Parse::Switch(m.Get().to_string());
      _engine.xshare_defined = true;
    } else if (m.Key() == AIS::KEY_SETTING_SHARING_KEY)
      uuid = m.Get().to_string();
    else if (m.Key() == AIS::KEY_SETTING_SHARING_ZONE) {
      if (!m.Get().isArray())
        throw std::runtime_error(
            "\"sharing_zone\" must be an array of strings");
      for (const auto &v : m.Get().getArray())
        zones.push_back(v.to_string());
    }
  }

  if ((xchange || !uuid.empty()) && !_engine.comm_feed)
    _engine.createCommunityFeed();

  if (!uuid.empty() && _engine.comm_feed)
    _engine.comm_feed->SetKey(AIS::KEY_SETTING_UUID, uuid);

  if (!zones.empty() && _engine.comm_feed)
    _engine.comm_feed->zones = zones;
}

void Config::set(const std::string &str) {
  std::string config;
  int version = 0;

  JSON::Parser parser(JSON_DICT_SETTING);
  JSON::Document doc = parser.parse(str);

  // pass 1
  for (const auto &m : doc.getMembers()) {
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
    throw std::runtime_error("version and/or format of config file not "
                             "supported (required version <=1)");

  // rejected before setReceiverfromJSON runs, so nothing is applied from an
  // invalid config
  for (const auto &m : doc.getMembers())
    if (m.Key() == AIS::KEY_SETTING_CHANNEL ||
        m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL ||
        m.Key() == AIS::KEY_SETTING_ENGINES)
      throw std::runtime_error(std::string("Config file: \"") +
                               AIS::KeyMap[m.Key()][JSON_DICT_SETTING] +
                               "\" is not allowed in the main section, set it "
                               "inside a receiver object.");

  setReceiverfromJSON(doc.getMembers(), true);
  setSharing(doc.getMembers());

  // pass 2 - instantiate all receivers from the array before outputs apply tags
  for (const auto &p : doc.getMembers()) {
    if (p.Key() == AIS::KEY_SETTING_RECEIVER)
      setReceiverFromArray(p);
  }

  // pass 3
  for (const auto &m : doc.getMembers()) {
    switch (m.Key()) {
    // fields that are already processed for completeness
    case AIS::KEY_SETTING_CONFIG:
    case AIS::KEY_SETTING_VERSION:
    case AIS::KEY_SETTING_VERBOSE:
    case AIS::KEY_SETTING_SERIAL:
    case AIS::KEY_SETTING_INPUT:
    case AIS::KEY_SETTING_MODEL:
    case AIS::KEY_SETTING_META:
    case AIS::KEY_SETTING_LAT:
    case AIS::KEY_SETTING_LON:
    case AIS::KEY_SETTING_STATION:
    case AIS::KEY_SETTING_DESCRIPTION:
    case AIS::KEY_SETTING_DESC:
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
    case AIS::KEY_SETTING_SERVER: // the site's, read below
#ifdef HASWEBVIEWER
      break;
#else
      throw std::runtime_error("Config file: \"server\" needs a build with the web viewer (cmake -DWEBVIEWER=ON).");
#endif
    case AIS::KEY_SETTING_HTTP:
      addOutputsFromJSON<IO::HTTPStreamer>(m, "HTTP");
      break;
    case AIS::KEY_SETTING_DB:
      addDatabaseOutputsFromJSON(m);
      break;
    case AIS::KEY_SETTING_RECEIVER: // handled in pass 1.5
      break;
    // managed mode's: its viewer section is read with the site
    case AIS::KEY_SETTING_CONTROL:
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
          throw std::runtime_error("Config file: timeout must be false or a "
                                   "number between 1 and 3600.");
      } else {
        _engine.timeout = Util::Parse::Integer(m.Get().to_string(), 1, 3600);
      }
      break;
    case AIS::KEY_SETTING_TIMEOUT_NOMSG:
      _engine.timeout_nomsg = Util::Parse::Switch(m.Get().to_string());
      break;
    case AIS::KEY_SETTING_STATIONS:
      setStationsFromJSON(m.Get());
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
      _engine.screen.verboseUpdateTime =
          Util::Parse::Integer(m.Get().to_string(), 1, 3600);
      break;
    default:
      if (m.Key() >= 0 && m.Key() < AIS::KEY_COUNT)
        throw std::runtime_error(std::string("Config file: field \"") +
                                 AIS::KeyMap[m.Key()][JSON_DICT_SETTING] +
                                 "\" in main section is not allowed.");
      else
        throw std::runtime_error(
            "Config file: unknown field in main section is not allowed.");
    }
  }

#ifdef HASAPI
  if (managed)
    managedSite(doc.root, control_port, viewer_port, bind, _engine.site_config, _engine.station_defaults, &_engine.stations);
  else
    ofFile([&] { siteFromJSON(doc.root, _engine.site_config, &_engine.station_defaults, &_engine.stations); });
#endif
}

// Once every key is read: the last -N becomes its server, and every station a
// receiver names is one the configuration defines.
void Config::finish() {
#ifdef HASAPI
  flushViewer();
#endif

  // a station a receiver names is one the configuration defines; the default station, 0, is there always
  for (std::size_t k = 0; k < _engine.receivers.size(); k++) {
    const int id = _engine.receivers[k]->getStation();
    if (id != 0 && !_engine.stations.find(id))
      throw std::runtime_error("Receiver " + std::to_string(k + 1) + " names station " + std::to_string(id) +
                               ", which \"stations\" does not define.");
  }
}
