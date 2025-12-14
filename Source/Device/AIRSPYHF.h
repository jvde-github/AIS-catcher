/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#ifdef HASAIRSPYHF
#include <airspyhf.h>
#endif

namespace Device {

	class AIRSPYHF : public Device {

		// Device settings (always available)
		bool preamp = false;
		bool threshold_high = false;

#ifdef HASAIRSPYHF

		struct airspyhf_device* dev = NULL;
		std::vector<uint32_t> rates;
		bool lost = false;
		uint64_t serial;

		static int callback_static(airspyhf_transfer_t* tf);
		void callback(CFLOAT32*, int);

		void setThreshold(int);
		void setLNA(int);
		void setAGC(void);

		void applySettings();

		void setDefaultRate();

	public:
		// Control
		void Open(uint64_t h);
#ifdef HASAIRSPYHF_ANDROID
		void OpenWithFileDescriptor(int);
#endif
		void Play();
		void Stop();
		void Close();

		bool isStreaming();
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		std::string getSerial() { return Util::Convert::toHexString(serial); }

		void setFormat(Format f) {}
#endif

	public:
		AIRSPYHF() : Device(Format::CF32, 0, Type::AIRSPYHF) {}

		std::string getProduct() { return "AIRSPYHF"; }
		std::string getVendor() { return "AIRSPY"; }

		// Settings (always available)
		Setting& Set(std::string option, std::string arg);
		std::string Get();
	};
}
