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

#include "Screen.h"

namespace IO
{
	void ScreenOutput::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (fmt == MessageFormat::SILENT)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.includeGPS())
			{
				switch (fmt)
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

	void ScreenOutput::Receive(const AIS::Message *data, int len, TAG &tag)
	{

		if (fmt == MessageFormat::SILENT)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.include(data[i]))
			{
				switch (fmt)
				{
				case MessageFormat::NMEA:
					for (const auto &s : data[i].NMEA)
						std::cout << s << std::endl;
					break;
				case MessageFormat::NMEA_TAG:
					std::cout << data[i].getNMEATagBlock();
					break;
				case MessageFormat::FULL:
					for (const auto &s : data[i].NMEA)
					{

						std::cout << s << " ( ";

						if (data[i].getLength() > 0)
							std::cout << "MSG: " << data[i].type() << ", REPEAT: " << data[i].repeat() << ", MMSI: " << data[i].mmsi();
						else
							std::cout << "empty";

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
					std::cout << data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, include_sample_start) << std::endl;
					break;
				default:
					break;
				}
			}
		}
	}

	void ScreenOutput::Receive(const JSON::JSON *data, int len, TAG &tag)
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
