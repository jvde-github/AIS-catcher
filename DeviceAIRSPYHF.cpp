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
#include "DeviceAIRSPYHF.h"
#include "Common.h"

namespace Device {


	//---------------------------------------
	// Device AIRSPYHF

#ifdef HASAIRSPYHF

	void AIRSPYHF::openDevice(uint64_t h)
	{
		if (airspyhf_open_sn(&dev, h) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: cannot open device";
	}

	void AIRSPYHF::openDevice()
	{
		if (airspyhf_open(&dev) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: cannot open device";

		return;
	}

	void AIRSPYHF::setSampleRate(uint32_t s)
	{
		if (airspyhf_set_samplerate(dev, s) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set sample rate.";
		Control::setSampleRate(s);
	}

	void AIRSPYHF::setFrequency(uint32_t f)
	{
		if (airspyhf_set_freq(dev, f) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set frequency.";
		Control::setFrequency(f);
	}

	void AIRSPYHF::setAGC()
	{
		airspyhf_set_hf_agc(dev, 1);
		if (airspyhf_set_hf_agc(dev, 1) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC to auto.";
		if (airspyhf_set_hf_agc_threshold(dev, 0) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC treshold to low.";
	}

	void AIRSPYHF::callback(CFLOAT32* data, int len)
	{
		Send((const CFLOAT32*)data, len );
	}

	int AIRSPYHF::callback_static(airspyhf_transfer_t* tf)
	{
		((AIRSPYHF*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPYHF::Play()
	{
		Control::Play(); 

		if (airspyhf_start(dev, AIRSPYHF::callback_static, this) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: Cannot open device";

		SleepSystem(10);
	}

	void AIRSPYHF::Pause()
	{
		airspyhf_stop(dev);
		streaming = false;

		Control::Pause();
	}

	std::vector<uint32_t> AIRSPYHF::SupportedSampleRates()
	{
		uint32_t nRates; 
		std::vector<uint32_t> rates;

		airspyhf_get_samplerates(dev, &nRates, 0);
		rates.resize(nRates);

		airspyhf_get_samplerates(dev, rates.data(), nRates);

		return rates;
	}

	void AIRSPYHF::pushDeviceList(std::vector<Description>& DeviceList)
	{
		std::vector<uint64_t> serials;
		int device_count = airspyhf_list_devices(0, 0);

		serials.resize(device_count);

		if (airspyhf_list_devices(serials.data(), device_count) > 0) 
		{
			for (int i = 0; i < device_count; i++) {
				std::stringstream serial;
				serial << std::uppercase << std::hex << serials[i];
				DeviceList.push_back(Description("AIRSPY", "AIRSPY HF+", serial.str(), (uint64_t)i, Type::AIRSPYHF));
			}
		}
	}

	int AIRSPYHF::getDeviceCount()
	{
		return airspyhf_list_devices(0, 0);
	}

	bool AIRSPYHF::isStreaming()
	{
		return airspyhf_is_streaming(dev) == 1;
	}

	void AIRSPYHF::setSettings(SettingsAIRSPYHF &s)
	{
		setAGC();
	}

#endif
}
