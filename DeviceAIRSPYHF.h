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

#ifdef HASAIRSPYHF
#include <libairspyhf/airspyhf.h>
#endif

namespace Device{


	class SettingsAIRSPYHF : public DeviceSettings
	{
	private:

		bool preamp = false;
		bool treshold_high = false;

	public:

		friend class AIRSPYHF;

		void Print();
		void Set(std::string option, std::string arg);
	};

	class AIRSPYHF : public DeviceBase, public StreamOut<CFLOAT32>
	{
#ifdef HASAIRSPYHF

		struct airspyhf_device* dev = NULL;

		static int callback_static(airspyhf_transfer_t* tf);
		void callback(CFLOAT32 *,int);

		void setTreshold(int);
		void setLNA(int);
		void setAGC(void);

	public:

		// Control

		void Play();
		void Stop();

		void setSampleRate(uint32_t);
		void setFrequency(uint32_t);

		std::vector<uint32_t> SupportedSampleRates();

		bool isStreaming();

		virtual bool isCallback() { return true; }

		static void pushDeviceList(std::vector<Description>& DeviceList);

		// Device specific
		void openDevice(uint64_t h);
		void openDevice();

		void setSettings(SettingsAIRSPYHF& s);

#endif
	};
}
