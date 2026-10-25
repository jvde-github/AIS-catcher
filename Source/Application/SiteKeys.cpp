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

#include "SiteKeys.h"

#include <stdexcept>

#include "Helper.h"
#include "Keys.h"
#include "Parse.h"

namespace
{
	const char *name(int key) { return AIS::KeyMap[key][JSON_DICT_SETTING].p; }

	void each(const std::string &list, std::vector<std::string> &out)
	{
		std::vector<std::string> parts;
		Util::Parse::Split(list, ',', parts);
		for (const auto &p : parts)
			if (!p.empty())
				out.push_back(p);
	}

	int integer(int key, const std::string &arg, long min, long max)
	{
		return (int)Util::Parse::Integer(arg, min, max, name(key));
	}
}

namespace SiteKeys
{
	bool viewer(int key, const std::string &arg, ViewerOptions &o)
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
		case AIS::KEY_SETTING_MBTILES: each(arg, o.mbtiles); break;
		case AIS::KEY_SETTING_MBOVERLAY: each(arg, o.mboverlay); break;
		case AIS::KEY_SETTING_FSTILES: each(arg, o.fstiles); break;
		case AIS::KEY_SETTING_FSOVERLAY: each(arg, o.fsoverlay); break;
		case AIS::KEY_SETTING_PLUGIN: each(arg, o.plugins); break;
		case AIS::KEY_SETTING_STYLE: each(arg, o.styles); break;
		case AIS::KEY_SETTING_PLUGIN_DIR: each(arg, o.plugin_dirs); break;
		default: return false;
		}
		return true;
	}

	bool tracking(int key, const std::string &arg, TrackingTarget target, const std::string &base_dir)
	{
		TrackingConfig &t = target.tracking;
		switch (key)
		{
		case AIS::KEY_SETTING_FILE:
			if (!target.backup_file)
				return false;
			*target.backup_file = Util::Helper::resolvePath(arg, base_dir);
			break;
		case AIS::KEY_SETTING_PLACES:
			if (!target.places)
				return false;
			*target.places = Util::Helper::resolvePath(arg, base_dir);
			break;
		case AIS::KEY_SETTING_BACKUP:
			if (!target.backup_minutes)
				return false;
			*target.backup_minutes = integer(key, arg, 0, 2 * 24 * 60);
			if (*target.backup_minutes > 0 && *target.backup_minutes < 5)
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

	bool isTracking(int key)
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

	bool station(int key, const std::string &arg, StationDefaults &d)
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
}
