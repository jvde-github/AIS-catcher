/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include "Device.h"

#ifdef HASRTLSDR
#include <rtl-sdr.h>

namespace Device
{

	enum class RTLSDRGainMode
	{
		Default
	};

	class RTLSDR : public Device
	{

		bool tuner_AGC = true;
		bool RTL_AGC = true;
		FLOAT32 tuner_Gain = 33.0;
		bool bias_tee = false;
		uint32_t BUFFER_COUNT = 24;

		rtlsdr_dev_t *dev = nullptr;

		std::string vendor, product, serial;

		std::thread async_thread;
		std::thread run_thread;

		bool lost = true;

		FIFO fifo;

		static void callback_static(CU8 *buf, uint32_t len, void *ctx);
		void callback(CU8 *buf, int len);

		void RunAsync();
		void Run();

		static const uint32_t BUFFER_SIZE = 16 * 16384;

		void setTuner_GainMode(int);
		void setTuner_Gain(FLOAT32);
		void setRTL_AGC(int);
		void setBiasTee(int);
		void setBandwidth(int);
		void setFrequencyCorrection(int);

		void applySettings();

	public:
		RTLSDR() : Device(Format::CU8, 1536000, Type::RTLSDR, "RTLSDR") {}
		~RTLSDR()
		{
			try { Close(); } catch (...) {}
		}

		void Open(uint64_t h) override;
#ifdef HASRTL_ANDROID
		void OpenWithFileDescriptor(int);
#endif
		void Play() override;
		void Stop() override;
		void Close() override;

		bool isStreaming() override { return Device::isStreaming() && !lost; }
		bool isCallback() override { return true; }

		void getDeviceList(std::vector<Description> &DeviceList) override;

		std::string getProduct() override { return product; }
		std::string getVendor() override { return vendor; }
		std::string getSerial() override { return serial; }

		void setFormat(Format f) override {}

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;
		std::string Get() override;
	};
}

#else // HASRTLSDR

namespace Device
{
	class RTLSDR : public Unavailable
	{
	public:
		RTLSDR() : Unavailable("RTLSDR", "HASRTLSDR", Type::RTLSDR) {}
	};
}

#endif // HASRTLSDR
