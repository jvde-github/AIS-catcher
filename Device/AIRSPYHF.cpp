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
#include <algorithm>

#include "AIRSPYHF.h"

namespace Device {

	//---------------------------------------
	// Device AIRSPYHF

#ifdef HASAIRSPYHF

	void AIRSPYHF::Open(uint64_t h)
	{
		if (airspyhf_open_sn(&dev, h) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot open device";

		setAGC();
		setTreshold(treshold_high ? 1: 0);
		if(preamp) setLNA(1);

		uint32_t nRates;

		airspyhf_get_samplerates(dev, &nRates, 0);
		rates.resize(nRates);
		airspyhf_get_samplerates(dev, rates.data(), nRates);
		std::sort(rates.begin(), rates.end());
		setSampleRate(rates[0]);

		DeviceBase::Open(h);
	}

	void AIRSPYHF::Play()
	{
		if (airspyhf_set_samplerate(dev, sample_rate) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set sample rate.";
		if (airspyhf_set_freq(dev, frequency) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set frequency.";
		if (airspyhf_start(dev, AIRSPYHF::callback_static, this) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: Cannot start device";

		DeviceBase::Play();

		SleepSystem(10);
	}

	void AIRSPYHF::Stop()
	{
		if(DeviceBase::isStreaming())
		{
			DeviceBase::Stop();
			airspyhf_stop(dev);
		}
	}

	void AIRSPYHF::Close()
	{
		DeviceBase::Close();
		if(!disconnected) airspyhf_close(dev);
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
		RAW r = { Format::CF32, data, (int)(len * sizeof(CFLOAT32)) };
		Send(&r, 1);
	}

	int AIRSPYHF::callback_static(airspyhf_transfer_t* tf)
	{
		((AIRSPYHF*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPYHF::getDeviceList(std::vector<Description>& DeviceList)
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
		if(DeviceBase::isStreaming() && airspyhf_is_streaming(dev) != 1)
		{
			Stop();
			disconnected = true;
		}

		return DeviceBase::isStreaming();
	}

	void AIRSPYHF::Print()
	{
		std::cerr << "Airspy HF + Settings: -gh agc ON treshold " << (treshold_high ? "HIGH" : "LOW") << " preamp " << (preamp ? "ON" : "OFF") << std::endl;
	}

	void AIRSPYHF::Set(std::string option, std::string arg)
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


#endif
}
