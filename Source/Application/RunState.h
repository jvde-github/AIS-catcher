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
#ifdef HASWEBVIEWER
#include "WebViewer.h"
#endif
#include "Screen.h"

struct RunState {
	// Receivers and outputs
	std::vector<std::unique_ptr<Receiver>> receivers;
#ifdef HASWEBVIEWER
	std::vector<std::unique_ptr<WebViewer>> servers;
#endif
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;

	// Community feed output, owned by msg; set up at most once
	IO::OutputMessage *comm_feed = nullptr;

	// Screen and statistics
	IO::ScreenOutput screen;
	std::vector<OutputStatistics> stat;
	std::vector<uint64_t> msg_count;

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

	// Each device option on the command line / in config defines a new receiver,
	// except the first which reuses the default-constructed one.
	Receiver &newReceiver() {
		if (++nrec > 1)
			receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
		return *receivers.back();
	}

	IO::OutputMessage &createCommunityFeed() {
		msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
		comm_feed = msg.back().get();
		comm_feed->SetKey(AIS::KEY_SETTING_HOST, AISCATCHER_URL)
			.SetKey(AIS::KEY_SETTING_PORT, AISCATCHER_PORT)
			.SetKey(AIS::KEY_SETTING_DESCRIPTION, "Community Feed")
			.SetKey(AIS::KEY_SETTING_MSGFORMAT, "COMMUNITY_HUB")
			.SetKey(AIS::KEY_SETTING_FILTER, "on")
			.SetKey(AIS::KEY_SETTING_GPS, "off")
			.SetKey(AIS::KEY_SETTING_REMOVE_EMPTY, "on")
			.SetKey(AIS::KEY_SETTING_KEEP_ALIVE, "on")
			.SetKey(AIS::KEY_SETTING_OWN_INTERVAL, "10")
			.SetKey(AIS::KEY_SETTING_INCLUDE_SAMPLE_START, "on");
		return *comm_feed;
	}
};
