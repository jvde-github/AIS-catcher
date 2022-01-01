/*
Copyright(c) 2021-2022 gtlittlewing
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

#ifdef HASAIRSPY
#include <airspy.h>
#endif

namespace Device {


	class AIRSPY : public DeviceBase
	{
#ifdef HASAIRSPY

		struct airspy_device* dev = NULL;
		std::vector<uint32_t> rates;

		enum class AIRSPYGainMode { Free, Sensitivity, Linearity } mode = AIRSPYGainMode::Linearity;
		int gain = 17;

		bool mixer_AGC = true;
		bool LNA_AGC = true;

		int mixer_Gain = 10;
		int LNA_Gain = 10;
		int VGA_Gain = 10;

		bool bias_tee = false;

		static int callback_static(airspy_transfer_t* tf);
		void callback(CFLOAT32 *,int);

		void setBiasTee(bool);
		void setLNA_AGC(int);
		void setMixer_AGC(int);

		void setLNA_Gain(int);
		void setVGA_Gain(int);
		void setMixer_Gain(int);

		void setSensitivity_Gain(int);
		void setLinearity_Gain(int);

	public:

		// Control
		void Play();
		void Stop();

		bool isStreaming();
		bool isCallback() { return true;}

		static void pushDeviceList(std::vector<Description>& DeviceList);

		// Device specific
		void Open(uint64_t h);
		void applySettings();

		// Settings
		void Print();
		void Set(std::string option, std::string arg);

#endif
	};
}
