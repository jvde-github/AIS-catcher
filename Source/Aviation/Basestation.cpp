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

#include <cstring>

#include "Basestation.h"
#include "Convert.h"
#include "Parse.h"
#include "SWAR.h"

namespace
{
	constexpr int SBS_MAX_FIELDS = 22;

	inline int splitCSV(const char *base, int size, int *delim)
	{
		int count = 0;
		delim[0] = -1;

		const char *p = base;
		const char *end = base + size;
		constexpr size_t mask_comma = SWAR::mask(',');

		while (p < end && count < SBS_MAX_FIELDS)
		{
			SWAR_SKIP_PTR(p, end, SWAR::has_byte(word, mask_comma));
			while (p < end && *p != ',')
				p++;
			if (p >= end)
				break;
			delim[++count] = (int)(p - base);
			p++;
		}

		delim[++count] = size;
		return count;
	}

	inline const char *partPtr(const char *base, const int *delim, int i)
	{
		return base + delim[i] + 1;
	}
	inline int partLen(const int *delim, int i)
	{
		return delim[i + 1] - delim[i] - 1;
	}
	inline bool partEmpty(const int *delim, int i)
	{
		return partLen(delim, i) == 0;
	}

	inline void unquote(const char *&p, int &len)
	{
		if (len >= 2 && p[0] == '"' && p[len - 1] == '"')
		{
			p++;
			len -= 2;
		}
	}

	inline bool parseInt(const char *p, int len, int &out)
	{
		if (len <= 0)
			return false;
		bool neg = (p[0] == '-');
		int i = (neg || p[0] == '+') ? 1 : 0;
		if (i >= len)
			return false;
		int64_t v = 0;
		for (; i < len; i++)
		{
			char c = p[i];
			if (c < '0' || c > '9')
				return false;
			v = v * 10 + (c - '0');
		}
		out = neg ? -(int)v : (int)v;
		return true;
	}

	inline bool parseFloat(const char *p, int len, float &out)
	{
		if (len <= 0)
			return false;
		bool neg = (p[0] == '-');
		int i = (neg || p[0] == '+') ? 1 : 0;
		if (i >= len)
			return false;
		double v = 0;
		bool seen_digit = false;
		while (i < len && p[i] >= '0' && p[i] <= '9')
		{
			v = v * 10 + (p[i] - '0');
			i++;
			seen_digit = true;
		}
		if (i < len && p[i] == '.')
		{
			i++;
			double scale = 0.1;
			while (i < len && p[i] >= '0' && p[i] <= '9')
			{
				v += (p[i] - '0') * scale;
				scale *= 0.1;
				i++;
				seen_digit = true;
			}
		}
		if (!seen_digit || i != len)
			return false;
		out = neg ? -(float)v : (float)v;
		return true;
	}

	inline bool parseHex(const char *p, int len, uint32_t &out)
	{
		if (len <= 0)
			return false;
		uint32_t v = 0;
		for (int i = 0; i < len; i++)
		{
			int d = Util::Convert::hexValue(p[i]);
			if (d < 0) return false;
			v = (v << 4) | (uint32_t)d;
		}
		out = v;
		return true;
	}

	// "YYYY/MM/DD" + "HH:MM:SS[.fff]" → Unix epoch UTC.
	inline bool parseSBSDateTime(const char *dp, int dl, const char *tp, int tl, std::time_t &out)
	{
		if (dl != 10 || dp[4] != '/' || dp[7] != '/')
			return false;
		if (tl < 8 || tp[2] != ':' || tp[5] != ':')
			return false;

		const char digits[] = {dp[0],dp[1],dp[2],dp[3],dp[5],dp[6],dp[8],dp[9],
		                       tp[0],tp[1],tp[3],tp[4],tp[6],tp[7]};
		for (char c : digits)
			if (c < '0' || c > '9')
				return false;

		int year = (dp[0]-'0')*1000 + (dp[1]-'0')*100 + (dp[2]-'0')*10 + (dp[3]-'0');
		unsigned mon  = (dp[5]-'0')*10 + (dp[6]-'0');
		unsigned day  = (dp[8]-'0')*10 + (dp[9]-'0');
		unsigned hour = (tp[0]-'0')*10 + (tp[1]-'0');
		unsigned min  = (tp[3]-'0')*10 + (tp[4]-'0');
		unsigned sec  = (tp[6]-'0')*10 + (tp[7]-'0');

		out = (std::time_t)(Util::Convert::daysFromEpoch(year, mon, day) * 86400
		      + (int64_t)hour * 3600 + (int64_t)min * 60 + (int64_t)sec);
		return true;
	}
}

void Basestation::processLine(const char *base, size_t size_in, std::time_t rxtime)
{
	Plane::ADSB msg;
	msg.reset(rxtime);
	TAG tag;

	int size = (int)size_in;

	int delim[SBS_MAX_FIELDS + 2];
	int nfields = splitCSV(base, size, delim);

	if (nfields < 10)
		return;
	{
		const char *p = partPtr(base, delim, 0);
		int len = partLen(delim, 0);
		unquote(p, len);
		if (len != 3 || p[0] != 'M' || p[1] != 'S' || p[2] != 'G')
			return;
	}

	{
		const char *p = partPtr(base, delim, 1);
		int len = partLen(delim, 1);
		unquote(p, len);
		int type;
		if (!parseInt(p, len, type) || type < 1 || type > 8)
			return;
		switch (type)
		{
		case 1: msg.message_types = 1 << 17; msg.message_subtypes |= 1 << 4;  break;
		case 2: msg.message_types = 1 << 17; msg.message_subtypes |= 1 << 5;  break;
		case 3: msg.message_types = 1 << 17; msg.message_subtypes |= 1 << 9;  break;
		case 4: msg.message_types = 1 << 17; msg.message_subtypes |= 1 << 19; break;
		case 5: msg.message_types = 1 << 4;  break;
		case 6: msg.message_types = 1 << 5;  break;
		case 7: msg.message_types = 1 << 16; break;
		case 8: msg.message_types = 1 << 11; break;
		}
	}

	{
		const char *p = partPtr(base, delim, 4);
		int len = partLen(delim, 4);
		unquote(p, len);
		if (len > 0)
		{
			uint32_t hex;
			if (p[0] == '~' && parseHex(p + 1, len - 1, hex))
			{
				msg.hexident = hex;
				msg.hexident_status = HEXINDENT_NON_ICAO;
			}
			else if (parseHex(p, len, hex))
			{
				msg.hexident = hex;
				msg.hexident_status = HEXINDENT_DIRECT;
			}
		}
	}

	if (!partEmpty(delim, 6) && !partEmpty(delim, 7))
	{
		const char *p6 = partPtr(base, delim, 6); int l6 = partLen(delim, 6); unquote(p6, l6);
		const char *p7 = partPtr(base, delim, 7); int l7 = partLen(delim, 7); unquote(p7, l7);
		std::time_t ts;
		if (parseSBSDateTime(p6, l6, p7, l7, ts))
			msg.timestamp = ts;
	}

	auto getField = [&](int i, const char *&p, int &len) -> bool {
		if (nfields <= i)
			return false;
		p = partPtr(base, delim, i);
		len = partLen(delim, i);
		unquote(p, len);
		return len > 0;
	};

	const char *p; int len;
	int   ival;
	float fval;

	if (getField(10, p, len))
	{
		int n = len < 8 ? len : 8;
		std::memcpy(msg.callsign, p, n);
		std::memset(msg.callsign + n, 0, sizeof(msg.callsign) - n);
	}
	if (getField(11, p, len) && parseInt(p, len, ival))
		msg.altitude = ival;
	if (getField(12, p, len) && parseFloat(p, len, fval))
		msg.speed = fval;
	if (getField(13, p, len) && parseFloat(p, len, fval))
		msg.heading = fval;
	if (nfields > 15 && !partEmpty(delim, 14) && !partEmpty(delim, 15))
	{
		const char *p14 = partPtr(base, delim, 14); int l14 = partLen(delim, 14); unquote(p14, l14);
		const char *p15 = partPtr(base, delim, 15); int l15 = partLen(delim, 15); unquote(p15, l15);
		float lat, lon;
		if (parseFloat(p14, l14, lat) && parseFloat(p15, l15, lon))
		{
			msg.lat = lat;
			msg.lon = lon;
			msg.position_status = Plane::ValueStatus::VALID;
			msg.position_timestamp = rxtime;
		}
	}
	if (getField(16, p, len) && parseInt(p, len, ival))
		msg.vertrate = ival;
	if (getField(17, p, len) && parseInt(p, len, ival))
		msg.squawk = ival;

	if (nfields > 21 && !partEmpty(delim, 21))
	{
		const char *p21 = partPtr(base, delim, 21); int l21 = partLen(delim, 21); unquote(p21, l21);
		if (l21 == 2 && p21[0] == '-' && p21[1] == '1')
			msg.airborne = 0;
		else
			msg.airborne = 1;
	}
	else
	{
		msg.airborne = 2;
	}

	if (msg.hexident_status == HEXINDENT_DIRECT)
		msg.setCountryCode();

	Send(&msg, 1, tag);
}

void Basestation::Receive(const RAW *data, int len, TAG &tag)
{
	constexpr size_t mask_lf = SWAR::mask('\n');
	constexpr size_t mask_cr = SWAR::mask('\r');

	if (line.size() != MAX_BASESTATION_LINE_LEN)
		AISC_STRING_RESIZE_UNINIT(line, MAX_BASESTATION_LINE_LEN);

	std::time_t rxtime;
	std::time(&rxtime);

	for (int j = 0; j < len; j++)
	{
		const char *src = (const char *)data[j].data;
		const char *const chunk_end = src + data[j].size;

		while (src < chunk_end)
		{
			const size_t avail = (size_t)(chunk_end - src);
			const size_t room = MAX_BASESTATION_LINE_LEN - line_end;
			const char *const src_end = src + (avail < room ? avail : room);
			char *dst = &line[0] + line_end;

			while (src + sizeof(size_t) <= src_end)
			{
				size_t word;
				std::memcpy(&word, src, sizeof(word));
				std::memcpy(dst, src, sizeof(word));
				size_t hit = SWAR::has_byte(word, mask_lf) | SWAR::has_byte(word, mask_cr);
				if (hit)
				{
					size_t pos = SWAR::first_byte_pos(hit);
					src += pos;
					dst += pos;
					break;
				}
				src += sizeof(word);
				dst += sizeof(word);
			}
			while (src < src_end && *src != '\n' && *src != '\r')
				*dst++ = *src++;

			line_end = (size_t)(dst - &line[0]);

			if (src < src_end)
			{
				if (!dropping && line_end > 0)
					processLine(line.data(), line_end, rxtime);
				while (src < chunk_end && (*src == '\n' || *src == '\r'))
					src++;
				line_end = 0;
				dropping = false;
			}
			else if (src_end < chunk_end)
			{
				dropping = true;
				line_end = 0;
			}
		}
	}
}
