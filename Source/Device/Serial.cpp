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
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#endif

#include "Serial.h"
#include "Helper.h"

namespace
{
#ifndef _WIN32
	// Helper for POSIX serial write with CR and drain
	void WriteSerialCommand(int fd, const std::string &cmd)
	{
		std::string out = cmd + "\r";
		if (write(fd, out.c_str(), out.length()) < 0)
		{
			throw std::runtime_error("Serial: failed to send command: '" + cmd + "'.");
		}
		tcdrain(fd);
	}
#else
	// Helper for Windows serial write with CR and flush
	void WriteSerialCommand(HANDLE handle, const std::string &cmd)
	{
		std::string out = cmd + "\r";
		DWORD bytesWritten;
		if (!WriteFile(handle, out.c_str(), out.length(), &bytesWritten, nullptr))
		{
			throw std::runtime_error("Serial: failed to send command: '" + cmd + "'.");
		}
		FlushFileBuffers(handle);
	}
#endif
}

namespace Device
{
	// Initialize static member
	std::vector<std::string> SerialPort::device_list;

	void SerialPort::ReadAsync()
	{

		char buffer[16384];
		RAW r = {getFormat(), buffer, 0};

#ifdef _WIN32
		Debug() << "Serial: starting thread" << std::endl;
		DWORD bytesRead;

		while (isStreaming())
		{
			if (ReadFile(serial_handle, buffer, sizeof(buffer), &bytesRead, nullptr))
			{
				if (bytesRead > 0)
				{
					r.size = bytesRead;
					Dump(r);
					Send(&r, 1, tag);
				}
				else
				{
					SleepSystem(100);
				}
			}
			else
			{
				DWORD error = GetLastError();
				Error() << "Serial read error: " << error << std::endl;
				lost = true;
				break;
			}
		}
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

	void SerialPort::Open(uint64_t handle)
	{
		if (port.empty())
		{
			if (handle < device_list.size())
			{
				port = device_list[handle];
			}
			else
			{
				throw std::runtime_error("Serial: Invalid device handle " + std::to_string(handle));
			}
		}
	}

	void SerialPort::Play()
	{

#ifdef _WIN32
		serial_handle = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
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
			dcb.fOutX = FALSE; // Disable XON/XOFF output flow control
			dcb.fInX = FALSE;  // Disable XON/XOFF input flow control
		}
		else
			throw std::runtime_error("Serial: GetCommState failed. Error: " + GetLastErrorAsString());

		if (!SetCommState(serial_handle, &dcb))
		{
			throw std::runtime_error("Serial: SetCommState failed. Error: " + GetLastErrorAsString());
		}

		// Set timeouts for immediate return with available data
		COMMTIMEOUTS ct = {0};
		ct.ReadIntervalTimeout = MAXDWORD; // Return immediately
		ct.ReadTotalTimeoutMultiplier = 0;
		ct.ReadTotalTimeoutConstant = 0;
		ct.WriteTotalTimeoutConstant = 2000; // write timeout
		ct.WriteTotalTimeoutMultiplier = 0;

		if (!SetCommTimeouts(serial_handle, &ct))
			throw std::runtime_error("Serial: SetCommTimeouts failed. Error: " + GetLastErrorAsString());

		if (init_sequence.length())
		{
			SleepSystem(100);
			// Send initial carriage return (empty command)
			WriteSerialCommand(serial_handle, "");

			// Split by comma and send each command separately
			std::stringstream ss(init_sequence);
			std::string cmd;

			while (std::getline(ss, cmd, ','))
			{
				SleepSystem(250);
				if (!cmd.empty())
				{
					WriteSerialCommand(serial_handle, cmd);
					Info() << "Serial: init command sent \"" << cmd << "\"";
				}
			}
		}
#else
		serial_fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
		if (serial_fd == -1)
		{
			throw std::runtime_error("Failed to open serial port " + port + " at baudrate " + std::to_string(baudrate) + ".");
		}

		struct termios tty;
		if (tcgetattr(serial_fd, &tty) < 0)
		{
			throw std::runtime_error(std::string("Serial: tcgetattr failed: ") + strerror(errno));
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
			throw std::runtime_error(std::string("Serial: cfsetospeed failed: ") + strerror(errno));
		}

		if (cfsetispeed(&tty, brc) < 0)
		{
			throw std::runtime_error(std::string("Serial: cfsetispeed failed: ") + strerror(errno));
		}

		// ===== STEP 1: Control flags - set CLOCAL and CREAD first =====
		tty.c_cflag |= (CREAD | CLOCAL); // Enable receiver, ignore modem control lines

		// ===== STEP 2: Local flags  =====
		tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);

		// ===== STEP 3: Output flags =====
		tty.c_oflag &= ~OPOST; // Raw output - no processing

		// ===== STEP 4: Input flags - initial clearing (NOT ISTRIP/INPCK yet) =====
		tty.c_iflag &= ~(INLCR | IGNCR | ICRNL | IGNBRK);
#ifdef IUCLC
		tty.c_iflag &= ~IUCLC;
#endif
#ifdef PARMRK
		tty.c_iflag &= ~PARMRK;
#endif

		// ===== STEP 5: Character size =====
		tty.c_cflag &= ~CSIZE; // Clear all size bits
		tty.c_cflag |= CS8;	   // 8 bits per byte

		// ===== STEP 6: Stop bits =====
		tty.c_cflag &= ~CSTOPB; // 1 stop bit

		// ===== STEP 7: Parity - and clear INPCK/ISTRIP here =====
		tty.c_iflag &= ~(INPCK | ISTRIP);
		tty.c_cflag &= ~(PARENB | PARODD); // No parity

		// ===== STEP 8: Flow control =====
		if (flowcontrol == FlowControl::HARDWARE)
		{
			tty.c_cflag |= CRTSCTS;
			Info() << "Serial: hardware flow control (RTS/CTS) enabled" << std::endl;
		}
		else
		{
			tty.c_cflag &= ~CRTSCTS;
		}

		// Software flow control
		if (flowcontrol == FlowControl::SOFTWARE)
		{
			// Enable XON/XOFF
#ifdef IXANY
			tty.c_iflag |= (IXON | IXOFF);
			tty.c_iflag &= ~IXANY; // But not IXANY
#else
			tty.c_iflag |= (IXON | IXOFF);
#endif
			Info() << "Serial: software flow control (XON/XOFF) enabled" << std::endl;
		}
		else if (flowcontrol == FlowControl::NONE)
		{
			// Explicitly disable XON/XOFF
#ifdef IXANY
			tty.c_iflag &= ~(IXON | IXOFF | IXANY);
#else
			tty.c_iflag &= ~(IXON | IXOFF);
#endif
			Info() << "Serial: flow control disabled" << std::endl;
		}

		// ===== STEP 9: VMIN/VTIME =====
		tty.c_cc[VMIN] = 1;
		tty.c_cc[VTIME] = 0;

		// ===== STEP 10: Apply settings =====
		if (tcsetattr(serial_fd, TCSANOW, &tty) < 0)
		{
			throw std::runtime_error(std::string("Serial: tcsetattr failed: ") + strerror(errno));
		}

		// ===== POST-CONFIGURATION: Flush buffers and stabilize =====
		SleepSystem(200);
		tcflush(serial_fd, TCIOFLUSH);

		// Set DTR/RTS
		int status;
		if (ioctl(serial_fd, TIOCMGET, &status) == 0)
		{
			status |= TIOCM_DTR | TIOCM_RTS;
			ioctl(serial_fd, TIOCMSET, &status);
			SleepSystem(100);
		}

		// Send initialization sequence if provided
		if (init_sequence.length())
		{
			SleepSystem(100);
			// Send initial carriage return (empty command)
			WriteSerialCommand(serial_fd, "");

			// Split by comma and send each command separately
			std::stringstream ss(init_sequence);
			std::string cmd;

			while (std::getline(ss, cmd, ','))
			{
				if (!cmd.empty())
				{
					SleepSystem(250);
					Info() << "Serial: init command sent \"" << cmd << "\"";
					WriteSerialCommand(serial_fd, cmd);
				}
			}
		}

		// Switch to non-blocking mode for async reading
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
		else if (option == "INIT_SEQ")
		{
			init_sequence = arg;
		}
		else if (option == "DISABLE_XONXOFF")
		{
			throw std::runtime_error("Serial: DISABLE_XONXOFF option is deprecated. Use FLOWCONTROL instead.");
		}
		else if (option == "FLOWCONTROL")
		{
			Util::Convert::toUpper(arg);
			if (arg == "NONE")
				flowcontrol = FlowControl::NONE;
			else if (arg == "HARDWARE")
				flowcontrol = FlowControl::HARDWARE;
			else if (arg == "SOFTWARE")
				flowcontrol = FlowControl::SOFTWARE;
			else
				throw std::runtime_error("Serial: invalid flowcontrol option: \"" + arg + "\". Valid options are NONE, HARDWARE, SOFTWARE.");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string SerialPort::Get()
	{
		std::string fc_str;
		switch (flowcontrol)
		{
		case FlowControl::NONE:
			fc_str = "none";
			break;
		case FlowControl::HARDWARE:
			fc_str = "hardware";
			break;
		case FlowControl::SOFTWARE:
			fc_str = "software";
			break;
		}

		return Device::Get() +
			   " baudrate " + std::to_string(baudrate) +
			   " flowcontrol " + fc_str +
			   " port " + port +
			   " print " + Util::Convert::toString(print);
	}
	void SerialPort::getDeviceList(std::vector<Description> &DeviceList)
	{
		device_list.clear();

#ifndef _WIN32
		// Scan /dev/serial/by-id for persistent device names (symlinks)
		std::vector<std::string> by_id_files = Util::Helper::getFilesInDirectory("/dev/serial/by-id");
		for (const auto &name : by_id_files)
		{
			std::string device_path = "/dev/serial/by-id/" + name;

			uint64_t handle = device_list.size();
			device_list.push_back(device_path);
			DeviceList.push_back(Description("Serial", "USB Serial", name, handle, Type::SERIALPORT));
		}
#endif
	}
}
