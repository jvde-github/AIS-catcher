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

#include <cstring>
#include <algorithm>

#include "AIRSPY.h"

namespace Device {

	//---------------------------------------
	// Device AIRSPY

#ifdef HASAIRSPY

	void AIRSPY::Open(uint64_t h)
	{
		if (airspy_open_sn(&dev, h) != AIRSPY_SUCCESS) throw "AIRSPY: cannot open device";

		applyGainSettings();

		uint32_t nRates;
		airspy_get_samplerates(dev, &nRates, 0);
		rates.resize(nRates);
		airspy_get_samplerates(dev, rates.data(), nRates);
		std::sort(rates.begin(), rates.end());

		setSampleRate(rates[0]);

		Device::Open(h);
	}

	void AIRSPY::Play()
	{
		if (airspy_set_samplerate(dev, sample_rate) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set sample rate.";
		if (airspy_set_freq(dev, frequency) != AIRSPY_SUCCESS) throw "AIRSPY: cannot set frequency.";
		if (airspy_start_rx(dev, AIRSPY::callback_static, this) != AIRSPY_SUCCESS) throw "AIRSPY: Cannot open device";

		Device::Play();

		SleepSystem(10);
	}

	void AIRSPY::Stop()
	{
		if(Device::isStreaming())
		{
			Device::Stop();
			airspy_stop_rx(dev);
		}
	}

	void AIRSPY::Close()
	{
		Device::Close();
		airspy_close(dev);
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
		RAW r = { Format::CF32 , data, (int)(len * sizeof(CFLOAT32)) };
		Send(&r, 1 );
	}

	int AIRSPY::callback_static(airspy_transfer_t* tf)
	{
		((AIRSPY*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPY::getDeviceList(std::vector<Description>& DeviceList)
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
		if(Device::isStreaming() && airspy_is_streaming(dev) != 1)
		{
			lost = true;
		}

		return Device::isStreaming() && airspy_is_streaming(dev) == 1;
	}

	void AIRSPY::applyGainSettings()
	{
		switch (mode)
		{
		case AIRSPYGainMode::Sensitivity:
			setSensitivity_Gain(gain);
			break;

		case AIRSPYGainMode::Linearity:
			setLinearity_Gain(gain);
			break;

		case AIRSPYGainMode::Free:
			setLNA_AGC( (int) LNA_AGC );
			if (!LNA_AGC) setLNA_Gain(LNA_Gain);

			setMixer_AGC((int)mixer_AGC);
			if (!mixer_AGC) setMixer_Gain(mixer_Gain);

			setVGA_Gain(VGA_Gain);

			break;
		}
		if(bias_tee) setBiasTee(true);
	}

	void AIRSPY::Print()
	{
		std::cerr << "Airspy Settings: -gm";

		switch (mode)
		{
		case AIRSPYGainMode::Sensitivity:
			std::cerr << " sensitivity " << gain;
			break;

		case AIRSPYGainMode::Linearity:
			std::cerr << " linearity " << gain;
			break;

		case AIRSPYGainMode::Free:

			std::cerr << " lna ";
			if (LNA_AGC) std::cerr << "AUTO";
			else std::cerr << LNA_Gain;

			std::cerr << " mixer ";
			if (mixer_AGC) std::cerr << "AUTO";
			else std::cerr << mixer_Gain;

			std::cerr << " vga " << VGA_Gain;
			break;
		}
		std::cerr << " biastee " << (bias_tee ? "ON" : "OFF") << std::endl;;
	}

	void AIRSPY::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "SENSITIVITY")
		{
			mode = AIRSPYGainMode::Sensitivity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "LINEARITY")
		{
			mode = AIRSPYGainMode::Linearity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "VGA")
		{
			mode = AIRSPYGainMode::Free;
			VGA_Gain = Util::Parse::Integer(arg, 0, 14);
		}
		else if (option == "MIXER")
		{
			mode = AIRSPYGainMode::Free;
			mixer_AGC = Util::Parse::AutoInteger(arg, 0, 14, mixer_Gain);
		}
		else if (option == "LNA")
		{
			mode = AIRSPYGainMode::Free;
			LNA_AGC = Util::Parse::AutoInteger(arg, 0, 14, LNA_Gain);;
		}
		else if (option == "BIASTEE")
		{
			bias_tee = Util::Parse::Switch(arg);
		}
		else
			throw " Invalid setting for AIRSPY.";
	}
#endif
}
