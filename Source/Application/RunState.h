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
#include "WebViewer.h"
#include "Screen.h"

struct RunState {
	// Receivers and outputs
	std::vector<std::unique_ptr<Receiver>> receivers;
	std::vector<std::unique_ptr<WebViewer>> servers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;

	// Screen and statistics
	IO::ScreenOutput screen;
	std::vector<OutputStatistics> stat;
	std::vector<int> msg_count;

	// Configuration flags set during arg parsing / config reading
	int own_mmsi = -1;
	int timeout = 0;
	bool timeout_nomsg = false;
	bool xshare_defined = false;
	bool verbose = false;

	// One-shot flags
	bool no_run = false;
	bool show_copyright = true;
	bool list_devices = false;
	bool list_devices_JSON = false;
	bool list_support = false;
	bool list_options = false;

	// Run-time state
	int exit_code = 0;
	int nrec = 0;

	RunState() {
		receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
	}

	void reset() {
		*this = RunState();
	}
};
