/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

	void Client::Close() {
		if (sock != -1) closesocket(sock);
		sock = -1;
		msg.clear();
	}

	void Client::Start(SOCKET s) {
		msg.clear();
		stamp = std::time(0);
		sock = s;
	}

	int Client::Inactive(std::time_t now) {
		return (int)((long int)now - (long int)stamp);
	}

	void Client::Read() {
		char buffer[1024];

		if (isConnected()) {

			int nread = recv(sock, buffer, sizeof(buffer), 0);
#ifdef _WIN32
			if (nread == 0 || (nread < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
				int e = WSAGetLastError();
#else
			if (nread == 0 || (nread < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
				int e = errno;
#endif
				if (nread < 0)
					std::cerr << "Socket: connection closed by error: " << strerror(e) << ", sock = " << sock << std::endl;

				Close();
			}
			else if (nread > 0) {
				msg += std::string(buffer, nread);
				stamp = std::time(0);
			}
		}
	}

	// TCP Server

	TCPServer::~TCPServer() {
		stop = true;
		if (run_thread.joinable()) run_thread.join();
		if (sock != -1) closesocket(sock);
	}

	int TCPServer::findFreeClient() {
		for (int i = 0; i < MAX_CONN; i++)
			if (!client[i].isConnected()) return i;
		return -1;
	}

	void TCPServer::acceptClients() {
		int addrlen = sizeof(service);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR*)&service, (socklen_t*)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				std::cerr << "TCP listener: error accepting connection. " << strerror(WSAGetLastError()) << std::endl;
				return;
			}
		}
#else
		if (conn_socket == -1) {
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				std::cerr << "TCP Server: error accepting connection. " << strerror(errno) << std::endl;
				return;
			}
		}
#endif
		else {
			int ptr = findFreeClient();
			if (ptr == -1) {
				std::cerr << "TCP Server: max connections reached (" << MAX_CONN << ", closing socket." << std::endl;
				closesocket(conn_socket);
			}
			else {
				client[ptr].Start(conn_socket);
				if (!setNonBlock(conn_socket)) {
					std::cerr << "TCP Server: cannot make client socket non-blocking." << std::endl;
					client[ptr].Close();
				}
			}
		}
	}

	void TCPServer::cleanUp() {

		for (auto& c : client)
			if (c.isConnected() && timeout && c.Inactive(time(0)) > timeout) {
				std::cerr << "TCP Server: timeout, close client " << c.sock << std::endl;
				c.Close();
			}
	}

	void TCPServer::readClients() {

		for (auto& c : client) c.Read();
	}

	void TCPServer::processClients() {
		for (auto& c : client) {
			if (c.isConnected()) {
				c.msg.clear();
			}
		}
	}

	void TCPServer::Run() {

		while (!stop) {
			acceptClients();
			readClients();
			processClients();
			cleanUp();
			SleepAndWait();
		}

		std::cerr << "TCP Server: thread ending.\n";
	}

	void TCPServer::SleepAndWait() {
		struct timeval tv;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		int maxfds = sock;

		for (auto& c : client) {
			if (c.isConnected()) {
				FD_SET(c.sock, &fds);
				if (c.sock > maxfds) maxfds = c.sock;
			}
		}

		tv = { 1, 0 };
		select(maxfds + 1, &fds, NULL, NULL, &tv);
	}

	bool TCPServer::Send(SOCKET s, const char* data, int len) {
		int sent = 0;
		int bytes = 0;

		while (sent < len) {
			int remaining = len - sent;
			bytes = ::send(s, data + sent, remaining, 0);

#ifndef _WIN32
			if (bytes != remaining) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					SleepSystem(100);
				}
				else {
					std::cerr << "TCP Server: error sending response: " << strerror(errno) << std::endl;
					return false;
				}
			}
#else
			if (bytes != remaining) {
				if (WSAGetLastError() == WSAEWOULDBLOCK) {
					SleepSystem(100);
				}
				else {
					std::cerr << "TCP Server: error sending response: " << strerror(WSAGetLastError()) << std::endl;
					return false;
				}
			}
#endif
			sent += bytes;
		}
		return true;
	}

	bool TCPServer::start(int port) {

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) return false;

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
			std::cerr << "TCP Server: cannot set socket to non-blocking\n";
		}
		stop = false;
		std::cerr << "TCP Server: start thread at port " << port << std::endl;
		run_thread = std::thread(&TCPServer::Run, this);

		return true;
	}


	// HTTP Server
	void Server::processClients() {
		for (auto& c : client) {
			if (c.isConnected()) {

				std::size_t pos = c.msg.find("\r\n\r\n");
				if (pos != std::string::npos) {
					std::string request;
					bool gzip;
					Parse(c.msg, request, gzip);
					if (!request.empty())
						Request(c, request, gzip);

					c.msg.erase(0, pos + 4);
				}
				else if (c.msg.size() > 4096) {
					std::cerr << "Server: closing connection, client flooding server: " << c.sock << std::endl;
					c.Close();
				}
			}
		}
	}

	void Server::Request(Client& c, const std::string& r, bool) {
		// TO DO: return 404 by default
	}


	void Server::Parse(const std::string& s, std::string& get, bool& accept_gzip) {
		int max = 100;

		get = "";
		accept_gzip = false;

		std::istringstream iss(s);
		std::string line;
		while (std::getline(iss, line) && !line.empty()) {
			std::istringstream line_stream(line);
			std::string key, value;
			std::getline(line_stream, key, ' ');
			Util::Convert::toUpper(key);

			if (key == "GET") {
				std::getline(line_stream, value, ' ');
				get = value;
			}
			else if (key == "ACCEPT-ENCODING:") {
				std::getline(line_stream, value);
				accept_gzip = value.find("gzip") != std::string::npos;
			}
		}
	}

	void Server::Response(Client& c, std::string type, const std::string& content, bool gzip) {
#ifdef HASZLIB
		if (gzip) {
			zip.zip(content);
			Response(c, type, (char*)zip.getOutputPtr(), zip.getOutputLength(), true);
			return;
		}
#endif

		Response(c, type, (char*)content.c_str(), content.size());
	}

	void Server::Response(Client& c, std::string type, char* data, int len, bool gzip) {

		std::string header = "HTTP/1.1 200 OK\r\nServer: AIS-catcher\r\nContent-Type: " + type;
		if (gzip) header += "\r\nContent-Encoding: gzip";
		header += "\r\nConnection: keep-alive\r\nContent-Length: " + std::to_string(len) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

		if (!Send(c.sock, header.c_str(), header.length())) {
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
		if (!Send(c.sock, data, len)) {
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
	}
}