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

#include <functional>

class Site;

// Where a program built on AIS-catcher attaches its own services to the site.
struct Host
{
	// once the servers serve and the trackers are wired, before the receivers start
	static std::function<void(Site &)> before_run;
	// when the run has ended, while the trackers and servers are still alive: the place to save
	static std::function<void(Site &)> after_run;
};
