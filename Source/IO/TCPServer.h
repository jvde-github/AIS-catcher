/*
	Copyright(c) 2023-2024 jvde.github@gmail.com

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

#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <vector>
#include <deque>
#include <functional>

#include "SocketUtil.h"

#ifndef _WIN32
#include <poll.h>
#endif

#include "Common.h"
#include "OutputStats.h"

namespace IO
{

	class TCPServerConnection
	{
	private:
		const static int MAX_BUFFER_SIZE = 1024 * 1024 * 8;
		// Reclaim the already-sent prefix only once it grows past this, so the
		// memmove is amortized across many sends instead of paid on every drain.
		const static size_t OUT_COMPACT_THRESHOLD = 64 * 1024;
		// clear() keeps capacity; buffers grown past this are freed so a slot
		// can't pin its peak size for the server's lifetime.
		const static size_t BUFFER_RETAIN_LIMIT = 256 * 1024;
		bool verbose = true;

		size_t pending() const { return out.size() - out_pos; }
		void compact()
		{
			if (out_pos == 0)
				return;
			out.erase(out.begin(), out.begin() + out_pos);
			out_pos = 0;
		}
		template <typename T> static void shrink(T &buf)
		{
			if (buf.capacity() > BUFFER_RETAIN_LIMIT)
				T().swap(buf);
			else
				buf.clear();
		}
		void releaseBuffers()
		{
			shrink(out);
			shrink(msg);
			out_pos = 0;
		}
		// append to the out buffer, compacting first and enforcing the size cap
		bool queue(const char *data, int length);

	public:
		~TCPServerConnection() { Close(); }

		SOCKET sock = -1;

		std::string msg;
		std::vector<char> out;
		size_t out_pos = 0; // read cursor into out; [out_pos, out.size()) is unsent
		std::time_t stamp;
		// bumped on every Start(); a deferred reference (an SSE subscriber) carries
		// this value and is validated before use, so a reused slot is detected, not aliased
		uint32_t generation = 0;
		// SSE streams read nothing after the GET, so exempt them from the idle sweep
		bool no_timeout = false;
		// close once the out buffer drains (HTTP/1.0 or Connection: close)
		bool close_after_send = false;
		// current request is HEAD: full headers, suppressed body
		bool head_request = false;
		// interim 100 Continue already sent for the request being received
		bool continue_sent = false;
		// arrival time of the pending request's first byte, 0 if none (HTTP header timeout)
		std::time_t request_start = 0;

		void Close();
		void Start(SOCKET s);
		int Inactive(std::time_t now) const;
		bool isConnected() const { return sock != -1; }
		uint32_t getGeneration() const { return generation; }
		void setNoTimeout() { no_timeout = true; }
		bool hasSendBuffer() const { return out_pos < out.size(); }
		void SendBuffer();
		bool Send(const char *buffer, int length);
		bool SendDirect(const char *buffer, int length);
		void Read();
		void setVerbosity(bool v) { verbose = v; }
	};

	class TCPServer
	{
	public:
		virtual ~TCPServer();

		bool start(int port);
		// Stop and join the server thread. Call before destroying any state the
		// request handlers touch — the base-class destructor runs too late for
		// members of derived classes.
		void stopThread();
		void SendAll(std::string m); // by value: moved into the queued Command
		void SendAll(const char *data, int len) { SendAll(std::string(data, len)); }

		void setReusePort(bool b) { reuse_port = b; }
		void setIP(const std::string &ip) { IP_BIND = ip; }

	protected:
		SOCKET sock = -1;
		int timeout = 30;
		bool reuse_port = false;
		std::string IP_BIND;
		int listening_port = -1;

		static std::vector<int> active_ports;

		const static int MAX_CONN = 128;
		std::array<TCPServerConnection, MAX_CONN> client;

		std::thread run_thread;

		std::atomic<bool> stop{false};

		void Run();

		// Single-owner discipline: only the Run() thread ever touches a
		// connection's socket/buffers. Cross-thread producers (broadcast output,
		// SSE fan-out) enqueue a Command; the loop drains and executes them every
		// cycle. cmd_mtx is the only shared lock, and it guards the queue alone —
		// never a connection.
		struct Command
		{
			// Derived is opaque to the base loop: it owns neither the id space nor
			// the payload format.
			enum Kind { BroadcastRaw, Derived } kind;
			int id;
			std::string data;
		};
		std::mutex cmd_mtx;
		std::deque<Command> cmds;
		std::deque<Command> cmd_scratch; // swapped with cmds so the nodes are reused
		// Poked by post() so a queued command wakes the loop immediately instead
		// of waiting for the poll timeout; degrades to timeout-only if unavailable.
		Net::Wakeup wakeup;
		std::time_t last_housekeeping = 0;
		void post(Command c);
		void drainCommands();
		// Whatever a subclass queues, executed on the Run() thread.
		virtual void onCommand(int, const std::string &) {}

		// pfds[0] listening socket, pfds[1] the wake handle, pfds[i + 2] client[i];
		// unused slots get fd = -1 so poll skips them.
		std::array<pollfd, MAX_CONN + 2> pfds;

		bool Send(TCPServerConnection &c, const char *data, int len)
		{
			if (!c.Send(data, len))
				return false;

			if (pstats)
				pstats->bytes_out += len;

			return true;
		}

		int findFreeClient();
		int numberOfClients();
		void acceptClients();
		void readClients();
		void writeClients();
		virtual void processClients();
		void cleanUp(std::time_t now);
		void SleepAndWait();

		void setStats(IO::OutputStats *s) { pstats = s; }

	private:
		IO::OutputStats *pstats = nullptr;
	};
}