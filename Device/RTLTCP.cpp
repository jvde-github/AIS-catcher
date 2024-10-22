/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#include "RTLTCP.h"

namespace Device {

	//---------------------------------------
	// Device RTLTCP

	void RTLTCP::Close() {
		Device::Close();
	}

	void RTLTCP::sendProtocol() {

		if (Protocol == PROTOCOL::RTLTCP) {
			struct {
				uint32_t magic = 0, tuner = 0, gain = 0;
			} dongle;
			// RTLTCP protocol, check for dongle information
			int len = client.read((char*)&dongle, 12, timeout);
			if (len != 12 || dongle.magic != 0x304C5452) {
				std::cerr << "RTLTCP: no or invalid response, likely not an rtl-tcp server." << std::endl;
				StopRequest();
			}
		}
		else if (Protocol == PROTOCOL::GPSD) {
			const std::string str = "?WATCH={\"enable\":true,\"json\":true,\"nmea\":false}\n";
			int len = client.send(str.c_str(), str.size());
			if (len != str.size()) {
				std::cerr << "GPSD: no or invalid response, likely not a gpsd server." << std::endl;
				StopRequest();
			}
		}
	}

	void RTLTCP::Play() {

		if (!client.connect(host, port, persistent, timeout)) {
			if (!persistent)
				throw std::runtime_error("RTLTCP: cannot open socket.");

			std::cerr << "RTLTCP: cannot open socket. Retrying in a few seconds." << std::endl;
		}

		Device::Play();

		if (getFormat() != Format::TXT) {
			fifo.Init(BUFFER_SIZE);
		}
		else {
			fifo.Init(1, BUFFER_SIZE);
		}

		applySettings();

		lost = false;

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop() {
		if (Device::isStreaming()) {
			if (Protocol == PROTOCOL::GPSD) {
				const std::string str = "?WATCH={\"enable\":false}\n";
				client.send(str.c_str(), str.size());
			}
			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}
		client.disconnect();
	}

	void RTLTCP::RunAsync() {
		if (buffer.size() < TRANSFER_SIZE)
			buffer.resize(TRANSFER_SIZE);

		while (isStreaming()) {
			// send protocol
			if (client.numberOfConnects() != connects) {
				connects ++;
				sendProtocol();
			}

			int len = client.read(buffer.data(), TRANSFER_SIZE, 2);

			if (len < 0) {
				lost = true;
				std::cerr << "RTLTCP: error receiving data from remote host. Cancelling. " << std::endl;
				break;
			}
			else if (isStreaming() && !fifo.Push(buffer.data(), len))
				std::cerr << "RTLTCP: buffer overrun." << std::endl;
		}
	}

	void RTLTCP::Run() {
		std::vector<char> output(fifo.BlockSize());
		RAW r = { getFormat(), NULL, fifo.BlockSize() };

		while (isStreaming()) {
			if (fifo.Wait()) {
				r.data = fifo.Front();
				Send(&r, 1, tag);
				fifo.Pop();
			}
			else {
				if (isStreaming() && format != Format::TXT) std::cerr << "RTLTCP: timeout." << std::endl;
			}
		}
	}

	void RTLTCP::setParameterRTLTCP(uint8_t c, uint32_t p) {
		char instruction[5];

		instruction[0] = c;
		instruction[4] = p;
		instruction[3] = p >> 8;
		instruction[2] = p >> 16;
		instruction[1] = p >> 24;
		client.send((const char*)instruction, 5);
	}

	void RTLTCP::applySettings() {
		// client.setTimeout(timeout);
		client.setResetTime(reset_time);

		if (Protocol == PROTOCOL::RTLTCP) {
			setParameterRTLTCP(5, freq_offset);
			setParameterRTLTCP(3, tuner_AGC ? 0 : 1);

			if (!tuner_AGC) setParameterRTLTCP(4, tuner_Gain * 10);
			if (RTL_AGC) setParameterRTLTCP(8, 1);

			setParameterRTLTCP(2, sample_rate);
			setParameterRTLTCP(1, frequency);

			setFormat(Format::CU8);
		}
	}

	void RTLTCP::getDeviceList(std::vector<Description>& DeviceList) {
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

	Setting& RTLTCP::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "TUNER") {
			tuner_AGC = Util::Parse::AutoFloat(arg, 0, 50, tuner_Gain);
		}
		else if (option == "RTLAGC") {
			RTL_AGC = Util::Parse::Switch(arg);
		}
		else if (option == "FREQOFFSET") {
			freq_offset = Util::Parse::Integer(arg, -150, 150);
		}
		else if (option == "PERSIST") {
			persistent = Util::Parse::Switch(arg);
		}
		else if (option == "TIMEOUT") {
			timeout = Util::Parse::Integer(arg, 1, 60);
		}
		else if (option == "RESET") {
			reset_time = Util::Parse::Integer(arg, 1, 60);
		}
		else if (option == "HOST") {
			host = arg;
		}
		else if (option == "PORT") {
			port = arg;
		}
		else if (option == "PROTOCOL") {
			Util::Convert::toUpper(arg);
			if (arg == "NONE")
				Protocol = PROTOCOL::NONE;
			else if (arg == "RTLTCP") {
				Protocol = PROTOCOL::RTLTCP;
				setFormat(Format::CU8);
			}
			else if (arg == "GPSD") {
				Protocol = PROTOCOL::GPSD;
				setFormat(Format::TXT);
			}
			else if (arg == "TXT") {
				Protocol = PROTOCOL::TXT;
				setFormat(Format::TXT);
			}
			else
				throw std::runtime_error("RTLTCP: unknown protocol");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string RTLTCP::Get() {
		std::string str = " host " + host + " port " + port + " timeout " + std::to_string(timeout);
		str += " tuner " + Util::Convert::toString(tuner_AGC, tuner_Gain);
		str += " rtlagc " + Util::Convert::toString(RTL_AGC);
		str += " persist " + Util::Convert::toString(persistent);
		str += " reset " + (reset_time < 0 ? std::string("none") : std::to_string(reset_time));
		str += " protocol " + getProtocolString();

		return Device::Get() + str;
	}
}
