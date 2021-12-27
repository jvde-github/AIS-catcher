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
#include "FIFO.h"

#ifdef HASSDRPLAY
#include <sdrplay_api.h>
#endif

namespace Device {

	class SettingsSDRPLAY : public DeviceSettings
	{
	private:

		int LNAstate = 5;
		int gRdB = 40;
		bool AGC = false;

	public:

		friend class SDRPLAY;

		void Print();
		void Set(std::string option, std::string arg);
	};

	class SDRPLAY : public DeviceBase//, public StreamOut<RAW>
	{

#ifdef HASSDRPLAY

		// Data is processed in seperate thread
		std::thread run_thread;
		void Run();

		FIFO<char> fifo;
		std::vector<CFLOAT32> output;

		// SDRPLAY specific
		sdrplay_api_DeviceT device;
		sdrplay_api_DeviceParamsT* deviceParams = NULL;
		sdrplay_api_RxChannelParamsT* chParams;

		static void callback_static(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,unsigned int numSamples, unsigned int reset, void *cbContext);
		static void callback_event_static(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext);

		void callback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,unsigned int numSamples, unsigned int reset);
		void callback_event(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params);

	public:

		// Control

		void Play();
		void Stop();

		bool isStreaming();
		virtual bool isCallback() { return true; }

		static void pushDeviceList(std::vector<Description>& DeviceList);

		// Device specific
		void Open(uint64_t h,SettingsSDRPLAY &s);
		void Open(SettingsSDRPLAY &s);

		void applySettings(SettingsSDRPLAY& s);

		// static constructor and data
		static struct _API { bool running = false; _API(); ~_API(); } _api;

		~SDRPLAY() { sdrplay_api_ReleaseDevice(&device); }
#endif
	};
}
