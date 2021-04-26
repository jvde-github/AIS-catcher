/*
Copyright(c) 2021 Jasper van den Eshof

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

#include "Device.h"
#include "Common.h"

namespace Device {

	//---------------------------------------
	// RAW CU8 file

	bool RAWFile::isStreaming()
	{
		buffer.resize(buffer_size);
		file.read((char*)buffer.data(), buffer.size() * sizeof(CU8));
		int len = file.gcount() / sizeof(CU8);

		if (!file)
			Pause();
		else
		{
			Send((CU8*)buffer.data(), len);
		}

		return streaming;
	}
	void RAWFile::openFile(std::string filename)
	{
		file.open(filename, std::ios::out | std::ios::binary);
		if (!file) throw "Error: Cannot read from RAW file.";
	}

	void RAWFile::getAvailableSampleRates(std::vector<uint32_t>& rates)
	{
		rates.clear();
		rates.push_back(288000);
		rates.push_back(384000);
		rates.push_back(768000);
		rates.push_back(1536000);
	}

	//---------------------------------------
	// WAV file, FLOAT only

	struct WAVFileFormat
	{
		uint32_t groupID;
		uint32_t size;
		uint32_t riffType;
		uint32_t chunkID;
		uint32_t chunkSize;	
		uint16_t wFormatTag;
		uint16_t wChannels;
		uint32_t dwSamplesPerSec;	
		uint32_t dwAvgBytesPerSec;
		uint16_t wBlockAlign;
		uint16_t wBitsPerSample;
		uint32_t dataID;	
		uint32_t dataSize;
	};

	bool WAVFile::isStreaming()
	{
		if (buffer.size() != buffer_size) buffer.resize(buffer_size);

		file.read((char*)buffer.data(), buffer_size);

		if (!file) 
			Pause();
		else
			StreamOut<CFLOAT32>::Send((CFLOAT32*)buffer.data(), file.gcount() / (sizeof(CFLOAT32)));
		
		return streaming;
	}
	void WAVFile::openFile(std::string filename)
	{
		struct WAVFileFormat header;

		file.open(filename, std::ios::out | std::ios::binary);
		file.read((char*)&header, sizeof(struct WAVFileFormat));

		if (!file) throw "Error: Cannot read from WAV file.";

		bool valid = true;

		valid &= header.wChannels == 2;
		valid &= header.wFormatTag == 3; // FLOAT

		valid &= header.groupID == 0x46464952;
		valid &= header.riffType == 0x45564157;
		valid &= header.dataID == 0x61746164;

		if (!valid) throw "Eror: Not a supported WAV-file.";

		sample_rate = header.dwSamplesPerSec;
	}

	void WAVFile::getAvailableSampleRates(std::vector<uint32_t>& rates)
	{
		rates.clear();
		rates.push_back(sample_rate);
	}

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

	void RTLSDR::setAGCtoAuto()
	{
		if (rtlsdr_set_tuner_gain_mode(dev, 0) != 0) throw "RTLSDR: cannot set AGC.";
	}

	void RTLSDR::callback(CU8* buf, int len)
	{
		Send((const CU8*)buf, len / 2);
	}

	void RTLSDR::callback_static(CU8* buf, uint32_t len, void* ctx)
	{
		((RTLSDR*)ctx)->callback(buf, len);
	}

	void RTLSDR::start_async_static(RTLSDR* c)
	{
		rtlsdr_read_async(c->getDevice(), (rtlsdr_read_async_cb_t) &(RTLSDR::callback_static), c, 0, BufferLen);
	}

	void RTLSDR::Play()
	{
		Control::Play(); Start();

		rtlsdr_reset_buffer(dev);
		async_thread = std::thread(RTLSDR::start_async_static, this);
		SleepSystem(10);
	}

	void RTLSDR::Pause()
	{
		if (async_thread.joinable())
		{
			rtlsdr_cancel_async(dev);
			async_thread.join();
		}

		Stop(); Control::Pause();
	}

	void RTLSDR::getAvailableSampleRates(std::vector<uint32_t>& rates)
	{
		rates.clear();
		rates.push_back(288000);
		rates.push_back(1536000);
	}

	void RTLSDR::pushDeviceList(std::vector<Description>& DeviceList)
	{
		char vendor[256], product[256], serial[256];

		int DeviceCount = rtlsdr_get_device_count();

		for (int i = 0; i < DeviceCount; i++)
		{
			rtlsdr_get_device_usb_strings(i, vendor, product, serial);

			std::stringstream s;
			s << "RTL SDR " << vendor << " " << product << " [" << serial << "]";

			Description d = Description(s.str(), (uint64_t)i, Type::RTLSDR);
			DeviceList.push_back(d);
		}
	}

	int RTLSDR::getDeviceCount()
	{
		return rtlsdr_get_device_count();
	}


#endif


	//---------------------------------------
	// Device AIRSPYHF

#ifdef HASAIRSPYHF

	void AIRSPYHF::openDevice(uint64_t h)
	{
		if (airspyhf_open_sn(&dev, h) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: cannot open device";
	}

	void AIRSPYHF::openDevice()
	{
		if (airspyhf_open(&dev) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: cannot open device";

		return;
	}

	void AIRSPYHF::setSampleRate(uint32_t s)
	{
		if (airspyhf_set_samplerate(dev, s) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set sample rate.";
		Control::setSampleRate(s);
	}

	void AIRSPYHF::setFrequency(uint32_t f)
	{
		if (airspyhf_set_freq(dev, f) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set frequency.";
		Control::setFrequency(f);
	}

	void AIRSPYHF::setAGCtoAuto()
	{
		airspyhf_set_hf_agc(dev, 1);
		if (airspyhf_set_hf_agc(dev, 1) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC to auto.";
		if (airspyhf_set_hf_agc_threshold(dev, 1) != AIRSPYHF_SUCCESS) throw "AIRSPYHF: cannot set AGC treshold to high.";

		Control::setAGCtoAuto();
	}

	void AIRSPYHF::callback(CFLOAT32* data, int len)
	{
		Send((const CFLOAT32*)data, len );
	}

	int AIRSPYHF::callback_static(airspyhf_transfer_t* tf)
	{
		((AIRSPYHF*)tf->ctx)->callback((CFLOAT32 *)tf->samples, tf->sample_count);
		return 0;
	}

	void AIRSPYHF::Play()
	{
		Control::Play(); Start();

		if (airspyhf_start(dev, AIRSPYHF::callback_static, this) != AIRSPYHF_SUCCESS)
			throw "AIRSPYHF: Cannot open device";

		SleepSystem(10);
	}

	void AIRSPYHF::Pause()
	{
		airspyhf_stop(dev);
		streaming = false;

		Stop(); Control::Pause();
	}

	void AIRSPYHF::getAvailableSampleRates(std::vector<uint32_t> &rates)
	{
		uint32_t nRates; 
		std::vector<uint32_t> s;

		airspyhf_get_samplerates(dev, &nRates, 0);
		rates.resize(nRates);

		airspyhf_get_samplerates(dev, rates.data(), nRates);
	}

	void AIRSPYHF::pushDeviceList(std::vector<Description>& DeviceList)
	{
		std::vector<uint64_t> serials;
		int device_count = airspyhf_list_devices(0, 0);

		serials.resize(device_count);

		if (airspyhf_list_devices(serials.data(), device_count) > 0) 
		{
			for (int i = 0; i < device_count; i++) {
				std::stringstream s;
				s << "AIRSPY HF+  [" << std::uppercase << std::hex << serials[i] << "]";

				Description d = Description(s.str(), (uint64_t)i, Type::AIRSPYHF);
				DeviceList.push_back(d);
			}
		}
	}

	int AIRSPYHF::getDeviceCount()
	{
		return airspyhf_list_devices(0, 0);
	}

	bool AIRSPYHF::isStreaming()
	{
		return airspyhf_is_streaming(dev) == 1;
	}
#endif
}
