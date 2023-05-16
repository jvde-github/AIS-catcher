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


#include <cstring>

#include "Utilities.h"
#include "UDP.h"

namespace Device {

	UDP::UDP() : Device(Format::TXT, 288000) {}

	UDP::~UDP() {}

	void UDP::StopServer() {
		if (sock != -1) {
			closesocket(sock);
			sock = -1;
		}
		
		if(address != NULL) {
			freeaddrinfo(address);
			address = NULL;
		}
	}

	void UDP::StartServer() {

		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		int r = getaddrinfo(server.c_str(), port.c_str(), &hints, &address);
		if (r != 0 || address == NULL) {
			throw std::runtime_error("UDP: cannot create socket.");
		}

		sock = socket(address->ai_family, SOCK_DGRAM, 0);
		if (sock == -1) {
			throw std::runtime_error("UDP: cannot create socket.");
		}

#ifndef _WIN32
		int optval = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
			throw std::runtime_error("UDP: cannot set socket option.");
		}

		r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

		if (r == -1) {
			throw std::runtime_error("UDP: cannot make the socket non-blocking.");
		}
#else
		u_long mode = 1; // 1 to enable non-blocking socket
		ioctlsocket(sock, FIONBIO, &mode);
#endif


		if (bind(sock, address->ai_addr, address->ai_addrlen) != 0) {
			std::cerr << "UDP: binding to " << server << " port " << port << ": " << strerror(errno) << "\n";
			throw std::runtime_error("UDP: cannot bind to port.");
		}

		SleepSystem(100);
		std::cerr << "UDP: server opened at port " << port << std::endl;
	}

	void UDP::Close() {
		Device::Close();
	}

	void UDP::Play() {

		Device::Play();
		applySettings();

		StartServer();
		lost = false;
		run_thread = std::thread(&UDP::Run, this);

		SleepSystem(10);
	}

	void UDP::Stop() {
		if (Device::isStreaming()) {
			Device::Stop();

			if (run_thread.joinable()) run_thread.join();
		}
		StopServer();
	}

	void UDP::Run() {
		std::cerr << "UDP: starting thread.\n";
		char buffer[16384];
		RAW r = { getFormat(), buffer, 0 };
		int nread;

		while (isStreaming()) {
			do {
				nread = recv(sock, buffer, sizeof(buffer), 0);

				if (nread > 0) {
					r.size = nread;
					Send(&r, 1, tag);
				}
			} while (nread > 0);

			struct timeval tv;
			fd_set fds;

			FD_ZERO(&fds);
			FD_SET(sock, &fds);

			tv = { 1, 0 };
			select(sock + 1, &fds, NULL, NULL, &tv);

			// SleepSystem(100);
		}

		std::cerr << "UDP: ending thread.\n";
	}


	void UDP::applySettings() {
	}

	void UDP::getDeviceList(std::vector<Description>& DeviceList) {
		DeviceList.push_back(Description("UDP", "UDP", "UDP", (uint64_t)0, Type::UDP));
	}

	Setting& UDP::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "PORT") {
			port = arg;
		}
		else if (option == "SERVER") {
			server = arg;
		}
		else if (option == "FORMAT") {
			throw std::runtime_error("UDP: format cannot be changed and need to be TXT.");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string UDP::Get() {
		return Device::Get() + " server " + server + " port " + port;
	}
}
