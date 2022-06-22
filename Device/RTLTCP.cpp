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

#include "RTLTCP.h"

namespace Device {

	//---------------------------------------
	// Device RTLTCP

	RTLTCP::RTLTCP()
	{
		setSampleRate(288000);
	}

	RTLTCP::~RTLTCP()
	{
	}

	void RTLTCP::Close()
	{
		client.disconnect();
		Device::Close();
	}

	void RTLTCP::Play()
	{
		if(!client.connect(host, port))
			throw "RTLTCP: cannot open socket.";

		if (Protocol == PROTOCOL::RTLTCP)
		{
			struct { uint32_t magic = 0, tuner = 0, gain = 0; } dongle;
			// RTLTCP protocol, check for dongle information
			int len = client.read((char*)&dongle, 12);
			if (len != 12 || dongle.magic != 0x304C5452) throw "RTLTCP: no or invalid response, likely not an rtl-tcp server.";
		}

		fifo.Init(BUFFER_SIZE);
		applySettings();

		lost = false;

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
		Device::Play();

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

	void RTLTCP::RunAsync()
	{
		std::vector<char> data(TRANSFER_SIZE);

		while (isStreaming())
		{
			int len = client.read(data.data(), TRANSFER_SIZE);

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
		client.send((const char *)instruction, 5);
	}

	void RTLTCP::applySettings()
	{
		client.setTimeout(timeout);

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
		std::cerr << " rtlagc " << (RTL_AGC ? "ON" : "OFF") << " timeout " << timeout;
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
