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

TCPclient::TCPclient()
{

#ifdef _WIN32
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		throw "RTLTCP: Cannot initialize Winsocket.";
		return;
	}
#endif
}

TCPclient::~TCPclient()
{
#ifdef _WIN32
	WSACleanup();
#endif
}

void TCPclient::disconnect()
{
	if (sock != -1)
		closesocket(sock);
}

bool TCPclient::connect(std::string host,std::string port)
{
	int r;
	fd_set fd;

	// RTLTCP protocol
	struct { uint32_t magic = 0, tuner = 0, gain = 0; } dongle;

	struct addrinfo h;

	std::memset(&h, 0, sizeof(h));
	h.ai_family = AF_UNSPEC;
	h.ai_socktype = SOCK_STREAM;
	h.ai_protocol = IPPROTO_TCP;

#ifndef _WIN32
	h.ai_flags = AI_ADDRCONFIG;
#endif

	int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);

	if (code != 0 || address == NULL)
	{
		std::cerr << "TCP: network address and/or port not valid." << std::endl;
		return false;
	}

	sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
	if (sock == -1)
	{
		std::cerr << "TCP: Error creating socket." << std::endl;
		return false;
	}

#ifndef _WIN32
	r = fcntl(sock, F_GETFL, 0);
	r = fcntl(sock, F_SETFL, r | O_NONBLOCK);
	if (r == -1)
	{
		std::cerr << "RTLTCP: cannot set NON BLOCKING flag.";
		return false;
	}
#else
	u_long mode = 1;  // 1 to enable non-blocking socket
	ioctlsocket(sock, FIONBIO, &mode);
#endif

	FD_ZERO(&fd);
	FD_SET(sock, &fd);

	r = ::connect(sock, address->ai_addr, (int)address->ai_addrlen);

	if(r == -1)
	{
#ifndef _WIN32
		if(errno != EINPROGRESS)
		{
			closesocket(sock);
			return false;
		}
#endif
		timeval to = {timeout, 0};

		if(select(sock+1, &fd, NULL, NULL, &to) == 1)
		{
			int se;
			socklen_t len = sizeof(se);

			getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&se, &len);

			if (se != 0)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

int TCPclient::read(void *data,int length)
{
	fd_set fd;

	FD_ZERO(&fd);
	FD_SET(sock, &fd);

	timeval to = {timeout, 0};

	if(select(sock + 1, &fd, NULL, NULL, &to) < 0)
	{
		return -1;
	}

	if (FD_ISSET(sock, &fd))
	{
		return recv(sock,(char*)data,length, 0);
	}
	return 0;
}

