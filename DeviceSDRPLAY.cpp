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

#include <cstring>

#include "Device.h"
#include "DeviceSDRPLAY.h"
#include "Common.h"
#include "Utilities.h"

namespace Device {

// API described here: https://www.sdrplay.com/docs/SDRplay_API_Specification_v3.01.pdf

	void SettingsSDRPLAY::Print()
	{
		std::cerr << "SDRPLAY Settings: -gs agc " << (AGC?"ON":"OFF") << " lnastate " << LNAstate << " grdb " << gRdB << std::endl;
	}

	void SettingsSDRPLAY::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "AGC")
		{
			AGC = Util::Parse::Switch(arg);
		}
		else if (option == "LNASTATE")
		{
			LNAstate = Util::Parse::Integer(arg,0,9);
		}
		else if (option == "GRDB")
		{
			gRdB = Util::Parse::Integer(arg,0,59);
		}
		else
			throw "Invalid setting for RTLSDR.";
	}

	//---------------------------------------
	// Device SDRPLAY

#ifdef HASSDRPLAY

	// static constructor and data

	SDRPLAY::_API SDRPLAY::_api;

	SDRPLAY::_API::_API()
	{
		float version = 0.0;

		if(sdrplay_api_Open() != sdrplay_api_Success) 
		{
			running = false;
			return;
		}
		if(sdrplay_api_ApiVersion(&version) != sdrplay_api_Success)
		{
			running = false;
			return;
		}
		if((int)version != 3)
		{
			running = false;
			return;
		}

		running = true;
	}

	SDRPLAY::_API::~_API()
	{
		sdrplay_api_Close();
		running = false;
	}

	void SDRPLAY::callback_static(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,unsigned int numSamples, unsigned int reset, void *cbContext)
	{
		((SDRPLAY*)cbContext)->callback(xi, xq, params, numSamples, reset);
	}

	void SDRPLAY::callback_event_static(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
	{
		((SDRPLAY*)cbContext)->callback_event(eventId, tuner, params);
	}

	void SDRPLAY::callback_event(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT* params)
	{
		if (eventId == sdrplay_api_DeviceRemoved)
		{
			std::cerr << "SDRPLAY: device disconnected" << std::endl;
			Pause();
		}
	}

	void SDRPLAY::callback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,unsigned int len, unsigned int reset)
	{
		if (count == sizeFIFO)
		{
			std::cerr << "SDRPLAY: Buffer overrun!" << std::endl;
		}
		else
		{
			if (fifo[tail].size() != len) fifo[tail].resize(len);

			for (int i = 0; i < len; i++)
			{
				fifo[tail][i].real(xi[i] / 32768.0f);
				fifo[tail][i].imag(xq[i] / 32768.0f);
			}

			tail = (tail + 1) % sizeFIFO;

			{
				std::lock_guard<std::mutex> lock(fifo_mutex);
				count++;
			}

			fifo_cond.notify_one();
		}
	}

	void SDRPLAY::demod_async_static(SDRPLAY* c)
	{
		c->Demodulation();
	}

	void SDRPLAY::Demodulation()
	{
		std::cerr << "SDRPLAY: Start demodulation thread." << std::endl;

		while (isStreaming())
		{
			if (count == 0)
			{
				std::unique_lock <std::mutex> lock(fifo_mutex);
				fifo_cond.wait_for(lock, std::chrono::milliseconds((int)((float)buffer_size / (float)sample_rate * 1000.0f * 1.05f)), [this] {return count != 0; });

				if (count == 0) std::cerr << "SDRPLAY: timeout." << std::endl;
			}

			if (count != 0)
			{
				if (output.size() < buffer_size) output.resize(buffer_size);

				for (int i = 0; i < fifo[head].size(); i++)
				{
					output[ptr] = fifo[head][i];

					if (++ptr == buffer_size)
					{
						ptr = 0;
						Send(output.data(), buffer_size);

					}
				}

				head = (head + 1) % sizeFIFO;
				count--;
			}
		}

		std::cerr << "SDRPLAY: Stop demodulation thread." << std::endl;
	}

	void SDRPLAY::openDevice(uint64_t h)
	{
		unsigned int DeviceCount;

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		if(DeviceCount < h) throw "SDRPLAY: cannot open device, handle not available.";

		device = devices[h];

		if(sdrplay_api_SelectDevice(&device) != sdrplay_api_Success)
		{
			sdrplay_api_UnlockDeviceApi();
			throw "SDRPLAY: cannot open device";
		}
		sdrplay_api_UnlockDeviceApi();

		if (sdrplay_api_GetDeviceParams(device.dev, &deviceParams) != sdrplay_api_Success)
		{
			throw "SDRPLAY: cannot get device parameters.";
		}

		chParams = deviceParams->rxChannelA;
	}

	void SDRPLAY::openDevice()
	{
		openDevice(0);
	}

	void SDRPLAY::setSampleRate(uint32_t s)
	{

		if(streaming) throw "SDRPLAY: internal error, settings modified while streaming.";

		deviceParams->devParams->fsFreq.fsHz = s;

		chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
		chParams->ctrlParams.decimation.enable = 0;
		chParams->ctrlParams.decimation.decimationFactor = 1;
		chParams->ctrlParams.decimation.wideBandSignal = 1;
		chParams->tunerParams.bwType = sdrplay_api_BW_1_536;

		Control::setSampleRate(s);
	}

	void SDRPLAY::setFrequency(uint32_t f)
	{
		if(streaming) throw "SDRPLAY: internal error, settings modified while streaming.";

		chParams->tunerParams.rfFreq.rfHz = f;
		Control::setFrequency(f);
	}

	void SDRPLAY::Play()
	{
		// set up FIFO
		fifo.resize(sizeFIFO);

		count = 0;
		tail = 0;
		head = 0;

		// SDRPLAY

		sdrplay_api_CallbackFnsT cbFns;

		cbFns.StreamACbFn = callback_static;
		cbFns.EventCbFn = callback_event_static;

		if(sdrplay_api_Init(device.dev, &cbFns, (void *)this) != sdrplay_api_Success)
			throw "SDRPLAY: cannot start device";

		Control::Play();

		demod_thread = std::thread(SDRPLAY::demod_async_static, this);

		SleepSystem(10);
	}

	void SDRPLAY::Pause()
	{
		Control::Pause();

		if (demod_thread.joinable())
		{
			sdrplay_api_Uninit(device.dev);
			demod_thread.join();
		}
	}

	std::vector<uint32_t> SDRPLAY::SupportedSampleRates()
	{
		// for now....
		return { 2304000, 3072000 };
	}

	void SDRPLAY::pushDeviceList(std::vector<Description>& DeviceList)
	{
		unsigned int DeviceCount;
		if(!_api.running) throw "SDRPLAY: API not running";

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		for (int i = 0; i < DeviceCount; i++) 
		{
			if(devices[i].hwVer == SDRPLAY_RSP1A_ID)
			{
				DeviceList.push_back(Description("SDRPLAY", "RSP1A", devices[i].SerNo, (uint64_t)i, Type::SDRPLAY));
			}
		}

		sdrplay_api_UnlockDeviceApi();
	}

	int SDRPLAY::getDeviceCount()
	{
		return 0;
	}

	bool SDRPLAY::isStreaming()
	{
		return streaming;
	}

	void SDRPLAY::setSettings(SettingsSDRPLAY &s)
	{
		if(streaming) throw "SDRPLAY: internal error, settings modified while streaming.";

		chParams->ctrlParams.agc.enable = s.AGC ? sdrplay_api_AGC_DISABLE : sdrplay_api_AGC_CTRL_EN;
		chParams->tunerParams.gain.gRdB = s.gRdB;
		chParams->tunerParams.gain.LNAstate = s.LNAstate;
	}

#endif
}
