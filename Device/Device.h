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

#pragma once

#include <vector>
#include <thread>
#include <fstream>

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "FIFO.h"

#include "../Library/Stream.h"
#include "../Library/Signal.h"
#include "../Library/Common.h"
#include "../Library/Utilities.h"

namespace Device{

	enum class Type { NONE, RTLSDR, AIRSPYHF, AIRSPY, SDRPLAY, WAVFILE, RAWFILE, RTLTCP, HACKRF };

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

	// will be centralized in the future and used for other clases as well...
	class Setting
	{
	public:
		// Settings
		virtual void Print(void) {}
		virtual void Set(std::string option, std::string arg) { }
	};


	class DeviceBase : public MessageIn<SystemMessage>, public StreamOut<RAW>, public Setting
	{
	protected:

		bool streaming = false;
		uint32_t sample_rate = 0;
		uint32_t frequency = 0;

	public:

		// DeviceBase
		virtual void Open(uint64_t) { std::cerr << "OPEN" << std::endl; }
		virtual void Close() { std::cerr << "CLOSE" << std::endl; }
		virtual void Play() { std::cerr << "START" << std::endl; streaming = true; }
		virtual void Stop() { std::cerr << "STOP"<<std::endl; streaming = false; }

		virtual void setSampleRate(uint32_t s) { sample_rate = s; }
		virtual void setFrequency(uint32_t f) { frequency = f; }

		virtual uint32_t getSampleRate() { return sample_rate; }
		virtual uint32_t getFrequency() { return frequency; }

		virtual bool isCallback() { return true; }
		virtual bool isStreaming() { return streaming;  }

		virtual std::vector<uint32_t> SupportedSampleRates() { return std::vector<uint32_t>(); }

		virtual void getDeviceList(std::vector<Description>& DeviceList) {}

		// MessageIn
		virtual void Message(const SystemMessage& msg) { Stop(); };
	};
}
