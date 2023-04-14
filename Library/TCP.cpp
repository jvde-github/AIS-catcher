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
#include <string>
#include "TCP.h"

namespace TCP {
	void Client::disconnect() {
		if (sock != -1)
			closesocket(sock);
	}

	bool Client::connect(std::string host, std::string port) {
		int r;
		struct addrinfo h;
		fd_set fdr, fdw;

		std::memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;

#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);
		if (code != 0 || address == NULL) return false;

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (sock == -1) return false;

#ifndef _WIN32
		r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

		if (r == -1) return false;
#else
		u_long mode = 1; // 1 to enable non-blocking socket
		ioctlsocket(sock, FIONBIO, &mode);
#endif

		if (::connect(sock, address->ai_addr, (int)address->ai_addrlen) != -1)
			return true;

#ifndef _WIN32
		if (errno != EINPROGRESS) {
			closesocket(sock);
			sock = -1;
			return false;
		}
#endif

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = { timeout, 0 };

		if (select(sock + 1, &fdr, &fdw, NULL, &to) > 0) {
			int error;
			socklen_t len = sizeof(error);

			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
			if (error != 0) return false;

			return true;
		}
		return false;
	}

	int Client::read(void* data, int length, bool wait) {
		fd_set fd;

		FD_ZERO(&fd);
		FD_SET(sock, &fd);

		timeval to = { timeout, 0 };

		if (select(sock + 1, &fd, NULL, NULL, &to) < 0) {
			return -1;
		}

		if (FD_ISSET(sock, &fd)) {
			return recv(sock, (char*)data, length, wait ? MSG_WAITALL : 0);
		}
		return 0;
	}

	void ClientPersistent::disconnect() {
		if (sock != -1)
			closesocket(sock);
		sock = -1;
		state = DISCONNECTED;
	}

	bool ClientPersistent::connect(std::string host, std::string port) {
		int r;
		struct addrinfo h;

		this->host = host;
		this->port = port;

		std::memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;

#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);
		if (code != 0 || address == NULL) return false;

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (sock == -1) return false;

#ifndef _WIN32
		r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

		if (r == -1) return false;
#else
		u_long mode = 1; // 1 to enable non-blocking socket
		ioctlsocket(sock, FIONBIO, &mode);
#endif

		stamp = time(NULL);

		if (::connect(sock, address->ai_addr, (int)address->ai_addrlen) != -1) {
			state = READY;
			return true;
		}

#ifndef _WIN32
		if (errno != EINPROGRESS) {
			closesocket(sock);
			sock = -1;
			return false;
		}
#endif

		return isConnected();
	}

	bool ClientPersistent::isConnected() {
		fd_set fdr, fdw;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = { 0, 1 };

		if (select(sock + 1, &fdr, &fdw, NULL, &to) > 0) {
			int error;
			socklen_t len = sizeof(error);

			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
			if (error != 0) return false;

			state = READY;
			return true;
		}

		state = CONNECTING;
		return false;
	}

	int ClientPersistent::send(const void* data, int length) {
		if (state == DISCONNECTED) {
			if ((long)time(NULL) - (long)stamp > 10) {
				std::cerr << "TCP feed (" << host << ":" << port << "): not connected, reconnecting." << std::endl;
				if (connect(host, port)) {
					std::cerr << "TCP feed: connected to " << host << ":" << port << std::endl;
					return true;
				}
			}
		}

		// check if sock is connected
		if (state == CONNECTING) {
			bool connected = isConnected();

			if (connected) {
				std::cerr << "TCP feed (" << host << ":" << port << "): connected to server." << std::endl;
			}
			else if ((long)time(NULL) - (long)stamp > 10) {
				std::cerr << "TCP feed (" << host << ":" << port << "): timeout connecting to server, reconnect." << std::endl;
				reconnect();
			}			
		}

		if (state == READY) {
			int sent = ::send(sock, (char*)data, length, 0);

			if (sent < length) {
				std::cerr << "TCP feed (" << host << ":" << port << "): send error, reconnect." << std::endl;
				reconnect();
				return -1;
			}
			return sent;
		}

		// connecting or disconnected, so nothing sent
		return -1;
	}


}