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

namespace IO
{
	class OutputMessage;
}

// What the running engine is made of, told to the site at its start and
// withdrawn at its stop, before its outputs go
struct DeviceFacts
{
	// model: the device's decoders, one a line; label: what a status page calls it
	std::string product, vendor, serial, model, sample_rate, label;
};
struct EngineFacts
{
	std::vector<DeviceFacts> devices;
	std::vector<IO::OutputMessage *> outputs;
	bool sharing = false, sharing_uuid = false;
};

struct ReceiverState
{
	int index = 0, station = 0;
	std::string input, label;
	bool streaming = false;
	bool lost = false;
	uint64_t messages = 0;
	uint64_t per_minute = 0;
	uint64_t bytes = 0;
	// for an input that dials a remote
	bool connection = false;
	uint64_t connect_ok = 0, connect_fail = 0, reconnects = 0, connected = 0;

	const char *status() const { return lost ? "lost" : streaming ? "streaming" : "starting"; }
};

// The parts of the running engine a status page writes; ENGINE_OUTPUT_LIST is
// its outputs as a list of the API, with its envelope
enum EnginePart
{
	ENGINE_ALL,
	ENGINE_SHARING,
	ENGINE_OUTPUTS,
	ENGINE_RUNNING,
	ENGINE_OUTPUT_LIST
};

// a station whose receivers stream and that has heard nothing for this long is
// quiet: the health warns, and the journal says so once
const int STATION_QUIET_S = 900;

struct OutputFacts
{
	std::string type, label;
	bool connection = false;
	uint64_t messages = 0, bytes_out = 0, bytes_in = 0, connect_ok = 0, connect_fail = 0, reconnects = 0, connected = 0, dropped = 0;
};

struct SystemFacts
{
	bool running = false;
	std::time_t born = 0, session = 0;
	uint64_t received = 0;
	std::vector<ReceiverState> receivers;
	std::vector<OutputFacts> outputs;
	// input blocks dropped because decoding fell behind
	uint32_t overruns = 0;
};
