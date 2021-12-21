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
#include "Utilities.h"

namespace Device {

	//---------------------------------------
	// Device AIRSPYHF

	void SettingsAIRSPYHF::Print()
	{
		std::cerr << "Airspy HF + Settings: -gh agc ON treshold " << (treshold_high ? "HIGH" : "LOW") << " preamp " << (preamp ? "ON" : "OFF") << std::endl;
	}

	void SettingsAIRSPYHF::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "PREAMP")
		{
			preamp = Util::Parse::Switch(arg);
		}
		else if(option == "TRESHOLD")
		{
			treshold_high = Util::Parse::Switch(arg,"HIGH","LOW");
		}
		else
			throw "Command line: Invalid setting for AIRSPY HF+.";
	}

#ifdef HASAIRSPYHF

	void AIRSPYHF::Open(uint64_t h,SettingsAIRSPYHF &s)
	{
		if (airspyhf_open_sn(&dev, h) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot open device";

		applySettings(s);
	}

	void AIRSPYHF::Open(SettingsAIRSPYHF &s)
	{
		if (airspyhf_open(&dev) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot open device";

		applySettings(s);
	}

	void AIRSPYHF::setSampleRate(uint32_t s)
	{
		if (airspyhf_set_samplerate(dev, s) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set sample rate.";
		DeviceBase::setSampleRate(s);
	}

	void AIRSPYHF::setFrequency(uint32_t f)
	{
		if (airspyhf_set_freq(dev, f) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set frequency.";
		DeviceBase::setFrequency(f);
	}

	void AIRSPYHF::setAGC()
	{
		if (airspyhf_set_hf_agc(dev, 1) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC to auto.";
	}

	void AIRSPYHF::setTreshold(int s)
	{
		if (airspyhf_set_hf_agc_threshold(dev, s) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC treshold";
	}

	void AIRSPYHF::setLNA(int s)
	{
		if (airspyhf_set_hf_lna(dev, s) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set LNA";
	}

	void AIRSPYHF::callback(CFLOAT32* data, int len)
	{
		RAW r = { Format::CF32, data, len * sizeof(CFLOAT32) };
		Send(&r, 1);
	}

	int AIRSPYHF::callback_static(airspyhf_transfer_t* tf)
	{
		((AIRSPYHF*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPYHF::Play()
	{
		DeviceBase::Play();

		if (airspyhf_start(dev, AIRSPYHF::callback_static, this) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: Cannot start device";

		SleepSystem(10);
	}

	void AIRSPYHF::Stop()
	{
		airspyhf_stop(dev);
		DeviceBase::Stop();
	}

	std::vector<uint32_t> AIRSPYHF::SupportedSampleRates()
	{
		uint32_t nRates;

		airspyhf_get_samplerates(dev, &nRates, 0);
		std::vector<uint32_t> rates(nRates);
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
			for (int i = 0; i < device_count; i++) 
			{
				std::stringstream serial;
				serial << std::uppercase << std::hex << serials[i];
				DeviceList.push_back(Description("AIRSPY", "AIRSPY HF+", serial.str(), (uint64_t)i, Type::AIRSPYHF));
			}
		}
	}

	bool AIRSPYHF::isStreaming()
	{
		return airspyhf_is_streaming(dev) == 1;
	}

	void AIRSPYHF::applySettings(SettingsAIRSPYHF &s)
	{
		setAGC();
		setTreshold(s.treshold_high ? 1: 0);
		if(s.preamp) setLNA(1);
	}
#endif
}
