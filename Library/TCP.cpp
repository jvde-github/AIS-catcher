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

	// TO DO: create a BaseSocket class and clean up between files Network (AC streamers) and TCP (low level TCP connections)

	void ServerConnection::Lock() {
		is_locked = true;
	}

	void ServerConnection::Unlock() {
		is_locked = false;
	}

	void ServerConnection::Close() {
		std::lock_guard<std::mutex> lock(mtx);
		CloseUnsafe();
	}

	void ServerConnection::CloseUnsafe() {
		if (sock != -1) {
			closesocket(sock);
			sock = -1;
		}
		msg.clear();
		out.clear();
	}

	void ServerConnection::Start(SOCKET s) {
		msg.clear();
		out.clear();
		stamp = std::time(0);
		sock = s;
	}

	int ServerConnection::Inactive(std::time_t now) {
		return (int)((long int)now - (long int)stamp);
	}

	void ServerConnection::Read() {
		std::lock_guard<std::mutex> lock(mtx);

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

				CloseUnsafe();
			}
			else if (nread > 0) {
				msg += std::string(buffer, nread);
				stamp = std::time(0);
			}
		}
	}

	void ServerConnection::SendBuffer() {
		std::lock_guard<std::mutex> lock(mtx);

		if (isConnected() && hasSendBuffer()) {

			int bytes = ::send(sock, out.data(), out.size(), 0);

			if (bytes < 0) {
#ifdef _WIN32
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
				if (errno != EWOULDBLOCK && errno != EAGAIN) {
#endif
					std::cerr << "TCP Connection: error message to client: " << strerror(errno) << std::endl;
					CloseUnsafe();
				}
			}
			else if (bytes < out.size())
				out.erase(out.begin(), out.begin() + bytes);
			else
				out.clear();
		}
	}
	bool ServerConnection::Send(const char* data, int length) {
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected()) return false;

		if (out.size() + length > 1024 * 1024) return false;

		out.insert(out.end(), data, data + length);
		return true;
	}

	bool ServerConnection::SendDirect(const char* data, int length) {
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected()) return false;

		int bytes = 0;

		if (!hasSendBuffer()) {
			bytes = ::send(sock, data, length, 0);

			if (bytes < 0) {
#ifdef _WIN32
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
				if (errno != EWOULDBLOCK && errno != EAGAIN) {
#endif
					std::cerr << "TCP Connection: error message to client: " << strerror(errno) << std::endl;
					CloseUnsafe();
					return false;
				}
				bytes = 0;
			}
		}

		if (bytes < length)
			out.insert(out.end(), data + bytes, data + length - bytes);

		return true;
	}

	// TCP Server

	Server::~Server() {
		stop = true;
		if (run_thread.joinable()) run_thread.join();
		if (sock != -1) closesocket(sock);
	}

	int Server::numberOfClients() {
		int n = 0;
		for (auto& c : client) if (c.isConnected()) n++;
		return n;
	}

	int Server::findFreeClient() {
		for (int i = 0; i < MAX_CONN; i++)
			if (!client[i].isLocked() && !client[i].isConnected()) return i;
		return -1;
	}

	void Server::acceptClients() {
		int addrlen = sizeof(service);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR*)&service, (socklen_t*)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				std::cerr << "TCP listener: error accepting connection. " << strerror(WSAGetLastError()) << std::endl;
			return;
		}
#else
		if (conn_socket == -1) {
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				std::cerr << "TCP Server: error accepting connection. " << strerror(errno) << std::endl;
			return;
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

	void Server::cleanUp() {

		for (auto& c : client)
			if (c.isConnected() && timeout && c.Inactive(time(0)) > timeout && !c.isLocked()) {
				c.Close();
			}
	}

	void Server::readClients() {

		for (auto& c : client) c.Read();
	}

	void Server::writeClients() {

		for (auto& c : client) c.SendBuffer();
	}

	void Server::processClients() {
		for (auto& c : client) {
			if (c.isConnected()) {
				c.msg.clear();
			}
		}
	}

	void Server::Run() {
		try {
			while (!stop) {
				acceptClients();
				readClients();
				processClients();
				writeClients();
				cleanUp();
				SleepAndWait();
			}

			std::cerr << "TCP Server: thread ending.\n";
		}
		catch (std::exception& e) {
			std::cerr << "Server Run: " << e.what() << std::endl;
			std::terminate();
		}
	}

	void Server::SleepAndWait() {
		struct timeval tv;
		fd_set fds, fdw;

		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		FD_ZERO(&fdw);

		int maxfds = sock;

		for (auto& c : client) {
			if (c.isConnected()) {
				FD_SET(c.sock, &fds);
				if (c.sock > maxfds) maxfds = c.sock;

				if (c.hasSendBuffer()) {
					FD_SET(c.sock, &fdw);
				}
			}
		}

		tv = { 1, 0 };
		select(maxfds + 1, &fds, &fdw, NULL, &tv);
	}

	bool Server::SendAll(const std::string& m) {
		for (auto& c : client) {
			if (c.isConnected()) {
				if (!c.Send(m.c_str(), m.length())) {
					c.Close();
					std::cerr << "TCP listener: client not reading, close connection." << std::endl;
					return false;
				}
			}
		}
		return true;
	}

	bool Server::setNonBlock(SOCKET sock) {

#ifndef _WIN32
		int r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

		if (r == -1) return false;
#else
		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode);
#endif
		return true;
	}

	bool Server::start(int port) {

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

		for (auto& c : client) {
			c.Close();
			c.Unlock();
		}

		if (!setNonBlock(sock)) {
			std::cerr << "TCP Server: cannot set socket to non-blocking\n";
		}
		stop = false;
		std::cerr << "TCP Server: start thread at port " << port << std::endl;
		run_thread = std::thread(&Server::Run, this);

		return true;
	}


	void Client::disconnect() {
		if (sock != -1)
			closesocket(sock);

		sock = -1;
		state = DISCONNECTED;
	}

	bool Client::connect(std::string host, std::string port, bool persist, int timeout, bool keep_alive) {
		int r;
		struct addrinfo h;
		struct addrinfo* address;

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

			if (r == -1) return false;
#else
			u_long mode = 1; // 1 to enable non-blocking socket
			ioctlsocket(sock, FIONBIO, &mode);
#endif
		}

		stamp = time(NULL);

		r = ::connect(sock, address->ai_addr, (int)address->ai_addrlen);
		freeaddrinfo(address);

		if (r != -1) {
			state = READY;
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
					std::cerr << " Failed.\n";
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
						std::cerr << " Failed.\n";
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
