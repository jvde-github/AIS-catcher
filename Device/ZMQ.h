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

#pragma once

#include "Device.h"

#include <fstream>
#include <iostream>

#ifdef HASZMQ
#include <zmq.h>
#endif

namespace Device {

	class ZMQ : public Device {
#ifdef HASZMQ

		void* context = NULL;
		void* subscriber = NULL;

		std::string endpoint;
		Format format = Format::CU8;

		// output vector
		static const int BUFFER_SIZE = 16 * 16384;

		std::thread async_thread;
		std::thread run_thread;

		void RunAsync();
		void Run();

		FIFO<char> fifo;
		int timeout = 100;

	public:
		// Control
		void Open(uint64_t h);
		void Close();
		void Play();
		void Stop();

		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		void Print();
		void Set(std::string option, std::string arg);
#endif
	};
}
