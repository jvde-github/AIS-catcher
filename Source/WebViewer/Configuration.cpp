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

#include "WebViewer.h"
#include "Helper.h"

void WebViewer::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;

	plugins += "console.log('plugin:" + arg + "');";
	plugins += "plugins += 'JS: " + arg + "\\n';";
	plugins += "\n\n//=============\n//" + arg + "\n\n";
	try
	{
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		std::shared_ptr<JSON::JSON> j = parser.parse(firstline);

		for (const auto &p : j->getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_VERSION:
				version = p.Get().getInt();
				break;
			case AIS::KEY_SETTING_AUTHOR:
				author = p.Get().getString();
				break;
			case AIS::KEY_SETTING_DESCRIPTION:
				description = p.Get().getString();
				break;
			default:
				throw std::runtime_error("Unknown key in plugin JSON field");
				break;
			}
		}
		Info() << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version;
		if (version != 3)
			throw std::runtime_error("Version not supported, expected 3, got " + std::to_string(version));
		plugin_code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin " + arg + "\", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{
		plugins += "// FAILED\n";
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		plugins += "server_message += \"Plugin error (" + arg + ") " + std::string(e.what()) + "\\n\"\n";
	}
}

Setting &WebViewer::Set(std::string option, std::string arg)
{
	Util::Convert::toUpper(option);

	if (option == "PORT")
	{
		port_set = true;
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535, option);
	}
	else if (option == "SERVER_MODE")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setServerMode(b);
	}
	else if (option == "ZLIB")
	{
		use_zlib = Util::Parse::Switch(arg);
	}
	else if (option == "GROUPS_IN")
	{
		groups_in = Util::Parse::Integer(arg);
	}
	else if (option == "PORT_MIN")
	{
		port_set = true;
		firstport = Util::Parse::Integer(arg, 1, 65535, option);
		lastport = MAX(firstport, lastport);
	}
	else if (option == "PORT_MAX")
	{
		port_set = true;
		lastport = Util::Parse::Integer(arg, 1, 65535, option);
		firstport = MIN(firstport, lastport);
	}
	else if (option == "STATION")
	{
		station = arg;
	}
	else if (option == "STATION_LINK")
	{
		station_link = arg;
	}
	else if (option == "LAT")
	{
		ships.setLat(Util::Parse::Float(arg));
		planes.setLat(Util::Parse::Float(arg));
	}
	else if (option == "CUTOFF")
	{
		int cutoff = Util::Parse::Integer(arg, 0, 10000, option);
		hist_minute.setCutoff(cutoff);
		hist_second.setCutoff(cutoff);
		hist_hour.setCutoff(cutoff);
		hist_day.setCutoff(cutoff);
		dataPrometheus.setCutOff(cutoff);
		counter.setCutOff(cutoff);
		counter_session.setCutOff(cutoff);
	}
	else if (option == "SHARE_LOC")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setShareLatLon(b);
		plugins += "param_share_loc=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "IP_BIND")
	{
		server.setIP(arg);
	}
	else if (option == "CONTEXT")
	{
		plugins += "context='" + arg + "';\n";
	}
	else if (option == "MESSAGE" || option == "MSG")
	{
		bool b = Util::Parse::Switch(arg);
		ships.setMsgSave(b);
		plugins += "message_save=" + (b ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "LON")
	{
		ships.setLon(Util::Parse::Float(arg));
		planes.setLon(Util::Parse::Float(arg));
	}
	else if (option == "USE_GPS")
	{
		ships.setUseGPS(Util::Parse::Switch(arg));
	}
	else if (option == "KML")
	{
		KML = Util::Parse::Switch(arg);
	}
	else if (option == "GEOJSON")
	{
		GeoJSON = Util::Parse::Switch(arg);
	}
	else if (option == "OWN_MMSI")
	{
		ships.setOwnMMSI(Util::Parse::Integer(arg, 0, 999999999, option));
	}
	else if (option == "HISTORY")
	{
		ships.setTimeHistory(Util::Parse::Integer(arg, 5, 12 * 3600, option));
	}
	else if (option == "FILE")
	{
		backup_filename = arg;
	}
	else if (option == "CDN")
	{
		cdn = arg;
		Info() << "Fetch Web Libraries locally at " << arg;
	}
	else if (option == "MBTILES")
	{
		addMBTilesSource(arg, false);
	}
	else if (option == "MBOVERLAY")
	{
		addMBTilesSource(arg, true);
	}
	else if (option == "FSTILES")
	{
		addFileSystemTilesSource(arg, false);
	}
	else if (option == "FSOVERLAY")
	{
		addFileSystemTilesSource(arg, true);
	}
	else if (option == "BACKUP")
	{
		backup_interval = Util::Parse::Integer(arg, 5, 2 * 24 * 60, option);
	}
	else if (option == "REALTIME")
	{
		realtime = Util::Parse::Switch(arg);
		if (realtime)
			plugins += "realtime_enabled = true;\n";
		else
			plugins += "realtime_enabled = false;\n";
	}
	else if (option == "LOG")
	{
		showlog = Util::Parse::Switch(arg);
		if (showlog)
			plugins += "log_enabled = true;\n";
		else
			plugins += "log_enabled = false;\n";
	}
	else if (option == "DECODER")
	{
		showdecoder = Util::Parse::Switch(arg);
		if (showdecoder)
			plugins += "decoder_enabled = true;\n";
		else
			plugins += "decoder_enabled = false;\n";

		sse_streamer.setObfuscate(!showdecoder);
	}
	else if (option == "PLUGIN")
	{
		addPlugin(arg);
	}
	else if (option == "STYLE")
	{
		Info() << "Server: adding plugin (CSS): " << arg;
		plugins += "console.log('css:" + arg + "');";
		plugins += "plugins += 'CSS: " + arg + "\\n';";

		stylesheets += "/* ================ */\n";
		stylesheets += "/* CSS plugin: " + arg + "*/\n";
		try
		{
			stylesheets += Util::Helper::readFile(arg) + "\n";
		}
		catch (const std::exception &e)
		{
			stylesheets += "/* FAILED */\r\n";
			Warning() << "Server: style plugin error - " << e.what();
			plugins += "server_message += \"Plugin error: " + std::string(e.what()) + "\\n\"\n";
		}
	}
	else if (option == "PLUGIN_DIR")
	{
		const std::vector<std::string> &files_js = Util::Helper::getFilesWithExtension(arg, ".pjs");
		for (auto f : files_js)
			Set("PLUGIN", f);

		const std::vector<std::string> &files_ss = Util::Helper::getFilesWithExtension(arg, ".pss");
		for (auto f : files_ss)
			Set("STYLE", f);

		if (!files_ss.size() && !files_js.size())
			Info() << "Server: no plugin files found in directory.";
	}
	else if (option == "ABOUT")
	{
		Info() << "Server: about context from " << arg;
		about = Util::Helper::readFile(arg);
		aboutPresent = true;
	}
	else if (option == "PROME")
	{
		supportPrometheus = Util::Parse::Switch(arg);
	}
	else if (option == "REUSE_PORT")
	{
		server.setReusePort(Util::Parse::Switch(arg));
	}
	else if (!filter.SetOption(option, arg))
	{
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);
	}
	else
	{
		ships.setFilterOption(option, arg);
		raw_counter.setFilterOption(option, arg);
	}

	return *this;
}
