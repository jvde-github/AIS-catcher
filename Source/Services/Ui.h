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
#include <vector>

#include "Http.h"
#include "MapTiles.h"
#include "PluginStore.h"
#include "SiteConfig.h"
#include "StationRegistry.h"
#include "Tracker.h"

namespace Services
{
	// The viewer: the files built into the program, as they were baked, and the
	// viewer's own endpoints in the compact shapes its page reads. Those are the
	// viewer's, not the API: undocumented, changed with the viewer. It answers
	// only for what it has, so /api/v1 on the same server gets everything else.
	class Ui : public Service
	{
	public:
		Ui(Tracker &tracker, std::function<StationRegistry()> stations, const ServerSpec &spec,
		   EngineWriter engine = nullptr, std::function<int()> clients = nullptr);

		int scope(const Request &request) const override;
		bool handle(const Request &request, Response &response) override;

	private:
		Tracker &tracker;
		std::function<StationRegistry()> stations;
		bool outlines; // a place's outline is served, not only its marker
		ViewerOptions options;
		// the server's view: the page sees nothing outside it, and only its stations
		ViewFilter view;
		// what the station owner adds to the page, and the map tiles it serves
		PluginStore plugins;
		struct Tiles
		{
			std::shared_ptr<MapTiles> source;
			bool overlay;
		};
		std::vector<Tiles> maps;
		EngineWriter engine;
		std::function<int()> clients; // the server's own connections
		// the station the page draws: the one in focus, or the only one, when it shares where it is
		bool drawnStation(const std::vector<int> &focus, StationRegistry::Station &at);
		void openTiles(const std::vector<std::string> &lists, bool overlay, bool mbtiles);
		bool tile(const std::string &path, Response &response);

		// what the page needs to know before its first request
		std::string pageConfig(bool versioned = true);
		// the page's title: the configured one, else the only station's name
		std::string title();
		std::string configVersion();
		bool viewerData(const Request &request, Response &response);
		std::string statJSON(const std::vector<int> &focus, const DB::Keep &keep);
		// The stations the page looks through, by the `stations` it sends: their
		// numbers, separated by commas; none, or -1, for all. Its ships are those
		// they heard within the time a ship stays on the map.
		std::vector<int> focusOf(const Request &request);
		// whose figures the page reads: the stations in focus, else the server's
		// stations; empty for everything
		std::vector<int> figuresOf(const std::vector<int> &focus) const;
		DB::Keep keepFor(const std::vector<int> &focus);
	};
}
