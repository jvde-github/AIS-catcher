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

#include <algorithm>
#include <cstring>
#include <string>

#include "TCPServer.h"

#ifdef _WIN32
#include <mstcpip.h>
#else
#include <arpa/inet.h> // For inet_addr() and INADDR_ANY
#include <netinet/tcp.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#endif

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
			Net::closeSocket(sock);
			sock = -1;
		}
		releaseBuffers();
	}

	void TCPServerConnection::Start(SOCKET s, bool local)
	{
		releaseBuffers();
		close_after_send = false;
		head_request = false;
		request_start = 0;
		stamp = std::time(nullptr);
		is_local = local;
		sock = s;
	}

	int TCPServerConnection::Inactive(std::time_t now) const
	{
		return (int)std::difftime(now, stamp);
	}

	void TCPServerConnection::Read()
	{
		std::lock_guard<std::mutex> lock(mtx);

		char buffer[16384];

		if (isConnected())
		{
			int nread = recv(sock, buffer, sizeof(buffer), 0);

			if (nread > 0)
			{
				if (msg.size() + nread > MAX_BUFFER_SIZE)
				{
					Warning() << "TCPServer: input buffer overflow, closing connection";
					CloseUnsafe();
				}
				else
				{
					msg.append(buffer, nread);
					stamp = std::time(nullptr);
				}
			}
			else if (nread == 0)
			{
				CloseUnsafe();
			}
			else
			{
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose)
						Error() << "Socket: connection closed by error: " << Net::errorString(e) << ", sock = " << sock;

					CloseUnsafe();
				}
			}
		}
	}
	void TCPServerConnection::SendBuffer()
	{
		std::lock_guard<std::mutex> lock(mtx);

		if (isConnected() && hasSendBuffer())
		{

			int bytes = ::send(sock, out.data() + out_pos, pending(), 0);

			if (bytes < 0)
			{
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose)
						Error() << "TCP Connection: error message to client: " << Net::errorString(e);

					CloseUnsafe();
				}
			}
			else
			{
				out_pos += bytes;
				if (out_pos == out.size())
				{
					shrink(out);
					out_pos = 0;
				}
			}
		}
	}
	bool TCPServerConnection::Send(const char *data, int length)
	{
		std::lock_guard<std::mutex> lock(mtx);

		if (!isConnected())
			return false;

		if (out_pos >= OUT_COMPACT_THRESHOLD)
			compact();

		if (pending() + length > MAX_BUFFER_SIZE)
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
			int e = Net::lastError();
			if (!Net::wouldBlock(e))
			{
				if (verbose)
					Error() << "TCP Connection: error message to client: " << Net::errorString(e);

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
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose)
						Error() << "TCP Connection: error message to client: " << Net::errorString(e);

					CloseUnsafe();
					return false;
				}
				bytes = 0;
			}
		}

		if (bytes < length)
		{
			// A client this far behind (e.g. a stalled SSE consumer) will not
			// recover; close instead of growing the buffer without bound.
			if (out_pos >= OUT_COMPACT_THRESHOLD)
				compact();

			if (pending() + (length - bytes) > MAX_BUFFER_SIZE)
			{
				if (verbose)
					Error() << "TCP Connection: send buffer limit exceeded, closing connection.";

				CloseUnsafe();
				return false;
			}
			out.insert(out.end(), data + bytes, data + length);
		}

		return true;
	}

	// TCP Server

	void TCPServer::stopThread()
	{
		stop = true;

		if (run_thread.joinable())
			run_thread.join();
	}

	TCPServer::~TCPServer()
	{
		stopThread();

		for (auto &c : client)
		{
			c.Close();
		}

		if (sock != -1)
			Net::closeSocket(sock);

		if (listening_port != -1)
		{
			auto it = std::find(active_ports.begin(), active_ports.end(), listening_port);
			if (it != active_ports.end())
				active_ports.erase(it);
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
		sockaddr_in peer;
		int addrlen = sizeof(peer);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR *)&peer, (socklen_t *)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR)
#else
		if (conn_socket == -1)
#endif
		{
			int e = Net::lastError();
			if (!Net::wouldBlock(e))
				Error() << "TCP Server: error accepting connection: " << Net::errorString(e);
			return;
		}

		int ptr = findFreeClient();
		if (ptr == -1)
		{
			Error() << "TCP Server: max connections reached (" << MAX_CONN << "), closing socket.";
			Net::closeSocket(conn_socket);
			return;
		}

		// Configure fully before Start() publishes the socket to broadcast threads.
		int flag = 1;
		if (setsockopt(conn_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) != 0)
		{
			Error() << "TCP Server: cannot set TCP_NODELAY on client socket.";
			Net::closeSocket(conn_socket);
			return;
		}

		const int idle = 60, interval = 20, count = 3;
		Net::setTCPKeepAlive(conn_socket, idle, interval, count);
		Net::setTCPUserTimeout(conn_socket, (idle + interval * count) * 1000);

		if (!Net::setNonBlocking(conn_socket))
		{
			Error() << "TCP Server: cannot make client socket non-blocking.";
			Net::closeSocket(conn_socket);
			return;
		}

		bool local = addrlen >= (int)sizeof(peer) && peer.sin_family == AF_INET &&
					 (ntohl(peer.sin_addr.s_addr) >> 24) == 127;
		client[ptr].Start(conn_socket, local);
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
		for (int i = 0; i < MAX_CONN; i++)
			if (pfds[i + 1].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
				client[i].Read();
	}

	void TCPServer::writeClients()
	{

		for (auto &c : client)
		{
			c.SendBuffer();

			if (c.close_after_send && c.isConnected() && !c.isLocked() && !c.hasSendBuffer())
				c.Close();
		}
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
			SleepAndWait();

			try
			{
				if (pfds[0].revents & POLLIN)
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
		}

		Debug() << "TCP Server: thread ending.";
	}

	void TCPServer::SleepAndWait()
	{
		// poll() rather than select(): keyed on fd values with no FD_SETSIZE ceiling,
		// so a high-numbered socket in a busy process can't overflow an fd_set.
		// Blocks until a socket is ready or 1s elapses; the revents left in pfds
		// tell Run() which sockets to accept/read.
		pfds[0].fd = sock;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		for (int i = 0; i < MAX_CONN; i++)
		{
			TCPServerConnection &c = client[i];
			pollfd &p = pfds[i + 1];

			p.fd = c.isConnected() ? c.sock : -1;
			p.events = POLLIN;
			if (c.hasSendBuffer())
				p.events |= POLLOUT;
			p.revents = 0;
		}

#ifdef _WIN32
		int r = WSAPoll(pfds.data(), (ULONG)pfds.size(), 1000);
#else
		int r = poll(pfds.data(), (nfds_t)pfds.size(), 1000);
#endif
		// on poll failure fall back to sweeping every socket rather than none
		if (r < 0)
			for (auto &p : pfds)
				p.revents = POLLIN;
	}

	bool TCPServer::SendAll(const std::string &m)
	{
		bool success = true;
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				if (!c.Send(m.c_str(), m.length()))
				{
					c.Close();
					Error() << "TCP listener: client not reading, close connection.";
					if (pstats)
						pstats->dropped++;
					success = false;
				}
				else if (pstats)
					pstats->bytes_out += m.length();
			}
		}
		return success;
	}

	bool TCPServer::SendAllDirect(const std::string &m)
	{
		return SendAllDirect(m.c_str(), m.length());
	}

	bool TCPServer::SendAllDirect(const char *data, int len)
	{
		bool success = true;
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				if (!c.SendRaw(data, len))
				{
					c.Close();
					Error() << "TCP listener: client not reading, close connection.";
					success = false;
				}
				else if (pstats)
					pstats->bytes_out += len;
			}
		}
		return success;
	}

	bool TCPServer::start(int port)
	{
		for (const auto &p : active_ports)
		{
			if (port && p == port)
			{
				Error() << "TCP Server: port " << port << " is already in use by another server instance";
				return false;
			}
		}

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return false;

#ifndef _WIN32
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		}
		if (reuse_port)
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		}
#endif
		sockaddr_in service;
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
			Net::closeSocket(sock);
			sock = -1;
			return false;
		}

		if (listen(sock, 511) < 0)
		{
			Net::closeSocket(sock);
			sock = -1;
			return false;
		}

		if (port == 0)
		{
			struct sockaddr_in bound;
			socklen_t len = sizeof(bound);
			if (getsockname(sock, (SOCKADDR *)&bound, &len) == 0)
				port = ntohs(bound.sin_port);
		}

		for (auto &c : client)
		{
			c.Close();
			c.Unlock();
		}

		if (!Net::setNonBlocking(sock))
		{
			Error() << "TCP Server: cannot set socket to non-blocking";
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