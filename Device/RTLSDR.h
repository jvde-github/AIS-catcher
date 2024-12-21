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

#ifdef HASRTLSDR
#include <rtl-sdr.h>
#endif

namespace Device {

	enum class RTLSDRGainMode {
		Default
	};

	// to be expanded with device specific parameters and allowable parameters (e.g. sample rate, gain modes, etc)

	class RTLSDR : public Device {
#ifdef HASRTLSDR

		rtlsdr_dev_t* dev = nullptr;

		std::string vendor, product, serial;

		std::thread async_thread;
		std::thread run_thread;

		// Device settings
		bool tuner_AGC = true;
		bool RTL_AGC = true;
		FLOAT32 tuner_Gain = 33.0;
		bool bias_tee = false;

		bool lost = true;

		// FIFO
		FIFO fifo;

		// callbacks
		static void callback_static(CU8* buf, uint32_t len, void* ctx);
		void callback(CU8* buf, int len);

		void RunAsync();
		void Run();

		static const uint32_t BUFFER_SIZE = 16 * 16384;
		uint32_t BUFFER_COUNT = 24;

		void setTuner_GainMode(int);
		void setTuner_Gain(FLOAT32);
		void setRTL_AGC(int);
		void setBiasTee(int);
		void setBandwidth(int);
		void setFrequencyCorrection(int);

		void applySettings();

	public:
		RTLSDR() : Device(Format::CU8, 1536000, Type::RTLSDR) {}
		~RTLSDR() { Close(); }
		// Control
		void Open(uint64_t h);
#ifdef HASRTL_ANDROID
		void OpenWithFileDescriptor(int);
#endif
		void Play();
		void Stop();
		void Close();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return product; }
		std::string getVendor() { return vendor; }
		std::string getSerial() { return serial; }

		void setFormat(Format f) {}
#endif
	};
}
