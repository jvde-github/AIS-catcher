/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#pragma once

#include "Device.h"

#include <fstream>
#include <iostream>

#include "TCP.h"

namespace Device {

	class RTLTCP : public Device {

		enum class PROTOCOL {
			NONE,
			RTLTCP,
			GPSD,
			TXT
		} Protocol = PROTOCOL::RTLTCP;

		std::string getProtocolString() const {
			switch (Protocol) {
				case PROTOCOL::NONE:
					return "NONE";
				case PROTOCOL::RTLTCP:
					return "RTLTCP";
				case PROTOCOL::GPSD:
					return "GPSD";
				case PROTOCOL::TXT:
					return "TXT";
			}
			return "";
		}

		// Protocol: RTL_TCP
		int freq_offset = 0;
		bool tuner_AGC = true;
		bool RTL_AGC = false;
		FLOAT32 tuner_Gain = 33.0;
		bool bias_tee = false;

		// Protocol: NONE

		::TCP::Client client;

		std::string host = "localhost";
		std::string port = "1234";

		struct addrinfo* address;

		int TRANSFER_SIZE = 1024;
		static const int BUFFER_SIZE = 16 * 16384;

		int timeout = 2;
		bool persistent = true;
		int reset_time = -1;
		bool lost = false;

		std::thread async_thread;
		std::thread run_thread;

		void RunAsync();
		void Run();

		FIFO fifo;

		void setParameterRTLTCP(uint8_t cmd, uint32_t param);
		void applySettings();

	public:
		RTLTCP() : Device(Format::CF32, 288000) {}

		void OpenWithFileDescriptor(int) { Open(0); }
		// Control

		void Close();
		void Play();
		void Stop();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "RTLTCP"; }
	};
}
