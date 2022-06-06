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

#include "SoapySDR.h"

namespace Device {

	//---------------------------------------
	// Device SOAPYSDR

#ifdef HASSOAPYSDR

	SOAPYSDR::SOAPYSDR()
	{
		setSampleRate(0);
	}

	void SOAPYSDR::Open(uint64_t h)
	{
		Device::Open(h);
	}

	void SOAPYSDR::Close()
	{
		Device::Close();
	}

	void SOAPYSDR::Play()
	{
		fifo.Init(BUFFER_SIZE, 8);

    		try {
        		dev = SoapySDR::Device::make(device_args);
    		}
    		catch (std::exception& e) {
			throw "SOAPYSDR: cannot open device.";
		}

		applySettings();

		Device::Play();
		lost = false;

		PrintActuals();

		async_thread = std::thread(&SOAPYSDR::RunAsync, this);
		run_thread = std::thread(&SOAPYSDR::Run, this);

		SleepSystem(10);
	}

	void SOAPYSDR::Stop()
	{
		if(Device::isStreaming())
		{
			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}

		if(dev != NULL)
		{
			SoapySDR::Device::unmake(dev);
		}
	}

	void SOAPYSDR::RunAsync()
	{
		std::vector<size_t> channels;
		channels.push_back(channel);

		auto stream = dev->setupStream(SOAPY_SDR_RX, "CF32", channels, stream_args);

		const int BUFFER_SIZE = dev->getStreamMTU(stream);

		std::vector<CFLOAT32> input(BUFFER_SIZE);
		void *buffers[] = { input.data() };
		long long timeNs;
		int flags;

		dev->activateStream(stream);
		try
		{
			while(isStreaming())
			{
				flags = 0; timeNs = 0;
				int ret = dev->readStream(stream, buffers, BUFFER_SIZE, flags, timeNs);

				if(ret < 0)
					lost = true;
				else if (isStreaming() && !fifo.Push((char*)buffers[0], ret * sizeof(CFLOAT32) ))
					std::cerr << "SOAPYSDR: buffer overrun." << std::endl;
			}
		}
		catch (std::exception& e)
		{
			std::cerr << "SOAPYSDR: exception " << e.what() << std::endl;
			lost = true;
		}
		flags = 0; timeNs = 0;
		dev->deactivateStream(stream,flags,timeNs);
		dev->closeStream(stream);

		// did we terminate too early?
		if (isStreaming()) lost = true;
	}

	void SOAPYSDR::Run()
	{
		while (isStreaming())
		{
			if (fifo.Wait())
			{
				RAW r = { Format::CF32, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1);
				fifo.Pop();
			}
			else
			{
				if (isStreaming()) std::cerr << "SOAPYSDR: timeout." << std::endl;
			}
		}
	}


	void SOAPYSDR::applySettings()
	{
		dev->setSampleRate(SOAPY_SDR_RX, 0, sample_rate);
		dev->setFrequency(SOAPY_SDR_RX, 0, frequency);
		dev->setFrequencyCorrection(SOAPY_SDR_RX,channel,freq_offset);

		if(antenna != "")
			dev->setAntenna(SOAPY_SDR_RX,channel,antenna);

		for (auto const&x : setting_args)
		{
			dev->writeSetting(x.first,x.second);
		}

		dev->setGainMode(SOAPY_SDR_RX,channel,AGC);
		if(!AGC)
		{
			if(gains_args.size())
			for (auto const&g : gains_args)
				dev->setGain(SOAPY_SDR_RX,channel,g.first,(double)Util::Parse::Float(g.second));
			else
				dev->setGain(SOAPY_SDR_RX,channel,gaindb);
		}
	}

	void SOAPYSDR::getDeviceList(std::vector<Description>& DeviceList)
	{
		int i = 0;
		const auto foundDevices = SoapySDR::Device::enumerate("");

		for(auto d: foundDevices)
		{
			DeviceList.push_back(Description("SOAPYSDR", d.at("driver"), d.at("serial"), (uint64_t)i++, Type::SOAPYSDR));
		}
	}

	void SOAPYSDR::Print()
	{
		int i;

		std::cerr << "SOAPYSDR settings: -gu DEVICE \"";
		i = 0;
		for (auto const&x : device_args)
		{
			std::cerr << x.first << "=" << x.second;
			if(++i != device_args.size()) std::cerr << ", ";
		}
		std::cerr << "\" GAINS \"";
		i = 0;
		for (auto const&x : gains_args)
		{
			std::cerr << x.first << "=" << x.second;
			if(++i != gains_args.size()) std::cerr << ", ";
		}
		std::cerr << "\"" << std::endl;
	}

	void SOAPYSDR::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "DEVICE")
		{
			device_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "GAINS")
		{
			gains_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "STREAMS")
		{
			stream_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "SETTINGS")
		{
			setting_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "ANTENNA")
		{
			antenna = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		if (option == "AGC")
                {
                        AGC = Util::Parse::Switch(arg);
                }
                else if (option == "GAINDB")
                {
                        gaindb = Util::Parse::Float(arg,0,100);
                }
                else if (option == "PRINT")
                {
                        print = Util::Parse::Switch(arg);
                }
                else if (option == "CHANNEL")
                {
                        channel = Util::Parse::Integer(arg,0,32);
                }
                else if (option == "FREQOFFSET")
                {
                        freq_offset = Util::Parse::Float(arg,-150,150);
                }
		else
			Device::Set(option,arg);
	}

	void SOAPYSDR::PrintActuals()
	{
		std::cerr << std::endl << "Actual device settings:\n" << "======================\n";
		std::cerr << " driver      : " << dev->getDriverKey() << std::endl;
		std::cerr << " hardware    : " << dev->getHardwareKey() << std::endl;
		std::cerr << " channel     : " << channel << std::endl;
		std::cerr << " antenna     : " << dev->getAntenna(SOAPY_SDR_RX,channel) << std::endl;
		std::cerr << " sample rate : " << dev->getSampleRate(SOAPY_SDR_RX,channel) << std::endl;;
		std::cerr << " frequency   : ";
		for(const auto& f : dev->listFrequencies(SOAPY_SDR_RX,channel))
			std::cerr << f << "=" << dev->getFrequency(SOAPY_SDR_RX,channel,f) << " ";
		std::cerr << "\n hardawre    : ";
		for (const auto &it : dev->getHardwareInfo())
		{
			std::cerr << it.first << "=" << it.second << " ";
		}

		std::cerr << std::endl << " setting     : ";
		for(const auto& s :dev->getSettingInfo())
		{
			std::cerr << s.key << "=" << dev->readSetting(s.key) << " ";
		}
		std::cerr << std::endl;
		std::cerr << " gain mode   : " << (dev->getGainMode(SOAPY_SDR_RX,channel)?"AGC":"MANUAL") << std::endl;
		std::cerr << " gain levels : ";
		for(const auto&g : dev->listGains(SOAPY_SDR_RX,channel))
		{
			std::cerr << g <<"=" << dev->getGain(SOAPY_SDR_RX,channel,g) << " ";
		}
		std::cerr << std::endl;
		std::cerr << std::endl;
	}

#endif
}
