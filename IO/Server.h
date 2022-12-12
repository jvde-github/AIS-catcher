/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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
#include <list>
#include <thread>
#include <mutex>

#ifdef HASCURL
#include <curl/curl.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET		int
#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Stream.h"
#include "Common.h"
#include "AIS.h"
#include "Keys.h"
#include "TCP.h"
#include "Utilities.h"
#include "ZIP.h"

#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

namespace IO {

	class Server {

		SOCKET sock = -1;
		int timeout = 2;
		int port = 8089;

		struct addrinfo* address;
		std::thread run_thread;

		bool stop = false;

		void Run();

		sockaddr_in service;

		std::string parse(const std::string& s) {
			int max = 10;
			bool found_get = false;

			std::istringstream iss(s);
			std::string item;
			while (std::getline(iss, item, ' ') && --max) {
				if (found_get) return item;
				if (item == "GET") found_get = true;
			}
			return "";
		}
		int readLine(SOCKET s, std::string& str);

		fd_set fdr, fdw;

	public:
		Server();
		~Server();

		virtual void Request(SOCKET s, const std::string& msg);
		void Response(SOCKET s, std::string type, const std::string& content);
		void Response(SOCKET s, std::string type, const char* data, int len, bool gzip = false);

		bool start(int port);
	};
}
