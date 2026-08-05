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

#include "FrontendConfig.h"

#include "Helper.h"
#include "JSON/Parser.h"
#include "Logger.h"
#include "Writer.h"

// --- PluginStore ---

void PluginStore::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;

	code += "\n//=============\n//" + arg + "\n\n";
	try
	{
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		JSON::Document j = parser.parse(firstline);

		for (const auto &p : j.getMembers())
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
			case AIS::KEY_SETTING_DESC:
				description = p.Get().getString();
				break;
			default:
				throw std::runtime_error("Unknown key in plugin JSON field");
				break;
			}
		}
		Info() << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version;
		// v3: pre-CSP (inline-string handlers, no longer run under strict CSP).
		// v4: function-callback contract for addShipcardItem etc.
		// v5: ES-module script.js — overrides go through AISCatcher hooks.
		if (version < 3 || version > 5)
			throw std::runtime_error("Version not supported, expected 3..5, got " + std::to_string(version));
		if (version == 3)
			Warning() << "Plugin \"" << arg << "\" declares version 3 (pre-CSP). It may render but inline-string handlers will not run under strict CSP. Consider updating to version 5.";
		loaded_list.emplace_back(arg, version);
		std::string safe_arg = JSON::Writer::escape(arg);
		code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin \" + " + safe_arg + ", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		error_list.push_back(arg + ": " + e.what());
	}
}

void PluginStore::addStyle(const std::string &arg)
{
	Info() << "Server: adding plugin (CSS): " << arg;
	loaded_list.emplace_back("CSS: " + arg, 0);

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
		error_list.push_back(arg + ": " + e.what());
	}
}

void PluginStore::addDir(const std::string &dir)
{
	const std::vector<std::string> &files_js = Util::Helper::getFilesWithExtension(dir, ".pjs");
	for (const auto &f : files_js)
		addPlugin(f);

	const std::vector<std::string> &files_ss = Util::Helper::getFilesWithExtension(dir, ".pss");
	for (const auto &f : files_ss)
		addStyle(f);

	if (files_ss.empty() && files_js.empty())
		Info() << "Server: no plugin files found in directory.";
}

void PluginStore::setAbout(const std::string &path)
{
	Info() << "Server: about context from " << path;
	about = Util::Helper::readFile(path);
	about_present = true;
}

void PluginStore::reset()
{
	code = "\n\nfunction loadPlugins() {\n";
	stylesheets.clear();
	about = "This content can be set by the station owner";
	about_present = false;
	loaded_list.clear();
	error_list.clear();
}

// --- FrontendConfig ---

FrontendConfig::FrontendConfig()
{
	build_version = VERSION;
	build_describe = VERSION_DESCRIBE;
}

void FrontendConfig::reset()
{
	const std::string version = build_version, describe = build_describe;
	const bool was_managed = managed;

	*this = FrontendConfig();

	build_version = version;
	build_describe = describe;
	managed = was_managed;
}

void FrontendConfig::setStation(const std::string &name)
{
	station = name;
}

void FrontendConfig::setReceivers(const std::vector<std::unique_ptr<ReceiverTracker>> &states)
{
	receivers.clear();
	if (states.size() > 1)
		for (int i = 0; i < (int)states.size(); i++)
			receivers.emplace_back(i, states[i]->label);
}

std::string FrontendConfig::render(const PluginStore &plugins) const
{
	std::string out;

	// 1. Emit the structured config blob. Single source of truth for all
	//    server-side state the frontend needs at startup.
	out += "window.__SERVER_CONFIG__ = ";
	{
		JSON::Writer w(out);
		w.beginObject()
			.key("build")
			.beginObject()
			.kv("version", build_version)
			.kv("describe", build_describe)
			.endObject()
			.kv("context", context)
			.kv("station", station)
			.kv("webcontrol_http", webcontrol_http)
			.key("features")
			.beginObject()
			.kv("share_location", share_location)
			.kv("save_messages", save_messages)
			.kv("replay", replay)
			.kv("realtime", realtime)
			.kv("log", log_enabled)
			.kv("decoder", decoder)
			.kv("managed", managed)
			.kv("about_md", plugins.isAboutPresent())
			.kv("sharing", sharing)
			.kv("sharing_uuid", sharing_uuid)
			.endObject()
			.key("receivers")
			.beginArray();
		for (const auto &r : receivers)
			w.beginObject().kv("idx", r.first).kv("label", r.second).endObject();
		w.endArray()
			.key("plugins")
			.beginObject()
			.key("loaded")
			.beginArray();
		for (const auto &p : plugins.loaded())
			w.beginObject().kv("name", p.first).kv("version", p.second).endObject();
		w.endArray().key("errors").beginArray();
		for (const auto &e : plugins.errors())
			w.val(e);
		w.endArray().endObject().endObject();
		w.finish();
	}
	out += ";\n";

	// 2. Plugin function body and trailing
	out += plugins.getCode() + "}\n";

	return out;
}

