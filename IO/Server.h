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
#include <time.h>


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

	struct Client {
		SOCKET sock = -1;
		std::string msg;
		std::time_t stamp;

		void Close() {
			if (sock != -1) closesocket(sock);
			sock = -1;
			msg.clear();
			std::cerr << "Closing socket" << std::endl;
		}

		void Start(SOCKET s) {
			msg.clear();
			stamp = std::time(0);
			sock = s;
		}

		int Inactive(std::time_t now) {
			return (int)((long int)now - (long int)stamp);
		}
		void Read() {
			char buffer[1024];

			if (sock != -1) {

				int nread = recv(sock, buffer, sizeof(buffer), 0);
#ifdef _WIN32
				if (nread == 0 || (nread < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
#else
				if (nread == 0 || (nread < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
#endif
					std::cerr << "Server: connection closed by client or error: " << sock << std::endl;
					Close();
				}
				else if (nread > 0) {
					msg += std::string(buffer, nread);
					stamp = std::time(0);
				}
			}
		}
	};

	class Server {
	public:
		Server();
		~Server();

		const int MAX_CONN = 64;
		std::vector<Client> client;

		virtual void Request(SOCKET s, const std::string& msg);

		void Response(SOCKET s, std::string type, const std::string& content);
		void Response(SOCKET s, std::string type, const char* data, int len, bool gzip = false);

		bool start(int port);
		void setReusePort(bool b) { reuse_port = b; }
		bool setNonBlock(SOCKET sock) {
#ifndef _WIN32
			int r = fcntl(sock, F_GETFL, 0);
			r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

			if (r == -1) return false;
#else
			u_long mode = 1; // 1 to enable non-blocking socket
			ioctlsocket(sock, FIONBIO, &mode);
#endif
			return true;
		}

	private:
		SOCKET sock = -1;
		int timeout = 2;
		int port = 8100;
		bool reuse_port = true;

		struct addrinfo* address;
		std::thread run_thread;

		bool stop = false;
		std::string ret, header;

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
		void Process(SOCKET s);
		void acceptClients();
		void readClients();
		void processClients();
		void cleanUp();
		void Sleep();

		fd_set fdr, fdw;
	};
}
