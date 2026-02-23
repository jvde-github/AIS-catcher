/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include "TCPServer.h"

namespace IO
{

	const int TCPServer::MAX_CONN;
	std::vector<int> TCPServer::active_ports;

	void TCPServerConnection::Lock()
	{
		is_locked = true;
	}

	void TCPServerConnection::Unlock()
	{
		is_locked = false;
	}

	void TCPServerConnection::Close()
	{
		std::lock_guard<std::mutex> lock(mtx);
		CloseUnsafe();
	}

	void TCPServerConnection::CloseUnsafe()
	{
		if (sock != -1)
		{
			closesocket(sock);
			sock = -1;
		}
		msg.clear();
		out.clear();
	}

	void TCPServerConnection::Start(SOCKET s)
	{
		msg.clear();
		out.clear();
		stamp = std::time(nullptr);
		sock = s;
	}

	int TCPServerConnection::Inactive(std::time_t now)
	{
		return (int)((long int)now - (long int)stamp);
	}

	void TCPServerConnection::Read()
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
	void TCPServerConnection::SendBuffer()
	{
		std::lock_guard<std::mutex> lock(mtx);

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
	bool TCPServerConnection::Send(const char *data, int length)
	{
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected())
			return false;

		if (out.size() + length > MAX_BUFFER_SIZE)
			return false;

		out.insert(out.end(), data, data + length);
		return true;
	}

	bool TCPServerConnection::SendRaw(const char *data, int length)
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
		}
		return true;
	}

	bool TCPServerConnection::SendDirect(const char *data, int length)
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

	TCPServer::~TCPServer()
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

		// Remove port from active_ports
		if (listening_port != -1)
		{
			for (auto it = active_ports.begin(); it != active_ports.end(); ++it)
			{
				if (*it == listening_port)
				{
					active_ports.erase(it);
					break;
				}
			}
		}
	}

	int TCPServer::numberOfClients()
	{
		int n = 0;
		for (auto &c : client)
			if (c.isConnected())
				n++;
		return n;
	}

	int TCPServer::findFreeClient()
	{
		for (int i = 0; i < MAX_CONN; i++)
			if (!client[i].isLocked() && !client[i].isConnected())
				return i;
		return -1;
	}

	void TCPServer::acceptClients()
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

	void TCPServer::cleanUp()
	{

		for (auto &c : client)
			if (c.isConnected() && timeout && c.Inactive(time(0)) > timeout && !c.isLocked())
			{
				c.Close();
			}
	}

	void TCPServer::readClients()
	{

		for (auto &c : client)
			c.Read();
	}

	void TCPServer::writeClients()
	{

		for (auto &c : client)
			c.SendBuffer();
	}

	void TCPServer::processClients()
	{
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				c.msg.clear();
			}
		}
	}

	void TCPServer::Run()
	{
		while (!stop)
		{
			try
			{
				acceptClients();
				readClients();
				processClients();
				writeClients();
				cleanUp();

				if (pstats)
				{
					pstats->connected = numberOfClients();
				}
			}
			catch (const std::exception &e)
			{
				Error() << "TCP Server: exception in main loop: " << e.what() << ", closing all connections";
				for (auto &c : client)
					c.Close();
			}

			if (!stop)
				SleepAndWait();
		}

		Debug() << "TCP Server: thread ending.\n";
	}

	void TCPServer::SleepAndWait()
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

	bool TCPServer::SendAll(const std::string &m)
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

				if (pstats)
					pstats->bytes_out += m.length();
			}
		}
		return true;
	}

	bool TCPServer::SendAllDirect(const std::string &m)
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

				if (pstats)
					pstats->bytes_out += m.length();
			}
		}
		return true;
	}

	bool TCPServer::setNonBlock(SOCKET s)
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

	bool TCPServer::start(int port)
	{
		// Check if port is already in use
		for (const auto &p : active_ports)
		{
			if (p == port)
			{
				Error() << "TCP Server: port " << port << " is already in use by another server instance";
				return false;
			}
		}

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
			Debug() << "TCP Server: start thread at port " << port;
		else
			Debug() << "TCP Server: start thread at IP " << IP_BIND << " port " << port;

		listening_port = port;
		active_ports.push_back(port);

		run_thread = std::thread(&TCPServer::Run, this);

		return true;
	}
}