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

#include "Screen.h"

// Forward declarations
class Receiver;
class WebViewer;
class OutputStatistics;

namespace IO
{
	class OutputMessage;
	class OutputJSON;
}

struct ApplicationState
{
	std::vector<std::unique_ptr<Receiver>> receivers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;
	std::vector<std::unique_ptr<IO::OutputJSON>> json;
	std::vector<std::unique_ptr<WebViewer>> servers;
	std::vector<OutputStatistics> stat;
	std::vector<int> msg_count;
	
	IO::ScreenOutput screen;
	IO::OutputMessage *community_feed = nullptr;
	
	int nrec = 0;
	int timeout = 0;
	int own_mmsi = -1;
	int cb = -1;
	int exit_code = 0;
	
	bool list_devices = false;
	bool list_support = false;
	bool list_options = false;
	bool list_devices_JSON = false;
	bool no_run = false;
	bool show_copyright = true;
	bool timeout_nomsg = false;
	
	ApplicationState();
	~ApplicationState();
};
