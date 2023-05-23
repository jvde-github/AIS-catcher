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

#pragma once

#include <vector>
#include <thread>
#include <fstream>
#include <iostream>

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "FIFO.h"

#include "Stream.h"
#include "Common.h"
#include "Utilities.h"

namespace Device {


	class Description {
		Type type;
		uint64_t handle;

		std::string vendor;
		std::string product;
		std::string serial;

	public:
		Description(std::string v, std::string p, std::string s, uint64_t h, Type t) : vendor(v), product(p), serial(s), handle(h), type(t) {}

		std::string toString() { return vendor + ", " + product + ", SN: " + serial; }

		std::string getSerial() { return serial; }
		Type getType() { return type; }
		uint64_t getHandle() { return handle; }
	};

	class Device : public SimpleStreamOut<RAW>, public Setting {
	protected:
		bool streaming = false;
		FLOAT32 freq_offset = 0;
		int tuner_bandwidth = 0;
		uint32_t sample_rate = 0;
		uint32_t frequency = 0;
		Format format = Format::UNKNOWN;

		uint32_t getCorrectedFrequency() {
			return frequency * (1-freq_offset/1000000.0f);
		}

	public:
		// DeviceBase
		Device() {}
		Device(Format f, uint32_t s) : format(f), sample_rate(s) {}

		virtual void Open(uint64_t) {}
		virtual void OpenWithFileDescriptor(int) { throw std::runtime_error("Not supported for this device."); }

		virtual void Close() {}
		virtual void Play() { streaming = true; }
		virtual void Stop() { streaming = false; }

		virtual void setSampleRate(uint32_t s) { sample_rate = s; }
		virtual void setFrequency(uint32_t f) { frequency = f; }

		virtual uint32_t getSampleRate() { return sample_rate; }
		virtual uint32_t getFrequency() { return frequency; }

		virtual bool isCallback() { return true; }
		virtual bool isStreaming() { return streaming; }

		virtual std::vector<uint32_t> SupportedSampleRates() { return std::vector<uint32_t>(); }
		virtual void getDeviceList(std::vector<Description>& DeviceList) {}

		virtual Setting& Set(std::string option, std::string arg) {
			Util::Convert::toUpper(option);

			if (option == "RATE" || option == "SAMPLE_RATE") {
				setSampleRate((Util::Parse::Integer(arg, 0, 20000000)));
			}
			else if (option == "BW" || option == "BANDWIDTH") {
				tuner_bandwidth = Util::Parse::Integer(arg, 0, 1000000);
			}
			else if (option == "FREQOFFSET") {
				freq_offset = Util::Parse::Float(arg, -150, 150);
			}
			else if (option == "FORMAT") {
				Format f;
				if (!Util::Parse::StreamFormat(arg, f))
					throw std::runtime_error("Unknown file format specification.");
				else
					setFormat(f);
			}
			else
				throw std::runtime_error("Invalid Device setting: \"" + option + "\"");

			return *this;
		}

		virtual std::string Get() {

			std::string str = "rate " + std::to_string(getSampleRate() / 1000) + "K";
			if (tuner_bandwidth) str += " bw " + std::to_string(tuner_bandwidth / 1000) + "K";
			if (freq_offset) str += " freqoffset " + std::to_string(freq_offset);
			str += " format " + Util::Convert::toString(format);

			return str;
		}

		virtual std::string getProduct() {
			return "";
		}
		virtual std::string getVendor() {
			return "";
		}
		virtual std::string getSerial() {
			return "";
		}

		virtual void setFormat(Format f) { format = f; }
		virtual Format getFormat() { return format; }
	};
}
