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

#ifdef HASBLUETOOTH

#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2bth.h>		   

#else
#include <sys/socket.h>
#include <bluetooth/bluetooth.h> 
#include <bluetooth/rfcomm.h>	
#include <fcntl.h>
#include <unistd.h>

#endif  

#include "Bluetooth.h" 

namespace Bluetooth
{

	static int str_to_bt_addr(const char *str, bdaddr_t *ba)
	{
		unsigned int b0, b1, b2, b3, b4, b5;
		int result = sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x", &b5, &b4, &b3, &b2, &b1, &b0);
		if (result != 6)
		{
			return -1;
		}
		ba->b[0] = (uint8_t)b0;
		ba->b[1] = (uint8_t)b1;
		ba->b[2] = (uint8_t)b2;
		ba->b[3] = (uint8_t)b3;
		ba->b[4] = (uint8_t)b4;
		ba->b[5] = (uint8_t)b5;
		return 0;
	}

#ifndef _WIN32

	static int find_available_channel()
	{
		for (int channel = 1; channel <= 30; channel++)
		{
			int sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
			if (sock < 0)
				continue;

			struct sockaddr_rc addr;
			memset(&addr, 0, sizeof(addr));
			addr.rc_family = AF_BLUETOOTH;
			addr.rc_bdaddr = {{0, 0, 0, 0, 0, 0}};
			addr.rc_channel = channel;

			if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
			{
				close(sock);
				return channel;
			}
			close(sock);
		}
		return -1;
	}
#endif

	const int Server::MAX_CONN;

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
					Error() << "Bluetooth Socket: connection closed by error: " << strerror(e) << ", sock = " << sock; // Changed: error message from "Socket" to "Bluetooth Socket"

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
						Error() << "Bluetooth Connection: error message to client: " << strerror(errno); // Changed: error message from "TCP Connection" to "Bluetooth Connection"

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

		if (!isConnected())
			return false;

		if (out.size() + length > MAX_BUFFER_SIZE)
			return false;

		out.insert(out.end(), data, data + length);
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
					Error() << "Bluetooth Connection: error message to client: " << strerror(errno);

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
						Error() << "Bluetooth Connection: error message to client: " << strerror(errno);

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

	Server::~Server()
	{
		stop = true;
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
#ifdef _WIN32
		int addrlen = sizeof(SOCKADDR_BTH);
		SOCKADDR_BTH clientaddr;
#else
		int addrlen = sizeof(struct sockaddr_rc);
		struct sockaddr_rc clientaddr;
#endif
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR *)&clientaddr, (socklen_t *)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				Error() << "Bluetooth listener: error accepting connection. " << strerror(WSAGetLastError());
			return;
		}
#else
		if (conn_socket == -1)
		{
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				Error() << "Bluetooth Server: error accepting connection. " << strerror(errno);
			return;
		}
#endif
		else
		{
			int ptr = findFreeClient();
			if (ptr == -1)
			{
				Error() << "Bluetooth Server: max connections reached (" << MAX_CONN << ", closing socket.";
				closesocket(conn_socket);
			}
			else
			{
				client[ptr].Start(conn_socket);

				if (!setNonBlock(conn_socket))
				{
					Error() << "Bluetooth Server: cannot make client socket non-blocking.";
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
				SleepAndWait();
			}

			Info() << "Bluetooth Server: thread ending.\n";
		}
		catch (std::exception &e)
		{
			Error() << "Server Run: " << e.what();
			std::terminate();
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
					Error() << "Bluetooth listener: client not reading, close connection.";
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
					Error() << "Bluetooth listener: client not reading, close connection.";
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

	bool Server::start(int channel)
	{
		current_channel = channel;

#ifdef _WIN32
		sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
#else
		sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
#endif
		if (sock < 0)
			return false;

#ifndef _WIN32
		if (reuse_channel)
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		}
#endif
		memset(&service, 0, sizeof(service));

#ifdef _WIN32
		service.addressFamily = AF_BTH;
		service.serviceClassId = RFCOMM_PROTOCOL_UUID;
		service.port = channel;

		if (!BT_ADDR.empty())
		{

			BTH_ADDR btAddr = 0;
			sscanf(BT_ADDR.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
				   ((unsigned char *)&btAddr) + 5, ((unsigned char *)&btAddr) + 4, ((unsigned char *)&btAddr) + 3,
				   ((unsigned char *)&btAddr) + 2, ((unsigned char *)&btAddr) + 1, ((unsigned char *)&btAddr) + 0);
			service.btAddr = btAddr;
		}
		else
		{
			service.btAddr = BTH_ADDR_ANY;
		}
#else
		service.rc_family = AF_BLUETOOTH;
		service.rc_channel = (uint8_t)channel;

		if (!BT_ADDR.empty())
		{
			str_to_bt_addr(BT_ADDR.c_str(), &service.rc_bdaddr);
		}
		else
		{
			memset(&service.rc_bdaddr, 0, sizeof(service.rc_bdaddr));
		}
#endif

		int r = bind(sock, (SOCKADDR *)&service, sizeof(service));
		if (r == SOCKET_ERROR)
		{
			closesocket(sock);
			sock = -1;
			current_channel = -1;
			return false;
		}

		if (listen(sock, 511) < 0)
		{
			current_channel = -1;
			return false;
		}

		for (auto &c : client)
		{
			c.Close();
			c.Unlock();
		}

		if (!setNonBlock(sock))
		{
			Error() << "Bluetooth Server: cannot set socket to non-blocking\n";
		}
		stop = false;

		if (BT_ADDR.empty())
			Info() << "Bluetooth Server: start thread at channel " << channel;
		else
			Info() << "Bluetooth Server: start thread at address " << BT_ADDR << " channel " << channel;

		run_thread = std::thread(&Server::Run, this);

		return true;
	}

	bool Server::startAuto()
	{
#ifndef _WIN32
		int channel = find_available_channel();
		if (channel == -1)
		{
			Error() << "Bluetooth Server: no available channels found";
			return false;
		}
		Info() << "Bluetooth Server: auto-selected channel " << channel;
		return start(channel);
#else

		Warning() << "Bluetooth Server: auto channel selection not implemented for Windows, using channel 1";
		return start(1);
#endif
	}

	int Server::getChannel()
	{
		return current_channel;
	}
}

#endif