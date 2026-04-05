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

#pragma once
#include <fstream>

#include "MsgOut.h"

namespace IO
{
	class FileOutput : public OutputMessage
	{
		std::ofstream file;
		std::string filename;

		bool append_mode = true;

	public:
		FileOutput() : OutputMessage("File") { fmt = MessageFormat::NMEA; }

		~FileOutput()
		{
			Stop();
		}

		void Start()
		{
			file.open(filename, append_mode ? std::ios::app : std::ios::out);

			if (!file)
			{
				throw std::runtime_error("File: failed to open file - " + filename);
			}
		}

		void Stop()
		{
			if (file.is_open())
				file.close();
		}

		void Receive(const AIS::Message *data, int len, TAG &tag)
		{
			if (fmt == MessageFormat::NMEA)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					for (const auto &s : data[i].NMEA)
					{
						file << s << std::endl;
					}
				}
			}
			else if (fmt == MessageFormat::NMEA_TAG)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					int n = data[i].getNMEATagBlock(jsonBuf, sizeof(jsonBuf));
					file.write(jsonBuf, n);
				}
			}
			else if (fmt == MessageFormat::BINARY_NMEA)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					int n = data[i].getBinaryNMEA(jsonBuf, sizeof(jsonBuf), tag);
					file.write(jsonBuf, n);
				}
			}
			else
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					int n = data[i].getNMEAJSON(jsonBuf, sizeof(jsonBuf), tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, false, tag.ipv4, "", "\n");
					file.write(jsonBuf, n);
				}
			}

			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
			{
				if (filter.include(*(AIS::Message *)data[i].binary))
				{
					int n = fastBuilder.stringify(data[i], jsonBuf, sizeof(jsonBuf), "\n");
					file.write(jsonBuf, n);
				}
			}
			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

		Setting &SetKey(AIS::Keys key, const std::string &arg)
		{
			switch (key)
			{
			case AIS::KEY_SETTING_FILE:
				filename = arg;
				break;
			case AIS::KEY_SETTING_MODE:
			{
				std::string a = arg;
				Util::Convert::toUpper(a);
				if (a != "APPEND" && a != "APP" && a != "OUT")
					throw std::runtime_error("File output - unknown mode: " + arg);
				append_mode = a == "APPEND" || a == "APP";
				break;
			}
			default:
				if (!setOptionKey(key, arg) && !filter.SetOptionKey(key, arg))
					throw std::runtime_error("File output - unknown option.");
				break;
			}
			return *this;
		}
	};
}
