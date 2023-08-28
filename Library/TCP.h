/*
	Copyright(c) 2023 jvde.github@gmail.com

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

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define SOCKET		 int
#define SOCKADDR	 struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close

#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

namespace TCP {

	class ServerConnection {
	private:
		std::mutex mtx;

	public:
		~ServerConnection() { Close(); }

		SOCKET sock = -1;

		std::string msg;
    	char outBuffer[524288];
    	int outLength = 0;
		std::time_t stamp;

		void Close();
		void Start(SOCKET s);
		int Inactive(std::time_t now);
		bool isConnected() { return sock != -1; }
		bool hasSendBuffer() { return outLength>0; }
		void SendBuffer();
		bool Send(const char* buffer, int length);
		void Read();
	};

	class Server {
	public:
		~Server();

		bool start(int port);
		bool SendAll(const std::string &m) {
			for (auto& c : client) {
				if (c.isConnected()) {
					if (!c.Send(m.c_str(), m.length())) {
						c.Close();
						std::cerr << "TCP listener: client not reading, close connection." << std::endl;
						return false;
					}
				}
			}
			return true;
		}
		void setReusePort(bool b) { reuse_port = b; }
		bool setNonBlock(SOCKET sock) {

#ifndef _WIN32
			int r = fcntl(sock, F_GETFL, 0);
			r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

			if (r == -1) return false;
#else
			u_long mode = 1;
			ioctlsocket(sock, FIONBIO, &mode);
#endif
			return true;
		}

	protected:
		SOCKET sock = -1;
		int timeout = 30;
		bool reuse_port = true;

		const static int MAX_CONN = 16;
		std::array<ServerConnection,MAX_CONN> client;

		std::thread run_thread;

		bool stop = false;

		void Run();
		sockaddr_in service;

		bool Send(ServerConnection &c, const char* data, int len) {
			return c.Send(data, len);
		}

		int findFreeClient();

		void acceptClients();
		void readClients();
		void writeClients();
		virtual void processClients();
		void cleanUp();
		void SleepAndWait();
	};


	class Client {
	public:
		
		Client() {}
		~Client() { disconnect(); }

		void disconnect();
		bool connect(std::string host, std::string port, bool persist, int timeout);

		void setResetTime(int t) { reset_time = t; }
		int read(void* data, int length, int t, bool wait = false);
		int send(const void* data, int length);
	private:
		enum State { DISCONNECTED, CONNECTING, READY };

		std::string host;
		std::string port;
		bool persistent = true;
		int reset_time = -1;
		int timeout = 0;

		int sock = -1;
		State state = DISCONNECTED;
		time_t stamp = 0;

		struct addrinfo* address = NULL;

		void updateState();
		bool isConnected(int t);

		bool reconnect() {
			disconnect();
			if (connect(host, port, persistent, timeout)) {
				std::cerr << "TCP (" << host << ":" << port << "): connected." << std::endl;
				return true;
			}
			return false;
		}
	};
}
