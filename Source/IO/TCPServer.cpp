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
#include "Logger.h"

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

	void TCPServerConnection::Close()
	{
		if (sock != -1)
		{
			Net::closeSocket(sock);
			sock = -1;
		}
		releaseBuffers();
	}

	void TCPServerConnection::Start(SOCKET s)
	{
		releaseBuffers();
		close_after_send = false;
		head_request = false;
		continue_sent = false;
		no_timeout = false;
		verbose = true;
		request_start = 0;
		stamp = std::time(nullptr);
		++generation; // new incarnation: invalidates any handle held to the old one
		sock = s;
	}

	int TCPServerConnection::Inactive(std::time_t now) const
	{
		return (int)std::difftime(now, stamp);
	}

	void TCPServerConnection::Read()
	{
		char buffer[16384];

		if (isConnected())
		{
			int nread = recv(sock, buffer, sizeof(buffer), 0);

			if (nread > 0)
			{
				if (msg.size() + nread > MAX_BUFFER_SIZE)
				{
					Warning() << "TCPServer: input buffer overflow, closing connection";
					Close();
				}
				else
				{
					msg.append(buffer, nread);
					stamp = std::time(nullptr);
				}
			}
			else if (nread == 0)
			{
				Close();
			}
			else
			{
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose && !Net::peerGone(e))
						Debug() << "Socket: read failed: " << Net::errorString(e) << ", sock = " << sock;

					Close();
				}
			}
		}
	}
	void TCPServerConnection::SendBuffer()
	{
		if (isConnected() && hasSendBuffer())
		{

			int bytes = ::send(sock, out.data() + out_pos, pending(), 0);

			if (bytes < 0)
			{
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose && !Net::peerGone(e))
						Error() << "TCP Connection: error message to client: " << Net::errorString(e);

					Close();
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
	bool TCPServerConnection::queue(const char *data, int length)
	{
		if (out_pos >= OUT_COMPACT_THRESHOLD)
			compact();

		if (pending() + length > MAX_BUFFER_SIZE)
			return false;

		out.insert(out.end(), data, data + length);
		return true;
	}

	bool TCPServerConnection::Send(const char *data, int length)
	{
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
					if (verbose && !Net::peerGone(e))
						Error() << "TCP Connection: error message to client: " << Net::errorString(e);

					Close();
					return false;
				}
				bytes = 0;
			}
		}

		// A client this far behind (e.g. a stalled SSE consumer) will not
		// recover; close instead of growing the buffer without bound.
		if (bytes < length && !queue(data + bytes, length - bytes))
		{
			if (verbose)
				Error() << "TCP Connection: send buffer limit exceeded, closing connection.";

			Close();
			return false;
		}

		return true;
	}

	// TCP Server

	void TCPServer::stopThread()
	{
		stop = true;
		wakeup.notify(); // break out of poll() now rather than on the backstop

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
			if (!client[i].isConnected())
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
			if (!Net::wouldBlock(e) && !Net::peerGone(e))
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
		// TCP_NODELAY is best-effort: a client that reset since accept() fails it,
		// and that is not a reason to drop the connection here.
		int flag = 1;
		setsockopt(conn_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

		const int idle = 60, interval = 20, count = 3;
		Net::setTCPKeepAlive(conn_socket, idle, interval, count);
		Net::setTCPUserTimeout(conn_socket, (idle + interval * count) * 1000);

		// not optional: a blocking client socket would stall the loop
		if (!Net::setNonBlocking(conn_socket))
		{
			int e = Net::lastError();
			if (!Net::peerGone(e))
				Error() << "TCP Server: cannot make client socket non-blocking: " << Net::errorString(e);
			Net::closeSocket(conn_socket);
			return;
		}

		client[ptr].Start(conn_socket);
	}

	void TCPServer::cleanUp(std::time_t now)
	{
		if (!timeout)
			return;

		for (auto &c : client)
			if (c.isConnected() && c.Inactive(now) > timeout && !c.no_timeout)
			{
				c.Close();
			}
	}

	void TCPServer::readClients()
	{
		for (int i = 0; i < MAX_CONN; i++)
			if (pfds[i + 2].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
				client[i].Read();
	}

	void TCPServer::writeClients()
	{

		for (auto &c : client)
		{
			c.SendBuffer();

			if (c.close_after_send && c.isConnected() && !c.hasSendBuffer())
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
				// execute all queued cross-thread work here, on the sole owner
				// thread; drain the wake first so a command posted mid-cycle is
				// either already in the queue or re-wakes the next poll
				if (pfds[1].revents & POLLIN)
					wakeup.drain();
				drainCommands();
				if (pfds[0].revents & POLLIN)
					acceptClients();
				readClients();
				processClients();
				writeClients();

				// the loop wakes per command now, so keep these sweeps at ~1 Hz
				const std::time_t now = std::time(nullptr);
				if (now != last_housekeeping)
				{
					last_housekeeping = now;
					cleanUp(now);

					if (pstats)
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

		// the wake handle; -1 when unavailable so poll simply skips it
		pfds[1].fd = wakeup.valid() ? wakeup.fd() : -1;
		pfds[1].events = POLLIN;
		pfds[1].revents = 0;

		for (int i = 0; i < MAX_CONN; i++)
		{
			TCPServerConnection &c = client[i];
			pollfd &p = pfds[i + 2];

			p.fd = c.isConnected() ? c.sock : -1;
			p.events = POLLIN;
			if (c.hasSendBuffer())
				p.events |= POLLOUT;
			p.revents = 0;
		}

		// With the wake handle latency comes from poll() returning on the wake, so
		// this timeout is just a liveness backstop — keep it long (1/s idle, gentle
		// on low-power boards). Only when the wake is unavailable does it also carry
		// command latency, so it tightens to 50 ms then.
		const int POLL_MS = wakeup.valid() ? 1000 : 50;
#ifdef _WIN32
		int r = WSAPoll(pfds.data(), (ULONG)pfds.size(), POLL_MS);
#else
		int r = poll(pfds.data(), (nfds_t)pfds.size(), POLL_MS);
#endif
		// on poll failure fall back to sweeping every socket rather than none
		if (r < 0)
			for (auto &p : pfds)
				p.revents = POLLIN;
	}

	void TCPServer::post(Command c)
	{
		{
			std::lock_guard<std::mutex> lk(cmd_mtx);
			cmds.push_back(std::move(c));
		}
		// wake the loop now; the byte is coalesced so a burst can't flood the fd
		wakeup.notify();
	}

	void TCPServer::drainCommands()
	{
		{
			std::lock_guard<std::mutex> lk(cmd_mtx);
			if (cmds.empty())
				return;
			cmd_scratch.swap(cmds);
		}
		for (auto &c : cmd_scratch)
		{
			switch (c.kind)
			{
			case Command::BroadcastRaw:
				for (auto &cl : client)
				{
					if (cl.isConnected() && !Send(cl, c.data.data(), (int)c.data.size()))
					{
						cl.Close();
						Error() << "TCP listener: client not reading, close connection.";
						if (pstats)
							pstats->dropped++;
					}
				}
				break;
			case Command::Derived:
				onCommand(c.id, c.data);
				break;
			}
		}
		cmd_scratch.clear();
	}

	// Cross-thread producers only enqueue; the Run() loop performs the actual
	// sends, so no other thread ever touches a connection.
	void TCPServer::SendAll(std::string m)
	{
		post({Command::BroadcastRaw, 0, std::move(m)});
	}

	bool TCPServer::start(int port)
	{
		for (auto &c : client)
			c.owner = this;

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
			c.Close();

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

		// not fatal if it fails: the loop still drains on the poll timeout
		if (!wakeup.init())
			Debug() << "TCP Server: wake handle unavailable, relying on poll timeout.";

		run_thread = std::thread(&TCPServer::Run, this);

		return true;
	}
}