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

#include "HACKRF.h"

namespace Device {

	//---------------------------------------
	// Device HACKRF

#ifdef HASHACKRF

	void HACKRF::Print()
	{
		std::cerr << "Hackrf Settings: -gf";
		std::cerr << " preamp ";
		if(preamp) std::cerr << "ON"; else std::cerr << "OFF";
		std::cerr << " lna " << LNA_Gain;
		std::cerr << " vga " << VGA_Gain;
		std::cerr << std::endl;
	}

	void HACKRF::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);
		Util::Convert::toUpper(arg);

		if (option == "LNA")
		{
			LNA_Gain = ((Util::Parse::Integer(arg, 0, 40) + 4) / 8) * 8;
		}
		else if (option == "VGA")
		{
			VGA_Gain = ((Util::Parse::Integer(arg, 0, 62) + 1) / 2) * 2;
		}
		else if (option == "PREAMP")
		{
			preamp = Util::Parse::Switch(arg);
		}
		else
			throw "Invalid setting for HACKRF.";

	}

	void HACKRF::Open(uint64_t h)
	{
		int result = hackrf_open(&device);
		if (result != HACKRF_SUCCESS) throw "HACKRF: cannot open device";

		setPREAMP(preamp ? 1 : 0);
		setLNA_Gain(LNA_Gain);
		setVGA_Gain(VGA_Gain);

		setSampleRate(6000000);

		Device::Open(h);
	}

	void HACKRF::setLNA_Gain(int a)
	{
		if (hackrf_set_lna_gain(device, a) != HACKRF_SUCCESS) throw "HACKRF: cannot set LNA gain.";
	}

	void HACKRF::setVGA_Gain(int a)
	{
		if (hackrf_set_vga_gain(device, a) != HACKRF_SUCCESS) throw "HACKRF: cannot set VGA gain.";
	}

	void HACKRF::setPREAMP(int a)
	{
		if (hackrf_set_amp_enable(device, a) != HACKRF_SUCCESS) throw "HACKRF: cannot set amp.";
	}

	void HACKRF::Close()
	{
		Device::Close();
		hackrf_close(device);
	}

	int HACKRF::callback_static(hackrf_transfer* tf)
	{
		((HACKRF*)tf->rx_ctx)->callback(tf->buffer, tf->valid_length);
		return 0;
	}

	void HACKRF::callback(uint8_t* data, int len)
	{
		int len2 = len / 2;

		if (output.size() < len2) output.resize(len2);

		// clean up to move conversion to conversion block
		for (int i = 0; i < len2; i++)
		{
			output[i].real(((int8_t)data[2 * i]) / 128.0f);
			output[i].imag(((int8_t)data[2 * i + 1]) / 128.0f);
		}

		RAW r = { Format::CF32, output.data(), (int)(output.size() * sizeof(CFLOAT32)) };
		Send(&r, 1);
	}

	void HACKRF::Play()
	{
		if (hackrf_set_sample_rate(device, sample_rate) != HACKRF_SUCCESS) throw "HACKRF: cannot set sample rate.";
		if (hackrf_set_baseband_filter_bandwidth(device, hackrf_compute_baseband_filter_bw(sample_rate)) != HACKRF_SUCCESS) throw "HACKRF: cannot set bandwidth filter to auto.";
		if (hackrf_set_freq(device, frequency) != HACKRF_SUCCESS) throw "HACKRF: cannot set frequency.";
		if (hackrf_start_rx(device, HACKRF::callback_static, this)  != HACKRF_SUCCESS) throw "HACKRF: Cannot open device";

		Device::Play();

		SleepSystem(10);
	}

	void HACKRF::Stop()
	{
		if(Device::isStreaming())
		{
			Device::Stop();
			hackrf_stop_rx(device);
		}
	}

	void HACKRF::getDeviceList(std::vector<Description>& DeviceList)
	{
		std::vector<uint64_t> serials;
		hackrf_device_list_t* list;

		if (hackrf_init() != HACKRF_SUCCESS) throw "Cannot open hackrf library";

		list = hackrf_device_list();
		serials.resize(list->devicecount);

		for (int i = 0; i < list->devicecount; i++)
		{
			if (list->serial_numbers[i])
			{
				std::stringstream serial;
				serial << std::uppercase << list->serial_numbers[i];
				DeviceList.push_back(Description("HACKRF", "HACKRF", serial.str(), (uint64_t)i, Type::HACKRF));
			}
		}
		hackrf_device_list_free(list);
	}

	bool HACKRF::isStreaming()
	{
		return Device::isStreaming() && hackrf_is_streaming(device) == HACKRF_TRUE;
	}

#endif
}
