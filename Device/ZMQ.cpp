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

#include "ZMQ.h"
#include "Utilities.h"

namespace Device {

	//---------------------------------------
	// Device RTLSDR

#ifdef HASZMQ
	void ZMQ::Print()
	{
		std::cerr << "ZMQ settings: -gz endpoint " << endpoint;
		std::cerr  << std::endl;
	}

	void ZMQ::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "ENDPOINT")
		{
			endpoint = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		if (option == "FORMAT")
		{
			if(!Util::Parse::StreamFormat(arg, format))
				throw "ZMQ: Unknown file format specification.";
		}
		else 
			throw "Invalid setting for ZMQ.";
	}

	void ZMQ::Close()
	{
		Device::Close();

		zmq_close(subscriber);
		zmq_ctx_destroy(context);
	}

	void ZMQ::Open(uint64_t handle)
	{
		context = zmq_ctx_new();
		subscriber = zmq_socket(context, ZMQ_SUB);
		int rc = zmq_connect(subscriber, endpoint.c_str());

		if (rc != 0)
		{
			std::cerr << "ZMQ: subscribing to " << endpoint << std::endl;
			throw "ZMQ: cannot connect subscriber.";
		}
		rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
		if (rc != 0) throw "ZMQ: cannot set socket option ZMQ_SUBSCRIBE.";
		rc = zmq_setsockopt(subscriber, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
		if (rc != 0) throw "ZMQ: cannot set socket option ZMQ_RCVTIMEO.";

		setSampleRate(288000);

		Device::Open(handle);
	}

	void ZMQ::RunAsync()
	{
		std::vector<char> data(BUFFER_SIZE);

		while (isStreaming())
		{
			int len = zmq_recv(subscriber, data.data(), BUFFER_SIZE, 0);
			if (len > 0 && !fifo.Push(data.data(), len)) std::cerr << "ZMQ: buffer overrun." << std::endl;
		}
	}

	void ZMQ::Run()
	{
		std::vector<char> output(fifo.BlockSize());

		while (isStreaming())
		{
			if (fifo.Wait())
			{
				RAW r = { format, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1);
				fifo.Pop();
			}
			else
			{
				std::cerr << "ZMQ: no signal." << std::endl;
			}
		}
	}

	void ZMQ::Play()
	{
		fifo.Init(BUFFER_SIZE);

		Device::Play();

		//set sample_rate;
		//set frequency;

		Device::Play();

		async_thread = std::thread(&ZMQ::RunAsync, this);
		run_thread = std::thread(&ZMQ::Run, this);

		SleepSystem(10);
	}

	void ZMQ::Stop()
	{
		if(Device::isStreaming())
		{
			Device::Stop();
			fifo.Reset();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}
	}

	void ZMQ::pushDeviceList(std::vector<Description>& DeviceList)
	{
		DeviceList.push_back(Description("ZMQ", "ZMQ", "ZMQ", (uint64_t)0, Type::ZMQ));
	}
#endif
}
