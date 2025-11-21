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
			for (int i = 0; i < len; i++)
			{
				if (filter.include(data[i]))
				{
					for (const std::string &s : data[0].NMEA)
					{
						file << s << std::endl;
					}
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
					json.clear();
					builder.stringify(data[i], json);
					file << json << std::endl;
				}
			}
			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

		Setting &Set(std::string option, std::string arg)
		{
			Util::Convert::toUpper(option);

			if (option == "GROUPS_IN")
			{
				StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
			}
			else if (option == "FILE")
			{
				filename = arg;
			}
			else if (option == "MODE")
			{
				Util::Convert::toUpper(arg);

				if (arg != "APPEND" && arg != "APP" && arg != "OUT")
					throw std::runtime_error("File output - unknown mode: " + arg);

				append_mode = arg == "APPEND" || arg == "APP";
			}
			else if (!OutputMessage::setOption(option, arg))
			{
				throw std::runtime_error("File output - unknown option: " + option);
			}
			return *this;
		}
	};
}
