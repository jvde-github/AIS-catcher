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

#include "SiteConfig.h"

// The keys a viewer section (-N, "server", managed mode's old viewer section),
// a server under "services" and the "tracking" section share: each read here
// once, with one range wherever it is written. A value comes as text, a list
// as text separated by commas. Each returns false for a key that is not its own.
namespace SiteKeys
{
	// the options of a server's map pages
	bool viewer(int key, const std::string &arg, ViewerOptions &o);
	// how the site keeps ships: the tracking, the backup file and its interval,
	// the places; a file or directory resolves against base_dir
	bool tracking(int key, const std::string &arg, SiteConfig &site, const std::string &base_dir);
	// what a viewer section says of the default station
	bool station(int key, const std::string &arg, StationDefaults &d);
}
