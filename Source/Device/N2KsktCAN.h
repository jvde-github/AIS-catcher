/*
	Copyright(c) 2024 jvde.github@gmail.com

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
#include <iostream>
#include <vector>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace Device {

	class N2KSCAN : public Device, public Callback<tN2kMsg> {
		
#ifdef HASNMEA2000
		std::string _iface = "";
		bool lost = false;
		std::vector<std::string> available_intefaces;

#endif
	public:
#ifdef HASNMEA2000
		N2KSCAN() : Device(Format::N2K, 288000, Type::N2K){};
		~N2KSCAN();

		std::string getRateDescription() { return "N/A"; }

		bool isStreaming() { return !lost; }
		bool isCallback() { return true; }

		std::string Get();

		std::string getProduct() { return "NMEA2000"; }
		std::string getVendor() { return "Unknown"; }
		std::string getSerial() { return _iface; }

		void setFormat(Format f) {}
		
		void onMsg(const tN2kMsg & msg) {
			RAW r = { getFormat(), (void*)&msg, 1 };
			Send(&r, 1, tag);
		}

		// Control
		void Open(uint64_t h);
		void Close();
		void Play();
		void Stop();

		virtual void getDeviceList(std::vector<Description>& DeviceList);

#endif
		Setting& Set(std::string option, std::string arg);
	};
}
