/*
Copyright(c) 2021 jvde.github@gmail.com

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

	// to be expanded with device specific parameters and allowable parameters (e.g. sample rate, gain modes, etc)
	class SettingsRTLSDR : public DeviceSettings
	{
	public:
		int correctionPPM = 0;
	};

	class RTLSDR : public Control, public StreamOut<CU8>
	{
#ifdef HASRTLSDR

		rtlsdr_dev_t* dev = NULL;
		std::thread async_thread;
		std::thread demod_thread;

		std::vector<std::vector<CU8>> fifo;
		static const int sizeFIFO = 2;
		int head = 0;
		int tail = 0;
		std::atomic<int> count;

		std::mutex fifo_mutex;
		std::condition_variable fifo_cond;

		static void callback_static(CU8* buf, uint32_t len, void* ctx);
		static void start_async_static(RTLSDR* c);
		static void demod_async_static(RTLSDR* c);

		void callback(CU8* buf, int len);
		void Demodulation();

		static const uint32_t BufferLen = 16 * 16384;
		rtlsdr_dev_t* getDevice() { return dev; }

	public:

		// Control
		void Play();
		void Pause();

		void setSampleRate(uint32_t);
		void setFrequency(uint32_t);
		void setAGC(void);
		void setGainManual(void);
		void setTunerGain(int);


		std::vector<uint32_t> SupportedSampleRates();

		bool isCallback() { return true; }

		static void pushDeviceList(std::vector<Description>& DeviceList);
		static int getDeviceCount();

		// Device specific

		void openDevice(uint64_t h);
		void setFrequencyCorrection(int);

		void setSettings(SettingsRTLSDR &s);
#endif
	};
}
