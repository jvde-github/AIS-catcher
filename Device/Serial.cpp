/*
	Copyright(c) 2023 jvde.github@gmail.com

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

#include "Serial.h"

namespace Device {

    void SerialPort::ReadAsync() {
#ifdef _WIN32
        DWORD bytesRead;
#else
        int bytesRead;
#endif
        while (!done) {
            buffer.resize(BUFFER_SIZE);

#ifdef _WIN32
            ReadFile(serial_handle, buffer.data(), buffer.size(), &bytesRead, NULL);
#else
            bytesRead = read(serial_fd, buffer.data(), buffer.size());
#endif
            if (bytesRead > 0) {
                // Process the data read from the serial port
                std::cout.write(buffer.data(), bytesRead);
            }
        }
    }

    SerialPort::~SerialPort() {
        Close();
    }

    void SerialPort::Close() {
#ifdef _WIN32
        if (serial_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(serial_handle);
            serial_handle = INVALID_HANDLE_VALUE;
        }
#else
        if (serial_fd != -1) {
            close(serial_fd);
            serial_fd = -1;
        }
#endif
    }

    void SerialPort::Play() {
#ifdef _WIN32
        serial_handle = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (serial_handle == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open serial port " + port + " at baudrate " + std::to_string(baudrate) + ".");
        }
        
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        dcbSerialParams.BaudRate = baudrate;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        SetCommState(serial_handle, &dcbSerialParams);

#else
        serial_fd = open(port.c_str(), O_RDWR | O_NOCTTY);
        if (serial_fd == -1) {
            throw std::runtime_error("Failed to open serial port " + port + " at baudrate " + std::to_string(baudrate) + ".");
        }

        struct termios tty;
        tcgetattr(serial_fd, &tty);

        cfsetospeed(&tty, baudrate);
        cfsetispeed(&tty, baudrate);

        tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
        tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication
        tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
        tty.c_cflag |= CS8; // 8 bits per byte

        tcsetattr(serial_fd, TCSANOW, &tty);
#endif

        done = false;
        read_thread = std::thread(&SerialPort::ReadAsync, this);
    }

    void SerialPort::Stop() {
        done = true;
        if (read_thread.joinable()) {
            read_thread.join();
        }
    }

	Setting& SerialPort::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "BAUDRATE") {
			baudrate = Util::Parse::Integer(arg);
		}
		else if (option == "PORT") {
			port = arg;
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string SerialPort::Get() {
		return Device::Get() + " baudrate " + std::to_string(baudrate) + " port " + port;
	}
}
