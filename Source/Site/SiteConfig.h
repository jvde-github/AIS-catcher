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


#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <memory>

#include "Common.h"
#include "Message.h"
#include "TrackingConfig.h"
#include "ViewFilter.h"

class PlaceCatalogue;
class StationRegistry;
namespace JSON
{
	class JSON;
	class Value;
}

// What the map on a server offers, under the names the map pages use
struct ViewerOptions
{
	// frame_ancestors: its CSP's frame-ancestors; any when empty
	std::string station, station_link, webcontrol_http, context = "settings", frame_ancestors, about;
	bool realtime = false, log = false, decoder = false, replay = true, msg = false;
	bool managed = false;
	std::vector<std::string> mbtiles, mboverlay, fstiles, fsoverlay, plugins, styles, plugin_dirs;

	bool operator==(const ViewerOptions &o) const
	{
		return station == o.station && station_link == o.station_link && webcontrol_http == o.webcontrol_http && context == o.context &&
			   frame_ancestors == o.frame_ancestors && about == o.about && realtime == o.realtime && log == o.log &&
			   decoder == o.decoder && replay == o.replay && msg == o.msg &&
			   mbtiles == o.mbtiles && mboverlay == o.mboverlay && fstiles == o.fstiles && fsoverlay == o.fsoverlay &&
			   plugins == o.plugins && styles == o.styles && plugin_dirs == o.plugin_dirs && managed == o.managed;
	}
};

// A key a server accepts, as the configuration has it: never the key itself,
// only its SHA-256, and what it may do
struct ApiKey
{
	enum Scope
	{
		DATA = 1,
		SYSTEM = 2
	};
	std::string name, sha256; // lower-case hex
	int scopes = DATA;

	bool operator==(const ApiKey &o) const { return name == o.name && sha256 == o.sha256 && scopes == o.scopes; }
};

// A tracker of a server's own, behind a filter on the message stream: which
// receivers feed it (zones, or the group mask) and what passes (the terms of
// AIS::Filter, as given); its places are the site's. A server without one
// reads the site's tracker.
struct TrackerSpec
{
	bool own = false;
	std::vector<std::pair<int, std::string>> filter;
	std::vector<std::string> zones;
	uint64_t groups_in = GROUPS_ALL;
	TrackingConfig tracking;
	std::string backup_file;

	bool operator==(const TrackerSpec &o) const
	{
		return own == o.own && filter == o.filter && zones == o.zones && groups_in == o.groups_in && tracking == o.tracking && backup_file == o.backup_file;
	}
	bool operator!=(const TrackerSpec &o) const { return !(*this == o); }
};

struct ServerSpec
{
	int port = 0;
	std::string bind = "127.0.0.1";
	// HTTPS with these files when given: never with plain HTTP on the same port
	std::string tls_cert, tls_key;
	bool ui = true, api = false, mcp = false, system = false, geojson = false, metrics = false, kml = false;
	TrackerSpec tracking;
	ViewFilter view;
	std::vector<int> stations;
	// whether it shows its stations' locations: 1 or 0, and -1 for each station's own share_loc
	int share_loc = -1;
	bool locates(bool station) const { return share_loc < 0 ? station : share_loc > 0; }
	bool showsStation(int id) const { return stations.empty() || std::find(stations.begin(), stations.end(), id) != stations.end(); }
	ViewerOptions viewer;
	// with keys, a request to the api, system or mcp service names one; a
	// public server lets anyone read its data, never its system
	std::vector<ApiKey> keys;
	bool open = false;
	bool limited = false;
	// the map's JSON for scripts: /ships.json, /stat.json, /api/ships.json, /api/ships_full.json
	bool api_v0 = false;
	// managed mode's server beside the control panel: its port and address are the panel's
	bool main = false;

	std::vector<std::string> served() const
	{
		std::vector<std::string> out;
		if (ui) out.push_back("ui");
		if (ui && api_v0) out.push_back("api_v0");
		if (api) out.push_back("api");
		if (mcp) out.push_back("mcp");
		if (system) out.push_back("system");
		if (geojson) out.push_back("geojson");
		if (kml) out.push_back("kml");
		if (metrics) out.push_back("metrics");
		return out;
	}

	bool operator==(const ServerSpec &o) const
	{
		return port == o.port && bind == o.bind && tls_cert == o.tls_cert && tls_key == o.tls_key && ui == o.ui && api == o.api && mcp == o.mcp && system == o.system &&
			   geojson == o.geojson && metrics == o.metrics && kml == o.kml && tracking == o.tracking && view == o.view &&
			   stations == o.stations && viewer == o.viewer && keys == o.keys && open == o.open && limited == o.limited && api_v0 == o.api_v0 && main == o.main &&
			   share_loc == o.share_loc;
	}
	bool operator!=(const ServerSpec &o) const { return !(*this == o); }
};

// The default station where "stations" does not list it, as -N or a "server"
// entry describes it
struct StationDefaults
{
	bool share_loc = false, gps = true;
	uint32_t mmsi = 0;
	std::string name;
	float alt = ALTITUDE_UNDEFINED; // metres, of the antenna
	// where a station without a position of its own stands; none when 0, 0 as the panel writes it
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	bool placed() const { return isValidCoord(lat, lon); }
};

struct WebhookSpec
{
	std::string url, secret;
	std::vector<std::string> events;
	ViewFilter view;
	bool batch = false;

	bool operator==(const WebhookSpec &o) const
	{
		return url == o.url && secret == o.secret && events == o.events && view == o.view && batch == o.batch;
	}
};

// The station's state on an MQTT broker: retained, for Home Assistant
struct MqttStateSpec
{
	std::string url, username, password, client_id, topic = "ais-catcher";
	bool discovery = true;
	std::vector<uint32_t> watch;

	bool any() const { return !url.empty(); }
	bool operator==(const MqttStateSpec &o) const
	{
		return url == o.url && username == o.username && password == o.password && client_id == o.client_id && topic == o.topic &&
			   discovery == o.discovery && watch == o.watch;
	}
};

struct SiteConfig
{
	TrackingConfig tracking;
	bool tracking_set = false;
	std::string backup_file;
	int backup_minutes = 10;
	std::string places;
	// or a catalogue kept elsewhere, as managed mode's editor keeps it
	std::shared_ptr<PlaceCatalogue> catalogue;
	std::vector<ServerSpec> servers;
	std::vector<WebhookSpec> webhooks;
	MqttStateSpec mqtt_state;

	bool wantsTracker() const
	{
		for (const auto &s : servers)
			if (!s.tracking.own)
				return true;
		return !webhooks.empty() || mqtt_state.any();
	}
};

// A mistake in a file is said with "Config file: " in front; what -N and -W
// give is said in their own words.
namespace SiteReader
{
	void stations(const JSON::Value &v, StationRegistry &into);
	// a tracking key whose value is a file or directory, resolved against the configuration's folder
	bool isPath(int key);
	std::string backupFile(const std::string &file, const std::string &base_dir);
#ifdef HASAPI
	// files resolved against base_dir; command_line_servers names, per "server" entry,
	// the -N that gave it ("-N 8100"), empty for a file's
	void site(const JSON::JSON &root, const std::string &base_dir, SiteConfig &site, StationDefaults &defaults, StationRegistry &stations, const std::vector<std::string> *command_line_servers = nullptr);
	// managed mode's site: the main server, beside the control panel, listens on viewer_port
	void managedSite(const JSON::JSON &root, const std::string &base_dir, int control_port, int viewer_port, const std::string &bind, SiteConfig &site, StationDefaults &defaults, StationRegistry &stations);
#endif
#ifdef HASDELIVERY
	// a "webhooks" array and an "mqtt_state" object, their mistakes said as they are
	void webhooks(const JSON::Value &v, SiteConfig &site);
	void mqttState(const JSON::Value &v, SiteConfig &site);
#endif
}
