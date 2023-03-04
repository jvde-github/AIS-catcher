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

		SOCKET sock = -1;
		int timeout = 2;

		struct addrinfo* address = NULL;

	public:
		bool connect(std::string host, std::string port);
		void disconnect();

		void setTimeout(int t) { timeout = t; }
		int read(void* data, int length, bool wait = false);
		int send(const char* msg, int len) { return ::send(sock, msg, len, 0); }
	};

	class Client2 {

		SOCKET sock = -1;

		struct addrinfo* address;

		enum { READY,
			   CONNECTING } state = CONNECTING;

		std::time_t stamp = 0;
		std::string host, port;

	public:
		bool connect(std::string, std::string);
		void disconnect();

		void reconnect();

		int read(void* data, int length, bool wait = false);
		int send(const char* msg, int len) {

			if (state != READY) reconnect();
			if(sock == -1) return 0;

			int n = ::send(sock, msg, len, 0);

			if (n <= 0) {
				if(state == READY) 
					std::cerr << "TCP: failed to write to server, resetting connection.\n";
				reconnect();
			}
			if (n > 0) {
				if(state == CONNECTING)
					std::cerr << "TCP: connected to server.\n";

				state = READY;
			}

			return n;
		}
	};


}
