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
#include <vector>

#include "Receiver.h"
#include "SiteConfig.h"
#include "StationRegistry.h"

class Site;

// The receiving stations of an engine: those the configuration defines, and
// the default station, 0, when a receiver names none and the file does not
// list it. Every receiver stands at one; the site hears of them.
namespace Stations
{
	// Every receiver stands at a station: the one the configuration names, or the
	// default station, 0, made from the defaults when the file does not list it. A
	// station without a position takes its first receiver's with one; a receiver
	// takes its station's. Then the site hears of them.
	void resolve(StationRegistry &stations, const StationDefaults &defaults, const std::vector<std::unique_ptr<Receiver>> &receivers, Site *site);
	// Stations told again while the engine runs: the configured ones as the file
	// now has them; one a receiver still names, or a receiver's own, as it was;
	// then the site hears of them. Which receiver uses which station changes only
	// with a restart.
	void refresh(StationRegistry &stations, const StationRegistry &configured, const std::vector<std::unique_ptr<Receiver>> &receivers, Site *site);
	// a station a receiver names is one the configuration defines; the default station, 0, is there always
	void validate(const StationRegistry &stations, const std::vector<std::unique_ptr<Receiver>> &receivers);
}
