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

#ifdef HASSOAPYSDR
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>
#endif

namespace Device {

	class SoapyDevice {
		std::string device;
		int channel = 0;
		int sample_rate = 0;

	public:
		SoapyDevice(std::string d, int c, int s) : device(d), channel(c), sample_rate(s) {}

		std::string getDeviceString() { return device; }
		int getChannel() { return channel; }
		int getDefaultSampleRate() { return sample_rate; }
	};

	class SOAPYSDR : public Device {
#ifdef HASSOAPYSDR

		std::vector<SoapyDevice> dev_list;
		SoapySDR::Device* dev = NULL;

		std::string device_args;
		SoapySDR::Kwargs stream_args;
		SoapySDR::Kwargs setting_args;
		SoapySDR::Kwargs gains_args;

		std::string antenna = "";
		int channel = 0;
		bool AGC = true;
		bool print = false;

		std::thread async_thread;
		std::thread run_thread;

		void PrintActuals(void);

		bool lost = true;

		// FIFO
		FIFO fifo;

		void RunAsync();
		void Run();

		static const uint32_t BUFFER_SIZE = 16 * 16384;

		void applySettings();
		int findRate(const std::vector<double>&);

	public:
		SOAPYSDR() : Device(Format::CF32, 0) {}

		// Control
		void Open(uint64_t h);
		void Play();
		void Stop();
		void Close();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "SOAPYSDR"; }
		void setFormat(Format f) {}
#endif
	};
}
