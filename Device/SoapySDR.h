/*
Copyright(c) 2021-2022 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "Device.h"

#ifdef HASSOAPYSDR
#include <rtl-sdr.h>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>
#endif

namespace Device{

	class SOAPYSDR : public Device
	{
#ifdef HASSOAPYSDR

		SoapySDR::Device *dev = NULL;

		SoapySDR::Kwargs device_args;
		SoapySDR::Kwargs stream_args;
		SoapySDR::Kwargs setting_args;
		SoapySDR::Kwargs gains_args;

		std::string antenna = "";
		FLOAT32 freq_offset = 0.0;
		FLOAT32 gaindb;
		int channel = 0;
		bool AGC = true;

		std::thread async_thread;
		std::thread run_thread;


		bool lost = true;

		// FIFO
		FIFO<char> fifo;

		void RunAsync();
		void Run();

		static const uint32_t BUFFER_SIZE = 16 * 16384;

		void applySettings();

	public:

		SOAPYSDR();

		// Control
		void Open(uint64_t h);
		void Play();
		void Stop();
		void Close();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		void Print();
		void Set(std::string option, std::string arg);
#endif
	};
}
