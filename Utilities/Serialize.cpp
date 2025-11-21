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

#include "Serialize.h"

namespace Util
{
	void Serialize::Uint8(uint8_t i, std::vector<char> &v)
	{
		v.push_back(i);
	}

	void Serialize::Uint16(uint16_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 8));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint32(uint32_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 24));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint64(uint64_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 56));
		v.push_back((char)((i >> 48) & 0xFF));
		v.push_back((char)((i >> 40) & 0xFF));
		v.push_back((char)((i >> 32) & 0xFF));
		v.push_back((char)((i >> 24) & 0xFF));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Int8(int8_t i, std::vector<char> &v)
	{
		v.push_back(i);
	}

	void Serialize::Int16(int16_t i, std::vector<char> &v)
	{
		Uint16((uint16_t)i, v);
	}

	void Serialize::Int32(int32_t i, std::vector<char> &v)
	{
		Uint32((uint32_t)i, v);
	}

	void Serialize::Int64(int64_t i, std::vector<char> &v)
	{
		Uint64((uint64_t)i, v);
	}

	void Serialize::String(const std::string &s, std::vector<char> &v)
	{
		Uint8((uint8_t)s.length(), v);
		v.insert(v.end(), s.begin(), s.end());
	}

	void Serialize::Float(FLOAT32 f, std::vector<char> &v)
	{
		Int16(f * 1000.0f, v);
	}

	void Serialize::FloatLow(FLOAT32 f, std::vector<char> &v)
	{
		Int16(f * 10.0f, v);
	}

	void Serialize::LatLon(FLOAT32 lat, FLOAT32 lon, std::vector<char> &v)
	{

		if (!(lat == 0 && lon == 0) && lat != 91 && lon != 181)
		{
			Util::Serialize::Int32(lat * 6000000, v);
			Util::Serialize::Int32(lon * 6000000, v);
		}
		else
		{
			Util::Serialize::Int32(91 * 6000000, v);
			Util::Serialize::Int32(181 * 6000000, v);
		}
	}
}
