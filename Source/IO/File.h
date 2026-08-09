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
#include "Logger.h"

namespace IO
{
	class FileOutput : public OutputMessage
	{
		std::ofstream file;
		std::string filename;

		bool append_mode = true;

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			file.write(data, len);
		}

		void batchDone(TAG &) override
		{
			file.flush();

			if (file.fail())
			{
				Error() << "File: cannot write to file.";
				StopRequest();
			}
		}

	public:
		FileOutput() : OutputMessage("File")
		{
			fmt = MessageFormat::NMEA;
			line_suffix = "\n";
			forward_gps = false;
		}

		~FileOutput()
		{
			Stop();
		}

		void Start() override
		{
			file.open(filename, append_mode ? std::ios::app : std::ios::out);

			if (!file)
			{
				throw std::runtime_error("File: failed to open file - " + filename);
			}
		}

		void Stop() override
		{
			if (file.is_open())
				file.close();
		}

		Setting &SetKey(AIS::Keys key, const std::string &arg) override
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
				return OutputMessage::SetKey(key, arg);
			}
			return *this;
		}
	};
}
