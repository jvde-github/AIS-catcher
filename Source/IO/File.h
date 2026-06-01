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

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag)
		{
			if (fmt == MessageFormat::NMEA)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					for (const auto &s : data[i].sentences())
					{
						file.write(s.data(), s.size());
						file.put('\n');
					}
				}
			}
			else if (fmt == MessageFormat::NMEA_TAG)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					json.clear();
					data[i].getNMEATagBlock(json);
					file.write(json.data(), json.size());
				}
			}
			else if (fmt == MessageFormat::BINARY_NMEA)
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					json.clear();
					data[i].getBinaryNMEA(json, tag);
					file.write(json.data(), json.size());
				}
			}
			else
			{
				for (int i = 0; i < len; i++)
				{
					if (!filter.include(data[i]))
						continue;

					json.clear();
					data[i].getNMEAJSON(json, tag, false, "", "\n");
					file.write(json.data(), json.size());
				}
			}

			file.flush();

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
					json.clear();
					builder.stringify(data[i], json, "\n");
					file.write(json.data(), json.size());
				}
			}
			file.flush();

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
