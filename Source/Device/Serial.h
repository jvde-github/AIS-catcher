/*
	Copyright(c) 2024 jvde.github@gmail.com

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
#include <iostream>
#include <vector>
#include <thread>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace Device
{

	class SerialPort : public Device
	{
#ifdef _WIN32
		HANDLE serial_handle = INVALID_HANDLE_VALUE;

		std::string GetLastErrorAsString()
		{
			// Get the error message ID, if any.
			DWORD errorMessageID = ::GetLastError();
			if (errorMessageID == 0)
			{
				return std::string(); // No error message has been recorded
			}

			LPSTR messageBuffer = nullptr;
			size_t size = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

			std::string message(messageBuffer, size);

			// Free the buffer.
			LocalFree(messageBuffer);

			return message;
		}
#else
		int serial_fd = -1;
#endif

		std::string port;
		int baudrate;

		std::thread read_thread;

		bool lost = false;
		bool print = false;

		static const uint32_t BUFFER_SIZE = 16 * 16384;

		void ReadAsync();
		void Dump(RAW &r);

	public:
		SerialPort() : Device(Format::TXT, 288000, Type::SERIALPORT), port(""), baudrate(38400){};
		~SerialPort();

		std::string getRateDescription() { return std::to_string(baudrate) + " baud"; }

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		Setting &Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "Serial Port"; }
		std::string getVendor() { return "Unknown"; }
		std::string getSerial() { return port; }

		void setFormat(Format f) {}

		// Control
		void Close();
		void Play();
		void Stop();
	};
}
