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

#include "ViewerConfiguration.h"
#include "Helper.h"
#include "Logger.h"
#include "SHA256.h"
#include <cctype>

// a setting holds one path or several separated by commas (a JSON array
// arrives joined that way); an empty entry, a blank field in a config, is no source
static std::vector<std::string> tilePaths(const std::string &list)
{
	std::vector<std::string> paths;
	std::size_t start = 0;
	while (start <= list.size())
	{
		std::size_t end = list.find(',', start);
		if (end == std::string::npos)
			end = list.size();
		std::size_t a = start, b = end;
		while (a < b && std::isspace((unsigned char)list[a]))
			a++;
		while (b > a && std::isspace((unsigned char)list[b - 1]))
			b--;
		if (b > a)
			paths.push_back(list.substr(a, b - a));
		start = end + 1;
	}
	return paths;
}

void ViewerConfiguration::addTileSources(const std::string &paths, bool overlay, bool mbtiles)
{
	const std::string key = std::string(overlay ? "overlay|" : "base|") + (mbtiles ? "mbtiles|" : "files|") + paths;
	auto reuse = previousTiles.find(key);
	if (reuse == previousTiles.end())
	{
		auto &opened = previousTiles[key];
		for (const std::string &path : tilePaths(paths))
		{
			std::shared_ptr<MapTiles> source;
#if HASSQLITE
			if (mbtiles) source = std::make_shared<MBTilesSupport>();
#endif
			if (!mbtiles) source = std::make_shared<FileSystemTiles>();
			if (!source) continue;
			if (source->open(path))
			{
				source->setLayerID(Util::SHA256::hex(path + (overlay ? "|overlay" : "|base")).substr(0, 16));
				opened.push_back(source);
			}
			else
				Error() << "Failed to load " << (mbtiles ? "MBTiles" : "FileSystemTiles") << " from: " << path;
		}
		reuse = previousTiles.find(key);
	}
	for (const auto &source : reuse->second)
	{
		mapSources.push_back(source);
		frontend.maps.push_back(source->json(overlay));
	}
	tiles[key] = reuse->second;
}

Setting &ViewerConfiguration::SetKey(AIS::Keys key, const std::string &arg)
{

	switch (key)
	{
	case AIS::KEY_SETTING_PLACES:
		values.places = arg;
		break;
	case AIS::KEY_SETTING_PORT:
		values.port_set = true;
		values.port = Util::Parse::Integer(arg, 1, 65535);
		break;
	case AIS::KEY_SETTING_SERVER_MODE:
		values.tracking.server_mode = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_ZLIB:
		values.use_zlib = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_GROUPS_IN:
		// a zone filter, if there is one, overrides this in attachEngine()
		values.groups_in = Util::Parse::Integer(arg);
		break;
	case AIS::KEY_SETTING_ZONE:
		Util::Parse::Split(arg, ',', values.zones);
		break;
	case AIS::KEY_SETTING_PORT_MIN:
	case AIS::KEY_SETTING_PORT_MAX:
	{
		static bool warned = false;
		if (!warned)
		{
			Warning() << "Webviewer: 'port_min'/'port_max' are deprecated, use 'port' instead";
			warned = true;
		}
		if (!values.port_set)
		{
			values.port_set = true;
			values.port = Util::Parse::Integer(arg, 1, 65535);
		}
		break;
	}
	case AIS::KEY_SETTING_STATION:
		values.station = arg;
		break;
	case AIS::KEY_SETTING_STATS_ON_CLOSE:
	{
		static bool warned = false;
		if (!warned)
		{
			Warning() << "Webviewer: 'stats_on_close' is deprecated and ignored";
			warned = true;
		}
		break;
	}
	case AIS::KEY_SETTING_STATION_LINK:
		values.station_link = arg;
		break;
	case AIS::KEY_SETTING_WEBCONTROL_HTTP:
		frontend.setWebControl(arg);
		break;
	case AIS::KEY_SETTING_FRAME_ANCESTORS:
		values.frame_ancestors = arg;
		break;
	case AIS::KEY_SETTING_LAT:
		values.tracking.lat = Util::Parse::Float(arg);
		break;
	case AIS::KEY_SETTING_CUTOFF:
		values.tracking.cutoff = Util::Parse::Integer(arg, 0, 10000);
		break;
	case AIS::KEY_SETTING_TRACK_MEMORY:
		values.tracking.track_memory = Util::Parse::Integer(arg, 16, 256 * 1024);
		break;
	case AIS::KEY_SETTING_MAX_SHIPS:
		values.tracking.max_ships = Util::Parse::Integer(arg, 1024, 4 * 1024 * 1024);
		break;
	case AIS::KEY_SETTING_REPLAY:
		values.replay = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_SPLIT:
		values.split = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_TRACK_TIME:
	case AIS::KEY_SETTING_REPLAY_TIME:
		values.tracking.track_time = Util::Parse::Integer(arg, 0, 7 * 24 * 3600);
		break;
	case AIS::KEY_SETTING_EXPIRE:
		values.tracking.expire_fields = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_SHARE_LOC:
		values.tracking.latlon_share = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_IP_BIND:
		values.bind_address = arg;
		break;
	case AIS::KEY_SETTING_CONTEXT:
		frontend.setContext(arg);
		break;
	case AIS::KEY_SETTING_MSGS:
		break;
	case AIS::KEY_SETTING_MESSAGE:
	case AIS::KEY_SETTING_MSG:
		values.tracking.msg_save = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_LON:
		values.tracking.lon = Util::Parse::Float(arg);
		break;
	case AIS::KEY_SETTING_USE_GPS:
		values.tracking.use_gps = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_KML:
		values.KML = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_GEOJSON:
		values.GeoJSON = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_OWN_MMSI:
		values.tracking.own_mmsi = Util::Parse::Integer(arg, 0, 999999999);
		break;
	case AIS::KEY_SETTING_HISTORY:
		values.tracking.time_history = Util::Parse::Integer(arg, 5, 12 * 3600);
		break;
	case AIS::KEY_SETTING_FILE:
		values.backup_file = arg; // resolved against the config directory when applied
		break;
	case AIS::KEY_SETTING_CDN:
		Warning() << "CDN option is no longer supported — web libraries are now bundled. Ignoring.";
		break;
	case AIS::KEY_SETTING_MBTILES:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_MBOVERLAY:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_FSTILES:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_FSOVERLAY:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_BACKUP:
		{
		const int minutes = Util::Parse::Integer(arg, 0, 2 * 24 * 60);
		if (minutes > 0 && minutes < 5) throw std::runtime_error("Backup interval must be 0 (disabled) or at least 5 minutes");
		values.backup_interval = minutes;
		}
		break;
	case AIS::KEY_SETTING_REALTIME:
		values.realtime = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_LOG:
		values.showlog = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_DECODER:
		values.showdecoder = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_PLUGIN:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_STYLE:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_PLUGIN_DIR:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_ABOUT:
		resources.emplace_back(key, arg); prepared = false;
		break;
	case AIS::KEY_SETTING_PROME:
		values.supportPrometheus = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_REUSE_PORT:
		values.reuse_port = Util::Parse::Switch(arg);
		break;
	default:
		if (!filter.SetOptionKey(key, arg))
			throw std::runtime_error(std::string("unrecognized setting for HTML service: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
		break;
	}

	return *this;
}


void ViewerConfiguration::prepare()
{
	if (prepared) return;
	mapSources.clear(); frontend.maps.clear(); plugins = PluginStore();
	for (const auto &resource : resources) {
		const auto &arg = resource.second;
		switch (resource.first) {
		case AIS::KEY_SETTING_MBTILES: addTileSources(arg, false, true); break;
		case AIS::KEY_SETTING_MBOVERLAY: addTileSources(arg, true, true); break;
		case AIS::KEY_SETTING_FSTILES: addTileSources(arg, false, false); break;
		case AIS::KEY_SETTING_FSOVERLAY: addTileSources(arg, true, false); break;
		case AIS::KEY_SETTING_PLUGIN: plugins.addPlugin(arg); break;
		case AIS::KEY_SETTING_STYLE: plugins.addStyle(arg); break;
		case AIS::KEY_SETTING_PLUGIN_DIR: plugins.addDir(arg); break;
		case AIS::KEY_SETTING_ABOUT: plugins.setAbout(arg); break;
		default: break;
		}
	}
	prepared = true;
}
