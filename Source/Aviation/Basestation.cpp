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

#include "Basestation.h"

void Basestation::processLine()
{
	Plane::ADSB msg;
	msg.clear();
	msg.Stamp();
	TAG tag;

	std::vector<std::string> fields;
	std::stringstream ss(line);
	std::string field;
	while (std::getline(ss, field, ','))
	{
		if (!field.empty() && field.front() == '"' && field.size() >= 2 && field.back() == '"')
			field = field.substr(1, field.size() - 2);
		fields.push_back(field);
	}

	if (fields.size() < 10 || fields[0] != "MSG")
		return;

	// Message Type (Field 1)
	int type = std::stoi(fields[1]);
	switch (type)
	{
	case 1:
	case 2:
	case 3:
	case 4:
		msg.message_types = 1 << 17;
		break;
	case 5:
		msg.message_types = 1 << 4;
		break;
	case 6:
		msg.message_types = 1 << 5;
		break;
	case 7:
		msg.message_types = 1 << 16;
		break;
	case 8:
		msg.message_types = 1 << 11;
		break;
	}

	if (!fields[4].empty())
	{
		if (fields[4][0] == '~')
			msg.hexident = std::stoul(fields[4].substr(1), nullptr, 16);
		else
			msg.hexident = std::stoul(fields[4], nullptr, 16);
	}

	// Timestamps (Fields 7-10)
	if (!fields[6].empty() && !fields[7].empty())
	{
		std::string datetime = fields[6] + " " + fields[7];
		msg.timestamp = Util::Parse::DateTime(datetime);
	}

	// Callsign (Field 10)
	if (fields.size() > 10 && !fields[10].empty())
	{
		std::strncpy(msg.callsign, fields[10].c_str(), 8);
		msg.callsign[8] = '\0'; // Ensure null termination
	}

	// Altitude (Field 11)
	if (fields.size() > 11 && !fields[11].empty())
	{
		msg.altitude = std::stoi(fields[11]);
	}

	// Groundspeed (Field 12)
	if (fields.size() > 12 && !fields[12].empty())
	{
		msg.speed = std::stof(fields[12]);
	}

	// Track (Field 13)
	if (fields.size() > 13 && !fields[13].empty())
	{
		msg.heading = std::stof(fields[13]);
	}

	// Position (Fields 14,15)
	if (fields.size() > 15 && !fields[14].empty() && !fields[15].empty())
	{
		msg.lat = std::stof(fields[14]);
		msg.lon = std::stof(fields[15]);
		msg.position_status = Plane::ValueStatus::VALID;
		std::time(&msg.position_timestamp);
	}

	// Vertical Rate (Field 16)
	if (fields.size() > 16 && !fields[16].empty())
	{
		msg.vertrate = std::stoi(fields[16]);
	}

	// Squawk (Field 17)
	if (fields.size() > 17 && !fields[17].empty())
	{
		msg.squawk = std::stoi(fields[17]);
	}

	// Ground (Field 21) - properly set airborne status
	if (fields.size() > 21 && !fields[21].empty())
	{
		if (fields[21] == "-1")
			msg.airborne = 0; // On ground
		else
			msg.airborne = 1; // Airborne
	}
	else
	{
		msg.airborne = 2; // Unknown status
	}

	msg.setCountryCode();

	// Send message to next processing stage
	Send(&msg, 1, tag);
}

void Basestation::Receive(const RAW *data, int len, TAG &tag)
{
	for (int j = 0; j < len; j++)
	{
		for (int i = 0; i < data[j].size; i++)
		{
			char c = ((char *)(data[j].data))[i];
			if (c == '\n')
			{
				if (dropping)
				{
					dropping = false;
					line.clear();
					continue;
				}

				if (line.length() > 0)
				{
					try
					{
						processLine();
					}
					catch (const std::exception &e)
					{
						Error() << "Error processing line: " << line << " " << e.what() << std::endl;
					}
					line.clear();
				}
			}
			else
			{
				if (c == '\r')
					continue;

				if (dropping)
					continue;

				if (line.size() >= MAX_BASESTATION_LINE_LEN)
				{
					dropping = true;
					continue;
				}

				line.push_back(c);
			}
		}
	}
}
