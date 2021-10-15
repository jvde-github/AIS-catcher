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
		std::cerr << "RTLTCP settings: host " << address <<  " port " << port << " - gt tuner ";

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
			address = arg;
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
		if(sock != -1) close(sock);
	}

	void RTLTCP::Open(uint64_t handle, SettingsRTLTCP& s)
	{
		host = s.address;
		port = s.port;

		struct addrinfo h;
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
	}

	void RTLTCP::setTuner_Gain(FLOAT32 a)
	{
	}

	void RTLTCP::RunAsync()
	{
		char buffer[1024];

		while (isStreaming())
		{

			if (count != sizeFIFO)
			{
				int len = recv(sock, buffer, 1024, 0);
				int len_iq = len / 2;

				std::cerr << len << std::endl;
				if (fifo[tail].size() != len_iq) fifo[tail].resize(len_iq);

				std::memcpy(fifo[tail].data(), buffer, len);

				tail = (tail + 1) % sizeFIFO;

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
		if (output.size() < buffer_size) output.resize(buffer_size);

		while (isStreaming())
		{
			if (count == 0)
			{
				std::unique_lock <std::mutex> lock(fifo_mutex);
				fifo_cond.wait_for(lock, std::chrono::milliseconds((int)((float)buffer_size / (float)sample_rate * 1000.0f * 1.05f)), [this] {return count != 0; });

				if (count == 0) continue;
			}


			for (int i = 0; i < fifo[head].size(); i++)
			{
				output[ptr] = fifo[head][i];

				if (++ptr == buffer_size)
				{
					ptr = 0;
					Send(output.data(), buffer_size);

				}
			}

			head = (head + 1) % sizeFIFO;
			count--;
		}
	}

	void RTLTCP::Play()
	{
		// set up FIFO
		fifo.resize(sizeFIFO);

		count = 0;
		tail = 0;
		head = 0;

		DeviceBase::Play();

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		demod_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop()
	{

		DeviceBase::Stop();

		if (async_thread.joinable())
			async_thread.join();

		if (demod_thread.joinable())
			demod_thread.join();

	}

	void RTLTCP::setFrequencyCorrection(int ppm)
	{
		// TO DO
	}

	void RTLTCP::applySettings(SettingsRTLTCP& s)
	{
		setFrequencyCorrection(s.freq_offset);
		setTuner_GainMode(s.tuner_AGC ? 0 : 1);

		if (!s.tuner_AGC) setTuner_Gain(s.tuner_Gain);
		if (s.RTL_AGC) setRTL_AGC(1);

		host = s.address;
		port = s.port;
	}

	std::vector<uint32_t> RTLTCP::SupportedSampleRates()
	{
		return { 240000, 288000, 1536000 };
	}

	void RTLTCP::pushDeviceList(std::vector<Description>& DeviceList)
	{
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

#endif
}
