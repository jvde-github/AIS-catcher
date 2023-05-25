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

#ifdef HASHACKRF
#include <hackrf.h>
#endif

namespace Device {

	class HACKRF : public Device {
#ifdef HASHACKRF

		hackrf_device* device = NULL;
		static hackrf_device_list_t* list;
		std::string serial;

		int LNA_Gain = 8;
		int VGA_Gain = 20;
		bool preamp = false;

		static int callback_static(hackrf_transfer* tf);
		void callback(uint8_t*, int);

		void applySettings();

	public:
		HACKRF() : Device(Format::CS8, 6144000) {
			if (hackrf_init() != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: Cannot open hackrf library");
		}
		~HACKRF() {
			if (list) {
				hackrf_device_list_free(list);
				list = NULL;
			}
		}

		// Control
		void Open(uint64_t h);
		void Close();
		void Play();
		void Stop();

		bool isStreaming() { return Device::isStreaming() && hackrf_is_streaming(device) == HACKRF_TRUE; }
		virtual bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "HACKRF"; }
		std::string getVendor() { return "Great Scott Gadgets"; }
		std::string getSerial() { return serial; }

		void setFormat(Format f) {}
#endif
	};
}
