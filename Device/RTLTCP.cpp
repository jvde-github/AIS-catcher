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

#include "RTLTCP.h"

namespace Device
{

	//---------------------------------------
	// Device RTLTCP

	void RTLTCP::Close()
	{
		Device::Close();
	}

	void RTLTCP::sendProtocol()
	{
	}

	void RTLTCP::Play()
	{

		switch (Protocol)
		{
		case PROTOCOL::MQTT:
			session = tcp.add(&mqtt);
			mqtt.setValue("SUBSCRIBE", "on");
			break;
		case PROTOCOL::GPSD:
			session = tcp.add(&gpsd);
			break;
		case PROTOCOL::RTLTCP:
			session = tcp.add(&rtltcp);
			break;
		case PROTOCOL::WS:
			session = tcp.add(&ws);
			break;
		case PROTOCOL::WSMQTT:
			session = tcp.add(&ws);
			session = ws.add(&mqtt);
			ws.setValue("PROTOCOLS", "mqtt");
			ws.setValue("BINARY", "on");
			mqtt.setValue("SUBSCRIBE", "on");
			break;
		default:
			break;
		}

		rtltcp.setValue("FREQUENCY", std::to_string(frequency));
		rtltcp.setValue("RATE", std::to_string(sample_rate));
		rtltcp.setValue("FREQOFFSET", std::to_string(freq_offset));
		rtltcp.setValue("BANDWIDTH", std::to_string(tuner_bandwidth));

		if (!session->connect())
		{
			throw std::runtime_error("RTLTCP: cannot open connection with " + tcp.getHost() + ":" + tcp.getPort());
		}

		Device::Play();

		if (getFormat() != Format::TXT)
		{
			fifo.Init(BUFFER_SIZE);
		}
		else
		{
			fifo.Init(1, BUFFER_SIZE);
		}

		lost = false;

		async_thread = std::thread(&RTLTCP::RunAsync, this);
		run_thread = std::thread(&RTLTCP::Run, this);

		SleepSystem(10);
	}

	void RTLTCP::Stop()
	{
		if (Device::isStreaming())
		{

			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable())
				async_thread.join();
			if (run_thread.joinable())
				run_thread.join();
		}
		session->disconnect();
	}

	void RTLTCP::RunAsync()
	{
		if (buffer.size() < TRANSFER_SIZE)
			buffer.resize(TRANSFER_SIZE);

		while (isStreaming())
		{

			int len = session->read(buffer.data(), TRANSFER_SIZE, 2);

			if (len < 0)
			{
				lost = true;
				Error() << "RTLTCP: error receiving data from remote host. Cancelling. ";
				break;
			}
			else if (isStreaming() && !fifo.Push(buffer.data(), len))
				Error() << "RTLTCP: buffer overrun.";
		}
	}

	void RTLTCP::Run()
	{
		std::vector<char> output(fifo.BlockSize());
		RAW r = {getFormat(), NULL, fifo.BlockSize()};

		while (isStreaming())
		{
			if (fifo.Wait())
			{
				r.data = fifo.Front();
				Send(&r, 1, tag);
				fifo.Pop();
			}
			else
			{
				if (isStreaming() && format != Format::TXT)
					Error() << "RTLTCP: timeout.";
			}
		}
	}

	void RTLTCP::getDeviceList(std::vector<Description> &DeviceList)
	{
		DeviceList.push_back(Description("RTLTCP", "RTLTCP", "RTLTCP", (uint64_t)0, Type::RTLTCP));
	}

	Setting &RTLTCP::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "URL")
		{
			std::string prot, host, port, path, username, password;
			Util::Parse::URL(arg, prot, username, password, host, port, path);

			if (!host.empty())
				Set("HOST", host);
			if (!port.empty())
				Set("PORT", port);
			if (!prot.empty())
				Set("PROTOCOL", prot);
			if (!username.empty())
				Set("USERNAME", username);
			if (!password.empty())
				Set("PASSWORD", password);
		}
		else if (option == "PROTOCOL")
		{

			if (!Util::Parse().Protocol(arg, Protocol))
				throw std::runtime_error("RTLTCP: unknown protocol: " + arg);

			switch (Protocol)
			{
			case PROTOCOL::NONE:
				setFormat(Format::TXT);
				break;
			case PROTOCOL::RTLTCP:
				setFormat(Format::CU8);
				break;
			case PROTOCOL::GPSD:
				setFormat(Format::TXT);
				break;
			case PROTOCOL::MQTT:
				setFormat(Format::TXT);
				break;
			case PROTOCOL::WS:
				setFormat(Format::TXT);
				break;
			case PROTOCOL::WSMQTT:
				setFormat(Format::TXT);
				break;
			case PROTOCOL::TXT:
				setFormat(Format::TXT);
				break;
			default:
				throw std::runtime_error("RTLTCP: unsupported protocol: " + arg);
			}
		}
		else
		{
			if (!tcp.setValue(option, arg) && !mqtt.setValue(option, arg) && !gpsd.setValue(option, arg) && !rtltcp.setValue(option, arg) && !ws.setValue(option, arg))
				Device::Set(option, arg);
		}

		return *this;
	}

	std::string RTLTCP::Get()
	{
		Protocol::ProtocolBase *p = session;
		std::string str = "protocol " + Util::Convert::toString(Protocol) + " " + Device::Get() + "\n";
		while (p)
		{
			str += "  " + p->getLayer() + ": " + p->getValues() + "\n";
			p = p->getPrev();
		}
		return str;
	}
}