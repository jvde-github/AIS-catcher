/*
Copyright(c) 2021 jvde.github@gmail.com

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

#include "Device.h"
#include "DeviceRTLTCP.h"
#include "Common.h"
#include "Utilities.h"

namespace Device {

	void SettingsRTLTCP::Print()
	{
		std::cerr << "RTLTCP settings: -gt host " << host <<  " port " << port << " tuner ";
		if (tuner_AGC) std::cerr << "AUTO"; else std::cerr << tuner_Gain;
		std::cerr << " rtlagc " << (RTL_AGC ? "ON" : "OFF");
		std::cerr << " -p " << freq_offset << std::endl;
	}

	void SettingsRTLTCP::Set(std::string option, std::string arg)
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

	//---------------------------------------
	// Device RTLSDR

#ifdef HASRTLTCP

	RTLTCP::RTLTCP()
	{
#ifdef WIN32
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
#ifdef WIN32
		WSACleanup();
#endif
	}

	void RTLTCP::Close()
	{
#ifdef WIN32
		if (sock != -1) closesocket(sock);
#else
		if (sock != -1) close(sock);
#endif
	}

	void RTLTCP::Open(uint64_t handle, SettingsRTLTCP& s)
	{
		struct addrinfo h;

		host = s.host;
		port = s.port;

		std::memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;

#ifndef WIN32
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

		applySettings(s);
	}

	void RTLTCP::setParameter(uint8_t c, uint32_t p)
	{
		char instruction[5];

		instruction[0] = c;
		instruction[4] = p; instruction[3] = p >> 8; instruction[2] = p >> 16; instruction[1] = p >> 24;
		send(sock, (const char *)instruction, 5, 0);
	}

	void RTLTCP::setSampleRate(uint32_t s)
	{
		DeviceBase::setSampleRate(s);
		setParameter(2, s);
	}

	void RTLTCP::setFrequency(uint32_t f)
	{
		DeviceBase::setFrequency(f);
		setParameter(1, f);
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
		setParameter(4, a);
	}

	void RTLTCP::RunAsync()
	{
		char buffer[TRANSFER_SIZE];

		while (isStreaming())
		{
			if (count != SIZE_FIFO)
			{
				int len = recv(sock, buffer, TRANSFER_SIZE, 0);

				if(len == 0)
				{
					DeviceBase::Stop();
					continue;
				}

				// FIFO is CU8 so half the size of input
				if (fifo[tail].size() != len / 2) fifo[tail].resize(len / 2);
				std::memcpy(fifo[tail].data(), buffer, len);

				tail = (tail + 1) % SIZE_FIFO;

				{
					std::lock_guard<std::mutex> lock(fifo_mutex);
					count++;
				}

				fifo_cond.notify_one();
			}
		}
	}

	void RTLTCP::Run()
	{
		if (output.size() < BUFFER_SIZE) output.resize(BUFFER_SIZE);

		while (isStreaming())
		{
			if (count == 0)
			{
				std::unique_lock <std::mutex> lock(fifo_mutex);
				fifo_cond.wait_for(lock, std::chrono::milliseconds((int)((float)BUFFER_SIZE / (float)sample_rate * 1000.0f)), [this] {return count != 0; });

				if (count == 0) continue;
			}

			// add latest additions in fifo to output vector and send when output-vector is buffer-size.
			for (int i = 0; i < fifo[head].size(); i++)
			{
				output[ptr] = fifo[head][i];

				if (++ptr == BUFFER_SIZE)
				{
					ptr = 0;
					RAW raw;
					raw.data = output.data();
					raw.format = Format::CU8;
					raw.len = BUFFER_SIZE * sizeof(CU8);					

					Send(&raw, 1);
				}
			}

			head = (head + 1) % SIZE_FIFO;
			count--;
		}
	}

	void RTLTCP::Play()
	{
		// set up FIFO
		fifo.resize(SIZE_FIFO);

		count = 0; tail = 0; head = 0;

		DeviceBase::Play();

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

	void RTLTCP::applySettings(SettingsRTLTCP& s)
	{
		setFrequencyCorrection(s.freq_offset);
		setTuner_GainMode(s.tuner_AGC ? 0 : 1);

		if (!s.tuner_AGC) setTuner_Gain(s.tuner_Gain);
		if (s.RTL_AGC) setRTL_AGC(1);

		host = s.host;
		port = s.port;
	}

	std::vector<uint32_t> RTLTCP::SupportedSampleRates()
	{
		return { 1536000, 288000, 96000, 240000 };
	}

	void RTLTCP::pushDeviceList(std::vector<Description>& DeviceList)
	{
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

#endif
}
