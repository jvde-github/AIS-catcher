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
#include "TCP.h"

namespace TCP {
	Client::Client() {

#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw "TCP: Cannot initialize Winsocket.";
			return;
		}
#endif
	}

	Client::~Client() {
#ifdef _WIN32
		WSACleanup();
#endif
	}

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
		if (nonblocking) {
			u_long mode = 1; // 1 to enable non-blocking socket
			ioctlsocket(sock, FIONBIO, &mode);
		}
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
}