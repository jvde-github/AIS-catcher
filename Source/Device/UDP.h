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

#include <fstream>
#include <iostream>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define SOCKET		 int
#define SOCKADDR	 struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close

#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Device.h"

namespace Device {

	class UDP : public Device {
		std::string port;
		std::string server;

		struct addrinfo* address;
		SOCKET sock;
		bool lost = false;

		std::thread run_thread;

		void StartServer();
		void StopServer();
		void Run();
		void applySettings();

	public:
		UDP() : Device(Format::TXT, 0, Type::UDP) {};
		~UDP() {};

		// Control
		void Close();
		void Play();
		void Stop();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }
		void getDeviceList(std::vector<Description>& DeviceList);
		std::string getRateDescription() { return "N/A"; }

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "UDP"; }
		std::string getSerial() { return /*"P" + port*/ ""; }
		std::string getVendor() { return "Network"; }
	};
}
