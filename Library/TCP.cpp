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
#include "Common.h"

namespace TCP {

	void Client::disconnect() {
		if (sock != -1)
			closesocket(sock);

		if (address != NULL) {
			freeaddrinfo(address);
			address = NULL;
		}

		sock = -1;
		state = DISCONNECTED;
	}

	bool Client::connect(std::string host, std::string port, bool persist, int timeout) {
		int r;
		struct addrinfo h;

		this->host = host;
		this->port = port;
		this->persistent = persist;
		this->timeout = timeout;

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

		return isConnected(timeout);
	}

	bool Client::isConnected(int t) {

		if (state == READY) return true;
		fd_set fdr, fdw;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = { t, 1 };

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


	void Client::updateState() {

		if (state == READY && reset_time > 0 && (long)time(NULL) - (long)stamp > reset_time * 60) {
			std::cerr << "TCP (" << host << ":" << port << "): connection expired, reconnect." << std::endl;
			reconnect();
		}

		if (state == DISCONNECTED) {
			if ((long)time(NULL) - (long)stamp > 10) {
				std::cerr << "TCP (" << host << ":" << port << "): not connected, reconnecting." << std::endl;
				if (connect(host, port, persistent, timeout)) {
					std::cerr << "TCP (" << host << ":" << port << "): connected." << std::endl;
					return;
				}
			}
		}

		if (state == CONNECTING) {
			bool connected = isConnected(0);

			if (connected) {
				std::cerr << "TCP (" << host << ":" << port << "): connected to server." << std::endl;
			}
			else if ((long)time(NULL) - (long)stamp > 10) {
				std::cerr << "TCP (" << host << ":" << port << "): timeout connecting to server, reconnect." << std::endl;
				reconnect();
				return;
			}
		}
	}

	int Client::send(const void* data, int length) {

		updateState();

		if (state == READY) {
			int sent = ::send(sock, (char*)data, length, 0);

			if (sent < length) {
					int error_code = errno; 
#ifdef _WIN32
					if (error_code == WSAEWOULDBLOCK) return 0;
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK) return 0;
#endif
					std::cerr << "TCP (" << host << ":" << port << "): send error. Error code: " << error_code << " (" << strerror(error_code) << ").";
					if (persistent) {
						reconnect();
						std::cerr << " Reconnect.\n";
						return 0;
					}
					else {
						std::cerr << std::endl;
						return -1;
					}
				}
			return sent;
		}

		return 0;
	}

	// zero if no input yet or connection being established
	int Client::read(void* data, int length, int t, bool wait) {

		updateState();

		if (state == READY) {
			fd_set fd, fe;

			FD_ZERO(&fd);
			FD_SET(sock, &fd);

			FD_ZERO(&fe);
			FD_SET(sock, &fe);

			timeval to = { t, 0 };

			if (select(sock + 1, &fd, NULL, &fe, &to) < 0) return 0;			

			if (FD_ISSET(sock, &fd) || FD_ISSET(sock, &fe)) {
				int retval = recv(sock, (char*)data, length, wait ? MSG_WAITALL : 0);

				if (retval <= 0) {
					int error_code = errno;
#ifdef _WIN32
					if (error_code == WSAEWOULDBLOCK) return 0;
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK) return 0;
#endif

					std::cerr << "TCP (" << host << ":" << port << "): receive error. Error code: " << error_code << " (" << strerror(error_code) << ").";

					if (persistent) {
						std::cerr << " Reconnect.\n";
						reconnect();
						return 0;
					}
					else {
						std::cerr << std::endl;
						return -1;
					}					
				}
				return retval;
			}
		}

		return 0;
	}
}