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

#include "IO.h"

namespace IO {

	UDP::UDP() {
#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw "Cannot initialize Winsocket.";
			return;
		}
#endif
	}

	UDP::~UDP() {
		closeConnection();

#ifdef _WIN32
		WSACleanup();
#endif
	}

	void UDP::Receive(const AIS::Message* data, int len, TAG& tag) {
		if (sock != -1)
			for (int i = 0; i < len; i++)
				for (const auto& s : data[i].sentence)
					sendto(sock, (s + "\r\n").c_str(), (int)s.length() + 2, 0, address->ai_addr,
						   (int)address->ai_addrlen);
	}

	void UDP::openConnection(const std::string& host, const std::string& port) {
		if (sock != -1) {
			throw "UDP: internal error, socket already defined.";
			return;
		}

		struct addrinfo h;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);

		if (code != 0 || address == NULL) {
			throw "UDP network address and/or port not valid.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1) {
			throw "Error creating socket for UDP.";
		}
	}

	void UDP::closeConnection() {
		if (sock != -1) {
			closesocket(sock);
			sock = -1;
		}
	}
}
