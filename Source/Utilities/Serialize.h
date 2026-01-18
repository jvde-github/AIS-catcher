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

#include <string>
#include <vector>
#include <cstdint>

#include "Stream.h"

namespace Util
{
	class Serialize
	{
	public:
		static void Uint8(uint8_t i, std::vector<char> &v);
		static void Uint16(uint16_t i, std::vector<char> &v);
		static void Uint32(uint32_t i, std::vector<char> &v);
		static void Uint64(uint64_t i, std::vector<char> &v);
		static void Int8(int8_t i, std::vector<char> &v);
		static void Int16(int16_t i, std::vector<char> &v);
		static void Int32(int32_t i, std::vector<char> &v);
		static void Int64(int64_t i, std::vector<char> &v);
		static void String(const std::string &s, std::vector<char> &v);
		static void LatLon(FLOAT32 lat, FLOAT32 lon, std::vector<char> &v);
		static void Float(FLOAT32 f, std::vector<char> &v);
		static void FloatLow(FLOAT32 f, std::vector<char> &v);
	};
}
