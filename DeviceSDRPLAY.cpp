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

// https://www.sdrplay.com/docs/SDRplay_API_Specification_v3.01.pdf

#ifdef HASSDRPLAY
	SDRPLAY::_API SDRPLAY::_api;

        SDRPLAY::_API::_API()
	{
		if(sdrplay_api_Open() != sdrplay_api_Success) 
		{
			status  = "cannot connect to API.";
			running = false;
			return;
		}
		if(sdrplay_api_ApiVersion(&version) != sdrplay_api_Success)
		{
			status = "error getting API version";
			running = false;
			return;
		}
		if((int)version != 3)
		{
			status = "program requires SDRPLAY version 3.x";
			running = false;
			return;
		}

		running = true;
		status = "Up";
	}

	SDRPLAY::_API::~_API()
	{
		sdrplay_api_Close();

		running = false;
		status = "Down";
	}
#endif

	void SettingsSDRPLAY::Print()
	{
		std::cerr << "SDRPLAY Settings: -gs";
		std::cerr << std::endl;;
	}

	void SettingsSDRPLAY::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		throw "Invalid setting for SDRPLAY.";
	}

	//---------------------------------------
	// Device SDRPLAY

#ifdef HASSDRPLAY

	void SDRPLAY::openDevice(uint64_t h)
	{
	}

	void SDRPLAY::openDevice()
	{
	}

	void SDRPLAY::setSampleRate(uint32_t s)
	{
		Control::setSampleRate(s);
	}

	void SDRPLAY::setFrequency(uint32_t f)
	{
		Control::setFrequency(f);
	}

	void SDRPLAY::Play()
	{
		Control::Play();
		SleepSystem(10);
	}

	void SDRPLAY::Pause()
	{
		Control::Pause();
	}

	std::vector<uint32_t> SDRPLAY::SupportedSampleRates()
	{
                return { 3000000 };

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
	}

#endif
}
