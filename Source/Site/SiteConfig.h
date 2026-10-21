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

#include "Message.h"
#include "TrackingConfig.h"
#include "ViewFilter.h"

class PlaceCatalogue;

// What the map on a server offers, under the names of the web viewer's own
// settings, so the keys mean the same in both
struct ViewerOptions
{
	// frame_ancestors: the pages that may frame the server's, its CSP's frame-ancestors; any when empty
	std::string station, station_link, webcontrol_http, context = "settings", frame_ancestors, about;
	bool realtime = false, log = false, decoder = false, replay = true, msg = false, geojson = false, kml = false, prome = false;
	bool managed = false; // the viewer of managed mode, beside its control panel
	std::vector<std::string> mbtiles, mboverlay, fstiles, fsoverlay, plugins, styles, plugin_dirs;

	bool operator==(const ViewerOptions &o) const
	{
		return station == o.station && station_link == o.station_link && webcontrol_http == o.webcontrol_http && context == o.context &&
			   frame_ancestors == o.frame_ancestors && about == o.about && realtime == o.realtime && log == o.log &&
			   decoder == o.decoder && replay == o.replay && msg == o.msg && geojson == o.geojson && kml == o.kml && prome == o.prome &&
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
		DATA = 1,	// ships, places, events, stations: the api and mcp services
		SYSTEM = 2	// receivers, outputs, health, the log: the system service
	};
	std::string name, sha256; // lower-case hex
	int scopes = DATA;

	bool operator==(const ApiKey &o) const { return name == o.name && sha256 == o.sha256 && scopes == o.scopes; }
};

// One server of the site: a port, an address, the services it offers and
// the ships it shows of all the site keeps.
struct ServerSpec
{
	int port = 0;
	std::string bind = "127.0.0.1";
	std::vector<std::string> serve;
	ViewFilter filter;
	bool place_geometry = true; // serve the outlines of places, not only their markers
	ViewerOptions viewer;
	// with keys, a request to the api, system or mcp service names one; a
	// public server lets anyone read its data, never its system
	std::vector<ApiKey> keys;
	bool open = false;
	// managed mode's server beside the control panel: its port and address are the panel's
	bool main = false;

	bool operator==(const ServerSpec &o) const
	{
		return port == o.port && bind == o.bind && serve == o.serve && filter == o.filter && place_geometry == o.place_geometry &&
			   viewer == o.viewer && keys == o.keys && open == o.open && main == o.main;
	}
	bool operator!=(const ServerSpec &o) const { return !(*this == o); }
};

// an address only this machine reaches
inline bool localBind(const std::string &bind)
{
	return bind == "localhost" || bind == "::1" || bind.compare(0, 4, "127.") == 0;
}

// what others could read beyond this machine is guarded, or said to be open
inline void checkServerAccess(const ServerSpec &spec, const std::string &where)
{
	const auto offers = [&](const char *name) {
		return std::find(spec.serve.begin(), spec.serve.end(), name) != spec.serve.end();
	};
	if (!localBind(spec.bind) && spec.keys.empty() && !spec.open && (offers("api") || offers("mcp") || offers("system")))
		throw std::runtime_error(where + " listens beyond this machine: give it \"keys\", or say \"public\": true for data anyone may read.");
	if (offers("system") && spec.open && spec.keys.empty() && !localBind(spec.bind))
		throw std::runtime_error(where + " is public: its \"system\" service needs \"keys\".");
}

// What managed mode's configuration says of the default station, from the
// main server's title or a viewer section from before, for when "stations"
// does not list it
struct StationDefaults
{
	bool share_loc = false, gps = true;
	uint32_t mmsi = 0;
	// the station's name, for the one station a single-station setup has
	std::string name;
	float alt = ALTITUDE_UNDEFINED; // metres, of the antenna
	// where a station without a position of its own stands; none when 0, 0 as the panel writes it
	float lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
	bool placed() const { return isValidCoord(lat, lon) && !(lat == 0 && lon == 0); }
};

// One webhook: where the events go, which of them, signed with what
struct WebhookSpec
{
	std::string url, secret;
	std::vector<std::string> events; // the kinds; every kind when empty
	ViewFilter filter;				 // a ship's event only when the ship passes
	bool batch = false;

	bool operator==(const WebhookSpec &o) const
	{
		return url == o.url && secret == o.secret && events == o.events && filter == o.filter && batch == o.batch;
	}
};

// The station's state on an MQTT broker: retained, for Home Assistant
struct MqttStateSpec
{
	std::string url, username, password, client_id, topic = "ais-catcher";
	bool discovery = true;
	std::vector<uint32_t> watch; // vessels whose position is published

	bool any() const { return !url.empty(); }
	bool operator==(const MqttStateSpec &o) const
	{
		return url == o.url && username == o.username && password == o.password && client_id == o.client_id && topic == o.topic &&
			   discovery == o.discovery && watch == o.watch;
	}
	bool operator!=(const MqttStateSpec &o) const { return !(*this == o); }
};

// What the configuration says about the site: how ships are kept and which
// servers read them. A site with no server keeps no ships.
struct SiteConfig
{
	TrackingConfig tracking;
	bool tracking_set = false; // the configuration's `tracking` says how; a viewer's settings do not then
	std::string backup_file;
	int backup_minutes = 10;
	std::string places; // a directory of place files
	// or a catalogue kept elsewhere, as managed mode's editor keeps it
	std::shared_ptr<PlaceCatalogue> catalogue;
	std::vector<ServerSpec> servers;
	std::vector<WebhookSpec> webhooks;
	MqttStateSpec mqtt_state;

	// a tracker is kept for whatever reads it: a server, a webhook, the MQTT state
	bool wantsTracker() const { return !servers.empty() || !webhooks.empty() || mqtt_state.any(); }
};
