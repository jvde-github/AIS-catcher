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
#include <algorithm>

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
		if (sock != -1) closesocket(sock);
	}

	void Server::acceptClients() {
		int addrlen = sizeof(service);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR*)&service, (socklen_t*)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				std::cerr << "Server: error accepting connection. " << strerror(WSAGetLastError()) << std::endl;
				return;
			}
		}
#else
		if (conn_socket == -1) {
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				std::cerr << "Server: error accepting connection. " << strerror(errno) << std::endl;
				return;
			}
		}
#endif


		else {
			// std::cerr << "Connection made on socket: " << conn_socket << std::endl;
			int ptr = -1;
			for (int i = 0; i < MAX_CONN; i++)
				if (!client[i].isConnected()) {
					ptr = i;
					break;
				}
			if (ptr == -1) {
				std::cerr << "Server: max connections of " << MAX_CONN << " reached, closing socket" << std::endl;
				closesocket(conn_socket);
			}
			else {
				client[ptr].Start(conn_socket);
				if (!setNonBlock(conn_socket)) {
					std::cerr << "Server: cannot make client socket non-blocking" << std::endl;
					client[ptr].Close();
				}
			}
		}
	}

	void Server::cleanUp() {

		for (auto& c : client)
			if (c.isConnected()) {
				if (c.Inactive(time(0)) > 30) {
					std::cerr << "Server: closing inactive client socket (" << c.sock << ") for" << c.Inactive(time(0)) << "s" << std::endl;
					c.Close();
				}
			}
	}

	void Server::readClients() {

		for (auto& c : client) c.Read();
	}

	void Server::processClients() {
		for (auto& c : client) {
			if (c.isConnected()) {
				std::size_t pos = c.msg.find("\r\n\r\n");

				if (pos != std::string::npos) {
					std::string request = parse(c.msg);
					std::cerr << "Request (" << c.sock << "): " << request << std::endl;
					if (!request.empty())
						Request(c.sock, request);
					else
						std::cerr << c.msg << std::endl;
					c.msg.erase(0, pos + 4);
				}
				else if (c.msg.size() > 4096) {
					std::cerr << "Server: closing connection, client flooding server: " << c.sock << std::endl;
					c.Close();
				}
			}
		}
	}

	void Server::Run() {
		std::vector<SOCKET> conn_sockets;
		std::cerr << "Server: starting server thread.\n";
		while (!stop) {
			acceptClients();
			readClients();
			processClients();
			cleanUp();
			Sleep();
		}
	}

	void Server::Request(SOCKET s, const std::string& r) {
		// TO DO: return 404 by default
	}

	void Server::Sleep() {
		struct timeval tv;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		int maxfds = sock;

		for (auto& c : client) {
			if (c.sock != -1) {
				FD_SET(c.sock, &fds);
				if (c.sock > maxfds) maxfds = c.sock;
			}
		}

		tv = { 0, 250 };
		select(maxfds + 1, &fds, NULL, NULL, &tv);
	}

	void Server::Response(SOCKET s, std::string type, const std::string& content) {
		Response(s, type, (const char*)content.c_str(), content.size());
	}

	void Server::Response(SOCKET s, std::string type, const char* data, int len, bool gzip) {

		std::string header = "HTTP/1.1 200 OK\r\nServer: AIS-catcher\r\nContent-Type: " + type;
		if (gzip) header += "\r\nContent-Encoding: gzip";
		header += "\r\nConnection: keep-alive\r\nContent-Length: " + std::to_string(len) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

		::send(s, header.c_str(), header.length(), 0);
		::send(s, data, len, 0);
	}

	bool Server::start(int port) {

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			return false;
		}

#ifndef _WIN32
		if (reuse_port) {
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		}
#endif
		memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = htonl(INADDR_ANY);
		service.sin_port = htons(port);

		int r = bind(sock, (SOCKADDR*)&service, sizeof(service));
		if (r == SOCKET_ERROR) {
			closesocket(sock);
			sock = -1;
			return false;
		}

		if (listen(sock, 511) < 0) return false;

		client.clear();
		client.resize(MAX_CONN);

		if (!setNonBlock(sock)) {
			std::cerr << "Server: cannot set socket to non-blocking\n";
		}
		stop = false;
		run_thread = std::thread(&Server::Run, this);

		return true;
	}
}
