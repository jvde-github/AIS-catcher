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
#include <string>
#include <time.h>

#include "Stream.h"

namespace Util
{
	class Convert
	{
	public:
		static std::string toTimeStr(const std::time_t &t);
		static std::string toTimestampStr(const std::time_t &t);
		static std::string toHexString(uint64_t l);

		// Days from civil 1970-01-01 to y-m-d (proleptic Gregorian).
		// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
		static inline int64_t daysFromEpoch(int y, unsigned m, unsigned d)
		{
			y -= m <= 2;
			const int era = (y >= 0 ? y : y - 399) / 400;
			const unsigned yoe = (unsigned)(y - era * 400);
			const unsigned doy = (153 * (m + (m > 2 ? -3u : 9u)) + 2) / 5 + d - 1;
			const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
			return (int64_t)era * 146097 + (int64_t)doe - 719468;
		}

		// Hex char ↔ nibble. `hexDigitValue` is the branchless fast path —
		// caller must `isHexDigit` first; `hexValue` returns -1 for non-hex.
		static inline bool isHexDigit(char c)
		{
			unsigned u = (unsigned char)c;
			return (u - '0' < 10u) | ((u | 0x20) - 'a' < 6u);
		}
		static inline int hexDigitValue(char c)
		{
			unsigned u = (unsigned char)c, d = u - '0';
			return d < 10u ? (int)d : (int)((u | 0x20) - 'a' + 10);
		}
		static inline int hexValue(char c)
		{
			return isHexDigit(c) ? hexDigitValue(c) : -1;
		}
		static std::string toString(Format format);
		static std::string toString(bool b) { return b ? std::string("ON") : std::string("OFF"); }
		static std::string toString(bool b, FLOAT32 v) { return b ? std::string("AUTO") : std::to_string(v); }
		static std::string toString(FLOAT32 f)
		{
			std::string s = std::to_string(f);
			if (s == "nan" || s == "-nan")
				return "null";
			return s;
		}

		static std::string toString(PROTOCOL protocol);
		static std::string toString(MessageFormat out);

		static std::string toBase64(const std::string &s);
		static std::string IPV4toString(uint32_t ipv4)
		{
			return std::to_string((ipv4 >> 24) & 0xFF) + "." +
				   std::to_string((ipv4 >> 16) & 0xFF) + "." +
				   std::to_string((ipv4 >> 8) & 0xFF) + "." +
				   std::to_string(ipv4 & 0xFF);
		}

		static void toUpper(std::string &s);
		static void toLower(std::string &s);
		static void toFloat(CU8 *in, CFLOAT32 *out, int len);
		static void toFloat(CS8 *in, CFLOAT32 *out, int len);
		static void toFloat(CS16 *in, CFLOAT32 *out, int len);
		static void toFloat(DC16H *in, CFLOAT32 *out, int len);
	};
}
