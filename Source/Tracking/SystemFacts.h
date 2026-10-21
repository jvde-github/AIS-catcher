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

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

// A receiver as the running engine last told it, about once a second
struct ReceiverState
{
	int index = 0, station = 0;
	std::string input, label; // the kind of input, and the device or the receiver's own name
	bool streaming = false;	  // the device delivers
	bool lost = false;		  // it stopped by itself
	uint64_t messages = 0;	  // since the session started
	uint64_t per_minute = 0;  // in the last minute

	// in one word: lost, streaming, or starting while its device has yet to deliver
	const char *status() const { return lost ? "lost" : streaming ? "streaming" : "starting"; }
};

// The parts of the running engine a status page writes: all of it, the
// community feed, its outputs, or whether it runs; or its outputs as a list
// of the API, with its envelope
enum EnginePart
{
	ENGINE_ALL,
	ENGINE_SHARING,
	ENGINE_OUTPUTS,
	ENGINE_RUNNING,
	ENGINE_OUTPUT_LIST
};

// What the site knows of itself and of the engine running against it: what
// the system service and the MQTT state tell
struct SystemFacts
{
	bool running = false;
	std::time_t born = 0, session = 0; // when the process started; when the running session did
	uint64_t received = 0;			   // bytes from every input
	std::vector<ReceiverState> receivers;
};
