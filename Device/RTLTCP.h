/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#include "Protocol.h"

namespace Device {

	class RTLTCP : public Device {

		enum class PROTOCOL {
			NONE,
			RTLTCP,
			GPSD,
			TXT,
			MQTT
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
			case PROTOCOL::MQTT:
				return "MQTT";
			}
			return "";
		}

		Protocol::TCP tcp;
		Protocol::MQTT mqtt;
		Protocol::GPSD gpsd;
		Protocol::RTLTCP rtltcp;
		Protocol::ProtocolBase* transport = &tcp;

		const int TRANSFER_SIZE = 16384;
		static const int BUFFER_SIZE = 16 * 16384;
		std::vector<char> buffer;

		bool lost = false;

		std::thread async_thread;
		std::thread run_thread;

		void RunAsync();
		void Run();

		FIFO fifo;

		void sendProtocol();

	public:
		RTLTCP() : Device(Format::CF32, 288000, Type::RTLTCP) {}

		void OpenWithFileDescriptor(int) { Open(0); }
		// Control

		void Close();
		void Play();
		void Stop();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		std::string getRateDescription() {
			if (getFormat() == Format::TXT) return "N/A";
			return Device::getRateDescription();
		}

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "RTLTCP"; }
		std::string getSerial() { return /*"P" + port*/ ""; }
		std::string getVendor() { return "Network"; }
	};
}