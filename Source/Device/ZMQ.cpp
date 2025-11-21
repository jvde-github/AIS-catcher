/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include "ZMQ.h"

namespace Device {

	//---------------------------------------
	// Device RTLSDR

#ifdef HASZMQ

	void ZMQ::Open(uint64_t handle) {
		context = zmq_ctx_new();
		subscriber = zmq_socket(context, ZMQ_SUB);
		int rc = zmq_connect(subscriber, endpoint.c_str());

		if (rc != 0) {
			Error() << "ZMQ: subscribing to " << endpoint << std::endl;
			throw std::runtime_error("ZMQ: cannot connect subscriber.");
		}
		rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
		if (rc != 0) throw std::runtime_error("ZMQ: cannot set socket option ZMQ_SUBSCRIBE.");
		rc = zmq_setsockopt(subscriber, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
		if (rc != 0) throw std::runtime_error("ZMQ: cannot set socket option ZMQ_RCVTIMEO.");

		Device::Open(handle);
	}

	void ZMQ::Close() {
		Device::Close();

		zmq_close(subscriber);
		zmq_ctx_destroy(context);
	}

	void ZMQ::Play() {
		fifo.Init(BUFFER_SIZE);

		Device::Play();

		// apply settings

		Device::Play();

		async_thread = std::thread(&ZMQ::RunAsync, this);
		run_thread = std::thread(&ZMQ::Run, this);

		SleepSystem(10);
	}

	void ZMQ::Stop() {
		if (Device::isStreaming()) {
			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}
	}

	void ZMQ::RunAsync() {
		std::vector<char> data(BUFFER_SIZE);

		while (isStreaming()) {
			int len = zmq_recv(subscriber, data.data(), BUFFER_SIZE, 0);
			if (len > 0 && !fifo.Push(data.data(), len)) Error() << "ZMQ: buffer overrun." << std::endl;
		}
	}

	void ZMQ::Run() {
		std::vector<char> output(fifo.BlockSize());

		while (isStreaming()) {
			if (fifo.Wait()) {
				RAW r = { getFormat(), fifo.Front(), fifo.BlockSize() };
				Send(&r, 1, tag);
				fifo.Pop();
			}
			else {
				Error() << "ZMQ: no signal." << std::endl;
			}
		}
	}

	void ZMQ::getDeviceList(std::vector<Description>& DeviceList) {
		DeviceList.push_back(Description("ZMQ", "ZMQ", "ZMQ", (uint64_t)0, Type::ZMQ));
	}

	Setting& ZMQ::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "ENDPOINT") {
			endpoint = arg;
			return *this;
		}

		Device::Set(option, arg);

		return *this;
	}

	std::string ZMQ::Get() {
		return Device::Get() + " endpoint " + endpoint;
	}
#endif
}
