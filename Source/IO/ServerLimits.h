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

#include <cstddef>

namespace IO
{
	// Every number that decides when a caller of a server waits, is refused or is cut off.
	namespace Limits
	{
		// connections a listener holds at once, the last RESERVED_SLOTS of them for this machine alone
		const int CONNECTIONS = 128;
		const int RESERVED_SLOTS = 16;
		// connections one other machine may hold, on a plain port and on an HTTP port
		const int PER_ADDRESS = 64;
		const int PER_ADDRESS_HTTP = 32;
		// seconds a TLS handshake, and a request, have to finish before the slot is reclaimed
		const int HANDSHAKE_TIMEOUT = 10;
		const int REQUEST_TIMEOUT = 15;
		// a request as a whole, and its request line with the headers
		const std::size_t REQUEST_SIZE = 1024 * 1024;
		const std::size_t HEADER_SIZE = 16 * 1024;
		// what a connection may hold in or out, and what a live stream may fall behind by, before it is cut
		const int BUFFER = 8 * 1024 * 1024;
		const int STREAM_SEND_LIMIT = 1024 * 1024;
		// live streams on a server at once, and how often they are pinged to find the dead
		const int STREAMS = 64;
		const int STREAM_PING_S = 20;
		// a silent peer is probed after this idle time, this often, this many times, then cut
		const int KEEPALIVE_IDLE_S = 60, KEEPALIVE_INTERVAL_S = 20, KEEPALIVE_COUNT = 3;
	}
}
