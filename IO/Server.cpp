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

#include <cstring>

#include "Server.h"

namespace IO {

	Server::Server() {
#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw std::runtime_error("TCP: Cannot initialize Winsocket.");
			return;
		}
#endif
	}

	Server::~Server() {

		stop = true;
		if (run_thread.joinable()) run_thread.join();

#ifdef _WIN32
		WSACleanup();
#endif
	}

	int Server::readLine(SOCKET s, std::string& str) {
		int r = 0;
		char c;
		str.clear();

		while (true) {
			r = recv(s, &c, 1, 0);
			if (r == -1 || r == 0 || c == '\n') return r;
			if (c != '\r') str += c;
		}
		return r;
	}

	void Server::Run() {
		char c;
		std::string ret, header;
		int r = 0;

		while (!stop) {
			int addrlen = sizeof(service);
			SOCKET conn_socket;

			FD_ZERO(&fdr);
			FD_SET(sock, &fdr);

			struct timeval tv = { 0, 50000 };
			int nready = select(sock + 1, &fdr, 0, 0, &tv);

			if (FD_ISSET(sock, &fdr)) {
				if ((conn_socket = accept(sock, (SOCKADDR*)&service, (socklen_t*)&addrlen)) < 0) {
					std::cerr << "Error accepting incoming connection";
					continue;
				}

				header.clear();
				do {
					r = readLine(conn_socket, ret);
					header += ret + '\n';
				} while (r > 0 && !ret.empty());

				if (r >= 0) {
					std::string request = parse(header);
					if (!request.empty()) Request(conn_socket, request);
				}
				closesocket(conn_socket);
			}
		}
	}

	void Server::Request(SOCKET s, const std::string& r) {
		// TO DO: return 404 by default
	}


	void Server::Response(SOCKET s, std::string type, const std::string& content) {
		Response(s, type, (const char*)content.c_str(), content.size());
	}

	void Server::Response(SOCKET s, std::string type, const char* data, int len, bool gzip) {

		std::string header = "HTTP/1.1 200 OK\r\nServer: AIS-catcher\r\nContent-Type: " + type;
		if (gzip) header += "\r\nContent-Encoding: gzip";
		header += "\r\nConnection: close\r\nContent-Length: " + std::to_string(len) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

		::send(s, header.c_str(), header.length(), 0);
		::send(s, data, len, 0);
	}


	bool Server::start(int port) {

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			return false;
		}

		memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(INADDR_ANY);
		service.sin_port = htons(port);

		//----------------------
		// Bind the socket.
		int r = bind(sock, (SOCKADDR*)&service, sizeof(service));
		if (r == SOCKET_ERROR) {
			closesocket(sock);
			return false;
		}

		if (listen(sock, 32) < 0) {
			return false;
		}

		stop = false;
		run_thread = std::thread(&Server::Run, this);

		return true;
	}
}
