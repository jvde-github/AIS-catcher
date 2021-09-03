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

#pragma once

#include <vector>
#include <thread>
#include <fstream>

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "Stream.h"
#include "Signal.h"

namespace Device{

	enum class Type { NONE, RTLSDR, AIRSPYHF,  AIRSPY, WAVFILE, RAWFILE };
	enum class Format { CU8, CF32, CS16, UNKNOWN };

	class Description
	{
		uint64_t handle;
		Type type;

		std::string vendor;
		std::string product;
		std::string serial;

	public:

		Description(std::string v, std::string p, std::string s, uint64_t h, Type t) : vendor(v), product(p), serial(s), handle(h), type(t) {}

		std::string getVendor() { return vendor; }
		std::string getProduct() { return product; }
		std::string getSerial() { return serial; }

		Type getType() { return type; }
		uint64_t getHandle() { return handle; }

 		bool operator < (const Description& b) const { return (serial < b.serial); }
	};

	class DeviceSettings
	{
	};

	class Control : public MessageIn<SystemMessage>
	{
	protected:

		bool streaming = false; 
		uint32_t sample_rate = 0;
		uint32_t frequency = 0;

	public:

		// Control
		virtual void Play() { streaming = true; }
		virtual void Pause() { streaming = false; }

		virtual void setSampleRate(uint32_t s) { sample_rate = s; }
		virtual void setFrequency(uint32_t f) { frequency = f; }
		//virtual void setAGCtoAuto() {}

		virtual uint32_t getSampleRate() { return sample_rate; }
		virtual uint32_t getFrequency() { return frequency; }
		virtual bool getAGCisAuto() { return false;  }

		virtual bool isCallback() { return true; }
		virtual bool isStreaming() { return streaming;  }

		virtual std::vector<uint32_t> SupportedSampleRates() { return std::vector<uint32_t>(); }

		static void getDeviceList(std::vector<Description>& DeviceList) {}
		static int getDeviceCount() { return 0; }

		// MessageIn
		virtual void Message(const SystemMessage& msg) { Pause(); };
	};
}
