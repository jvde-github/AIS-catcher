/*
	Copyright(c) 2021-2025 jvde.github@gmail.com
	Copyright(c) 2021-2022 gtlittlewing
	Copyright(c) 2025 Benjamin Vernoux <bvernoux@hydrasdr.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <iomanip>

#include "HYDRASDR.h"

namespace Device
{

	//----------------------------------------
	// Device HYDRASDR

#ifdef HASHYDRASDR

	void HYDRASDR::Open(uint64_t h)
	{
		if (hydrasdr_open_sn(&dev, h) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot open device.");

		if (real_mode)
			hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_FLOAT32_REAL);
		else
			hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_FLOAT32_IQ);

		setDefaultRate();
		Device::Open(h);
		serial = h;
	}

#ifdef HASHYDRASDR_ANDROID
	void HYDRASDR::OpenWithFileDescriptor(int fd)
	{
		if (hydrasdr_open_fd(&dev, fd) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot open device.");

		if (real_mode)
			hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_FLOAT32_REAL);
		else
			hydrasdr_set_sample_type(dev, HYDRASDR_SAMPLE_FLOAT32_IQ);

		setDefaultRate();
		Device::Open(0);
	}
#endif

	void HYDRASDR::setDefaultRate()
	{
		uint32_t nRates;
		hydrasdr_get_samplerates(dev, &nRates, 0);
		if (nRates == 0)
			throw std::runtime_error("HYDRASDR: cannot get allowed sample rates.");

		rates.resize(nRates);
		hydrasdr_get_samplerates(dev, rates.data(), nRates);

		int rate = rates[0];
		int mindelta = rates[0];
		for (const auto &r : rates)
		{
			int delta = abs((int)r - (int)getSampleRate());
			if (delta < mindelta)
			{
				rate = r;
				mindelta = delta;
			}
		}

		setSampleRate(rate);
	}

	void HYDRASDR::Close()
	{
		Device::Close();
		hydrasdr_close(dev);
	}

	void HYDRASDR::Play()
	{
		applySettings();

		if (hydrasdr_start_rx(dev, HYDRASDR::callback_static, this) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: Cannot start streaming");
		Device::Play();

		SleepSystem(10);
	}

	void HYDRASDR::Stop()
	{
		if (Device::isStreaming())
		{
			Device::Stop();
			hydrasdr_stop_rx(dev);
		}
	}

	void HYDRASDR::callback(CFLOAT32 *data, int len)
	{
		RAW r = {real_mode ? Format::F32_FS4 : Format::CF32, data, (int)(len * (real_mode ? sizeof(FLOAT32) : sizeof(CFLOAT32)))};
		Send(&r, 1, tag);
	}

	int HYDRASDR::callback_static(hydrasdr_transfer *tf)
	{
		((HYDRASDR *)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void HYDRASDR::setLNA_AGC(int a)
	{
		if (hydrasdr_set_lna_agc(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set LNA AGC.");
	}

	void HYDRASDR::setMixer_AGC(int a)
	{
		if (hydrasdr_set_mixer_agc(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set MIXER AGC.");
	}

	void HYDRASDR::setLNA_Gain(int a)
	{
		if (hydrasdr_set_lna_gain(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set LNA gain.");
	}

	void HYDRASDR::setMixer_Gain(int a)
	{
		if (hydrasdr_set_mixer_gain(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set Mixer gain.");
	}

	void HYDRASDR::setBiasTee(bool b)
	{
		if (hydrasdr_set_rf_bias(dev, b) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set Bias Tee.");
	}

	void HYDRASDR::setVGA_Gain(int a)
	{
		if (hydrasdr_set_vga_gain(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set VGA gain.");
	}

	void HYDRASDR::setSensitivity_Gain(int a)
	{
		if (hydrasdr_set_sensitivity_gain(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set Sensitivity gain.");
	}

	void HYDRASDR::setLinearity_Gain(int a)
	{
		if (hydrasdr_set_linearity_gain(dev, a) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set Linearity gain.");
	}

	void HYDRASDR::getDeviceList(std::vector<Description> &DeviceList)
	{
		std::vector<uint64_t> serials;
		int device_count = hydrasdr_list_devices(0, 0);

		serials.resize(device_count);

		if (hydrasdr_list_devices(serials.data(), device_count) > 0)
			for (int i = 0; i < device_count; i++)
				DeviceList.push_back(Description("HYDRASDR", "HYDRASDR", Util::Convert::toHexString(serials[i]), serials[i], Type::HYDRASDR));
	}

	bool HYDRASDR::isStreaming()
	{
		if (Device::isStreaming() && hydrasdr_is_streaming(dev) != 1)
		{
			lost = true;
		}

		return Device::isStreaming() && hydrasdr_is_streaming(dev) == 1;
	}

	// R82x (R820T/R828D) tuner bandwidth control tables
	// Taken from acarsdec and pioneered by T. Leconte: https://github.com/TLeconte/acarsdec
	// HydraSDR RFOne uses R828D which is register-compatible with R820T
	static const unsigned int r82x_hf[] = {1953050, 1980748, 2001344, 2032592, 2060291, 2087988};
	static const unsigned int r82x_lf[] = {525548, 656935, 795424, 898403, 1186034, 1502073, 1715133, 1853622};

	void HYDRASDR::applyBandwidth()
	{
		if (tuner_bandwidth >= 0 && real_mode)
		{
			int i, j;

			for (i = 7; i >= 0; i--)
				if ((r82x_hf[5] - r82x_lf[i]) >= (unsigned int)tuner_bandwidth)
					break;

			if (i >= 0)
			{
				for (j = 5; j >= 0; j--)
					if ((r82x_hf[j] - r82x_lf[i]) <= (unsigned int)tuner_bandwidth)
						break;
				if (j < 5)
					j++;

				// Register 10 (0x0A): HPF corner frequency selection
				// Register 11 (0x0B): LPF corner frequency selection
				hydrasdr_r82x_write(dev, 10, 0xB0 | (15 - j));
				hydrasdr_r82x_write(dev, 11, 0xE0 | (15 - i));

				std::cerr << "HYDRASDR: setting tuner bandwidth to " << tuner_bandwidth << " Hz, using HF range " << r82x_hf[j] << " Hz to " << r82x_lf[i] << " Hz." << std::endl;
			}
		}
	}

	void HYDRASDR::applySettings()
	{
		switch (mode)
		{
		case HYDRASDRGainMode::Sensitivity:
			setSensitivity_Gain(gain);
			break;

		case HYDRASDRGainMode::Linearity:
			setLinearity_Gain(gain);
			break;

		case HYDRASDRGainMode::Free:
			setLNA_Gain(LNA_Gain);
			setMixer_Gain(mixer_Gain);
			setVGA_Gain(VGA_Gain);

			setLNA_AGC((int)LNA_AGC);
			setMixer_AGC((int)mixer_AGC);
			break;
		}
		if (bias_tee)
			setBiasTee(true);

		if (hydrasdr_set_samplerate(dev, sample_rate) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set sample rate.");

		applyBandwidth();

		if (hydrasdr_set_freq(dev, getCorrectedFrequency()) != HYDRASDR_SUCCESS)
			throw std::runtime_error("HYDRASDR: cannot set frequency.");
	}
#endif

	Setting &HYDRASDR::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "SENSITIVITY")
		{
			if (!explicit_gain)
				mode = HYDRASDRGainMode::Sensitivity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "LINEARITY")
		{
			if (!explicit_gain)
				mode = HYDRASDRGainMode::Linearity;
			gain = Util::Parse::Integer(arg, 0, 21);
		}
		else if (option == "VGA")
		{
			if (!explicit_gain)
				mode = HYDRASDRGainMode::Free;
			VGA_Gain = Util::Parse::Integer(arg, 0, 14);
		}
		else if (option == "MIXER")
		{
			if (!explicit_gain)
				mode = HYDRASDRGainMode::Free;
			mixer_AGC = Util::Parse::AutoInteger(arg, 0, 14, mixer_Gain);
		}
		else if (option == "LNA")
		{
			if (!explicit_gain)
				mode = HYDRASDRGainMode::Free;
			LNA_AGC = Util::Parse::AutoInteger(arg, 0, 14, LNA_Gain);
		}
		else if (option == "BIASTEE")
		{
			bias_tee = Util::Parse::Switch(arg);
		}
		else if (option == "REAL_MODE")
		{
			real_mode = Util::Parse::Switch(arg);

			if (real_mode)
				format = Format::F32_FS4;
			else
				format = Format::CF32;
		}
		else if (option == "GAIN_MODE")
		{
			Util::Convert::toUpper(arg);
			explicit_gain = true;

			if (arg == "SENSITIVITY")
				mode = HYDRASDRGainMode::Sensitivity;
			else if (arg == "LINEARITY")
				mode = HYDRASDRGainMode::Linearity;
			else if (arg == "FREE")
				mode = HYDRASDRGainMode::Free;
			else
				throw std::runtime_error("HYDRASDR: invalid gain mode.");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string HYDRASDR::Get()
	{
		std::string str;

		switch (mode)
		{
		case HYDRASDRGainMode::Sensitivity:
			str = " sensitivity " + std::to_string(gain);
			break;
		case HYDRASDRGainMode::Linearity:
			str = " linearity " + std::to_string(gain);
			break;
		case HYDRASDRGainMode::Free:
			str += " mixer " + Util::Convert::toString(mixer_AGC, mixer_Gain);
			str += " lna " + Util::Convert::toString(LNA_AGC, LNA_Gain) + " vga " + std::to_string(VGA_Gain);
			break;
		}

		return Device::Get() + str + " biastee " + Util::Convert::toString(bias_tee) + " real_mode " + Util::Convert::toString(real_mode) + " ";
	}
}
