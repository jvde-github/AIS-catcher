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

#include "SiteConfig.h"

#include <algorithm>
#include <stdexcept>

#include "Convert.h"
#include "Helper.h"
#include "JSON.h"
#include "Keys.h"
#include "Logger.h"
#include "Parse.h"
#include "StationRegistry.h"
#include "TCPServer.h"
#ifdef HASDELIVERY
#include "Event.h"
#endif

// The readers of what a configuration says of the site: the stations, the
// tracking, the servers, the webhooks and the MQTT state, each section as the
// file has it. A value comes as text, a list as text separated by commas.

namespace
{
	// what a configuration says wrong, without saying where: the readers of a
	// file add "Config file: ", a -N or -W names itself
	struct ConfigError : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	// the readers' errors, said of the file
	template <typename F>
	void ofFile(F f)
	{
		try
		{
			f();
		}
		catch (const ConfigError &e)
		{
			throw std::runtime_error(std::string("Config file: ") + e.what());
		}
	}
}

namespace SiteReader
{
	bool isPath(int key)
	{
		return key == AIS::KEY_SETTING_FILE || key == AIS::KEY_SETTING_PLACES;
	}

	// "stations": [ { "id": 0, "name": "Mast", "lat": 52.1, "lon": 4.27, "alt": 20 } ]
	// 0 is the default station, where every receiver stands that names none
	void stations(const JSON::Value &v, StationRegistry &into)
	{
		ofFile([&]
			   {
				   if (!v.isArray())
					   throw ConfigError("\"stations\" must be an array of station objects.");
				   for (const auto &item : v.getArray())
				   {
					   if (!item.isObject())
						   throw ConfigError("each entry of \"stations\" must be an object.");
					   StationRegistry::Station s;
					   // a configured station stays where it is put unless GPS is chosen for it
					   s.gps = false;
					   bool has_id = false;
					   for (const auto &m : item.getObject().getMembers())
					   {
						   const std::string value = m.Get().to_string();
						   switch (m.Key())
						   {
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
							   throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in a station.");
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
}

#ifdef HASAPI
namespace
{
	// one value or a list of them; a text value is split at its commas
	void addEach(const std::string &list, std::vector<std::string> &out)
	{
		std::vector<std::string> parts;
		Util::Parse::Split(list, ',', parts);
		for (const auto &p : parts)
			if (!p.empty())
				out.push_back(p);
	}

	void addEach(const JSON::Value &v, std::vector<std::string> &out)
	{
		if (!v.isNull())
			addEach(JSON::text(v), out);
	}

	int integer(int key, const std::string &arg, long min, long max)
	{
		return (int)Util::Parse::Integer(arg, min, max, AIS::settingName(key));
	}

	// The keys a server section (-N, "server", managed mode's old viewer section),
	// a server under "services" and the "tracking" section share: each read here
	// once, with one range wherever it is written. Each returns false for a key
	// that is not its own.

	// the options of a server's map pages
	bool viewerKey(int key, const std::string &arg, ViewerOptions &o)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_STATION: o.station = arg; break;
		case AIS::KEY_SETTING_STATION_LINK: o.station_link = arg; break;
		case AIS::KEY_SETTING_WEBCONTROL_HTTP: o.webcontrol_http = arg; break;
		case AIS::KEY_SETTING_FRAME_ANCESTORS: o.frame_ancestors = arg; break;
		case AIS::KEY_SETTING_CONTEXT: o.context = arg; break;
		case AIS::KEY_SETTING_ABOUT: o.about = arg; break;
		case AIS::KEY_SETTING_REALTIME: o.realtime = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_LOG: o.log = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_DECODER: o.decoder = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_REPLAY: o.replay = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_MSG:
		case AIS::KEY_SETTING_MESSAGE:
			o.msg = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_MBTILES: addEach(arg, o.mbtiles); break;
		case AIS::KEY_SETTING_MBOVERLAY: addEach(arg, o.mboverlay); break;
		case AIS::KEY_SETTING_FSTILES: addEach(arg, o.fstiles); break;
		case AIS::KEY_SETTING_FSOVERLAY: addEach(arg, o.fsoverlay); break;
		case AIS::KEY_SETTING_PLUGIN: addEach(arg, o.plugins); break;
		case AIS::KEY_SETTING_STYLE: addEach(arg, o.styles); break;
		case AIS::KEY_SETTING_PLUGIN_DIR: addEach(arg, o.plugin_dirs); break;
		default: return false;
		}
		return true;
	}

	// how a tracker keeps ships: the tracking, and for the site's the backup file
	// and its interval and the places, each resolved against base_dir; a server's
	// own tracker, read without a site, has no place for those: false
	bool trackingKey(int key, const std::string &arg, TrackingConfig &t, SiteConfig *site, const std::string &base_dir)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_FILE:
			if (!site)
				return false;
			site->backup_file = Util::Helper::resolvePath(arg, base_dir);
			break;
		case AIS::KEY_SETTING_PLACES:
			if (!site)
				return false;
			site->places = Util::Helper::resolvePath(arg, base_dir);
			break;
		case AIS::KEY_SETTING_BACKUP:
			if (!site)
				return false;
			site->backup_minutes = integer(key, arg, 0, 2 * 24 * 60);
			if (site->backup_minutes > 0 && site->backup_minutes < 5)
				throw std::runtime_error("\"backup\" is 0 (off) or at least 5 minutes.");
			break;
		case AIS::KEY_SETTING_HISTORY: t.time_history = integer(key, arg, 5, 12 * 3600); break;
		case AIS::KEY_SETTING_TRACK_TIME:
		case AIS::KEY_SETTING_REPLAY_TIME:
			t.track_time = integer(key, arg, 0, 7 * 24 * 3600);
			break;
		case AIS::KEY_SETTING_TRACK_MEMORY: t.track_memory = integer(key, arg, 16, 256 * 1024); break;
		case AIS::KEY_SETTING_MAX_SHIPS: t.max_ships = integer(key, arg, 1024, 4 * 1024 * 1024); break;
		case AIS::KEY_SETTING_CUTOFF: t.cutoff = integer(key, arg, 0, 10000); break;
		case AIS::KEY_SETTING_EXPIRE: t.expire_fields = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_SERVER_MODE: t.server_mode = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_LAT: t.lat = (float)Util::Parse::Float(arg, -90, 90); break;
		case AIS::KEY_SETTING_LON: t.lon = (float)Util::Parse::Float(arg, -180, 180); break;
		default: return false;
		}
		return true;
	}

	bool isTrackingKey(int key)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_FILE: case AIS::KEY_SETTING_PLACES: case AIS::KEY_SETTING_BACKUP: case AIS::KEY_SETTING_HISTORY:
		case AIS::KEY_SETTING_TRACK_TIME: case AIS::KEY_SETTING_REPLAY_TIME: case AIS::KEY_SETTING_TRACK_MEMORY:
		case AIS::KEY_SETTING_MAX_SHIPS: case AIS::KEY_SETTING_CUTOFF: case AIS::KEY_SETTING_EXPIRE:
		case AIS::KEY_SETTING_SERVER_MODE: case AIS::KEY_SETTING_LAT: case AIS::KEY_SETTING_LON:
			return true;
		default:
			return false;
		}
	}

	// what a server section says of the default station
	bool stationKey(int key, const std::string &arg, StationDefaults &d)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_STATION: d.name = arg; break;
		case AIS::KEY_SETTING_LAT: d.lat = (float)Util::Parse::Float(arg, -90, 90); break;
		case AIS::KEY_SETTING_LON: d.lon = (float)Util::Parse::Float(arg, -180, 180); break;
		case AIS::KEY_SETTING_ALT: d.alt = (float)Util::Parse::Float(arg, -1000, 20000); break;
		case AIS::KEY_SETTING_SHARE_LOC: d.share_loc = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_USE_GPS: d.gps = Util::Parse::Switch(arg); break;
		case AIS::KEY_SETTING_OWN_MMSI: d.mmsi = (uint32_t)integer(key, arg, 0, 999999999); break;
		default: return false;
		}
		return true;
	}

	// "keys": [ { "name": "home-assistant", "sha256": "…", "scopes": ["data", "system"] } ]
	void keysFromJSON(const JSON::Value &v, std::vector<ApiKey> &out)
	{
		if (!v.isArray())
			throw ConfigError("\"keys\" must be an array of key objects.");
		for (const auto &item : v.getArray())
		{
			if (!item.isObject())
				throw ConfigError("each entry of \"keys\" must be an object.");
			ApiKey k;
			bool scoped = false;
			for (const auto &m : item.getObject().getMembers())
			{
				switch (m.Key())
				{
				case AIS::KEY_SETTING_NAME: k.name = m.Get().to_string(); break;
				case AIS::KEY_SETTING_SHA256: k.sha256 = m.Get().to_string(); break;
				case AIS::KEY_SETTING_SCOPES:
				{
					std::vector<std::string> names;
					addEach(m.Get(), names);
					k.scopes = 0;
					scoped = true;
					for (const auto &n : names)
					{
						if (n == "data")
							k.scopes |= ApiKey::DATA;
						else if (n == "system")
							k.scopes |= ApiKey::SYSTEM;
						else
							throw ConfigError("key scope \"" + n + "\" is not data or system.");
					}
					break;
				}
				default:
					throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in a key.");
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

	// a server's or a webhook's "view", as its settings save it: a list as an
	// array, an unset bound as null
	void viewFilterFromJSON(const JSON::Value &v, ViewFilter &filter)
	{
		if (!v.isObject())
			throw ConfigError("a \"view\" must be an object.");
		for (const auto &m : v.getObject().getMembers())
		{
			const std::string key = AIS::settingName(m.Key());
			if (!ViewFilter::isKey(key))
				throw ConfigError("\"" + key + "\" is not a term of a view.");
			const JSON::Value &value = m.Get();
			try
			{
				if (value.isArray())
					for (const auto &item : value.getArray())
						filter.set(key, item.to_string());
				else if (value.isArrayString())
					for (const auto &item : value.getStringArray())
						filter.set(key, item);
				else if (!value.isNull())
					filter.set(key, value.to_string());
			}
			catch (const std::exception &e)
			{
				throw ConfigError(std::string("view: ") + e.what());
			}
		}
	}

	// an address only this machine reaches
	bool localBind(const std::string &bind)
	{
		return bind == "localhost" || bind == "::1" || bind.compare(0, 4, "127.") == 0;
	}

	// a server is keyed or public, never both; what others could read beyond this
	// machine is guarded by its keys, or said to be public, and the installation
	// is never public there
	void checkServerAccess(const ServerSpec &spec, const std::string &where)
	{
		if (spec.open && !spec.keys.empty())
			throw ConfigError(where + " is both public and keyed: choose one. \"public\" is for anyone who reaches it; \"keys\" for the readers it names.");
		if (!localBind(spec.bind) && spec.keys.empty() && !spec.open && (spec.api || spec.mcp || spec.system))
			throw ConfigError(where + " listens beyond this machine: give it \"keys\", say \"public\": true for data anyone may read, or bind it to 127.0.0.1.");
		if (spec.system && spec.open && !localBind(spec.bind))
			throw ConfigError(where + " is public on the network: \"system\" is never public there; give it \"keys\" instead, or switch \"system\" off.");
		// the viewer's log, which may name hosts and paths, is a key's on a server with keys
		if (spec.ui && spec.viewer.log && spec.keys.empty() && !localBind(spec.bind))
		{
			if (spec.open)
				throw ConfigError(where + " is public on the network: its \"log\" would be too; give it \"keys\" instead, or switch \"log\" off.");
			Warning() << where << ": its log is open to anyone who reaches it; give it \"keys\", bind it to 127.0.0.1, or switch \"log\" off";
		}
	}

	// A server, from whichever section writes one: -N, a "server" section, managed
	// mode's old viewer section, or an entry of "services". The terms of the
	// message filter, the zones and the groups make it a tracker of its own; the
	// tracking keys go to that tracker when it has one, else to the site's.
	struct ServerEntry
	{
		ServerSpec spec;
		StationDefaults at; // what the section says of the station
		std::vector<std::pair<int, std::string>> tracking_terms, station_terms;
		std::vector<int> station_ids; // the stations the server shows, and describes when the file does not list them
		AIS::Filter filter;			  // validates the filter terms where they stand, and tracks the switch
		std::vector<std::string> dropped;
		bool network_default = false; // a -N or server section: every interface, unless it says otherwise
		bool bind_given = false;
		bool ui_given = false;
		bool tls_given = false;

		// one key; `json` is its value where the file has one, for the keys that take an object
		bool read(int key, const std::string &value, const JSON::Value *json);
	};

	bool ServerEntry::read(int key, const std::string &value, const JSON::Value *json)
	{
		TrackerSpec &own = spec.tracking;
		switch (key)
		{
		case AIS::KEY_SETTING_PORT: spec.port = Util::Parse::Integer(value, 1, 65535); return true;
		case AIS::KEY_SETTING_IP_BIND:
		case AIS::KEY_SETTING_BIND: spec.bind = value; bind_given = true; return true;
		case AIS::KEY_SETTING_UI: spec.ui = Util::Parse::Switch(value); ui_given = true; return true;
		case AIS::KEY_SETTING_API: spec.api = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_MCP: spec.mcp = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_SYSTEM: spec.system = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_GEOJSON: spec.geojson = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_KML: spec.kml = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_PROME:
		case AIS::KEY_SETTING_METRICS: spec.metrics = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_PUBLIC: spec.open = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_PLACE_GEOMETRY:
			throw ConfigError("\"place_geometry\" is a term of the server's \"view\".");
		case AIS::KEY_SETTING_MAIN: spec.main = Util::Parse::Switch(value); return true;
		case AIS::KEY_SETTING_DESCRIPTION: return true; // the panel's name for it
		case AIS::KEY_SETTING_TLS:
			if (!json || !json->isObject())
				throw ConfigError("\"tls\" must be an object: { \"cert\": file, \"key\": file }.");
			tls_given = true;
			for (const auto &m : json->getObject().getMembers())
			{
				if (m.Key() == AIS::KEY_SETTING_CERT)
					spec.tls_cert = m.Get().to_string();
				else if (m.Key() == AIS::KEY_SETTING_KEY)
					spec.tls_key = m.Get().to_string();
				else
					throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in \"tls\".");
			}
			return true;
		case AIS::KEY_SETTING_STATION_ID:
		{
			std::vector<std::string> parts;
			Util::Parse::Split(value, ',', parts);
			for (const auto &p : parts)
				if (!p.empty())
					station_ids.push_back((int)Util::Parse::Integer(p, 0, 2147483647));
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
		case AIS::KEY_SETTING_TRACKING:
		{
			// a tracker of its own: its filter, zones and groups, and what it keeps
			if (!json || !json->isObject())
				throw ConfigError("a server's \"tracking\" must be an object.");
			for (const auto &m : json->getObject().getMembers())
			{
				const int k = m.Key();
				const std::string v = JSON::text(m.Get());
				if (k == AIS::KEY_SETTING_ZONE || k == AIS::KEY_SETTING_GROUPS_IN || isTrackingKey(k))
				{
					read(k, v, &m.Get());
					continue;
				}
				if (!filter.SetOptionKey((AIS::Keys)k, v))
					throw ConfigError("field \"" + AIS::settingName(k) + "\" is not allowed in a server's \"tracking\".");
				own.filter.emplace_back(k, v);
			}
			// a term in "tracking" is meant: the switch is on
			if (!filter.isOn() && !own.filter.empty())
			{
				filter.SetOptionKey(AIS::KEY_SETTING_FILTER, "on");
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
			dropped.push_back("\"" + AIS::settingName(key) + "\" has no effect and can go");
			return true;
		default:
			break;
		}
		// a viewer's option, a tracking key (the position is the station's too), a station key, a filter term
		bool taken = viewerKey(key, value, spec.viewer);
		if (isTrackingKey(key))
		{
			tracking_terms.emplace_back(key, value);
			taken = true;
		}
		if (stationKey(key, value, at))
		{
			station_terms.emplace_back(key, value);
			taken = true;
		}
		if (!taken && filter.SetOptionKey((AIS::Keys)key, value))
		{
			own.filter.emplace_back(key, value);
			taken = true;
		}
		return taken;
	}

	// One reading of a configuration's site: into the site, the station defaults
	// and the stations, with the file's folder for its files. In managed mode
	// main_port and main_bind are the server beside the control panel, for the
	// entry marked "main"; command_line names, per "server" entry, the -N that
	// gave it, empty for a file's.
	struct Reader
	{
		SiteConfig &site;
		StationDefaults &defaults;
		StationRegistry &stations;
		const std::string &base_dir;
		int main_port;
		std::string main_bind;
		const std::vector<std::string> *command_line;

		Reader(SiteConfig &s, StationDefaults &d, StationRegistry &st, const std::string &base, int port = 0, const std::string &bind = "", const std::vector<std::string> *servers = nullptr)
			: site(s), defaults(d), stations(st), base_dir(base), main_port(port), main_bind(bind), command_line(servers) {}

		void tracking(const JSON::Value &v);
		void server(ServerEntry &e, const std::string &where);
		void servers(const JSON::Value &v);
		void commandLineServer(const JSON::Value &entry, std::string where);
		void services(const JSON::Value &v);
		void managedViewer(const JSON::Value &viewer);
		void read(const JSON::JSON &root);
	};

	// "tracking": { "file": "/var/lib/aiscatcher/tracking.bin", "backup": 10, "history": 1800 }
	void Reader::tracking(const JSON::Value &v)
	{
		if (!v.isObject())
			throw ConfigError("\"tracking\" must be an object.");
		site.tracking_set = true;
		for (const auto &m : v.getObject().getMembers())
			if (!trackingKey(m.Key(), JSON::text(m.Get()), site.tracking, &site, base_dir))
				throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in \"tracking\".");
	}

	// the entry is complete: it keeps ships of its own when its filter is on or it
	// picks receivers, else it reads the site's tracker and configures that
	void Reader::server(ServerEntry &e, const std::string &where)
	{
		ServerSpec &spec = e.spec;
		TrackerSpec &own = spec.tracking;
		const StationDefaults &at = e.at;
		if (e.tls_given)
		{
			if (spec.tls_cert.empty() || spec.tls_key.empty())
				throw ConfigError(where + ": \"tls\" needs \"cert\" and \"key\", the files of the certificate and its key.");
			spec.tls_cert = Util::Helper::resolvePath(spec.tls_cert, base_dir);
			spec.tls_key = Util::Helper::resolvePath(spec.tls_key, base_dir);
			// managed mode checks a save and its start: a renewal gone bad does not stop its engine
			const std::string error = main_port ? std::string() : IO::TCPServer::checkTLS(spec.tls_cert, spec.tls_key);
			if (!error.empty())
				throw ConfigError(where + ": " + error);
		}
		// the map asks no key: a server with keys serves it only when it says so; the main server's is the panel's
		const bool keys_hide_map = !spec.keys.empty() && !e.ui_given && !spec.main;
		if (keys_hide_map)
			spec.ui = false;
		// only a filter makes a tracker of its own: one that keeps messages out, or
		// picks the receivers that feed it; a view, a station and the tracking do not
		bool terms = false;
		for (const auto &t : own.filter)
			terms |= t.first != AIS::KEY_SETTING_FILTER;
		const bool routed = !own.zones.empty() || own.groups_in != GROUPS_ALL;
		if (e.filter.isOn() && !terms && !routed)
			e.dropped.push_back("a filter without terms lets every message through: the server reads the site's tracker");
		if (!e.filter.isOn() && terms)
			e.dropped.push_back("filter terms without \"filter\" on are not used");
		own.own = (e.filter.isOn() && terms) || routed;
		if (own.own && spec.main)
			throw ConfigError(where + " shows every ship the station hears: give the filter, zones or groups to another server.");
		if (!e.station_ids.empty())
		{
			// the server shows these stations; what the section says of a station
			// describes the first of them, where "stations" does not list it
			for (int id : e.station_ids)
				if (std::find(spec.view.stations.begin(), spec.view.stations.end(), id) == spec.view.stations.end())
					spec.view.stations.push_back(id);
			// GPS moves it only when the section says so, as a listed station has it
			bool gps_said = false;
			for (const auto &t : e.station_terms)
				gps_said |= t.first == AIS::KEY_SETTING_USE_GPS;
			const bool gps = gps_said && at.gps;
			for (std::size_t i = 0; i < e.station_ids.size(); i++)
			{
				const int id = e.station_ids[i];
				if (stations.find(id))
					continue;
				StationRegistry::Station s;
				s.id = id;
				s.name = i == 0 && !at.name.empty() ? at.name : "Station " + std::to_string(id);
				if (i == 0)
				{
					if (at.placed())
					{
						s.lat = at.lat;
						s.lon = at.lon;
					}
					s.alt = at.alt;
					s.mmsi = at.mmsi;
					s.share_loc = at.share_loc;
				}
				s.gps = gps;
				stations.define(s);
			}
		}
		if (own.own)
		{
			own.tracking.msg_save = spec.viewer.msg;
			// backups and places are the site's: one file, one catalogue
			for (const auto &t : e.tracking_terms)
				if (!trackingKey(t.first, t.second, own.tracking, nullptr, base_dir))
					e.dropped.push_back("\"" + AIS::settingName(t.first) + "\" is the site's: a server with ships of its own keeps no backup and reads no places");
			// what the section says of the station is this tracker's, unless it names
			// the station: it measures from there, shares it or not, follows GPS or its own vessel
			if (e.station_ids.empty())
				for (const auto &t : e.station_terms)
					switch (t.first)
					{
					case AIS::KEY_SETTING_LAT:
					case AIS::KEY_SETTING_LON:
						own.tracking.fixed = at.placed();
						if (at.placed())
						{
							own.tracking.lat = at.lat;
							own.tracking.lon = at.lon;
						}
						break;
					case AIS::KEY_SETTING_SHARE_LOC: own.tracking.latlon_share = at.share_loc; break;
					case AIS::KEY_SETTING_USE_GPS: own.tracking.use_gps = at.gps; break;
					case AIS::KEY_SETTING_OWN_MMSI: own.tracking.own_mmsi = at.mmsi; break;
					default: break; // its name and height are the station list's
					}
		}
		else
		{
			if (e.station_ids.empty())
				for (const auto &t : e.station_terms)
					stationKey(t.first, t.second, defaults);
			if (site.tracking_set)
			{
				if (!e.tracking_terms.empty())
					Warning() << where << ": the configuration's \"tracking\" says how ships are kept; the server's tracking settings are not used";
			}
			else
			{
				const std::string file = site.backup_file;
				for (const auto &t : e.tracking_terms)
					trackingKey(t.first, t.second, site.tracking, &site, base_dir);
				if (!file.empty() && site.backup_file != file)
					Warning() << where << ": the backup file is now " << site.backup_file << ", not " << file << ": one tracker, one file";
			}
			own.filter.clear();
		}
		for (const auto &d : e.dropped)
			Warning() << where << ": " << d;
		if (spec.served().empty())
			throw ConfigError(where + " serves nothing: " + (keys_hide_map ? "with \"keys\" the map needs \"ui\": true, as anyone can read it," : "the map is switched off") + " and nothing else is on.");
		// the map of before listens on every interface; the API, MCP and System only with keys or public
		if (e.network_default && !e.bind_given && (spec.api || spec.mcp || spec.system) && spec.keys.empty() && !spec.open)
		{
			spec.bind = "127.0.0.1";
			Info() << where << ": the API, MCP and System keep this server on this machine; give it keys, public on, or bind 0.0.0.0 to open it";
		}
		checkServerAccess(spec, where);
		for (const auto &other : site.servers)
			if (other.port == spec.port)
				throw ConfigError("two servers on port " + std::to_string(spec.port) + ".");
		site.servers.push_back(spec);
	}

	// "server": [ { "port": 8100, "station": "Home", "lat": 52.1, "lon": 4.3, "filter": true, "allow_type": "1,2,3" } ]
	// the web viewer as its section wrote it: a server on the site's tracker, or on one of its own behind its filter
	void Reader::servers(const JSON::Value &v)
	{
		std::size_t index = 0;
		const auto one = [&](const JSON::Value &entry)
		{
			if (!entry.isObject())
				throw ConfigError("each entry of \"server\" must be an object.");
			const std::size_t at = index++;
			if (command_line && at < command_line->size() && !(*command_line)[at].empty())
			{
				commandLineServer(entry, (*command_line)[at]);
				return;
			}
			ServerEntry e;
			e.spec.bind = "0.0.0.0"; // the viewer of before listened on every interface
			e.network_default = true;
			bool active = true;
			for (const auto &m : entry.getObject().getMembers())
			{
				if (m.Key() == AIS::KEY_SETTING_ACTIVE)
				{
					active = Util::Parse::Switch(m.Get().to_string());
					continue;
				}
				if (!e.read(m.Key(), JSON::text(m.Get()), &m.Get()))
					throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in \"server\".");
			}
			if (!active)
				return;
			if (!e.spec.port)
				throw ConfigError("a \"server\" needs a \"port\".");
			server(e, "the web viewer on port " + std::to_string(e.spec.port));
		};
		if (v.isArray())
			for (const auto &entry : v.getArray())
				one(entry);
		else
			one(v);
	}

	// -N as the command line had it, now an entry of the document's "server": every
	// interface unless bind says otherwise, and its mistakes in its own words
	void Reader::commandLineServer(const JSON::Value &entry, std::string where)
	{
		try
		{
			ServerEntry e;
			e.spec.bind = "0.0.0.0";
			e.network_default = true;
			// a value as -N gives it, text: only the view, which -N writes for place_geometry, is an object
			for (const auto &m : entry.getObject().getMembers())
				if (!e.read(m.Key(), JSON::text(m.Get()), m.Get().isObject() ? &m.Get() : nullptr))
					throw ConfigError("\"" + AIS::settingName(m.Key()) + "\" is not a setting of -N.");
			if (!e.spec.port)
				throw ConfigError("no port: give one, as -N 8100 or -N PORT 8100.");
			where = "-N " + std::to_string(e.spec.port);
			server(e, where);
		}
		catch (const std::exception &ex)
		{
			// a message that names the server already keeps its own words
			const std::string what = ex.what();
			throw std::runtime_error(what.compare(0, where.size(), where) == 0 ? what : where + ": " + what);
		}
	}

	// "services": [ { "port": 8100, "bind": "0.0.0.0", "api": true, "view": { "stations": [3] },
	//                 "tracking": { "allow_type": "1,2,3", "file": "class-a.bin" } } ]
	// the map unless "ui" is false, the other services when switched on
	void Reader::services(const JSON::Value &v)
	{
		if (!v.isArray())
			throw ConfigError("\"services\" must be an array of server objects.");
		for (const auto &item : v.getArray())
		{
			if (!item.isObject())
				throw ConfigError("each entry of \"services\" must be an object.");
			ServerEntry e;
			ServerSpec &spec = e.spec;
			bool active = true;
			for (const auto &m : item.getObject().getMembers())
			{
				if (m.Key() == AIS::KEY_SETTING_ACTIVE)
				{
					active = Util::Parse::Switch(JSON::text(m.Get()));
					continue;
				}
				if (!e.read(m.Key(), JSON::text(m.Get()), &m.Get()))
					throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in a server.");
			}
			// switched off: its settings are kept, and read so a mistake shows, but it opens no port
			// and makes no tracker; the main server always serves
			if (spec.main && !active)
				throw ConfigError("the main server always serves: \"active\" is for the other servers.");
			if (!active)
				continue;
			// the main server is the control panel's: its port and address, the map always on
			if (spec.main)
			{
				if (!main_port)
					throw ConfigError("\"main\" marks the server beside the control panel of managed mode (-E); give this server a \"port\" instead.");
				if (spec.port)
					throw ConfigError("the main server listens on the control panel's port plus one: leave \"port\" out.");
				for (const auto &other : site.servers)
					if (other.main)
						throw ConfigError("two servers are marked \"main\".");
				spec.port = main_port;
				spec.bind = main_bind;
				spec.viewer.managed = true;
				spec.ui = true;
			}
			else if (!spec.port)
				throw ConfigError("a server needs a \"port\".");
			else if (main_port && spec.port == main_port)
				throw ConfigError("the server on port " + std::to_string(spec.port) + " takes the viewer's port.");
			server(e, spec.main ? "the main server" : "the server on port " + std::to_string(spec.port));
		}
	}

	// Managed mode's viewer section, from before the main server was an entry of
	// "services": a server with the map on the port beside the control panel, the
	// tracking, and what the section says of the default station. The panel moves
	// it into "services", "tracking" and "stations" when it saves.
	void Reader::managedViewer(const JSON::Value &viewer)
	{
		ServerEntry e;
		e.spec.port = main_port;
		e.spec.bind = main_bind;
		e.spec.main = true;
		e.spec.viewer.managed = true;
		if (viewer.isObject())
			for (const auto &m : viewer.getObject().getMembers())
				e.read(m.Key(), JSON::text(m.Get()), &m.Get()); // what else the section holds is the panel's
		server(e, "the main server");
	}

	// the parts of a configuration a site reads as they stand, without an engine:
	// managed mode applies them as the panel saves them
	void Reader::read(const JSON::JSON &root)
	{
		// the tracking first: a server's tracking settings yield to it
		if (const JSON::Value *t = root[AIS::KEY_SETTING_TRACKING])
			tracking(*t);
		for (const auto &m : root.getMembers())
		{
			switch (m.Key())
			{
			case AIS::KEY_SETTING_TRACKING: break;
			case AIS::KEY_SETTING_SERVICES: services(m.Get()); break;
			case AIS::KEY_SETTING_SERVER: servers(m.Get()); break;
#ifdef HASDELIVERY
			case AIS::KEY_SETTING_WEBHOOKS: SiteReader::webhooks(m.Get(), site); break;
			case AIS::KEY_SETTING_MQTT_STATE: SiteReader::mqttState(m.Get(), site); break;
#endif
			default: break;
			}
		}
	}
}

namespace SiteReader
{
	void site(const JSON::JSON &root, const std::string &base_dir, SiteConfig &site, StationDefaults &defaults, StationRegistry &stations, const std::vector<std::string> *command_line_servers)
	{
		ofFile([&]
			   { Reader(site, defaults, stations, base_dir, 0, "", command_line_servers).read(root); });
	}

	// Managed mode's site as its configuration says now: the servers, the tracking
	// and the webhooks and MQTT state as the file has them, and, where no entry of
	// "services" is marked main, the viewer section of a file from before as the
	// server beside the control panel, read into the same site so what it says of
	// the tracking and the station counts like any server's
	void managedSite(const JSON::JSON &root, const std::string &base_dir, int control_port, int viewer_port, const std::string &bind, SiteConfig &site, StationDefaults &defaults, StationRegistry &stations)
	{
		ofFile([&]
			   {
				   SiteConfig made;
				   Reader reader(made, defaults, stations, base_dir, viewer_port, bind);
				   reader.read(root);
				   for (const auto &spec : made.servers)
					   if (!spec.main && (spec.port == control_port || spec.port == viewer_port))
						   throw ConfigError("the server on port " + std::to_string(spec.port) + " takes the " + (spec.port == control_port ? "control panel's" : "viewer's") + " port.");
				   const auto main = std::find_if(made.servers.begin(), made.servers.end(), [](const ServerSpec &s) { return s.main; });
				   if (main == made.servers.end())
				   {
					   const JSON::Value *control = root[AIS::KEY_SETTING_CONTROL];
					   const JSON::Value *viewer = control && control->isObject() ? control->getObject()[AIS::KEY_SETTING_VIEWER] : nullptr;
					   reader.managedViewer(viewer ? *viewer : JSON::Value());
					   // first, as the panel lists it
					   std::rotate(made.servers.begin(), made.servers.end() - 1, made.servers.end());
				   }
				   else
					   defaults.name = main->viewer.station;
				   site = made;
			   });
	}

#ifdef HASDELIVERY
	// "webhooks": [ { "url": "https://…", "events": ["safety", "place_enter"], "secret": "whsec_…" } ]
	void webhooks(const JSON::Value &v, SiteConfig &site)
	{
		if (!v.isArray())
			throw ConfigError("\"webhooks\" must be an array of webhook objects.");
		for (const auto &item : v.getArray())
		{
			if (!item.isObject())
				throw ConfigError("each entry of \"webhooks\" must be an object.");
			WebhookSpec hook;
			bool active = true;
			for (const auto &m : item.getObject().getMembers())
			{
				switch (m.Key())
				{
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
						if (!Tracking::isJournalName(e))
						{
							std::string kinds;
							for (const auto &n : Tracking::kindNames())
								if (n.journal)
									kinds += (kinds.empty() ? "" : ", ") + std::string(n.journal);
							throw ConfigError("webhook event \"" + e + "\" is not one of " + kinds + ".");
						}
					break;
				default:
					throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in a webhook.");
				}
			}
			if (!active)
				continue;
			if (hook.url.compare(0, 7, "http://") != 0 && hook.url.compare(0, 8, "https://") != 0)
				throw ConfigError("a webhook needs a \"url\" starting with http:// or https://.");
			site.webhooks.push_back(hook);
		}
	}

	// "mqtt_state": { "url": "mqtt://broker:1883", "topic": "ais-catcher", "discovery": true, "watch": [244123456] }
	void mqttState(const JSON::Value &v, SiteConfig &site)
	{
		if (!v.isObject())
			throw ConfigError("\"mqtt_state\" must be an object.");
		MqttStateSpec spec;
		bool active = true;
		for (const auto &m : v.getObject().getMembers())
		{
			const std::string value = m.Get().to_string();
			switch (m.Key())
			{
			case AIS::KEY_SETTING_ACTIVE: active = Util::Parse::Switch(value); break;
			case AIS::KEY_SETTING_URL: spec.url = value; break;
			case AIS::KEY_SETTING_USERNAME: spec.username = value; break;
			case AIS::KEY_SETTING_PASSWORD: spec.password = value; break;
			case AIS::KEY_SETTING_CLIENT_ID: spec.client_id = value; break;
			case AIS::KEY_SETTING_TOPIC: spec.topic = value; break;
			case AIS::KEY_SETTING_DISCOVERY: spec.discovery = Util::Parse::Switch(value); break;
			case AIS::KEY_SETTING_WATCH:
			{
				std::vector<std::string> list;
				addEach(m.Get(), list);
				for (const auto &s : list)
					spec.watch.push_back((uint32_t)Util::Parse::Integer(s, 1, 999999999));
				break;
			}
			default:
				throw ConfigError("field \"" + AIS::settingName(m.Key()) + "\" is not allowed in \"mqtt_state\".");
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
}
#endif
