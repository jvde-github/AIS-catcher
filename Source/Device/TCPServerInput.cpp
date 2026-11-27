/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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


#include "TCPServerInput.h"
#include "Parse.h"

namespace Device
{
	void TCPServerInput::Listener::processClients()
	{
		if (!input->isStreaming())
			return;
		for (auto &c : client)
		{
			const std::size_t whole = c.msg.rfind('\n') + 1;
			if (whole)
			{
				RAW r = {input->getFormat(), &c.msg[0], (int)whole};
				input->Send(&r, 1, input->tag);
				c.msg.erase(0, whole);
			}
			if (c.msg.size() > MAX_LINE)
				c.Close();
		}
	}

	void TCPServerInput::Play()
	{
		if (!port)
			throw std::runtime_error("TCP server: no port given.");
		listener.input = this;
		listener.setIP(server);
		if (!listener.start(port))
			throw std::runtime_error("TCP server: cannot listen on " + (server.empty() ? "" : server + " ") + "port " + std::to_string(port) + ".");
		Device::Play();
	}

	void TCPServerInput::getDeviceList(std::vector<Description> &DeviceList)
	{
		DeviceList.push_back(Description("TCP server", "TCP server", "TCP server", (uint64_t)0, Type::TCPSERVER));
	}

	Setting &TCPServerInput::applyKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_PORT:
			port = Util::Parse::Integer(arg, 1, 0xFFFF, "PORT");
			break;
		case AIS::KEY_SETTING_SERVER:
			server = arg;
			break;
		case AIS::KEY_SETTING_FORMAT:
		{
			Format f = Format::UNKNOWN;
			if (!Util::Parse::StreamFormat(arg, f) || (f != Format::TXT && f != Format::N2K && f != Format::BASESTATION && f != Format::RAW1090))
				throw std::runtime_error("TCP server: format must be TXT, N2K, BASESTATION or RAW1090.");
			setFormat(f);
			break;
		}
		default:
			Device::applyKey(key, arg);
			break;
		}
		return *this;
	}

	std::string TCPServerInput::Get()
	{
		return Device::Get() + " server " + server + " port " + std::to_string(port);
	}
}
