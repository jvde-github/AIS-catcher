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

    	char buffer[16384];
		RAW r = { getFormat(), buffer, 0 };

#ifdef _WIN32
        DWORD bytesRead;
        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        DWORD dwCommEvent;

        while (isStreaming()) {
            if (WaitCommEvent(serial_handle, &dwCommEvent, &overlapped)) {
                if (!ReadFile(serial_handle, buffer, sizeof(buffer), &bytesRead, &overlapped)) {
                    if (GetLastError() != ERROR_IO_PENDING) { 
                        std::cerr << "Serial read encountered an error: " << GetLastError() << std::endl;
                        lost = true;
                    }
                }
                else if (bytesRead) {
                    r.size = bytesRead;
                    Send(&r, 1, tag);
                }
            }
        }

        CloseHandle(overlapped.hEvent);
#else

        while (isStreaming()) {
            fd_set read_fds;

            FD_ZERO(&read_fds);
            FD_SET(serial_fd, &read_fds);

            struct timeval timeout = { 1, 0 };

            int rslt = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);
            if (rslt > 0) {
                int nread = read(serial_fd, buffer, sizeof(buffer));
                if (nread > 0) {
                    r.size = nread;
                    Send(&r, 1, tag);
                }
                else {
                    if(nread == 0) {
                        std::cerr << "Serial read encountered an error: unexpected end." << std::endl;
                    }
                    else {
                        std::cerr << "Serial read encountered an error: " << strerror(errno) << std::endl;
                    }
                    lost = true;
                }
            } else if (rslt < 0) {        
                    std::cerr << "Serial read encountered an error: " << strerror(errno) << std::endl;
                lost = true;
            }
        }
#endif
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

        Device::Play();

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

        int flags = fcntl(serial_fd, F_GETFL, 0);
        fcntl(serial_fd, F_SETFL, flags | O_NONBLOCK);
#endif

        lost = false;
        read_thread = std::thread(&SerialPort::ReadAsync, this);
    }

    void SerialPort::Stop() {
        lost = true;
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
