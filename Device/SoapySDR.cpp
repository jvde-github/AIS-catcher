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

		if(dev != NULL)
		{
			SoapySDR::Device::unmake(dev);
		}
	}

	void SOAPYSDR::Play()
	{
		fifo.Init(BUFFER_SIZE, 2);

    		try {
        		dev = SoapySDR::Device::make(args);
    		}
    		catch (std::exception& e) {
			throw "SOAPYSDR: cannot open device.";
		}

		applySettings();

		Device::Play();
		lost = false;

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
	}

	void SOAPYSDR::RunAsync()
	{
		std::vector<size_t> channels;
		channels.push_back(0);

		auto kwargs = SoapySDR::KwargsFromString(args);
		auto stream = dev->setupStream(SOAPY_SDR_RX, "CF32", channels, kwargs);

        	const size_t N = dev->getStreamMTU(stream);
        	std::vector<CFLOAT32> buf(N);

		dev->activateStream(stream);
    		try {
			while(isStreaming())
			{

        			void *buffs[1];
        			buffs[0] = buf.data();

        			int flags = 0;
        			long long timeNs = 0;

        			int ret = dev->readStream(stream, buffs, N, flags, timeNs);

				if(ret < 0) lost = true;
				else if (isStreaming() && !fifo.Push((char*)buffs[0], ret)) std::cerr << "SOAPYSDR: buffer overrun." << std::endl;
			}
		}
		catch (std::exception& e) {
			lost = true;
    		}

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
	}

	void SOAPYSDR::getDeviceList(std::vector<Description>& DeviceList)
	{
		int i = 0;
		const auto foundDevices = SoapySDR::Device::enumerate("");

		for(auto d: foundDevices)
		{
			DeviceList.push_back(Description("SOAPYSDR", d.at("driver"), d.at("label"), (uint64_t)i++, Type::SOAPYSDR));
		}
	}

	void SOAPYSDR::Print()
	{
		std::cerr << "SOAPYSDR settings: -gu ARG " << args << std::endl;
	}

	void SOAPYSDR::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "ARG")
		{
			args = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		Device::Set(option,arg);
	}
#endif
}
