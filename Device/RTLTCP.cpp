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
	}

	void RTLTCP::Play() {

		switch (Protocol) {
		case PROTOCOL::MQTT:
			transport = tcp.add(&mqtt);
			mqtt.setValue("SUBSCRIBE", "on");
			break;
		case PROTOCOL::GPSD:
			transport = tcp.add(&gpsd);
			break;
		case PROTOCOL::RTLTCP:
			transport = tcp.add(&rtltcp);
		default:
			break;
		}

		rtltcp.setValue("FREQUENCY", std::to_string(frequency));
		rtltcp.setValue("RATE", std::to_string(sample_rate));
		rtltcp.setValue("FREQOFFSET", std::to_string(freq_offset));
		rtltcp.setValue("BANDWIDTH", std::to_string(tuner_bandwidth));

		if (!transport->connect()) {
				throw std::runtime_error("RTLTCP: cannot open socket.");
		}

		Device::Play();

		if (getFormat() != Format::TXT) {
			fifo.Init(BUFFER_SIZE);
		}
		else {
			fifo.Init(1, BUFFER_SIZE);
		}

		lost = false;

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop() {
		if (Device::isStreaming()) {

			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();
		}
		transport->disconnect();
	}

	void RTLTCP::RunAsync() {
		if (buffer.size() < TRANSFER_SIZE)
			buffer.resize(TRANSFER_SIZE);

		while (isStreaming()) {

			int len = transport->read(buffer.data(), TRANSFER_SIZE, 2);

			if (len < 0) {
				lost = true;
				Error() << "RTLTCP: error receiving data from remote host. Cancelling. ";
				break;
			}
			else if (isStreaming() && !fifo.Push(buffer.data(), len))
				Error() << "RTLTCP: buffer overrun.";
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
				if (isStreaming() && format != Format::TXT) Error() << "RTLTCP: timeout.";
			}
		}
	}

	void RTLTCP::getDeviceList(std::vector<Description>& DeviceList) {
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

	Setting& RTLTCP::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		tcp.setValue(option, arg);
		mqtt.setValue(option, arg);
		gpsd.setValue(option, arg);
		rtltcp.setValue(option, arg);

		Device::Set(option, arg);

		if (option == "PROTOCOL") {
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
			else if (arg == "MQTT") {
				Protocol = PROTOCOL::MQTT;
				setFormat(Format::TXT);
			}
			else
				throw std::runtime_error("RTLTCP: unknown protocol");
		}

		return *this;
	}

	std::string RTLTCP::Get() {

		Protocol::ProtocolBase* p = transport;
		std::string str;
		while (p) {
			str += p->getValues() + " ";
			p = p->getPrev();
		}
		return "protocol " + getProtocolString() + " " + Device::Get() + " " + str;
	}
}