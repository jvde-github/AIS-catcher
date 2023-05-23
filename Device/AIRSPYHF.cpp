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
#include <iomanip>

#include "AIRSPYHF.h"

namespace Device {

	//---------------------------------------
	// Device AIRSPYHF

#ifdef HASAIRSPYHF

	void AIRSPYHF::Open(uint64_t h) {
		if (airspyhf_open_sn(&dev, h) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot open device");

		setDefaultRate();
		Device::Open(h);
		serial = h;
	}

#ifdef HASAIRSPYHF_ANDROID
	void AIRSPYHF::OpenWithFileDescriptor(int fd) {
		if (airspyhf_open_file_descriptor(&dev, fd) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot open device");
		setDefaultRate();
		Device::Open(0);
	}
#endif

	void AIRSPYHF::setDefaultRate() {
		uint32_t nRates;
		airspyhf_get_samplerates(dev, &nRates, 0);
		if (nRates == 0) throw std::runtime_error("AIRSPY: cannot get allowed sample rates.");

		rates.resize(nRates);
		airspyhf_get_samplerates(dev, rates.data(), nRates);

		int rate = rates[0];
		int mindelta = rates[0];
		for (auto r : rates) {
			int delta = abs((int)r - (int)getSampleRate());
			if (delta < mindelta) {
				rate = r;
				mindelta = delta;
			}
		}
		setSampleRate(rate);
	}

	void AIRSPYHF::Close() {
		Device::Close();
		airspyhf_close(dev);
	}

	void AIRSPYHF::Play() {
		applySettings();

		if (airspyhf_start(dev, AIRSPYHF::callback_static, this) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: Cannot start device");
		Device::Play();

		SleepSystem(10);
	}

	void AIRSPYHF::Stop() {
		if (Device::isStreaming()) {
			Device::Stop();
			airspyhf_stop(dev);
		}
	}

	void AIRSPYHF::callback(CFLOAT32* data, int len) {
		RAW r = { Format::CF32, data, (int)(len * sizeof(CFLOAT32)) };
		Send(&r, 1, tag);
	}

	int AIRSPYHF::callback_static(airspyhf_transfer_t* tf) {
		((AIRSPYHF*)tf->ctx)->callback((CFLOAT32*)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPYHF::setAGC() {
		if (airspyhf_set_hf_agc(dev, 1) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot set AGC to auto.");
	}

	void AIRSPYHF::setTreshold(int s) {
		if (airspyhf_set_hf_agc_threshold(dev, s) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot set AGC treshold");
	}

	void AIRSPYHF::setLNA(int s) {
		if (airspyhf_set_hf_lna(dev, s) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot set LNA");
	}

	void AIRSPYHF::getDeviceList(std::vector<Description>& DeviceList) {
		std::vector<uint64_t> serials;
		int device_count = airspyhf_list_devices(0, 0);

		serials.resize(device_count);

		if (airspyhf_list_devices(serials.data(), device_count) > 0) {
			for (int i = 0; i < device_count; i++)
				DeviceList.push_back(Description("AIRSPY", "AIRSPY HF+", Util::Convert::toHexString(serials[i]), serials[i], Type::AIRSPYHF));
		}
	}

	bool AIRSPYHF::isStreaming() {
		if (Device::isStreaming() && airspyhf_is_streaming(dev) != 1) lost = true;

		return Device::isStreaming() && airspyhf_is_streaming(dev) == 1;
	}

	void AIRSPYHF::applySettings() {
		setAGC();
		setTreshold(treshold_high ? 1 : 0);
		if (preamp) setLNA(1);

		if (airspyhf_set_samplerate(dev, sample_rate) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot set sample rate.");
		if (airspyhf_set_freq(dev, getCorrectedFrequency()) != AIRSPYHF_SUCCESS) throw std::runtime_error("AIRSPYHF: cannot set frequency.");
	}

	Setting& AIRSPYHF::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "PREAMP") {
			preamp = Util::Parse::Switch(arg);
		}
		else if (option == "TRESHOLD") {
			treshold_high = Util::Parse::Switch(arg, "HIGH", "LOW");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string AIRSPYHF::Get() {
		return Device::Get() + " preamp " + Util::Convert::toString(preamp) + " treshold " + (treshold_high ? std::string("HIGH") : std::string("LOW")) + " ";
	}
#endif
}
