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

#include <cstring>
#ifndef _WIN32
#include <sys/select.h>
#endif

#include "Serial.h"

namespace Device
{

	void SerialPort::ReadAsync()
	{

		char buffer[16384];
		RAW r = {getFormat(), buffer, 0};

#ifdef _WIN32
		Info() << "Serial: starting thread" << std::endl;
		DWORD bytesRead;

		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		while (isStreaming())
		{

			OVERLAPPED overlapped = {0};
			overlapped.hEvent = hEvent;

			if (!ReadFile(serial_handle, buffer, sizeof(buffer), &bytesRead, &overlapped))
			{

				if (GetLastError() == ERROR_IO_PENDING)
				{

					DWORD dwWait = WaitForSingleObject(overlapped.hEvent, 1000);

					if (dwWait == WAIT_OBJECT_0)
					{

						if (!GetOverlappedResult(serial_handle, &overlapped, &bytesRead, FALSE))
						{
							Error() << "error reading from serial device: " << GetLastError() << std::endl;
							lost = true;
							continue;
						}
						else if (bytesRead)
						{
							r.size = bytesRead;
							Dump(r);
							Send(&r, 1, tag);
						}
					}
					else if (dwWait != WAIT_TIMEOUT)
					{
						Error() << "Serial: error reading from device: " << GetLastError() << std::endl;
						lost = true;
						continue;
					}
				}
				else
				{
					Error() << "Serial: read encountered an error: " << GetLastError() << std::endl;
					lost = true;
					continue;
				}
			}
			else if (bytesRead)
			{
				r.size = bytesRead;
				Dump(r);
				Send(&r, 1, tag);
			}
		}

		CloseHandle(hEvent);
#else

		while (isStreaming())
		{
			fd_set read_fds;

			FD_ZERO(&read_fds);
			FD_SET(serial_fd, &read_fds);

			struct timeval timeout = {1, 0};

			int rslt = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);
			if (rslt > 0)
			{
				int nread = read(serial_fd, buffer, sizeof(buffer));
				if (nread > 0)
				{
					r.size = nread;
					Dump(r);
					Send(&r, 1, tag);
				}
				else
				{
					if (nread == 0)
					{
						Error() << "Serial read encountered an error: unexpected end." << std::endl;
					}
					else
					{
						Error() << "Serial read encountered an error: " << strerror(errno) << std::endl;
					}
					lost = true;
				}
			}
			else if (rslt < 0)
			{
				Error() << "Serial read encountered an error: " << strerror(errno) << std::endl;
				lost = true;
			}
		}
#endif
	}

	void SerialPort::Dump(RAW &r)
	{
		if (print)
		{
			std::cerr << ">> ";
			for (int i = 0; i < r.size; i++)
			{
				char c = ((char *)r.data)[i];
				if (c != '\n' && c != '\r')
					std::cerr << c;
			}
			std::cerr << std::endl;
		}
	}

	SerialPort::~SerialPort()
	{
		Close();
	}

	void SerialPort::Close()
	{
#ifdef _WIN32
		if (serial_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(serial_handle);
			serial_handle = INVALID_HANDLE_VALUE;
		}
#else
		if (serial_fd != -1)
		{
			close(serial_fd);
			serial_fd = -1;
		}
#endif
	}

	void SerialPort::Play()
	{

#ifdef _WIN32
		serial_handle = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (serial_handle == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Serial: cannot open serial port " + port + ". Error: " + GetLastErrorAsString());
		}

		DCB dcb;

		if (GetCommState(serial_handle, &dcb))
		{
			dcb.BaudRate = baudrate;
			dcb.ByteSize = 8;
			dcb.Parity = NOPARITY;
			dcb.StopBits = ONESTOPBIT;
			dcb.fBinary = TRUE;
			dcb.fParity = TRUE;
			dcb.fDtrControl = DTR_CONTROL_ENABLE;
			dcb.fRtsControl = RTS_CONTROL_ENABLE;
		}
		else
			throw std::runtime_error("Serial: GetCommState failed. Error: " + GetLastErrorAsString());

		if (!SetCommState(serial_handle, &dcb))
		{
			throw std::runtime_error("Serial: SetCommState failed. Error: " + GetLastErrorAsString());
		}

		COMMTIMEOUTS ct;

		if (GetCommTimeouts(serial_handle, &ct))
		{
			ct.ReadIntervalTimeout = 2000;
			ct.ReadTotalTimeoutConstant = 2000;
			ct.ReadTotalTimeoutMultiplier = 0;
			ct.WriteTotalTimeoutConstant = 2000;
			ct.WriteTotalTimeoutMultiplier = 0;
		}
		else
			throw std::runtime_error("Serial: GetCommTimeouts failed. Error: " + GetLastErrorAsString());

		if (!SetCommTimeouts(serial_handle, &ct))
			throw std::runtime_error("Serial: SetCommTimeouts failed. Error: " + GetLastErrorAsString());
#else
		serial_fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
		if (serial_fd == -1)
		{
			throw std::runtime_error("Failed to open serial port " + port + " at baudrate " + std::to_string(baudrate) + ".");
		}

		struct termios tty;
		if (tcgetattr(serial_fd, &tty) < 0)
		{
			perror("tcgetattr");
			throw std::runtime_error("Serial: tcgetattr failed.");
		}

		speed_t brc = B9600;
		switch (baudrate)
		{
		case 9600:
			brc = B9600;
			break;
		case 19200:
			brc = B19200;
			break;
		case 38400:
			brc = B38400;
			break;
		case 57600:
			brc = B57600;
			break;
		case 115200:
			brc = B115200;
			break;
		default:
			throw std::runtime_error("Serial: unsupported baudrate.");
		}

		if (cfsetospeed(&tty, brc) < 0)
		{
			perror("cfsetospeed");
			throw std::runtime_error("Serial: cfsetospeed failed.");
		}

		if (cfsetispeed(&tty, brc) < 0)
		{
			perror("cfsetispeed");
			throw std::runtime_error("Serial: cfsetispeed failed.");
		}

		tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
		tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication
		tty.c_cflag &= ~CSIZE;	// Clear all bits that set the data size
		tty.c_cflag |= CS8;		// 8 bits per byte

		if (tcsetattr(serial_fd, TCSANOW, &tty) < 0)
		{
			perror("tcsetattr");
			throw std::runtime_error("Serial: tcsetattr failed.");
		}

		int flags = fcntl(serial_fd, F_GETFL, 0);
		fcntl(serial_fd, F_SETFL, flags | O_NONBLOCK);
#endif
		Device::Play();

		lost = false;
		read_thread = std::thread(&SerialPort::ReadAsync, this);
	}

	void SerialPort::Stop()
	{
		lost = true;
		if (read_thread.joinable())
		{
			read_thread.join();
		}
	}

	Setting &SerialPort::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "BAUDRATE")
		{
			baudrate = Util::Parse::Integer(arg);
		}
		else if (option == "PORT")
		{
#ifdef _WIN32
			if (arg.size() == 5 && std::toupper(arg[0]) == 'C' && std::toupper(arg[1]) == 'O' && std::toupper(arg[2]) == 'M' && std::isdigit(arg[3]) && std::isdigit(arg[4]))
			{
				arg = "\\\\.\\" + arg; // Windows COM port format
				Warning() << "Serial: using Windows COM port format: " << arg << std::endl;
			}
#endif
			port = arg;
		}
		else if (option == "PRINT")
		{
			print = Util::Parse::Switch(arg);
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string SerialPort::Get()
	{
		return Device::Get() + " baudrate " + std::to_string(baudrate) + " port " + port + " print " + Util::Convert::toString(print);
	}
}
