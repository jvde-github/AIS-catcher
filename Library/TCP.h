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
