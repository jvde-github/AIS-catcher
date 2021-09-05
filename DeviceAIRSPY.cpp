/*
Copyright(c) 2021 gtlittlewing
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
#include "DeviceAIRSPY.h"
#include "Common.h"

namespace Device {


	void SettingsAIRSPY::Print()
	{
		std::cerr << "Airspy Settings: ";

		switch (mode)
		{
		case Device::Legacy:
			std::cerr << "mode legacy";
			break;

		case Device::Sensitivity:
			std::cerr << "mode sensitivity, gain " << gain;
			break;

		case Device::Linearity:
			std::cerr << "mode linearity, gain " << gain;
			break;

		case Device::Manual:
			std::cerr << "mode free, ";
			std::cerr << "lna_mode " << (LNA_AGC?"auto" : "manual") << ", lna_gain " << LNA_Gain;
			std::cerr << ", vga_gain  " << VGA_Gain;
			std::cerr << ", mixer_mode " << (mixer_AGC ? "auto" : "manual") << ", mixer_gain: " << mixer_Gain;
			break;
		}
		std::cerr << ", bias_tee " << (bias_tee ? "on" : "off") << std::endl;;
	}

	//---------------------------------------
	// Device AIRSPY

#ifdef HASAIRSPY

	void AIRSPY::openDevice(uint64_t h)
	{
		if (airspy_open_sn(&dev, h) != AIRSPY_SUCCESS)
			throw "AIRSPY: cannot open device";
	}

	void AIRSPY::openDevice()
	{
		if (airspy_open(&dev) != AIRSPY_SUCCESS)
			throw "AIRSPY: cannot open device";

		return;
	}

	void AIRSPY::setSampleRate(uint32_t s)
	{
		if (airspy_set_samplerate(dev, s) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set sample rate.";
		Control::setSampleRate(s);
	}

	void AIRSPY::setFrequency(uint32_t f)
	{
		if (airspy_set_freq(dev, f) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set frequency.";
		Control::setFrequency(f);
	}

	void AIRSPY::setLNA_AGC(int a)
	{
		if (airspy_set_lna_agc(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set LNA AGC.";
	}

	void AIRSPY::setMIXER_AGC(int a)
	{
		if (airspy_set_mixer_agc(dev, 1) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set MIXER AGC.";
	}

	void AIRSPY::setLNA_Gain(int a)
	{
		if (airspy_set_lna_gain(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set LNA gain.";
	}


	void AIRSPY::setMixer_Gain(int a)
	{
		if (airspy_set_mixer_gain(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set Mixer gain.";
	}

        void AIRSPY::setBiasTee(bool b)
        {
                if (airspy_set_rf_bias(dev, b) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set Bias Tee.";
        }

	void AIRSPY::setVGA_Gain(int a)
	{
		if (airspy_set_vga_gain(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set VGA gain.";
	}

	void AIRSPY::setSensitivity_Gain(int a)
	{
		if (airspy_set_sensitivity_gain(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set Sensitivity gain.";
	}
	void AIRSPY::setLinearity_Gain(int a)
	{
		if (airspy_set_linearity_gain(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set Linearity gain.";
	}

	void AIRSPY::callback(CFLOAT32* data, int len)
	{
		Send((const CFLOAT32*)data, len );
	}

	int AIRSPY::callback_static(airspy_transfer_t* tf)
	{
		((AIRSPY*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPY::Play()
	{
		Control::Play(); 

		if (airspy_start_rx(dev, AIRSPY::callback_static, this) != AIRSPY_SUCCESS)
			throw "AIRSPY: Cannot open device";

		SleepSystem(10);
	}

	void AIRSPY::Pause()
	{
		airspy_stop_rx(dev);
		streaming = false;

		Control::Pause();
	}

	std::vector<uint32_t> AIRSPY::SupportedSampleRates()
	{
		uint32_t nRates; 
		std::vector<uint32_t> rates;

		airspy_get_samplerates(dev, &nRates, 0);
		rates.resize(nRates);

		airspy_get_samplerates(dev, rates.data(), nRates);

		return rates;
	}

	void AIRSPY::pushDeviceList(std::vector<Description>& DeviceList)
	{
		std::vector<uint64_t> serials;
		int device_count = airspy_list_devices(0, 0);

		serials.resize(device_count);

		if (airspy_list_devices(serials.data(), device_count) > 0) 
		{
			for (int i = 0; i < device_count; i++) {
				std::stringstream serial;
				serial << std::uppercase << std::hex << serials[i];
				DeviceList.push_back(Description("AIRSPY", "AIRSPY", serial.str(), (uint64_t)i, Type::AIRSPY));
			}
		}
	}

	int AIRSPY::getDeviceCount()
	{
		return airspy_list_devices(0, 0);
	}

	bool AIRSPY::isStreaming()
	{
		return airspy_is_streaming(dev) == 1;
	}

	void AIRSPY::setSettings(SettingsAIRSPY &s)
	{
		switch (s.mode)
		{
		case Device::Legacy:
			setLNA_AGC(1);
			break;

		case Device::Sensitivity:
			setSensitivity_Gain(s.gain);
			break;

		case Device::Linearity:
			setLinearity_Gain(s.gain);
			break;

		case Device::Manual:
			setLNA_AGC((int)s.LNA_AGC);
			if (!s.LNA_AGC) setLNA_Gain(s.LNA_Gain);
			setMIXER_AGC((int)s.mixer_AGC);
			if (!s.mixer_AGC) setLNA_Gain(s.mixer_Gain);
			setVGA_Gain(s.VGA_Gain);

			break;
		}
		if(s.bias_tee)
			setBiasTee(true);
	}

#endif
}
