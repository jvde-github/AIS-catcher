/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include "HACKRF.h"

namespace Device {

	//---------------------------------------
	// Device HACKRF

#ifdef HASHACKRF

	hackrf_device_list_t* HACKRF::list = NULL;
	
	void HACKRF::Open(uint64_t h) {
		if (!list) throw std::runtime_error("HACKRF: cannot open device, internal error.");
		if (h > list->devicecount) throw std::runtime_error("HACKRF: cannot open device.");

		int result = hackrf_open_by_serial(list->serial_numbers[h], &device);
		if (result != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot open device.");

		Device::Open(h);
		serial = list->serial_numbers[h];
	}

	void HACKRF::Close() {
		Device::Close();
		hackrf_close(device);
	}

	void HACKRF::Play() {
		applySettings();

		if (hackrf_start_rx(device, HACKRF::callback_static, this) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: Cannot open device");
		Device::Play();

		SleepSystem(10);
	}

	void HACKRF::Stop() {
		if (Device::isStreaming()) {
			Device::Stop();
			hackrf_stop_rx(device);
		}
	}

	int HACKRF::callback_static(hackrf_transfer* tf) {
		((HACKRF*)tf->rx_ctx)->callback(tf->buffer, tf->valid_length);
		return 0;
	}

	void HACKRF::callback(uint8_t* data, int len) {
		RAW r = { Format::CS8, data, len };
		Send(&r, 1, tag);
	}

	void HACKRF::applySettings() {
		if (hackrf_set_amp_enable(device, preamp ? 1 : 0) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set amp.");
		if (hackrf_set_lna_gain(device, LNA_Gain) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set LNA gain.");
		if (hackrf_set_vga_gain(device, VGA_Gain) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set VGA gain.");

		if (hackrf_set_sample_rate(device, sample_rate) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set sample rate.");
		if (hackrf_set_baseband_filter_bandwidth(device, hackrf_compute_baseband_filter_bw(sample_rate)) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set bandwidth filter to auto.");
		if (hackrf_set_freq(device, getCorrectedFrequency()) != HACKRF_SUCCESS) throw std::runtime_error("HACKRF: cannot set frequency.");
	}

	void HACKRF::getDeviceList(std::vector<Description>& DeviceList) {

		if(!list) {
			list = hackrf_device_list();
		}

		for (int i = 0; i < list->devicecount; i++) {
			if (list->serial_numbers[i]) {
				std::stringstream serial;
				serial << std::uppercase << list->serial_numbers[i];
				DeviceList.push_back(Description("HACKRF", "Great Scott Gadgets", serial.str(), (uint64_t)i, Type::HACKRF));
			}
		}
	}

	Setting& HACKRF::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "LNA") {
			LNA_Gain = ((Util::Parse::Integer(arg, 0, 40) + 4) / 8) * 8;
		}
		else if (option == "VGA") {
			VGA_Gain = ((Util::Parse::Integer(arg, 0, 62) + 1) / 2) * 2;
		}
		else if (option == "PREAMP") {
			preamp = Util::Parse::Switch(arg);
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string HACKRF::Get() {

		return Device::Get() + " lna " + std::to_string(LNA_Gain) + " vga " + std::to_string(VGA_Gain) + " preamp " + Util::Convert::toString(preamp);
	}
#endif
}
