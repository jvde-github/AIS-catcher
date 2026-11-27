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

#include <cstdint>

namespace Util
{
	class PackedInt
	{
	private:
		uint32_t value = 0;

	public:
		PackedInt() : value(0) {}
		PackedInt(int val) : value(val) {}

		inline int get(int position, int size) const
		{
			return (int)((value >> position) & ((1u << size) - 1));
		}

		inline void set(int position, int size, int fieldValue)
		{
			value = (value & ~(((1u << size) - 1) << position)) | (((uint32_t)fieldValue & ((1u << size) - 1)) << position);
		}

		inline void orOp(int position, int size, int fieldValue)
		{
			value |= ((uint32_t)fieldValue & ((1u << size) - 1)) << position;
		}

		inline uint32_t getPackedValue() const
		{
			return value;
		}

		inline void setPackedValue(int val)
		{
			value = val;
		}

		inline void reset()
		{
			value = 0;
		}
	};
}
