/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#ifdef HASBLUETOOTH

#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <functional>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2bth.h>
#include <bluetoothapis.h>

#else

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#define SOCKET int
#define SOCKADDR struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close

#endif

#endif

#include "Common.h"

namespace Bluetooth
{
#ifdef HASBLUETOOTH
	class ServerConnection
	{
	private:
		std::mutex mtx;
		void CloseUnsafe();
		const static int MAX_BUFFER_SIZE = 1024 * 1024 * 8;
		bool verbose = true;

	public:
		~ServerConnection() { Close(); }

		SOCKET sock = -1;

		std::string msg;
		std::vector<char> out;
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
		bool hasSendBuffer() { return !out.empty(); }
		void SendBuffer();
		bool Send(const char *buffer, int length);
		bool SendDirect(const char *buffer, int length);
		bool SendRaw(const char *buffer, int length);
		void Read();
		void setVerbosity(bool v) { verbose = v; }
	};

	class Server
	{
	public:
		virtual ~Server();

		bool start(int channel);
		bool startAuto();
		int getChannel();
		bool SendAll(const std::string &m);
		bool SendAllDirect(const std::string &m);

		void setReuseChannel(bool b) { reuse_channel = b; }
		bool setNonBlock(SOCKET sock);
		void setBluetoothAddr(std::string addr) { BT_ADDR = addr; }

	protected:
		SOCKET sock = -1;
		int timeout = 30;
		int current_channel = -1;
		bool reuse_channel = true;
		std::string BT_ADDR;

		const static int MAX_CONN = 16;
		std::array<ServerConnection, MAX_CONN> client;

		std::thread run_thread;

		bool stop = false;

		void Run();
#ifdef _WIN32
		SOCKADDR_BTH service;
#else
		struct sockaddr_rc service;
#endif

		bool Send(ServerConnection &c, const char *data, int len)
		{
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
	};
#else

	class Server
	{
	public:
		bool start(int channel)
		{
			Error() << "Bluetooth not supported on this platform.";
			return false;
		}
		bool startAuto()
		{
			Error() << "Bluetooth not supported on this platform.";
			return false;
		}

		virtual ~Server() {}

		int getChannel()
		{
			Error() << "Bluetooth not supported on this platform.";
			return -1;
		}
		bool SendAll(const std::string &m)
		{
			Error() << "Bluetooth not supported on this platform.";
			return false;
		}
		bool SendAllDirect(const std::string &m)
		{
			Error() << "Bluetooth not supported on this platform.";
			return false;
		}
	};
#endif
}