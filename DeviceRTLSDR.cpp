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

namespace Device {

	//---------------------------------------
	// Device RTLSDR

#ifdef HASRTLSDR

	void RTLSDR::openDevice(uint64_t h)
	{
		if (rtlsdr_open(&dev, h) != 0) throw "RTLSDR: cannot open device.";
	}

	void RTLSDR::setSampleRate(uint32_t s)
	{
		if (rtlsdr_set_sample_rate(dev, s) < 0) throw "RTLSDR: cannot set sample rate.";
		Control::setSampleRate(s);
	}

	void RTLSDR::setFrequency(uint32_t f)
	{
		if (rtlsdr_set_center_freq(dev, f) < 0) throw "RTLSDR: cannot set frequency.";
		Control::setFrequency(f);
	}

	void RTLSDR::setAGC()
	{
		if (rtlsdr_set_tuner_gain_mode(dev, 0) != 0) throw "RTLSDR: cannot set AGC.";
	}

	void RTLSDR::setGainManual()
	{
		if (rtlsdr_set_tuner_gain_mode(dev, 1) != 0) throw "RTLSDR: cannot set Manual.";
	}

	void RTLSDR::setTunerGain(int g)
	{
		if (rtlsdr_set_tuner_gain(dev, g) != 0) throw "RTLSDR: cannot set TunerGain.";
	}

	void RTLSDR::callback(CU8* buf, int len)
	{
		if(count == sizeFIFO)
		{
			std::cerr << "Buffer overrun!" << std::endl;
		}
		else
		{
			if(fifo[tail].size() != len) fifo[tail].resize(len);

			std::memcpy(fifo[tail].data(),buf,len);

			tail = (tail + 1) % sizeFIFO;

    		{
    			std::lock_guard<std::mutex> lock(fifo_mutex);
				count ++;
    		}

			fifo_cond.notify_one();
		}
	}

	void RTLSDR::callback_static(CU8* buf, uint32_t len, void* ctx)
	{
		((RTLSDR*)ctx)->callback(buf, len);
	}

	void RTLSDR::start_async_static(RTLSDR* c)
	{
		rtlsdr_read_async(c->getDevice(), (rtlsdr_read_async_cb_t) &(RTLSDR::callback_static), c, 0, BufferLen);
	}

	void RTLSDR::Demodulation()
	{
		std::cerr << "Start demodulation thread." << std::endl;

		while(isStreaming())
		{
			if (count == 0)
			{
				std::unique_lock <std::mutex> lock(fifo_mutex);
				fifo_cond.wait_for(lock, std::chrono::milliseconds((int)((float)BufferLen / (float)sample_rate * 1000.0f * 1.05f)), [this] {return count != 0; });

				if (count == 0)
					std::cerr << "Timeout on RTL SDR dongle" << std::endl;
			}

			if (count != 0)
			{
				Send((const CU8*)fifo[head].data(), fifo[head].size() / 2);
				head = (head + 1) % sizeFIFO;
				count--;
			}
		}

		std::cerr << "Stop demodulation thread." << std::endl;
	}

	void RTLSDR::demod_async_static(RTLSDR* c)
	{
		c->Demodulation();
	}

	void RTLSDR::Play()
	{
		// set up FIFO
		fifo.resize(sizeFIFO);

		count = 0;
		tail = 0;
		head = 0;

		Control::Play();

		rtlsdr_reset_buffer(dev);
		async_thread = std::thread(RTLSDR::start_async_static, this);
		demod_thread = std::thread(RTLSDR::demod_async_static, this);

		SleepSystem(10);
	}

	void RTLSDR::Pause()
	{
		Control::Pause();

		if (async_thread.joinable())
		{
			rtlsdr_cancel_async(dev);
			async_thread.join();
		}

		if (demod_thread.joinable())
		{
			demod_thread.join();
		}
	}

	void RTLSDR::setFrequencyCorrection(int ppm)
	{
		if(ppm !=0)
			if (rtlsdr_set_freq_correction(dev, ppm)<0) throw "RTLSDR: cannot set ppm error.";
	}

	void RTLSDR::setSettings(SettingsRTLSDR &s)
	{
		setFrequencyCorrection(s.correctionPPM);
		setAGC();
	}

	std::vector<uint32_t> RTLSDR::SupportedSampleRates()
	{
		return { 288000, 1536000, 1920000, 2304000 };
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

	int RTLSDR::getDeviceCount()
	{
		return rtlsdr_get_device_count();
	}

#endif
}
