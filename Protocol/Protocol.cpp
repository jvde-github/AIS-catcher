/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#include "Protocol.h"

namespace Protocol {
	void TCP::disconnect() {

		if (sock != -1) {
			onDisconnect();
			closesocket(sock);
		}

		if (state == READY)
			Info() << "TCP (" << host << ":" << port << "): disconnected.";

		sock = -1;
		state = DISCONNECTED;
	}

	bool TCP::connect() {

		state = DISCONNECTED;
		stamp = time(nullptr);

		int r;
		struct addrinfo h;
		struct addrinfo* address = nullptr;

		std::memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;

#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);
		if (code != 0 || address == nullptr)
			return false;

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1) {
			freeaddrinfo(address);
			return false;
		}

#ifndef _WIN32
		if (keep_alive) {
			int optval = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
				freeaddrinfo(address);
				disconnect();
				return false;
			}
		}
#endif

		if (persistent) {
#ifndef _WIN32
			r = fcntl(sock, F_GETFL, 0);
			r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

			if (r == -1) {
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#else
			u_long mode = 1; // 1 to enable non-blocking socket
			ioctlsocket(sock, FIONBIO, &mode);
#endif
		}

		r = ::connect(sock, address->ai_addr, (int)address->ai_addrlen);
		freeaddrinfo(address);

		if (r != -1) {
			state = READY;

			Info() << "TCP (" << host << ":" << port << "): connected.";
			onConnect();

			return true;
		}

#ifndef _WIN32

		if (errno != EINPROGRESS) {
			disconnect();
			return false;
		}
#else
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			disconnect();
			return false;
		}
#endif

		return isConnected(timeout);
	}


	bool TCP::isConnected(int t) {

		if (state == READY)
			return true;

		if (sock == -1)
			return false;

		fd_set fdr, fdw;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = { t, 1 }; // t seconds and 1 microsecond

		if (select(sock + 1, &fdr, &fdw, nullptr, &to) > 0) {
			int error = -1; // Initialize to a non-zero value
			socklen_t len = sizeof(error);

			if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) != 0) {
				Error() << "TCP (" << host << ":" << port << "): getsockopt failed.";
				disconnect();
				return false;
			}

			if (error != 0) {
				Error() << "TCP (" << host << ":" << port << "): connection failed with error " << error << " (" << strerror(error) << ").";
				disconnect();
				return false;
			}

			state = READY;

			Info() << "TCP (" << host << ":" << port << "): connected.";
			onConnect();

			return true;
		}


		state = CONNECTING;
		return false;
	}

	void TCP::updateState() {
		if (state == READY && reset_time > 0 && (long)time(nullptr) - (long)stamp > reset_time * 60) {
			Warning() << "TCP (" << host << ":" << port << "): connection expired, reconnect.";
			reconnect();
		}

		else if (state == DISCONNECTED) {
			if ((long)time(nullptr) - (long)stamp > 10) {
				Warning() << "TCP (" << host << ":" << port << "): not connected, reconnecting.";
				reconnect();
			}
		}

		else if (state == CONNECTING) {
			bool connected = isConnected(0);

			if (!connected && (long)time(nullptr) - (long)stamp > 10) {
				Warning() << "TCP (" << host << ":" << port << "): timeout connecting to server, reconnect.";
				reconnect();

				return;
			}
		}
	}

	int TCP::send(const void* data, int length) {
		if (state == READY) {

			int sent = ::send(sock, (char*)data, length, 0);

			if (sent < length) {
				int error_code = errno;
#ifdef _WIN32
				if (error_code == WSAEWOULDBLOCK)
					return 0;

#else
				if (error_code == EAGAIN || error_code == EWOULDBLOCK || error_code == EINPROGRESS) {
					Error() << "TCP (" << host << ":" << port << "): message might be lost. Error code: " << error_code << " (" << strerror(error_code) << ").";
					return 0;
				}

#endif
				Error() << "TCP (" << host << ":" << port << "): send error. Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");
				if (persistent) {
					reconnect();
					return 0;
				}
				else {
					return -1;
				}
			}
			return sent;
		}

		else
			updateState();


		return 0;
	}

	// zero if no input yet or connection being established
	int TCP::read(void* data, int length, int t, bool wait) {
		if (state == READY) {
			fd_set fd, fe;

			FD_ZERO(&fd);
			FD_SET(sock, &fd);

			FD_ZERO(&fe);
			FD_SET(sock, &fe);

			timeval to = { t, 0 };

			if (select(sock + 1, &fd, nullptr, &fe, &to) < 0)
				return 0;


			if (FD_ISSET(sock, &fd) || FD_ISSET(sock, &fe)) {
				int retval = recv(sock, (char*)data, length, wait ? MSG_WAITALL : 0);

				if (retval <= 0) {
					int error_code = errno;
#ifdef _WIN32
					if (error_code == WSAEWOULDBLOCK)
						return 0;
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK)
						return 0;
#endif

					Info() << "TCP (" << host << ":" << port << ") receive error with recv returns " << retval << ". Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");

					if (persistent) {
						reconnect();
						return 0;
					}
					else {
						return -1;
					}
				}

				return retval;
			}
		}
		else {
			updateState();
			return 0;
		}

		return 0;
	}
}
