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

#include <cstring>

#include "SDRPLAY.h"

namespace Device {

// API described here: https://www.sdrplay.com/docs/SDRplay_API_Specification_v3.01.pdf


#ifdef HASSDRPLAY

	//---------------------------------------
	// Device SDRPLAY

	void SDRPLAY::Print()
	{
		std::cerr << "SDRPLAY Settings: -gs agc " << (AGC?"ON":"OFF") << " lnastate " << LNAstate << " grdb " << gRdB << std::endl;
	}

	void SDRPLAY::Set(std::string option, std::string arg)
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
			throw "Invalid setting for SDRPLAY.";
	}

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
			Stop();
		}
	}

	void SDRPLAY::callback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,unsigned int len, unsigned int reset)
	{
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++)
		{
			output[i].real(xi[i] / 32768.0f);
			output[i].imag(xq[i] / 32768.0f);
		}

		if (!fifo.Push((char*)output.data(), len * sizeof(CFLOAT32))) std::cerr << "SDRPLAY: buffer overrun." << std::endl;

	}

	void SDRPLAY::Run()
	{
		while (isStreaming())
		{
			if (fifo.Wait())
			{
				RAW r = { Format::CF32, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1);
				fifo.Pop();
			}
			else std::cerr << "SDRPLAY: timeout." << std::endl;
		}
	}

	void SDRPLAY::Open(uint64_t h)
	{
		sdrplay_api_ErrT err;
		unsigned int DeviceCount;

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		if(DeviceCount < h) throw "SDRPLAY: cannot open device, handle not available.";

		device = devices[h];

		err = sdrplay_api_SelectDevice(&device);
		if(err != sdrplay_api_Success)
		{
			std::cerr << sdrplay_api_GetErrorString(err) << std::endl;
			sdrplay_api_UnlockDeviceApi();
			throw "SDRPLAY: cannot open device";
		}
		sdrplay_api_UnlockDeviceApi();

		if (sdrplay_api_GetDeviceParams(device.dev, &deviceParams) != sdrplay_api_Success)
		{
			throw "SDRPLAY: cannot get device parameters.";
		}

		chParams = deviceParams->rxChannelA;

		applySettings(s);
		setSampleRate(2304000);
	}

	void SDRPLAY::Open(SettingsSDRPLAY &s)
	{
		Open(0,s);
	}

	void SDRPLAY::Play()
	{
		fifo.Init(16 * 16384, 6);

		deviceParams->devParams->fsFreq.fsHz = sample_rate;
		chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
		chParams->ctrlParams.decimation.enable = 0;
		chParams->ctrlParams.decimation.decimationFactor = 1;
		chParams->ctrlParams.decimation.wideBandSignal = 1;
		chParams->tunerParams.bwType = sdrplay_api_BW_1_536;

		chParams->tunerParams.rfFreq.rfHz = frequency;

		sdrplay_api_CallbackFnsT cbFns;
		cbFns.StreamACbFn = callback_static;
		cbFns.EventCbFn = callback_event_static;

		if(sdrplay_api_Init(device.dev, &cbFns, (void *)this) != sdrplay_api_Success)
			throw "SDRPLAY: cannot start device";

		run_thread = std::thread(&SDRPLAY::Run, this);

		DeviceBase::Play();

		SleepSystem(10);
	}

	void SDRPLAY::Stop()
	{
		DeviceBase::Stop();

		if (run_thread.joinable())
		{
			sdrplay_api_Uninit(device.dev);
			run_thread.join();
		}
	}

	void SDRPLAY::pushDeviceList(std::vector<Description>& DeviceList)
	{
		unsigned int DeviceCount;
		if(!_api.running) throw "SDRPLAY: API v3.x not running";

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		for (int i = 0; i < DeviceCount; i++) 
		{
			// for now ....
			if(devices[i].hwVer == SDRPLAY_RSP1A_ID)
			{
				DeviceList.push_back(Description("SDRPLAY", "RSP1A", devices[i].SerNo, (uint64_t)i, Type::SDRPLAY));
			}
		}

		sdrplay_api_UnlockDeviceApi();
	}

	bool SDRPLAY::isStreaming()
	{
		return streaming;
	}

	void SDRPLAY::applySettings()
	{
		if(streaming) throw "SDRPLAY: internal error, settings modified while streaming.";

		chParams->ctrlParams.agc.enable = AGC ? sdrplay_api_AGC_DISABLE : sdrplay_api_AGC_CTRL_EN;
		chParams->tunerParams.gain.gRdB = gRdB;
		chParams->tunerParams.gain.LNAstate = LNAstate;
	}

#endif
}
