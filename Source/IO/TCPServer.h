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
#include <array>
#include <vector>
#include <functional>

#include "SocketUtil.h"

#include "Common.h"
#include "OutputStats.h"

namespace IO
{

	class TCPServerConnection
	{
	private:
		std::mutex mtx;
		void CloseUnsafe();
		const static int MAX_BUFFER_SIZE = 1024 * 1024 * 8;
		// Reclaim the already-sent prefix only once it grows past this, so the
		// memmove is amortized across many sends instead of paid on every drain.
		const static size_t OUT_COMPACT_THRESHOLD = 64 * 1024;
		bool verbose = true;

		size_t pending() const { return out.size() - out_pos; }
		void compact()
		{
			if (out_pos == 0)
				return;
			out.erase(out.begin(), out.begin() + out_pos);
			out_pos = 0;
		}

	public:
		~TCPServerConnection() { Close(); }

		SOCKET sock = -1;

		std::string msg;
		std::vector<char> out;
		size_t out_pos = 0; // read cursor into out; [out_pos, out.size()) is unsent
		std::time_t stamp;
		bool is_locked = false;

		void Lock();
		void Unlock();
		bool isLocked()
		{
			return is_locked;
		}

		void Close();
		void Start(SOCKET s);
		int Inactive(std::time_t now);
		bool isConnected() { return sock != -1; }
		bool hasSendBuffer() { return out_pos < out.size(); }
		void SendBuffer();
		bool Send(const char *buffer, int length);
		bool SendDirect(const char *buffer, int length);
		bool SendRaw(const char *buffer, int length);
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
		bool SendAll(const std::string &m);
		bool SendAllDirect(const std::string &m);
		bool SendAllDirect(const char *data, int len);

		void setReusePort(bool b) { reuse_port = b; }
		bool setNonBlock(SOCKET sock);
		void setIP(const std::string &ip) { IP_BIND = ip; }

	protected:
		SOCKET sock = -1;
		int timeout = 30;
		bool reuse_port = true;
		std::string IP_BIND;
		int listening_port = -1;

		static std::vector<int> active_ports;

		const static int MAX_CONN = 128;
		std::array<TCPServerConnection, MAX_CONN> client;

		std::thread run_thread;

		std::atomic<bool> stop{false};

		void Run();
		sockaddr_in service;

		bool Send(TCPServerConnection &c, const char *data, int len)
		{
			if (pstats)
				pstats->bytes_out += len;

			return c.Send(data, len);
		}

		int findFreeClient();
		int numberOfClients();
		void acceptClients();
		void readClients();
		void writeClients();
		virtual void processClients();
		void cleanUp();
		void SleepAndWait();

		void setStats(IO::OutputStats *s) { pstats = s; }

	private:
		IO::OutputStats *pstats = nullptr;
	};
}