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
#include "Utilities.h"

namespace Device {


	void SettingsAIRSPY::Print()
	{
		std::cerr << "Airspy Settings: -gm";

		switch (mode)
		{
		case Device::AIRSPYGainMode::Sensitivity:
			std::cerr << " sensitivity " << gain;
			break;

		case Device::AIRSPYGainMode::Linearity:
			std::cerr << " linearity " << gain;
			break;

		case Device::AIRSPYGainMode::Free:

			std::cerr << " lna ";
			if(LNA_AGC) std::cerr << "AUTO";
			else std::cerr << LNA_Gain;

			std::cerr << " mixer ";
			if(mixer_AGC) std::cerr << "AUTO";
			else std::cerr << mixer_Gain; 

			std::cerr << " vga " << VGA_Gain;
			break;
		}
		std::cerr << " biastee " << (bias_tee ? "ON" : "OFF") << std::endl;;
	}

	void SettingsAIRSPY::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "SENSITIVITY")
		{
			mode = Device::AIRSPYGainMode::Sensitivity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "LINEARITY")
		{
			mode = Device::AIRSPYGainMode::Linearity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "VGA")
		{
			mode = Device::AIRSPYGainMode::Free;
			VGA_Gain = Util::Parse::Integer(arg, 0, 14);
		}
		else if (option == "MIXER")
		{
			mode = Device::AIRSPYGainMode::Free;
			mixer_AGC = Util::Parse::AutoInteger(arg, 0, 14, mixer_Gain);
		}
		else if (option == "LNA")
		{
			mode = Device::AIRSPYGainMode::Free;
			LNA_AGC = Util::Parse::AutoInteger(arg, 0, 14, LNA_Gain);;
		}
		else if (option == "BIASTEE")
		{
			bias_tee = Util::Parse::Switch(arg);
		}
		else
			throw " Invalid setting for AIRSPY.";
	}

	//---------------------------------------
	// Device AIRSPY

#ifdef HASAIRSPY

	void AIRSPY::Open(uint64_t h,SettingsAIRSPY &s)
	{
		if (airspy_open_sn(&dev, h) != AIRSPY_SUCCESS) throw "AIRSPY: cannot open device";

		applySettings(s);
	}

	void AIRSPY::Open(SettingsAIRSPY &s)
	{
		if (airspy_open(&dev) != AIRSPY_SUCCESS) throw "AIRSPY: cannot open device";

		applySettings(s);
	}

	void AIRSPY::setSampleRate(uint32_t s)
	{
		if (airspy_set_samplerate(dev, s) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set sample rate.";
		DeviceBase::setSampleRate(s);
	}

	void AIRSPY::setFrequency(uint32_t f)
	{
		if (airspy_set_freq(dev, f) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set frequency.";
		DeviceBase::setFrequency(f);
	}

	void AIRSPY::setLNA_AGC(int a)
	{
		if (airspy_set_lna_agc(dev, a) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set LNA AGC.";
	}

	void AIRSPY::setMixer_AGC(int a)
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
		RAW r = { Format::CF32 , data, len * sizeof(CFLOAT32) };
		Send(&r, 1 );
	}

	int AIRSPY::callback_static(airspy_transfer_t* tf)
	{
		((AIRSPY*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPY::Play()
	{
		DeviceBase::Play(); 

		if (airspy_start_rx(dev, AIRSPY::callback_static, this) != AIRSPY_SUCCESS)
			throw "AIRSPY: Cannot open device";

		SleepSystem(10);
	}

	void AIRSPY::Stop()
	{
		airspy_stop_rx(dev);
		streaming = false;

		DeviceBase::Stop();
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
			for (int i = 0; i < device_count; i++) 
			{
				std::stringstream serial;
				serial << std::uppercase << std::hex << serials[i];
				DeviceList.push_back(Description("AIRSPY", "AIRSPY", serial.str(), (uint64_t)i, Type::AIRSPY));
			}
		}
	}

	bool AIRSPY::isStreaming()
	{
		return airspy_is_streaming(dev) == 1;
	}

	void AIRSPY::applySettings(SettingsAIRSPY &s)
	{
		switch (s.mode)
		{
		case Device::AIRSPYGainMode::Sensitivity:
			setSensitivity_Gain(s.gain);
			break;

		case Device::AIRSPYGainMode::Linearity:
			setLinearity_Gain(s.gain);
			break;

		case Device::AIRSPYGainMode::Free:
			setLNA_AGC( (int) s.LNA_AGC );
			if (!s.LNA_AGC) setLNA_Gain(s.LNA_Gain);

			setMixer_AGC((int)s.mixer_AGC);
			if (!s.mixer_AGC) setMixer_Gain(s.mixer_Gain);

			setVGA_Gain(s.VGA_Gain);

			break;
		}
		if(s.bias_tee) setBiasTee(true);
	}

#endif
}
