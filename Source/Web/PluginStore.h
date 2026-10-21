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

#include <string>
#include <utility>
#include <vector>

// The JS and CSS the station owner adds on top of the built-in viewer, plus the
// "about" text. Loading is best-effort: a broken plugin is reported to the
// frontend through the manifest rather than taking the viewer down.
class PluginStore {
  std::string code;
  mutable std::string revision;
  std::string stylesheets;
  std::string about = "This content can be set by the station owner";
  bool about_present = false;

  // version 0 means "no declared version" (CSS plugins).
  std::vector<std::pair<std::string, int>> loaded_list;
  std::vector<std::string> error_list;
  std::vector<std::string> dir_list;

public:
  PluginStore() : code("\n\nfunction loadPlugins() {\n") {}

  void addPlugin(const std::string &path);
  void addStyle(const std::string &path);
  void addDir(const std::string &dir);
  // generated rather than read from disk (map tile sources)
  void setAbout(const std::string &path);

  // the function loadPlugins that runs them all, without its closing brace
  const std::string &getCode() const { return code; }
  // the same, closed: the plugins as one script
  std::string script() const { return code + "}\n"; }
  const std::string &version() const;
  const std::string &getStylesheets() const { return stylesheets; }
  const std::string &getAbout() const { return about; }
  bool isAboutPresent() const { return about_present; }

  const std::vector<std::pair<std::string, int>> &loaded() const {
    return loaded_list;
  }
  const std::vector<std::string> &errors() const { return error_list; }
  const std::vector<std::string> &dirs() const { return dir_list; }
};
