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
#include "DeviceRTLSDR.h"
#include "Common.h"
#include "Utilities.h"

namespace Device {

	void SettingsRTLSDR::Print()
	{
		std::cerr << "RTLSDR settings: -gr tuner ";

		if (tuner_AGC) std::cerr << "AUTO"; else std::cerr << tuner_Gain;

		std::cerr << " rtlagc " << (RTL_AGC ? "ON" : "OFF");
		std::cerr << " biastee " << (bias_tee ? "ON" : "OFF") << " -p " << freq_offset << std::endl;
	}

	void SettingsRTLSDR::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "TUNER")
		{
			tuner_AGC = Util::Parse::AutoFloat(arg, 0, 50, tuner_Gain);
		}
		else if (option == "RTLAGC")
		{
			RTL_AGC = Util::Parse::Switch(arg);
		}
		else if (option == "BIASTEE")
		{
			bias_tee = Util::Parse::Switch(arg);
		}
		else if (option == "FREQOFFSET")
		{
			freq_offset = Util::Parse::Integer(arg,-150,150);
		}
		else throw "Invalid setting for RTLSDR.";
	}

	//---------------------------------------
	// Device RTLSDR

#ifdef HASRTLSDR

	void RTLSDR::Open(uint64_t h,SettingsRTLSDR &s)
	{
		if (rtlsdr_open(&dev, (uint32_t)h) != 0) throw "RTLSDR: cannot open device.";

		applySettings(s);
		setSampleRate(1536000);
	}

	void RTLSDR::setTuner_GainMode(int a)
	{
		if (rtlsdr_set_tuner_gain_mode(dev, a) != 0) throw "RTLSDR: cannot set gain mode.";
	}

	void RTLSDR::setRTL_AGC(int a)
	{
		if (rtlsdr_set_agc_mode(dev, a) != 0) throw "RTLSDR: cannot set RTL AGC.";
	}

	void RTLSDR::setBiasTee(int a)
	{
#ifdef LIBRTLSDR_LEGACY
		throw "RTLSDR: bias tee not supported in this version of librtlsdr.";
#else
		if (rtlsdr_set_bias_tee(dev, a) != 0) throw "RTLSDR: cannot set bias tee.";
#endif
	}

	void RTLSDR::setTuner_Gain(FLOAT32 a)
	{
		int g = (int) a * 10;

		if(rtlsdr_set_tuner_gain_mode(dev, 1) != 0) throw "RTLSDR: cannot set gain mode.";

		int nGains = rtlsdr_get_tuner_gains(dev, NULL);
		if (nGains <= 0) throw "RTLSDR: no gains available";

		std::vector<int> gains(nGains);
		nGains = rtlsdr_get_tuner_gains(dev, gains.data());

		int gain = gains[0];

		for(auto h : gains) if(abs(h - g) < abs(g - gain)) gain = h;

		if (rtlsdr_set_tuner_gain(dev, gain) != 0) throw "RTLSDR: cannot set tuner gain.";
	}

	void RTLSDR::callback(CU8* buf, int len)
	{
		if (!fifo.Push((char*)buf, len)) std::cerr << "RTLSDR: buffer overrun." << std::endl;
	}

	void RTLSDR::callback_static(CU8* buf, uint32_t len, void* ctx)
	{
		((RTLSDR*)ctx)->callback(buf, len);
	}

	void RTLSDR::RunAsync()
	{
		rtlsdr_read_async(dev, (rtlsdr_read_async_cb_t) & (RTLSDR::callback_static), this, 0, BUFFER_SIZE);
	}

	void RTLSDR::Run()
	{
		while(isStreaming())
		{
			if (fifo.Wait())
			{
				RAW r = { Format::CU8, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1);
				fifo.Pop();
			}
			else std::cerr << "RTLSDR: timeout." << std::endl;
		}
	}

	void RTLSDR::Play()
	{
		fifo.Init(BUFFER_SIZE, 2);
		DeviceBase::Play();

		if (rtlsdr_set_sample_rate(dev, sample_rate) < 0) throw "RTLSDR: cannot set sample rate.";
		if (rtlsdr_set_center_freq(dev, frequency) < 0) throw "RTLSDR: cannot set frequency.";

		rtlsdr_reset_buffer(dev);
		async_thread = std::thread(&RTLSDR::RunAsync, this);
		run_thread = std::thread(&RTLSDR::Run, this);

		SleepSystem(10);
	}

	void RTLSDR::Stop()
	{
		DeviceBase::Stop();

		if (async_thread.joinable())
		{
			rtlsdr_cancel_async(dev);
			async_thread.join();
		}

		if (run_thread.joinable())
		{
			run_thread.join();
		}
	}

	void RTLSDR::setFrequencyCorrection(int ppm)
	{
		if (ppm != 0 && rtlsdr_set_freq_correction(dev, ppm) < 0) throw "RTLSDR: cannot set ppm error.";
	}

	void RTLSDR::applySettings(SettingsRTLSDR &s)
	{
		setFrequencyCorrection(s.freq_offset);
		setTuner_GainMode(s.tuner_AGC ? 0 : 1);

		if (!s.tuner_AGC) setTuner_Gain(s.tuner_Gain);
		if (s.RTL_AGC) setRTL_AGC(1);
		if (s.bias_tee) setBiasTee(1);
	}

	void RTLSDR::pushDeviceList(std::vector<Description>& DeviceList)
	{
		char vendor[256], product[256], serial[256];

		int DeviceCount = rtlsdr_get_device_count();

		for (int i = 0; i < DeviceCount; i++)
		{
			rtlsdr_get_device_usb_strings(i, vendor, product, serial);
			DeviceList.push_back(Description(vendor, product, serial, (uint64_t)i, Type::RTLSDR));
		}
	}

#endif
}
