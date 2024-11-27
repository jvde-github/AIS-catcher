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

#include "MsgOut.h"
#include "Receiver.h"

namespace IO
{

	void OutputMessage::ConnectMessage(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<AIS::Message> *um = (StreamIn<AIS::Message> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.Output(j).Connect(um);
		}
	}

	void OutputMessage::ConnectJSON(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<JSON::JSON> *um = (StreamIn<JSON::JSON> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.OutputJSON(j).Connect(um);
		}
	}

	void OutputMessage::Connect(Receiver &r)
	{

		if (JSON_input)
			ConnectJSON(r);
		else
			ConnectMessage(r);

		for (int j = 0; j < r.Count(); j++)
		{

			StreamIn<AIS::GPS> *ug = (StreamIn<AIS::GPS> *)&*this;
			if (r.OutputGPS(j).canConnect(ug->getGroupsIn()))
				r.OutputGPS(j).Connect(ug);
		}
	}

	void OutputJSON::Connect(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<JSON::JSON> *um = (StreamIn<JSON::JSON> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.OutputJSON(j).Connect(um);

			StreamIn<AIS::GPS> *ug = (StreamIn<AIS::GPS> *)&*this;
			if (r.OutputGPS(j).canConnect(ug->getGroupsIn()))
				r.OutputGPS(j).Connect(ug);
		}
	}

	void MessageToScreen::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (level == MessageFormat::SILENT)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.includeGPS())
			{
				switch (level)
				{
				case MessageFormat::NMEA:
				case MessageFormat::NMEA_TAG:
				case MessageFormat::FULL:
					std::cout << data[i].getNMEA() << std::endl;
					break;
				default:
					std::cout << data[i].getJSON() << std::endl;
					break;
				}
			}
		}
	}

	void MessageToScreen::Receive(const AIS::Message *data, int len, TAG &tag)
	{

		if (level == MessageFormat::SILENT)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.include(data[i]))
			{
				switch (level)
				{
				case MessageFormat::NMEA:
				case MessageFormat::NMEA_TAG:
					for (const auto &s : data[i].NMEA)
						std::cout << s << std::endl;
					break;
				case MessageFormat::FULL:
					for (const auto &s : data[i].NMEA)
					{
						std::cout << s << " ( MSG: " << data[i].type() << ", REPEAT: " << data[i].repeat() << ", MMSI: " << data[i].mmsi();
						
						if (tag.mode & 1 && tag.ppm != PPM_UNDEFINED && tag.level != LEVEL_UNDEFINED)
							std::cout << ", signalpower: " << tag.level << ", ppm: " << tag.ppm;
						if (tag.mode & 2)
							std::cout << ", timestamp: " << data[i].getRxTime();
						if (data[i].getStation())
							std::cout << ", ID: " << data[i].getStation();
							
						std::cout << ")" << std::endl;
					}
					break;
				case MessageFormat::JSON_NMEA:
					std::cout << data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver) << std::endl;
					break;
				default:
					break;
				}
			}
		}
	}

	void JSONtoScreen::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.includeGPS())
			{
				std::cout << data[i].getJSON() << std::endl;
			}
		}
	}

	void JSONtoScreen::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json);
				std::cout << json << std::endl;
			}
		}
	}
}
