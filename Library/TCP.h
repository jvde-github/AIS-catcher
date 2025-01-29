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

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define SOCKET int
#define SOCKADDR struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close

#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Common.h"

namespace TCP
{

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
		void Read();
		void setVerbosity(bool v) { verbose = v; }
	};

	class Server
	{
	public:
		virtual ~Server();

		bool start(int port);
		bool SendAll(const std::string &m);
		void setReusePort(bool b) { reuse_port = b; }
		bool setNonBlock(SOCKET sock);
		void setIP(std::string ip) { IP_BIND = ip; }

	protected:
		SOCKET sock = -1;
		int timeout = 30;
		bool reuse_port = true;
		std::string IP_BIND;

		const static int MAX_CONN = 16;
		std::array<ServerConnection, MAX_CONN> client;

		std::thread run_thread;

		bool stop = false;

		void Run();
		sockaddr_in service;

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

	class Client
	{
	public:
		Client() {}
		~Client() { disconnect(); }

		void disconnect();
		bool connect(std::string host, std::string port, bool persist, int timeout, bool keep_alive = false, int idle = 300, int interval = 180, int count = 3);

		void setResetTime(int t) { reset_time = t; }
		int read(void *data, int length, int t = 1, bool wait = false);
		int send(const void *data, int length);
		void updateConnection();

		SOCKET getSocket() { return sock; }
		int numberOfConnects() { return connects; }

		void setOnConnectedCallback(std::function<void()> callback) { onConnected = callback; }
		void setOnDisconnectedCallback(std::function<void()> callback) { onDisconnected = callback; }
		void setVerbosity(bool v) { verbose = v; }

	private:
		enum State
		{
			DISCONNECTED,
			CONNECTING,
			READY
		};

		std::string host;
		std::string port;
		bool persistent = true;
		int reset_time = -1;
		int timeout = 0;
		int connects = 0;

		int sock = -1;
		State state = DISCONNECTED;
		time_t stamp = 0;
		bool verbose = true;

		std::function<void()> onConnected = nullptr;
		std::function<void()> onDisconnected = nullptr;

		void updateState();
		bool isConnected(int t);

		bool reconnect()
		{
			disconnect();
			if (connect(host, port, persistent, timeout))
				return true;

			return false;
		}
	};
}
