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

#ifdef HASRTLSDR
#include <rtl-sdr.h>
#endif

namespace Device{

	enum class RTLSDRGainMode
	{
		Default
	};

	// to be expanded with device specific parameters and allowable parameters (e.g. sample rate, gain modes, etc)

	class RTLSDR : public DeviceBase
	{
#ifdef HASRTLSDR

		rtlsdr_dev_t* dev = NULL;

		std::thread async_thread;
		std::thread run_thread;

		// Device settings
		int freq_offset = 0;
		bool tuner_AGC = true;
		bool RTL_AGC = false;
		FLOAT32 tuner_Gain = 33.0;
		bool bias_tee = false;
		bool auto_terminate = true;

		// FIFO
		FIFO<char> fifo;

		// callbacks
		static void callback_static(CU8* buf, uint32_t len, void* ctx);
		void callback(CU8* buf, int len);

		void RunAsync();
		void Run();

		static const uint32_t BUFFER_SIZE = 16 * 16384;

		void setTuner_GainMode(int);
		void setTuner_Gain(FLOAT32);
		void setRTL_AGC(int);
		void setBiasTee(int);
		void setFrequencyCorrection(int);

		void applySettings();

	public:

		// Control
		void Open(uint64_t h);
		void Play();
		void Stop();
		void Close();

		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		void Print();
		void Set(std::string option, std::string arg);
#endif
	};
}
