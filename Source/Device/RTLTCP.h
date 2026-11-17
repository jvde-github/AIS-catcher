/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

namespace Device
{

	class RTLTCP : public Device
	{

		PROTOCOL Protocol = PROTOCOL::RTLTCP;

		IO::OutputStats stats;
		Protocol::TCP tcp;
		Protocol::MQTT mqtt;
		Protocol::GPSD gpsd;
		Protocol::RTLTCP rtltcp;
		Protocol::ZMQ zmq;
		Protocol::WebSocket ws;
		Protocol::TLS tls;
#if defined(__linux__) && !defined(__ANDROID__)
		Protocol::SocketCAN can;
#endif
		Protocol::ProtocolBase *session = &tcp;

		const int TRANSFER_SIZE = 16384;
		static const int BUFFER_SIZE = 32 * 16384;

		// bytes per FIFO block for the text formats; one byte a block overruns
		// on a busy NMEA feed (-gt TEXT_BUFFER, 1-256)
		int text_buffer = 1;
		// the FIFO holds this many times its default size (-gt BUFFER_COUNT, 1-100):
		// what arrives while the consumer is held up longer than the FIFO lasts is lost
		int buffer_count = 1;
		std::vector<char> buffer;

		bool lossless = false;
		std::atomic<bool> connected{false};

		std::thread async_thread;
		std::thread run_thread;

		void RunAsync();
		void Run();

		FIFO fifo;


	public:
		RTLTCP(Type type = Type::RTLTCP) : Device(Format::CU8, 288000, type, type == Type::ZMQ ? "ZMQ" : type == Type::N2K ? "N2K" : "RTLTCP")
		{
			tcp.setStats(&stats);
			if (type == Type::ZMQ)
				applyKey(AIS::KEY_SETTING_PROTOCOL, "ZMQ");
			if (type == Type::N2K)
				applyKey(AIS::KEY_SETTING_PROTOCOL, "CAN");
		}
		const IO::OutputStats *getConnectionStats() const override { return &stats; }

		void Open(uint64_t h) override;
		void OpenWithFileDescriptor(int) override { Open(0); }
		// Control

		void Play() override;
		void Stop() override;

		bool isConnected() override { return connected; }

		void getDeviceList(std::vector<Description> &DeviceList) override;

		std::string getRateDescription() override
		{
			if (getFormat() == Format::TXT || getFormat() == Format::N2K)
				return "N/A";
			return Device::getRateDescription();
		}

		// Settings
		Setting &applyKey(AIS::Keys key, const std::string &arg) override;
		std::string Get() override;

		std::string getProduct() override { return getName(); }
		std::string getSerial() override { return ""; }
		std::string getIdentity() override { return getProduct() + ":" + session->getHost() + ":" + session->getPort(); }
		std::string getVendor() override { return "Network"; }
	};
}
