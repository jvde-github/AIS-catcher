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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AIS-catcher.h"

#include "ReceiverTracker.h"

// The JS and CSS the station owner adds on top of the built-in viewer, plus the
// "about" text. Loading is best-effort: a broken plugin is reported to the
// frontend through the manifest rather than taking the viewer down.
class PluginStore
{
	std::string code;
	std::string stylesheets;
	std::string about = "This content can be set by the station owner";
	bool about_present = false;

	// version 0 means "no declared version" (CSS plugins).
	std::vector<std::pair<std::string, int>> loaded_list;
	std::vector<std::string> error_list;

public:
	PluginStore() : code("\n\nfunction loadPlugins() {\n") {}

	void addPlugin(const std::string &path);
	void addStyle(const std::string &path);
	void addDir(const std::string &dir);
	// generated rather than read from disk (map tile sources)
	void addCode(const std::string &js) { code += js; }
	void setAbout(const std::string &path);

	void reset();

	// the function body render() emits, closing brace not included
	const std::string &getCode() const { return code; }
	const std::string &getStylesheets() const { return stylesheets; }
	const std::string &getAbout() const { return about; }
	bool isAboutPresent() const { return about_present; }

	const std::vector<std::pair<std::string, int>> &loaded() const { return loaded_list; }
	const std::vector<std::string> &errors() const { return error_list; }
};

// Everything the viewer page needs at startup, emitted as a single JSON blob the
// frontend reads via window.__SERVER_CONFIG__.
class FrontendConfig
{
	std::string build_version;
	std::string build_describe;
	std::string context = "settings";
	std::string station;
	std::string webcontrol_http;
	bool share_location = false;
	bool save_messages = false;
	bool replay = true;
	bool realtime = false;
	bool log_enabled = false;
	bool decoder = false;
	bool managed = false;
	bool sharing = false;
	bool sharing_uuid = false;
	std::vector<std::pair<int, std::string>> receivers;

public:
	FrontendConfig();

	void setContext(const std::string &ctx) { context = ctx; }
	void setWebControl(const std::string &url) { webcontrol_http = url; }
	void setShareLoc(bool b) { share_location = b; }
	void setMsgSave(bool b) { save_messages = b; }
	void setReplay(bool b) { replay = b; }
	void setRealtime(bool b) { realtime = b; }
	void setLog(bool b) { log_enabled = b; }
	void setDecoder(bool b) { decoder = b; }
	void setManaged(bool b) { managed = b; }
	void setSharing(bool s, bool uuid)
	{
		sharing = s;
		sharing_uuid = uuid;
	}
	void setStation(const std::string &name);
	void setReceivers(const std::vector<std::unique_ptr<ReceiverTracker>> &states);

	// back to defaults, keeping what is not a setting: the build version, and the
	// managed flag set once when the viewer is created. Sharing and the receiver
	// list are recomputed by applySettings()/startServing().
	void reset();

	// the config blob followed by the plugin function body
	std::string render(const PluginStore &plugins) const;
};
