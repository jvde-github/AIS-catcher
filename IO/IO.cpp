/*
Copyright(c) 2021-2022 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstring>

#include "IO.h"

namespace IO
{

#ifdef _WIN32
	UDP::UDP()
	{
		WSADATA wsaData;

		if( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			throw "Cannot initialize Winsocket.";
			return;
		}
	}

	UDP::~UDP()
	{
		WSACleanup();
	}
#endif

	void UDP::Receive(const NMEA* data, int len)
	{
		for(int i = 0; i < len; i++)
			for(const auto& s : data[i].sentence)
				sendto(sock, (s+"\r\n").c_str(), (int) s.length()+2, 0, address->ai_addr, (int) address->ai_addrlen);
	}

	void UDP::openConnection(const std::string& host, const std::string& portname)
	{
		struct addrinfo h;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), portname.c_str(), &h, &address);

		if(code != 0 || address == NULL)
		{
			throw "UDP network address and/or port not valid.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1)
		{
			throw "Error creating socket for UDP.";
		}
	}
}
