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

#include "RTLTCP.h"

namespace Device {

	//---------------------------------------
	// Device RTLTCP

	RTLTCP::RTLTCP()
	{
		setSampleRate(288000);

#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			throw "RTLTCP: Cannot initialize Winsocket.";
			return;
		}
#endif
	}

	RTLTCP::~RTLTCP()
	{
#ifdef _WIN32
		WSACleanup();
#endif
	}

	void RTLTCP::Close()
	{
		Device::Close();

		if (sock != -1)
			closesocket(sock);
	}

	void RTLTCP::Play()
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
			throw "RTLTCP: network address and/or port not valid.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (sock == -1) throw "RTLTCP: Error creating socket.";
#ifndef _WIN32
		r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);
		if (r == -1) throw "RTLTCP: cannot set NON BLOCKING flag.";
#else
		u_long mode = 1;  // 1 to enable non-blocking socket
		ioctlsocket(sock, FIONBIO, &mode);
#endif

		FD_ZERO(&fd);
		FD_SET(sock, &fd);

		r = connect(sock, address->ai_addr, (int)address->ai_addrlen);

		if(r == -1)
		{
#ifndef _WIN32
			if(errno != EINPROGRESS)
			{
				throw "RTLTCP: cannot connect to socket";
				closesocket(sock);
			}
#endif
			timeval to = {timeout, 0};

			if(select(sock+1, &fd, NULL, NULL, &to) == 1)
			{
				int se;
				socklen_t len = sizeof(se);

				getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&se, &len);

				if (se != 0)
					throw "RTLTCP: cannot open socket.";
			}
			else
				throw "RTLTCP: cannot open connection.";
		}

		if (Protocol == PROTOCOL::RTLTCP)
		{
			// RTLTCP protocol, check for dongle information
			int len = Read((char*)&dongle, 12, 1);
			if (len != 12 || dongle.magic != 0x304C5452) throw "RTLTCP: unexpected or invalid response, likely not an rtl-tcp process.";
		}

		fifo.Init(BUFFER_SIZE);
		applySettings();

		Device::Play();
		lost = false;

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop()
	{
		if (Device::isStreaming())
		{
			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}
	}

	int RTLTCP::Read(void *data,int length,int time)
	{
		fd_set fd;

		FD_ZERO(&fd);
		FD_SET(sock, &fd);

		timeval to = {time, 0};

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

	void RTLTCP::RunAsync()
	{
		std::vector<char> data(TRANSFER_SIZE);

		while (isStreaming())
		{
			int len = Read(data.data(), TRANSFER_SIZE, 2);

			if (len <= 0)
			{
				lost = true;
				std::cerr << "RTLTCP: error receiving data from remote host. Cancelling. " << std::endl;
				break;
			}
			else if (isStreaming() && !fifo.Push(data.data(), len)) std::cerr << "RTLTCP: buffer overrun." << std::endl;
		}
	}

	void RTLTCP::Run()
	{
		std::vector<char> output(fifo.BlockSize());
		RAW r = { format, NULL, fifo.BlockSize() };

		while (isStreaming())
		{
			if (fifo.Wait())
			{
				r.data = fifo.Front();
				Send(&r, 1);
				fifo.Pop();
			}
			else
			{
				if (isStreaming()) std::cerr << "RTLTCP: timeout." << std::endl;
			}
		}
	}

	void RTLTCP::setParameterRTLTCP(uint8_t c, uint32_t p)
	{
		char instruction[5];

		instruction[0] = c;
		instruction[4] = p; instruction[3] = p >> 8; instruction[2] = p >> 16; instruction[1] = p >> 24;
		send(sock, (const char *)instruction, 5, 0);
	}

	void RTLTCP::applySettings()
	{
		if(Protocol == PROTOCOL::RTLTCP)
		{
			setParameterRTLTCP(5, freq_offset);
			setParameterRTLTCP(3, tuner_AGC ? 0 : 1);

			if (!tuner_AGC) setParameterRTLTCP(4, tuner_Gain);
			if (RTL_AGC) setParameterRTLTCP(8, 1);

			setParameterRTLTCP(2, sample_rate);
			setParameterRTLTCP(1, frequency);

			format = Format::CU8;
		}
	}

	void RTLTCP::getDeviceList(std::vector<Description>& DeviceList)
	{
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

	void RTLTCP::Print()
	{
		std::cerr << "RTLTCP settings: -gt host " << host << " port " << port << " tuner ";
		if (tuner_AGC) std::cerr << "AUTO"; else std::cerr << tuner_Gain;
		std::cerr << " rtlagc " << (RTL_AGC ? "ON" : "OFF");
		std::cerr << " -p " << freq_offset << std::endl;
	}

	void RTLTCP::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "TUNER")
		{
			tuner_AGC = Util::Parse::AutoFloat(arg, 0, 50, tuner_Gain);
		}
		else if (option == "RTLAGC")
		{
			RTL_AGC = Util::Parse::Switch(arg);
		}
		else if (option == "FREQOFFSET")
		{
			freq_offset = Util::Parse::Integer(arg, -150, 150);
		}
		else if (option == "TIMEOUT")
		{
			timeout = Util::Parse::Integer(arg, 1, 60);
		}
		else if (option == "HOST")
		{
			host = arg;
		}
		else if (option == "PORT")
		{
			port = arg;
		}
		else if (option == "PROTOCOL")
		{
			if (arg == "NONE") Protocol = PROTOCOL::NONE;
			else if (arg == "RTLTCP") Protocol = PROTOCOL::RTLTCP;
			else throw "RTLTCP: unknown protocol";
		}
		else Device::Set(option, arg);
	}
}
