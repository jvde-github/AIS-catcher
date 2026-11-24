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

#include <string>
#include <vector>

#include "Device.h"
#include "TCPServer.h"

namespace Device
{
	class TCPServerInput : public Device
	{
		std::string server;
		int port = 0;

		struct Listener : public IO::TCPServer
		{
			static const int MAX_LINE = 8192;
			TCPServerInput *input = nullptr;
			Listener() { timeout = 0; }
			void processClients() override;
			void end()
			{
				stopThread();
				for (auto &c : client)
					c.Close();
				release();
			}
		} listener;

	public:
		TCPServerInput() : Device(Format::TXT, 0, Type::TCPSERVER, "TCP server") {}
		~TCPServerInput() { listener.end(); }

		void Play() override;
		void Stop() override { Device::Stop(); listener.end(); }
		void Close() override { Stop(); }

		void getDeviceList(std::vector<Description> &DeviceList) override;
		std::string getRateDescription() override { return "N/A"; }

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;
		std::string Get() override;

		std::string getProduct() override { return "TCP server"; }
		std::string getIdentity() override { return "TCP:" + server + ":" + std::to_string(port); }
		std::string getVendor() override { return "Network"; }
	};
}
