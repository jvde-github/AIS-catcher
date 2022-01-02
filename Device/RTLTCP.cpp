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
	// Device RTLSDR

	void RTLTCP::Print()
	{
		std::cerr << "RTLTCP settings: -gt host " << host <<  " port " << port << " tuner ";
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
		else if (option == "HOST")
		{
			host = arg;
		}
		else if (option == "PORT")
		{
			port = arg;
		}
		else
			throw "Invalid setting for RTLTCP.";
	}

	RTLTCP::RTLTCP()
	{
#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			throw "Cannot initialize Winsocket.";
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
#ifdef _WIN32
		if (sock != -1) closesocket(sock);
#else
		if (sock != -1) close(sock);
#endif
	}

	void RTLTCP::Open(uint64_t handle)
	{
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
			throw "TCP network address and/or port not valid.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1)
			throw "Error creating socket for TCP.";

		if (connect(sock, address->ai_addr, (int)address->ai_addrlen) == -1)
			throw "RTLTCP: cannot connect to socket";

		// check for dongle information
		int len = recv(sock, (char*) &dongle, 12, 0);
		if (len != 12 || dongle.magic != 0x304C5452) throw "RTLTCP: unexpected or invalid response, likely not an rtl-tcp process.";

		applySettings();
		setSampleRate(288000);
	}

	void RTLTCP::setParameter(uint8_t c, uint32_t p)
	{
		char instruction[5];

		instruction[0] = c;
		instruction[4] = p; instruction[3] = p >> 8; instruction[2] = p >> 16; instruction[1] = p >> 24;
		send(sock, (const char *)instruction, 5, 0);
	}

	void RTLTCP::setTuner_GainMode(int a)
	{
		setParameter(3, a);
	}

	void RTLTCP::setRTL_AGC(int a)
	{
		setParameter(8, a);
	}

	void RTLTCP::setTuner_Gain(FLOAT32 a)
	{
		setParameter(4, (uint32_t)a);
	}

	void RTLTCP::RunAsync()
	{
		std::vector<char> data(TRANSFER_SIZE);

		while (isStreaming())
		{
			int len = recv(sock, data.data(), TRANSFER_SIZE, 0);

			if (len < 0) DeviceBase::Stop();
			else if (!fifo.Push(data.data(), len)) std::cerr << "RTLTCP: buffer overrun." << std::endl;
		}
	}

	void RTLTCP::Run()
	{
		std::vector<char> output(fifo.BlockSize());

		while (isStreaming())
		{
			if (fifo.Wait())
			{
				RAW r = { Format::CU8, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1);
				fifo.Pop();
			}
			else
			{
				std::cerr << "RTLTCP: timeout." << std::endl;
			}
		}
	}

	void RTLTCP::Play()
	{
		fifo.Init(BUFFER_SIZE);

		DeviceBase::Play();

		setParameter(2, sample_rate);
		setParameter(1, frequency);

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop()
	{
		DeviceBase::Stop();

		if (async_thread.joinable()) async_thread.join();
		if (run_thread.joinable()) run_thread.join();
	}

	void RTLTCP::setFrequencyCorrection(int ppm)
	{
		setParameter(5, ppm);
	}

	void RTLTCP::applySettings()
	{
		setFrequencyCorrection(freq_offset);
		setTuner_GainMode(tuner_AGC ? 0 : 1);

		if (!tuner_AGC) setTuner_Gain(tuner_Gain);
		if (RTL_AGC) setRTL_AGC(1);
	}

	void RTLTCP::pushDeviceList(std::vector<Description>& DeviceList)
	{
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

}
