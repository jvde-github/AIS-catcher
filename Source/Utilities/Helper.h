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

#include <string>
#include <vector>
#include <cstdint>

namespace Util
{
	class Helper
	{
	public:
		static std::string readFile(const std::string &filename);
		static int lsb(uint64_t x);
		static std::vector<std::string> getFilesWithExtension(const std::string &directory, const std::string &extension);
		static long getMemoryConsumption();
		static std::string getOS();
		static std::string getHardware();
		static uint16_t CRC16(const uint8_t* data, size_t length);
		static bool isUUID(const std::string &s)
		{
			if (s.size() != 36)
				return false;
			for (int i = 0; i < 36; i++)
			{
				if (i == 8 || i == 13 || i == 18 || i == 23)
				{
					if (s[i] != '-')
						return false;
				}
				else
				{
					if (!isxdigit(s[i]))
						return false;
				}
			}
			return true;
		}
	};
}
