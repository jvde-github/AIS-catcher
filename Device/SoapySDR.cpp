/*
    Copyright(c) 2021-2022 jvde.github@gmail.com

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

		if(h < dev_list.size())
		{
			if(device_args == "")
			{
				device_args = dev_list[h].getDeviceString();
				channel = dev_list[h].getChannel();
			}

			if(sample_rate == 0) setSampleRate(dev_list[h].getDefaultSampleRate());
		}
		else
			throw "SOAPYSDR: invalid handle to open device.";
	}

	void SOAPYSDR::Close()
	{
		Device::Close();
	}

	void SOAPYSDR::Play()
	{
		fifo.Init(BUFFER_SIZE, 8);

		try
		{
			dev = SoapySDR::Device::make(device_args);
		}
		catch (std::exception& e)
		{
			throw "SOAPYSDR: cannot open device.";
		}

		applySettings();

		Device::Play();
		lost = false;

		if(print) PrintActuals();

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

		SoapySDR::Stream *stream;
		try
		{
			stream = dev->setupStream(SOAPY_SDR_RX, "CF32", channels, stream_args);
		}
		catch (std::exception& e)
		{
			std::cerr << "SOAPYSDR: " << e.what() << std::endl;
			lost = true;
			return;
		}

		const int BUFFER_SIZE = dev->getStreamMTU(stream);

		std::vector<CFLOAT32> input(BUFFER_SIZE);
		void *buffers[] = { input.data() };
		long long timeNs;
		int flags;

		try
		{
			dev->activateStream(stream);

			while(isStreaming())
			{
				flags = 0; timeNs = 0;
				int ret = dev->readStream(stream, buffers, BUFFER_SIZE, flags, timeNs);

				if(ret < -1)
				{
					std::cerr << "SOAPYSDR: error reading stream: " << SoapySDR_errToStr(ret) << std::endl;
					lost = true;
				}
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
		try
		{
			dev->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
			dev->setFrequency(SOAPY_SDR_RX, channel, frequency);

			if(antenna != "")
				dev->setAntenna(SOAPY_SDR_RX,channel,antenna);

			for (auto const&x : setting_args)
			{
				dev->writeSetting(x.first,x.second);
			}

			dev->setGainMode(SOAPY_SDR_RX,channel,AGC);
			for (auto const&g : gains_args)
			{
				std::cerr << g.first << " " << g.second << std::endl;
				dev->setGain(SOAPY_SDR_RX,channel,g.first,(double)Util::Parse::Float(g.second));
			}
			if(freq_offset)
				dev->setFrequencyCorrection(SOAPY_SDR_RX,channel,freq_offset);
			if(tuner_bandwidth)
				dev->setBandwidth(SOAPY_SDR_RX,channel,tuner_bandwidth);
		}
                catch (std::exception& e)
		{
                        throw "SOAPYSDR: cannot set SoapySDR parameters.";
                }
	}

	void SOAPYSDR::getDeviceList(std::vector<Description>& DeviceList)
	{
		const auto devs = SoapySDR::Device::enumerate("");
		dev_list.clear();

		if(devs.size() | 1)
		{
			dev_list.push_back(SoapyDevice("",0,0));
			DeviceList.push_back(Description("SOAPYSDR", std::to_string(devs.size()) + " device(s)", "SOAPYSDR", (uint64_t)0, Type::SOAPYSDR));
		}
		int cnt = 1;

		for(int i = 0; i < devs.size(); i++)
		{
			auto d = devs[i];

			std::string dev_str = "driver=" + d["driver"] + ",serial="+d["serial"];

			try
			{
				auto device = SoapySDR::Device::make(dev_str);
				int nChannels = device->getNumChannels(SOAPY_SDR_RX);
				for(int c = 0; c < nChannels; c++)
				{
					std::string serial_str = "SCH" + std::to_string(c)+ "-" + d["serial"];
					int rate = device->getSampleRate(SOAPY_SDR_RX,c);
					dev_list.push_back(SoapyDevice(dev_str,c,rate));
					DeviceList.push_back(Description("SOAPYSDR", dev_str, serial_str, (uint64_t)cnt, Type::SOAPYSDR));
					cnt++;
				}
				SoapySDR::Device::unmake(device);
			}
			catch (const std::exception &ex)
			{
			}

		}
	}

	void SOAPYSDR::Print()
	{
		int i;

		std::cerr << "SOAPYSDR settings: -gu DEVICE \"" << device_args;
		std::cerr << "\" GAINS \"";
		i = 0;
		for (auto const&x : gains_args)
		{
			std::cerr << x.first << "=" << x.second;
			if(++i != gains_args.size()) std::cerr << ", ";
		}
		std::cerr << "\" STREAM \"";
		i = 0;
		for (auto const&x : stream_args)
		{
			std::cerr << x.first << "=" << x.second;
			if(++i != gains_args.size()) std::cerr << ", ";
		}
		std::cerr << "\" SETTING \"";
		i = 0;
		for (auto const&x : setting_args)
		{
			std::cerr << x.first << "=" << x.second;
			if(++i != gains_args.size()) std::cerr << ", ";
		}
		std::cerr << "\" CHANNEL " << channel << " AGC " << (AGC?"on":"off") << (antenna==""?"":(" ANTENNA "+antenna)) << antenna << std::endl;
	}

	void SOAPYSDR::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "DEVICE")
		{
			device_args = arg;
			return;
		}
		else if(option == "GAIN")
		{
			gains_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "STREAM")
		{
			stream_args = SoapySDR::KwargsFromString(arg);
			return;
		}
		else if(option == "SETTING")
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
                else if (option == "PROBE")
                {
                        print = Util::Parse::Switch(arg);
                }
                else if (option == "CH")
                {
                        channel = Util::Parse::Integer(arg,0,32);
                }
		else
			Device::Set(option,arg);
	}

	void SOAPYSDR::PrintActuals()
	{
		std::cerr << std::endl << "Actual device settings:\n" << "======================\n";
		std::cerr << " DRIVER      : " << dev->getDriverKey() << std::endl;
		std::cerr << " HARDWARE    : " << dev->getHardwareKey() << std::endl;
		std::cerr << " CHANNEL     : " << channel << std::endl;
		std::cerr << " ANTENNA     : " << dev->getAntenna(SOAPY_SDR_RX,channel) << std::endl;
		std::cerr << " SAMPLE RATE : " << dev->getSampleRate(SOAPY_SDR_RX,channel) << std::endl;;
		std::cerr << " FREQUENCY   : ";
		for(const auto& f : dev->listFrequencies(SOAPY_SDR_RX,channel))
			std::cerr << f << "=" << dev->getFrequency(SOAPY_SDR_RX,channel,f) << " ";
		std::cerr << "\n HARDWARE    : ";
		for (const auto &it : dev->getHardwareInfo())
		{
			std::cerr << it.first << "=" << it.second << " ";
		}

		std::cerr << std::endl << " SETTING     : ";
		for(const auto& s :dev->getSettingInfo())
		{
			std::cerr << s.key << "=" << dev->readSetting(s.key) << " ";
		}
		std::cerr << std::endl;
		std::cerr << " GAIN MODE   : " << (dev->getGainMode(SOAPY_SDR_RX,channel)?"AGC":"MANUAL") << std::endl;
		std::cerr << " GAIN LEVELS : ";
		for(const auto&g : dev->listGains(SOAPY_SDR_RX,channel))
		{
			std::cerr << g <<"=" << dev->getGain(SOAPY_SDR_RX,channel,g) << " ";
		}
		std::cerr << std::endl;
		std::cerr << std::endl;
	}

#endif
}
