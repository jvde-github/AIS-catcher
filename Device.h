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

#ifdef HASRTLSDR
#include <rtl-sdr.h>
#endif
#ifdef HASAIRSPYHF
#include <libairspyhf/airspyhf.h>
#endif

namespace Device{

	enum class Type { NONE, RTLSDR, AIRSPYHF, WAVFILE, RAWFILE };
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
		virtual void setAGCtoAuto() {}

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

	class WAVFile : public Control, public StreamOut<CFLOAT32>
	{
		std::ifstream file;

		std::vector<uint8_t> buffer;
		const int buffer_size = 384000;

	public:

		// Control
		void Play() { Control::Play(); }
		void Pause() { Control::Pause(); }

		void setSampleRate(uint32_t s) { }

		bool isCallback() { return false; }
		bool isStreaming();

		static void pushDeviceList(std::vector<Description>& DeviceList)
		{
			DeviceList.push_back(Description("FILE", "WAV", "0", 0, Type::WAVFILE));
		}

		static int getDeviceCount() { return 1; }
		std::vector<uint32_t> SupportedSampleRates();

		// Device specific
		void openFile(std::string filename);
	};

	class RAWFile : public Control, public StreamOut<CFLOAT32>
	{
		std::ifstream file;

		std::vector<char> buffer;
		std::vector<CFLOAT32> output;
		const int buffer_size = 16 * 16384;

		Format format = Format::CU8;

	public:

		// Control
		void Play() { Control::Play(); }
		void Pause() { Control::Pause(); }

		bool isCallback() { return false; }
		bool isStreaming();

		static void pushDeviceList(std::vector<Description>& DeviceList)
		{
			DeviceList.push_back(Description("FILE", "RAV", "0", 0, Type::RAWFILE));
		}
		static int getDeviceCount() { return 1; }

		std::vector<uint32_t> SupportedSampleRates();

		// Device specific
		void setFormat(Format f) { format = f; }
		void openFile(std::string filename);
	};

	class RTLSDR : public Control, public StreamOut<CU8>
	{
#ifdef HASRTLSDR

		rtlsdr_dev_t* dev = NULL;
		std::thread async_thread;
		std::thread demod_thread;

		std::vector<std::vector<CU8>> fifo;
		static const int sizeFIFO = 2;
		int head = 0;
		int tail = 0;
		std::atomic<int> count;

		std::mutex fifo_mutex;
		std::condition_variable fifo_cond;

		static void callback_static(CU8* buf, uint32_t len, void* ctx);
		static void start_async_static(RTLSDR* c);
		static void demod_async_static(RTLSDR* c);

		void callback(CU8* buf, int len);
		void Demodulation();

		static const uint32_t BufferLen = 16 * 16384;
		rtlsdr_dev_t* getDevice() { return dev; }

	public:

		// Control
		void Play();
		void Pause();

		void setSampleRate(uint32_t);
		void setFrequency(uint32_t);
		void setAGCtoAuto(void);

		std::vector<uint32_t> SupportedSampleRates();

		bool isCallback() { return true; }

		static void pushDeviceList(std::vector<Description>& DeviceList);
		static int getDeviceCount();

		// Device specific

		void openDevice(uint64_t h);
		void setFrequencyCorrection(int);
	#endif
	};

	class AIRSPYHF : public Control, public StreamOut<CFLOAT32>
	{
#ifdef HASAIRSPYHF

		struct airspyhf_device* dev = NULL;

		static int callback_static(airspyhf_transfer_t* tf);
		void callback(CFLOAT32 *,int);

	public:

		// Control

		void Play();
		void Pause();

		void setSampleRate(uint32_t);
		void setFrequency(uint32_t);
		void setAGCtoAuto(void);

		std::vector<uint32_t> SupportedSampleRates();

		bool isStreaming();

		virtual bool isCallback() { return true; }

		static void pushDeviceList(std::vector<Description>& DeviceList);
		static int getDeviceCount();

		// Device specific
		void openDevice(uint64_t h);
		void openDevice();
#endif
	};
}
