#include "IO.h"

#include <cstring>
#ifdef WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif


namespace IO
{
	void UDP::Receive(const NMEA* data, int len)
	{
		for(int i = 0; i < len; i++)
			for(auto s : data[i].sentence)
				sendto(sock, s.c_str(), s.length(), 0, address->ai_addr, address->ai_addrlen);
	}

	void UDP::startWSA()
	{
#ifdef WIN32
		address = NULL;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			throw "Cannot create UDP socket.";
			return;
		}
#endif
	}

	void UDP::closeWSA()
	{
#ifdef WIN32
			WSACleanup();

#endif
	}
	void UDP::open(std::string host, std::string portname)
	{
		struct addrinfo h;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#ifndef WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		startWSA();

		if(getaddrinfo(host.c_str(), portname.c_str(), &h, &address) == -1)
		{
			throw "Failed to resolve remote socket address for UDP connection.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1)
		{
			throw "error creating socket for UDP.";
		}

	}
}
