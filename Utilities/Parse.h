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
#include <time.h>

#include "Stream.h"

namespace Util
{
	class Parse
	{
	public:
		static long Integer(std::string str, long min = 0, long max = 0, const std::string &setting = "");
		static double Float(std::string str, double min = -1e30, double max = +1e30);
		static bool StreamFormat(std::string str, Format &format);
		static bool DeviceType(std::string str, Type &type);
		static bool Protocol(std::string str, PROTOCOL &protocol);
		static bool OutputFormat(std::string str, MessageFormat &out);
		static std::string DeviceTypeString(Type type);
		static std::time_t DateTime(const std::string &datetime);
		static bool Switch(std::string arg, const std::string &TrueString = "ON", const std::string &FalseString = "OFF");
		static bool AutoInteger(std::string arg, int min, int max, int &val);
		static bool AutoFloat(std::string arg, double min, double max, double &val);
		static void HTTP_URL(const std::string &url, std::string &protocol, std::string &host, std::string &port, std::string &path);
		static void URL(const std::string &url, std::string &protocol, std::string &username, std::string &password, std::string &host, std::string &port, std::string &path);
	};
}
