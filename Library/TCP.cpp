/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#ifndef _WIN32
#include <arpa/inet.h> // For inet_addr() and INADDR_ANY
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#elif defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

#elif defined(__ANDROID__)

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <android/log.h>
#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

#endif

#include "TCP.h"

namespace TCP
{

	const int TCP::Server::MAX_CONN;
	// TO DO: create a BaseSocket class and clean up between files Network (AC streamers) and TCP (low level TCP connections)

	void ServerConnection::Lock()
	{
		is_locked = true;
	}

	void ServerConnection::Unlock()
	{
		is_locked = false;
	}

	void ServerConnection::Close()
	{
		std::lock_guard<std::mutex> lock(mtx);
		CloseUnsafe();
	}

	void ServerConnection::CloseUnsafe()
	{
		if (sock != -1)
		{
			closesocket(sock);
			sock = -1;
		}
		msg.clear();
		out.clear();
	}

	void ServerConnection::Start(SOCKET s)
	{
		msg.clear();
		out.clear();
		stamp = std::time(nullptr);
		sock = s;
	}

	int ServerConnection::Inactive(std::time_t now)
	{
		return (int)((long int)now - (long int)stamp);
	}

	void ServerConnection::Read()
	{
		std::lock_guard<std::mutex> lock(mtx);

		char buffer[1024];

		if (isConnected())
		{

			int nread = recv(sock, buffer, sizeof(buffer), 0);
#ifdef _WIN32
			if (nread == 0 || (nread < 0 && WSAGetLastError() != WSAEWOULDBLOCK))
			{
				int e = WSAGetLastError();
#else
			if (nread == 0 || (nread < 0 && errno != EWOULDBLOCK && errno != EAGAIN))
			{
				int e = errno;
#endif
				if (nread < 0 && verbose)
					Error() << "Socket: connection closed by error: " << strerror(e) << ", sock = " << sock;

				CloseUnsafe();
			}
			else if (nread > 0)
			{
				msg += std::string(buffer, nread);
				stamp = std::time(0);
			}
		}
	}

	void ServerConnection::SendBuffer()
	{
		std::lock_guard<std::mutex> lock(mtx);
		SendBufferUnsafe();
	}
	
	void ServerConnection::SendBufferUnsafe()
	{
		if (isConnected() && hasSendBuffer())
		{

			int bytes = ::send(sock, out.data(), out.size(), 0);

			if (bytes < 0)
			{
#ifdef _WIN32
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
#else
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
#endif
					if (verbose)
						Error() << "TCP Connection: error message to client: " << strerror(errno);

					CloseUnsafe();
				}
			}
			else if (bytes < out.size())
				out.erase(out.begin(), out.begin() + bytes);
			else
				out.clear();
		}
	}

	bool ServerConnection::Send(const char *data, int length)
	{
		std::lock_guard<std::mutex> lock(mtx);

		bool has_data_at_start = !out.empty();

		if (!isConnected())
			return false;

		if (out.size() + length > MAX_BUFFER_SIZE)
			return false;

		out.insert(out.end(), data, data + length);

		if(!has_data_at_start)
		{
			SendBufferUnsafe();
		}
		
		return true;
	}

	bool ServerConnection::SendRaw(const char *data, int length)
	{
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected())
			return false;

		int bytes = ::send(sock, data, length, 0);

		if (bytes < 0)
		{
#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
#else
			if (errno != EWOULDBLOCK && errno != EAGAIN)
			{
#endif
				if (verbose)
					Error() << "TCP Connection: error message to client: " << strerror(errno);

				CloseUnsafe();
				return false;
			}
			bytes = 0;
		}
		return true;
	}

	bool ServerConnection::SendDirect(const char *data, int length)
	{
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected())
			return false;

		int bytes = 0;

		if (!hasSendBuffer())
		{
			bytes = ::send(sock, data, length, 0);

			if (bytes < 0)
			{
#ifdef _WIN32
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
#else
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
#endif
					if (verbose)
						Error() << "TCP Connection: error message to client: " << strerror(errno);

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

	Server::~Server()
	{
		stop = true;

		for (auto &c : client)
		{
			c.Close();
		}

		if (run_thread.joinable())
			run_thread.join();
		if (sock != -1)
			closesocket(sock);
	}

	int Server::numberOfClients()
	{
		int n = 0;
		for (auto &c : client)
			if (c.isConnected())
				n++;
		return n;
	}

	int Server::findFreeClient()
	{
		for (int i = 0; i < MAX_CONN; i++)
			if (!client[i].isLocked() && !client[i].isConnected())
				return i;
		return -1;
	}

	void Server::acceptClients()
	{
		int addrlen = sizeof(service);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR *)&service, (socklen_t *)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				Error() << "TCP listener: error accepting connection. " << strerror(WSAGetLastError());
			return;
		}
#else
		if (conn_socket == -1)
		{
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				Error() << "TCP Server: error accepting connection. " << strerror(errno);
			return;
		}
#endif
		else
		{
			int ptr = findFreeClient();
			if (ptr == -1)
			{
				Error() << "TCP Server: max connections reached (" << MAX_CONN << ", closing socket.";
				closesocket(conn_socket);
			}
			else
			{
				client[ptr].Start(conn_socket);

				int flag = 1;
				if (setsockopt(conn_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) != 0)
				{
					Error() << "TCP Server: cannot set TCP_NODELAY on client socket.";
					client[ptr].Close();
					return;
				}

				if (!setNonBlock(conn_socket))
				{
					Error() << "TCP Server: cannot make client socket non-blocking.";
					client[ptr].Close();
				}
			}
		}
	}

	void Server::cleanUp()
	{

		for (auto &c : client)
			if (c.isConnected() && timeout && c.Inactive(time(0)) > timeout && !c.isLocked())
			{
				c.Close();
			}
	}

	void Server::readClients()
	{

		for (auto &c : client)
			c.Read();
	}

	void Server::writeClients()
	{

		for (auto &c : client)
			c.SendBuffer();
	}

	void Server::processClients()
	{
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				c.msg.clear();
			}
		}
	}

	void Server::Run()
	{
		try
		{
			while (!stop)
			{
				acceptClients();
				readClients();
				processClients();
				writeClients();
				cleanUp();

				if (!stop)
					SleepAndWait();
			}

			Info() << "TCP Server: thread ending.\n";
		}
		catch (std::exception &e)
		{
			Error() << "Server Run: " << e.what();
			StopRequest();
		}
	}

	void Server::SleepAndWait()
	{
		struct timeval tv;
		fd_set fds, fdw;

		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		FD_ZERO(&fdw);

		int maxfds = sock;

		for (auto &c : client)
		{
			if (c.isConnected())
			{
				FD_SET(c.sock, &fds);
				if (c.sock > maxfds)
					maxfds = c.sock;

				if (c.hasSendBuffer())
				{
					FD_SET(c.sock, &fdw);
				}
			}
		}

		tv = {1, 0};
		select(maxfds + 1, &fds, &fdw, NULL, &tv);
	}

	bool Server::SendAll(const std::string &m)
	{
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				if (!c.Send(m.c_str(), m.length()))
				{
					c.Close();
					Error() << "TCP listener: client not reading, close connection.";
					return false;
				}
			}
		}
		return true;
	}

	bool Server::SendAllDirect(const std::string &m)
	{
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				if (!c.SendRaw(m.c_str(), m.length()))
				{
					c.Close();
					Error() << "TCP listener: client not reading, close connection.";
					return false;
				}
			}
		}
		return true;
	}

	bool Server::setNonBlock(SOCKET s)
	{

#ifndef _WIN32
		int r = fcntl(s, F_GETFL, 0);
		r = fcntl(s, F_SETFL, r | O_NONBLOCK);

		if (r == -1)
			return false;
#else
		u_long mode = 1;
		ioctlsocket(s, FIONBIO, &mode);
#endif
		return true;
	}

	bool Server::start(int port)
	{

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return false;

#ifndef _WIN32
		if (reuse_port)
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		}
#endif
		memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;

		// Conditional binding based on IP_BIND
		if (!IP_BIND.empty())
		{
			service.sin_addr.s_addr = inet_addr(IP_BIND.c_str());
		}
		else
		{
			service.sin_addr.s_addr = htonl(INADDR_ANY);
		}

		service.sin_port = htons(port);

		int r = bind(sock, (SOCKADDR *)&service, sizeof(service));
		if (r == SOCKET_ERROR)
		{
			closesocket(sock);
			sock = -1;
			return false;
		}

		if (listen(sock, 511) < 0)
			return false;

		for (auto &c : client)
		{
			c.Close();
			c.Unlock();
		}

		if (!setNonBlock(sock))
		{
			Error() << "TCP Server: cannot set socket to non-blocking\n";
		}
		stop = false;

		if (IP_BIND.empty())
			Info() << "TCP Server: start thread at port " << port;
		else
			Info() << "TCP Server: start thread at IP " << IP_BIND << " port " << port;

		run_thread = std::thread(&Server::Run, this);

		return true;
	}

	void Client::disconnect()
	{
		if (sock != -1)
		{
			closesocket(sock);
		}

		if (state == READY && verbose)
			Info() << "TCP (" << host << ":" << port << "): disconnected.";

		sock = -1;
		state = DISCONNECTED;
	}

	bool Client::connect(std::string host, std::string port, bool persist, int timeout, bool keep_alive, int idle, int interval, int count)
	{
		if (sock != -1)
		{
			Warning() << "TCP (" << this->host << ":" << this->port << "): connection overwritten. Socket not properly closed.";
		}

		int r;
		struct addrinfo h;
		struct addrinfo *address = nullptr;

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
		if (code != 0 || address == nullptr)
			return false;

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (sock == -1)
		{
			freeaddrinfo(address);
			return false;
		}

		int flag = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) != 0)
		{
			freeaddrinfo(address);
			disconnect();
			return false;
		}

#ifndef _WIN32
		if (keep_alive)
		{
			int yes = 1;

#ifdef _WIN32
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&yes, sizeof(yes)))
#else
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&yes, sizeof(yes)))
#endif
			{
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#if defined(__APPLE__)
			if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)))
			{
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#elif defined(_WIN32)
			// Windows specific keepalive
			struct tcp_keepalive keepalive;
			keepalive.onoff = 1;
			keepalive.keepalivetime = idle * 1000;
			keepalive.keepaliveinterval = interval * 1000;
			DWORD br;
			if (WSAIoctl(sock, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &br, NULL, NULL) == SOCKET_ERROR)
			{
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#elif defined(__ANDROID__)
			// Android uses same config as Linux
			if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) ||
				setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) ||
				setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)))
			{
				freeaddrinfo(address);

				disconnect();
				return false;
			}
#else
			if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) ||
				setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) ||
				setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count)))
			{
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#endif
		}
#endif
		if (persistent)
		{
#ifndef _WIN32
			r = fcntl(sock, F_GETFL, 0);
			r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

			if (r == -1)
			{
				freeaddrinfo(address);
				disconnect();
				return false;
			}
#else
			u_long mode = 1; // 1 to enable non-blocking socket
			ioctlsocket(sock, FIONBIO, &mode);
#endif
		}

		stamp = time(nullptr);

		r = ::connect(sock, address->ai_addr, (int)address->ai_addrlen);
		freeaddrinfo(address);

		if (r != -1)
		{
			state = READY;

			if (verbose)
				Info() << "TCP (" << host << ":" << port << "): connected.";

			return true;
		}

#ifndef _WIN32
		if (errno != EINPROGRESS)
		{
			disconnect();
			return false;
		}
#else
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			disconnect();
			return false;
		}
#endif

		return isConnected(timeout);
	}

	bool Client::isConnected(int t)
	{

		if (state == READY)
			return true;
		if (sock == -1)
			return false;

		fd_set fdr, fdw;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = {t, 1};

		if (select(sock + 1, &fdr, &fdw, nullptr, &to) > 0)
		{
			int error;
			socklen_t len = sizeof(error);

			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
			if (error != 0)
				return false;

			state = READY;

			if (verbose)
				Info() << "TCP (" << host << ":" << port << "): connected.";

			connects++;
			//if (onConnected)
			//	onConnected();

			return true;
		}

		state = CONNECTING;
		return false;
	}

	void Client::updateState()
	{
		if (state == READY && reset_time > 0 && (long)time(nullptr) - (long)stamp > reset_time * 60)
		{
			Warning() << "TCP (" << host << ":" << port << "): connection expired, reconnect.";
			reconnect();
		}
		else if (state == DISCONNECTED)
		{
			if ((long)time(nullptr) - (long)stamp > 10)
			{
				Warning() << "TCP (" << host << ":" << port << "): not connected, reconnecting.";
				reconnect();
			}
		}
		else if (state == CONNECTING)
		{
			bool connected = isConnected(0);

			if (connected)
			{
				// Warning() << "TCP (" << host << ":" << port << "): connected to server.";
			}
			else if ((long)time(nullptr) - (long)stamp > 10)
			{
				Warning() << "TCP (" << host << ":" << port << "): timeout connecting to server, reconnect.";
				reconnect();
				return;
			}
		}
	}

	void Client::updateConnection()
	{
		updateState();
	}

	int Client::send(const void *data, int length)
	{

		updateState();

		if (state == READY)
		{
			int sent = ::send(sock, (char *)data, length, 0);

			if (sent < length)
			{
				int error_code = errno;
#ifdef _WIN32
				if (error_code == WSAEWOULDBLOCK)
					return 0;
#else
				if (error_code == EAGAIN || error_code == EWOULDBLOCK || error_code == EINPROGRESS)
				{
					Error() << "TCP (" << host << ":" << port << "): message might be lost. Error code: " << error_code << " (" << strerror(error_code) << ").";
					return 0;
				}
#endif

				Error() << "TCP (" << host << ":" << port << "): send error. Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");
				if (persistent)
				{
					reconnect();
					return 0;
				}
				else
				{
					return -1;
				}
			}
			return sent;
		}

		return 0;
	}

	// zero if no input yet or connection being established
	int Client::read(void *data, int length, int t, bool wait)
	{

		updateState();

		if (state == READY)
		{
			fd_set fd, fe;

			FD_ZERO(&fd);
			FD_SET(sock, &fd);

			FD_ZERO(&fe);
			FD_SET(sock, &fe);

			timeval to = {t, 0};

			if (select(sock + 1, &fd, nullptr, &fe, &to) < 0)
				return 0;

			if (FD_ISSET(sock, &fd) || FD_ISSET(sock, &fe))
			{
				int retval = recv(sock, (char *)data, length, wait ? MSG_WAITALL : 0);

				if (retval <= 0)
				{
					int error_code = errno;
#ifdef _WIN32
					if (error_code == WSAEWOULDBLOCK)
						return 0;
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK)
						return 0;
#endif
					Error() << "TCP (" << host << ":" << port << ") receive error with recv returns " << retval << ". Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");

					if (persistent)
					{
						reconnect();
						return 0;
					}
					else
					{
						return -1;
					}
				}
				return retval;
			}
		}

		return 0;
	}
}