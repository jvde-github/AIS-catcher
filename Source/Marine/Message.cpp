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

#include "Message.h"
#include "Parse.h"
#include "Helper.h"
#include "JSON/StringBuilder.h"
#include "Library/SWAR.h"

namespace AIS
{

	int Message::ID = 0;

	static int NMEAchecksum(const std::string &s)
	{
		int check = 0;
		for (int i = 1; i < s.length(); i++)
			check ^= s[i];
		return check;
	}

	const std::string GPS::formatLatLon(float value, bool isLatitude) const
	{
		std::stringstream ss;

		int degrees = static_cast<int>(std::abs(value));
		float minutes = (std::abs(value) - degrees) * 60;

		ss << std::setfill('0') << std::setw(isLatitude ? 2 : 3) << std::abs(degrees);
		ss << std::setfill('0') << std::setw(5) << std::fixed << std::setprecision(2) << minutes;

		return ss.str();
	}

	const std::string GPS::getNMEA() const
	{
		if (!isJSON)
			return source;

		std::string flat = formatLatLon(lat, true);
		std::string flon = formatLatLon(lon, false);

		char latDir = lat >= 0 ? 'N' : 'S';
		char lonDir = lon >= 0 ? 'E' : 'W';

		std::string line = "$GPGLL," + flat + "," + latDir + "," + flon + "," + lonDir + ",,,";

		int c = NMEAchecksum(line);

		line += '*';
		line += (c >> 4) < 10 ? (c >> 4) + '0' : (c >> 4) + 'A' - 10;
		line += (c & 0xF) < 10 ? (c & 0xF) + '0' : (c & 0xF) + 'A' - 10;

		return line;
	}

	const std::string GPS::getJSON() const
	{
		if (isJSON)
			return source;

		return "{\"class\":\"TPV\",\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) + "}";
	}

	int Message::getNMEAJSON(std::string &out, const TAG &tag, bool include_ssl, const std::string &uuid, const char *suffix) const
	{
		JSON::Writer w(out);

		w.append_lit("{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"version\":");
		w.append_int(tag.version);
		w.append_lit(",\"driver\":");
		w.append_int((int)tag.driver);
		w.append_lit(",\"hardware\":\"");
		w.append(tag.hardware.data(), tag.hardware.size());
		w.append_lit("\",\"channel\":\"");
		w.append(getChannel());
		w.append_lit("\",\"repeat\":");
		w.append_int((long long)repeat());

		if (include_ssl)
		{
			w.append_lit(",\"ssc\":");
			w.append_int((long long)start_idx);
			w.append_lit(",\"sl\":");
			w.append_int((long long)(end_idx - start_idx));
		}

		if (tag.status)
		{
			w.append_lit(",\"msg_status\":");
			w.append_int((long long)tag.status);
		}

		if (tag.mode & 2)
		{
			w.append_lit(",\"rxuxtime\":");
			if ((rxtime % 1000000) != 0)
				w.append_float((double)rxtime / 1000000.0);
			else
				w.append_int(rxtime / 1000000);
		}

		if (toa != 0)
		{
			w.append_lit(",\"toa\":");
			if ((toa % 1000000) != 0)
				w.append_float((double)toa / 1000000.0);
			else
				w.append_int(toa / 1000000);
		}

		if (!uuid.empty())
		{
			w.append_lit(",\"uuid\":\"");
			w.append(uuid.data(), uuid.size());
			w.append('"');
		}

		if (tag.ipv4)
		{
			w.append_lit(",\"ipv4\":");
			w.append_int((long long)tag.ipv4);
		}

		if (tag.mode & 1)
		{
			w.append_lit(",\"signalpower\":");
			if (tag.level == LEVEL_UNDEFINED)
				w.append_lit("null");
			else
				w.append_float(tag.level);
			w.append_lit(",\"ppm\":");
			if (tag.ppm == PPM_UNDEFINED)
				w.append_lit("null");
			else
				w.append_float(tag.ppm);
		}

		if (getStation())
		{
			w.append_lit(",\"station_id\":");
			w.append_int((long long)getStation());
		}

		if (getLength() > 0)
		{
			w.append_lit(",\"mmsi\":");
			w.append_int((long long)mmsi());
			w.append_lit(",\"type\":");
			w.append_int((long long)type());
		}

		w.append_lit(",\"nmea\":[");
		NMEAView s = sentences();
		for (size_t i = 0; i < s.size(); i++)
		{
			if (i > 0)
				w.append(',');
			w.append('"');
			w.append(s[i].data(), s[i].size());
			w.append('"');
		}
		w.append_lit("]}");
		if (suffix)
			w.append_lit(suffix);

		return w.finish();
	}

	int Message::getNMEATagBlock(std::string &out, const char *suffix) const
	{
		static int groupId = 0;

		int total = sentences().size();
		int seq = 1;

		if (total > 1)
			groupId = (groupId % 9999) + 1;

		JSON::Writer w(out);

		// source string: "s" + station number
		char src[16];
		int srcLen;
		{
			char *sp = src;
			*sp++ = 's';
			char tmp[12];
			int n = 0;
			int v = station;
			if (v < 0)
			{
				*sp++ = '-';
				v = -v;
			}
			do
			{
				tmp[n++] = '0' + v % 10;
				v /= 10;
			} while (v);
			for (int i = n - 1; i >= 0; i--)
				*sp++ = tmp[i];
			srcLen = sp - src;
		}

		for (const auto &nmea : sentences())
		{
			w.append('\\');
			size_t cs_off = w.written(); // checksum starts after backslash

			w.append_lit("s:");
			w.append(src, srcLen);
			w.append_lit(",c:");
			w.append_float((double)rxtime / 1000000.0);

			if (total > 1)
			{
				w.append_lit(",g:");
				w.append_int((long long)seq);
				w.append('-');
				w.append_int((long long)total);
				w.append('-');
				w.append_int((long long)groupId);
			}

			// Calculate checksum over tag block content (between backslashes).
			// Re-derive base pointer here since intermediate writes may have grown the buffer.
			int check = 0;
			const char *base = w.buffer_start();
			const char *cs_end = base + w.written();
			for (const char *p = base + cs_off; p < cs_end; p++)
				check ^= (unsigned char)*p;

			w.append('*');
			static const char hex[] = "0123456789ABCDEF";
			w.append(hex[(check >> 4) & 0xF]);
			w.append(hex[check & 0xF]);
			w.append('\\');

			w.append(nmea.data(), nmea.size());
			if (suffix)
				w.append_lit(suffix);
			else
				w.append_lit("\r\n");

			seq++;
		}

		return w.finish();
	}

	int Message::getBinaryNMEA(std::string &out, const TAG &tag, bool crc, const char *suffix) const
	{
		if (length < 0 || length > MAX_AIS_LENGTH)
			return 0;

		JSON::Writer w(out);

		auto push_escaped = [&](unsigned char byte)
		{
			if (byte == '\n')
			{
				w.append((char)0xad);
				w.append((char)0xae);
			}
			else if (byte == '\r')
			{
				w.append((char)0xad);
				w.append((char)0xaf);
			}
			else if (byte == 0xad)
			{
				w.append((char)0xad);
				w.append((char)0xad);
			}
			else
			{
				w.append((char)byte);
			}
		};

		push_escaped(0xac);
		push_escaped(0x00);

		unsigned char flags = 0;
		if (tag.level != LEVEL_UNDEFINED && tag.ppm != PPM_UNDEFINED)
			flags |= 0x01;
		flags |= crc ? 0x02 : 0x00;
		push_escaped(flags);

		long long ts = (long long)rxtime;
		for (int i = 7; i >= 0; i--)
			push_escaped((ts >> (i * 8)) & 0xFF);

		if (flags & 0x01)
		{
			int signal_tenths = (int)(tag.level * 10.0f);
			push_escaped((signal_tenths >> 8) & 0xFF);
			push_escaped(signal_tenths & 0xFF);

			int ppm_tenths = (int)(tag.ppm * 10.0f);
			push_escaped((unsigned char)(int8_t)ppm_tenths);
		}

		push_escaped((unsigned char)channel);
		push_escaped((length >> 8) & 0xFF);
		push_escaped(length & 0xFF);

		int numBytes = (length + 7) / 8;
		for (int i = 0; i < numBytes; i++)
			push_escaped(data[i]);

		if (crc)
		{
			// Re-derive base pointer here: buffer may have grown during writes.
			uint16_t crc_value = Util::Helper::CRC16((const uint8_t *)w.buffer_start(), w.written());
			push_escaped((crc_value >> 8) & 0xFF);
			push_escaped(crc_value & 0xFF);
		}

		if (suffix)
			w.append_lit(suffix);
		else
			w.append('\n');

		return w.finish();
	}

	bool Message::validate()
	{
		if (getLength() == 0)
			return true;

		const int ml[28] = {149, 149, 149, 168, 418, 88, 72, 56, 168, 70, 168, 72, 40, 40, 88, 92, 80, 168, 312, 70, 271, 145, 154, 160, 72, 60, 96, 168};

		if (type() < 1 || type() > 28)
			return false;
		if (getLength() < ml[type() - 1])
			return false;

		return true;
	}

	unsigned Message::getUint(int start, int len) const
	{
		if (start < 0 || start + len > MAX_AIS_LENGTH)
			return 0;

		// Branchless: 5-byte big-endian load covers any len<=32 with y<=7 (max 39 bits).
		// data[] has +4 padding so data[x+4] is always in-bounds here.
		const int x = start >> 3;
		const int y = start & 7;

		uint32_t hi;
		std::memcpy(&hi, data + x, 4);
#if defined(__GNUC__) || defined(__clang__)
		hi = __builtin_bswap32(hi);
#else
		hi = ((hi & 0xFFu) << 24) | ((hi & 0xFF00u) << 8) | ((hi & 0xFF0000u) >> 8) | ((hi & 0xFF000000u) >> 24);
#endif
		const uint64_t w = ((uint64_t)hi << 8) | (uint64_t)data[x + 4]; // bottom 40 bits = 5 source bytes BE

		const unsigned shift = 40u - (unsigned)y - (unsigned)len;
		const uint32_t mask = (len >= 32) ? 0xFFFFFFFFu : ((1u << len) - 1u);
		return (unsigned)((w >> shift) & mask);
	}

	bool Message::setUint(int start, int len, unsigned val)
	{
		const int end = start + len;

		if (end > MAX_AIS_LENGTH || start < 0)
			return false;

		int x = start >> 3, y = start & 7;
		int remaining = len;
		length = MAX(end, length);

		if (8 - y >= remaining)
		{
			uint8_t bitmask = (0xFF >> (8 - remaining)) << (8 - y - remaining);
			data[x] = (data[x] & ~bitmask) | ((val << (8 - y - remaining)) & bitmask);
			return true;
		}

		uint8_t bitmask = 0xFF >> y;
		data[x] = (data[x] & ~bitmask) | ((val >> (remaining - 8 + y)) & bitmask);
		remaining -= 8 - y;
		x++;

		while (remaining >= 8)
		{
			data[x++] = (val >> (remaining - 8)) & 0xFF;
			remaining -= 8;
		}

		if (remaining > 0)
		{
			uint8_t bitmask = 0xFF << (8 - remaining);
			data[x] = (data[x] & ~bitmask) | ((val << (8 - remaining)) & bitmask);
		}
		return true;
	}

	int Message::getInt(int start, int len) const
	{
		const unsigned ones = ~0;
		unsigned u = getUint(start, len);

		// extend sign bit for the full bit
		if (u & (1 << (len - 1)))
			u |= ones << len;

		return (int)u;
	}

	bool Message::setInt(int start, int len, int val)
	{
		return setUint(start, len, (unsigned)val);
	}

	void Message::getText(int start, int len, std::string &str) const
	{
		if (start < 0 || start + len > MAX_AIS_LENGTH)
		{
			str.resize(0);
			return;
		}

		// SWAR: extract sizeof(size_t) six-bit chars per iteration.
		// 32-bit: 4 chars (24 bits, 3-byte step). 64-bit: 8 chars (48 bits, 6-byte step).
		constexpr int CHARS_PER = (int)sizeof(size_t);
		constexpr int STEP_BYTES = CHARS_PER * 6 / 8;
		constexpr int WORD_BITS = CHARS_PER * 8;

		const int nchars = (len + 5) / 6;
		// Upper bound; `last_nonspace` trims uninitialized tail past the terminator.
		AISC_STRING_RESIZE_UNINIT(str, nchars);
		char *p = &str[0];
		int n = 0, last_nonspace = 0;

		int x = start >> 3;
		const int y = start & 7;
		int remaining = nchars;

		while (remaining >= CHARS_PER && x + CHARS_PER <= MAX_AIS_BYTES)
		{
			// Big-endian load; compiler folds to a single unaligned ldr+rev / bswap.
			size_t u = 0;
			for (int i = 0; i < CHARS_PER; ++i)
				u = (u << 8) | data[x + i];

			// Pack CHARS_PER 6-bit fields into one byte each, MSB-first.
			size_t packed = 0;
			for (int i = 0; i < CHARS_PER; ++i)
			{
				const int sh = WORD_BITS - 6 - y - 6 * i;
				packed |= ((u >> sh) & 0x3F) << (8 * (CHARS_PER - 1 - i));
			}

			// 6->8 bit AIS ASCII transform: c |= (~c & 0x20) << 1.
			packed |= (~packed & SWAR::mask(0x20)) << 1;

			// Terminator: original 0 -> '@' after transform.
			if (SWAR::has_byte(packed, SWAR::mask(0x40)))
				break;

			for (int i = 0; i < CHARS_PER; ++i)
				p[n + i] = char(packed >> (8 * (CHARS_PER - 1 - i)));

			// Last non-space byte: XOR with space mask, find lowest nonzero byte
			// (= highest char index in this word since bytes are MSB-first).
			size_t nsm = packed ^ SWAR::mask(' ');
			if (nsm) last_nonspace = n + CHARS_PER - SWAR::first_byte(nsm);

			n += CHARS_PER;
			x += STEP_BYTES;
			remaining -= CHARS_PER;
		}

		int bitpos = x * 8 + y;
		while (remaining-- > 0)
		{
			unsigned w = (unsigned(data[bitpos >> 3]) << 8) | data[(bitpos >> 3) + 1];
			unsigned c = (w >> (10 - (bitpos & 7))) & 0x3F;
			if (!c) break;
			if (!(c & 32)) c |= 64;
			p[n++] = char(c);
			if (c != ' ') last_nonspace = n;
			bitpos += 6;
		}

		str.resize(last_nonspace);
	}

	void Message::setText(int start, int len, const char *str)
	{
		const int end = start + len;

		if (end > MAX_AIS_LENGTH || start < 0)
			return;

		int idx = 0;
		bool reachedEnd = false;

		while (start < end)
		{
			unsigned c = 0;

			if (!reachedEnd)
			{
				c = str[idx++];

				if (!(c >= 32 && c <= 63) && !(c >= 65 && c <= 95))
				{
					reachedEnd = true;
					c = 0;
				}
				else if (c & 64)
					c &= 0b00111111;
			}

			setUint(start, 6, c);

			start += 6;
		}

		length = MAX(end, length);

		return;
	}

	void Message::buildNMEA(TAG &tag, int id)
	{
		const int IDX_OWN_MMSI = 5;
		const int IDX_COUNT = 7;
		const int IDX_NUMBER = 9;

		if (id >= 0 && id < 10)
			ID = id;

		int nAISletters = (length + 6 - 1) / 6;
		int nSentences = (nAISletters == 0) ? 1 : (nAISletters + MAX_NMEA_CHARS - 1) / MAX_NMEA_CHARS;

		while ((int)NMEA.size() < nSentences)
		{
			NMEA.emplace_back();
			NMEA.back().reserve(128);
		}

		static constexpr char hex[] = "0123456789ABCDEF";
		const char own = own_mmsi == mmsi() ? 'O' : 'M';
		const char count = (char)(nSentences + '0');
		const char seq = (nSentences > 1) ? (char)(ID + '0') : 0;
		if (nSentences > 1)
			ID = (ID + 1) % 10;

		for (int s = 0, l = 0; s < nSentences; s++)
		{
			std::string &out = NMEA[s];
			// Upper bound; trimmed to `i` after writing; interior bytes fully overwritten.
			AISC_STRING_RESIZE_UNINIT(out, 128);
			char *p = &out[0];

			std::memcpy(p, "!AIVDM,X,X,X,X,", 11);
			p[IDX_OWN_MMSI] = own;
			p[IDX_COUNT] = count;
			p[IDX_NUMBER] = (char)(s + 1 + '0');

			int i = 11;
			if (seq)
				p[i++] = seq;
			p[i++] = ',';
			if (channel != '?')
				p[i++] = channel;
			p[i++] = ',';

			int letters = MIN(nAISletters - l, MAX_NMEA_CHARS);
			getPayload(p + i, l, letters);
			i += letters;
			l += letters;

			p[i++] = ',';
			p[i++] = (char)(((s == nSentences - 1) ? nAISletters * 6 - length : 0) + '0');

			int c = 0;
			for (int k = 1; k < i; k++)
				c ^= (unsigned char)p[k];

			p[i++] = '*';
			p[i++] = hex[(c >> 4) & 0xF];
			p[i++] = hex[c & 0xF];

			out.resize(i);
		}
		nmea_count = nSentences;
	}

	// AIS armoring: v<40 ? v+48 : v+56  →  '0'..'W' and '`'..'w'
	static constexpr char sixbit[64] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
		'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w'
	};

	char Message::getLetter(int pos) const
	{
		const int start = pos * 6;
		const int end = start + 6;

		if (end > MAX_AIS_LENGTH || start < 0)
			return 0;

		int x = start >> 3, y = start & 7;
		uint16_t w = (data[x] << 8) | data[x + 1];

		int l = (w >> (16 - 6 - y)) & 0x3F;

		// zero for bits not formally set
		int overrun = end - length;

		if (overrun > 0)
			l &= 0x3F << overrun;

		return sixbit[l];
	}

	void Message::getPayload(char *dst, int pos, int count) const
	{
		int k = 0;
		// Fast path: byte-aligned bit offset → 3 bytes decode to 4 letters.
		// Reserve the final letter for the scalar tail so overrun masking stays correct.
		if ((pos & 3) == 0)
		{
			int x = (pos * 6) >> 3;
			while (k + 4 < count && x + 3 <= MAX_AIS_BYTES)
			{
				uint8_t b0 = data[x];
				uint8_t b1 = data[x + 1];
				uint8_t b2 = data[x + 2];
				dst[k + 0] = sixbit[b0 >> 2];
				dst[k + 1] = sixbit[((b0 & 0x3) << 4) | (b1 >> 4)];
				dst[k + 2] = sixbit[((b1 & 0xF) << 2) | (b2 >> 6)];
				dst[k + 3] = sixbit[b2 & 0x3F];
				k += 4;
				x += 3;
			}
		}
		for (; k < count; k++)
			dst[k] = getLetter(pos + k);
	}

	static constexpr uint8_t nmea_decode[256] = {
		// indices 0-47: unused (control chars, space, punctuation before '0')
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		// indices 48-87: '0'-'W' → 0-39  (c - 48)
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39,
		// indices 88-95: 'X'-'_' → 40-47  (c - 48)
		40, 41, 42, 43, 44, 45, 46, 47,
		// indices 96-119: '`'-'w' → 40-63  (c - 56)
		40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63};

	void Message::appendPayload(const char *src, int count)
	{
		int pos = length / 6;
		int bitOff = (pos * 6) & 7;
		int byteIdx = (pos * 6) >> 3;
		int endBits = (pos + count) * 6;

		if (endBits > MAX_AIS_LENGTH)
			count = (MAX_AIS_LENGTH - pos * 6) / 6;

		const uint8_t *usrc = (const uint8_t *)src;
		int i = 0;

		// 1. Head: align to byte boundary
		while (i < count && bitOff != 0)
		{
			uint8_t v = nmea_decode[usrc[i++]];
			switch (bitOff)
			{
			case 2:
				data[byteIdx] = (data[byteIdx] & 0xC0) | v;
				byteIdx++;
				break;
			case 4:
				data[byteIdx] = (data[byteIdx] & 0xF0) | (v >> 2);
				data[byteIdx + 1] = (data[byteIdx + 1] & 0x3F) | ((v & 3) << 6);
				byteIdx++;
				break;
			case 6:
				data[byteIdx] = (data[byteIdx] & 0xFC) | (v >> 4);
				data[byteIdx + 1] = (data[byteIdx + 1] & 0x0F) | ((v & 0xF) << 4);
				byteIdx++;
				break;
			}
			bitOff = (bitOff + 6) & 7;
		}

		// 2. Bulk: 4 chars → 3 bytes, no branching
		int bulk = (count - i) >> 2;
		while (bulk--)
		{
			uint8_t c0 = nmea_decode[usrc[i]];
			uint8_t c1 = nmea_decode[usrc[i + 1]];
			uint8_t c2 = nmea_decode[usrc[i + 2]];
			uint8_t c3 = nmea_decode[usrc[i + 3]];

			data[byteIdx] = (c0 << 2) | (c1 >> 4);
			data[byteIdx + 1] = (c1 << 4) | (c2 >> 2);
			data[byteIdx + 2] = (c2 << 6) | c3;

			byteIdx += 3;
			i += 4;
		}

		// 3. Tail: 0-3 remaining chars, fully unrolled (bitOff cycle: 0, 6, 4)
		int rem = count - i;
		if (rem >= 1)
		{
			uint8_t v0 = nmea_decode[usrc[i]];
			data[byteIdx] = (data[byteIdx] & 0x03) | (v0 << 2);
		}
		if (rem >= 2)
		{
			uint8_t v1 = nmea_decode[usrc[i + 1]];
			data[byteIdx] = (data[byteIdx] & 0xFC) | (v1 >> 4);
			byteIdx++;
			data[byteIdx] = (data[byteIdx] & 0x0F) | ((v1 & 0xF) << 4);
		}
		if (rem >= 3)
		{
			uint8_t v2 = nmea_decode[usrc[i + 2]];
			data[byteIdx] = (data[byteIdx] & 0xF0) | (v2 >> 2);
			byteIdx++;
			data[byteIdx] = (data[byteIdx] & 0x3F) | ((v2 & 3) << 6);
		}

		length = (pos + count) * 6;
	}

	void Message::setLetter(int pos, char c)
	{
		const int start = pos * 6;
		const int end = start + 6;

		if (end > MAX_AIS_LENGTH || start < 0)
			return;

		int x = start >> 3, y = start & 7;

		length = MAX(end, length);

		c = nmea_decode[(unsigned char)c];

		switch (y)
		{
		case 0:
			data[x] = (data[x] & 0b00000011) | (c << 2);
			break;
		case 2:
			data[x] = (data[x] & 0b11000000) | c;
			break;
		case 4:
			data[x] = (data[x] & 0b11110000) | (c >> 2);
			data[x + 1] = (data[x + 1] & 0b00111111) | ((c & 3) << 6);
			break;
		case 6:
			data[x] = (data[x] & 0b11111100) | (c >> 4);
			data[x + 1] = (data[x + 1] & 0b00001111) | ((c & 15) << 4);
			break;
		default:
			break;
		}
	}

	bool Filter::SetOptionKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_ALLOW_TYPE:
		{
			std::stringstream ss(arg);
			std::string type_str;
			allow = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned type = Util::Parse::Integer(type_str, 1, 28);
				allow |= 1U << type;
			}
			return true;
		}
		case AIS::KEY_SETTING_SELECT_REPEAT:
		case AIS::KEY_SETTING_ALLOW_REPEAT:
		{
			std::stringstream ss(arg);
			std::string type_str;
			allow_repeat = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned r = Util::Parse::Integer(type_str, 0, 3);
				allow_repeat |= 1U << r;
			}
			return true;
		}
		case AIS::KEY_SETTING_BLOCK_TYPE:
		{
			std::stringstream ss(arg);
			std::string type_str;
			unsigned block = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned type = Util::Parse::Integer(type_str, 1, 28);
				block |= 1U << type;
			}
			allow = ~block & all;
			return true;
		}
		case AIS::KEY_SETTING_BLOCK_REPEAT:
		{
			std::stringstream ss(arg);
			std::string type_str;
			unsigned block = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned r = Util::Parse::Integer(type_str, 0, 3);
				block |= 1U << r;
			}
			allow_repeat = ~block & all;
			return true;
		}
		case AIS::KEY_SETTING_FILTER:
			on = Util::Parse::Switch(arg);
			return true;
		case AIS::KEY_SETTING_UNIQUE:
			unique_interval = Util::Parse::Switch(arg) ? 3 : 0;
			return true;
		case AIS::KEY_SETTING_POSITION_INTERVAL:
			Util::Parse::OptionalInteger(arg, 0, 3600, position_interval);
			return true;
		case AIS::KEY_SETTING_OWN_INTERVAL:
			Util::Parse::OptionalInteger(arg, 0, 3600, own_interval);
			return true;
		case AIS::KEY_SETTING_DOWNSAMPLE:
			Error() << "Option 'DOWNSAMPLE' is deprecated, please use 'OWN_INTERVAL' instead.";
			own_interval = Util::Parse::Switch(arg) ? 10 : 0;
			return true;
		case AIS::KEY_SETTING_GPS:
			GPS = Util::Parse::Switch(arg);
			return true;
		case AIS::KEY_SETTING_AIS:
			AIS = Util::Parse::Switch(arg);
			return true;
		case AIS::KEY_SETTING_SELECT_ID:
		case AIS::KEY_SETTING_ID:
		{
			std::stringstream ss(arg);
			std::string id_str;
			while (getline(ss, id_str, ','))
			{
				ID_allowed.push_back(Util::Parse::Integer(id_str, 0, 999999));
			}
			return true;
		}
		case AIS::KEY_SETTING_SELECT_CHANNEL:
		case AIS::KEY_SETTING_ALLOW_CHANNEL:
		{
			allowed_channels = arg;
			Util::Convert::toUpper(allowed_channels);
			return true;
		}
		case AIS::KEY_SETTING_SELECT_MMSI:
		case AIS::KEY_SETTING_ALLOW_MMSI:
		{
			std::stringstream ss(arg);
			std::string mmsi_str;
			while (getline(ss, mmsi_str, ','))
			{
				MMSI_allowed.push_back(Util::Parse::Integer(mmsi_str, 0, 999999999));
			}
			return true;
		}
		case AIS::KEY_SETTING_BLOCK_MMSI:
		{
			std::stringstream ss(arg);
			std::string mmsi_str;
			while (getline(ss, mmsi_str, ','))
			{
				MMSI_blocked.push_back(Util::Parse::Integer(mmsi_str, 0, 999999999));
			}
			return true;
		}
		case AIS::KEY_SETTING_REMOVE_EMPTY:
			remove_empty = Util::Parse::Switch(arg);
			return true;
		default:
			return false;
		}
	}

	std::string Filter::getAllowed()
	{

		if (allow == all)
			return std::string("ALL");

		std::string ret;
		for (unsigned i = 1; i <= 28; i++)
		{
			if ((allow & (1U << i)) > 0)
				ret += (!ret.empty() ? std::string(",") : std::string("")) + std::to_string(i);
		}
		return ret;
	}

	std::string Filter::Get()
	{
		if (!on)
			return "";

		std::string ret;

		if (!GPS)
			ret += "gps OFF";

		if (allow != all)
		{
			if (!ret.empty())
				ret += ", ";
			ret += "allowed {" + getAllowed() + "}";
		}

		if (own_interval > 0)
		{
			if (!ret.empty())
				ret += ", ";
			ret += "own_interval " + std::to_string(own_interval);
		}

		if (position_interval > 0)
		{
			if (!ret.empty())
				ret += ", ";
			ret += "position_interval " + std::to_string(position_interval);
		}

		if (unique_interval > 0)
		{
			if (!ret.empty())
				ret += ", ";
			ret += "unique_interval " + std::to_string(unique_interval);
		}

		if (!MMSI_allowed.empty())
		{
			if (!ret.empty())
				ret += ", ";
			ret += "mmsi_filter ON";
		}

		if (!MMSI_blocked.empty())
		{
			if (!ret.empty())
				ret += ", ";
			ret += "mmsi_block ON";
		}

		if (!allowed_channels.empty())
		{
			if (!ret.empty())
				ret += ", ";
			ret += "channel {" + allowed_channels + "}";
		}

		return ret;
	}

	bool Filter::include(const Message &msg)
	{
		if (own_interval && msg.isOwn())
		{
			if (msg.getRxTimeUnix() - last_VDO < own_interval)
			{
				return false;
			}
			last_VDO = msg.getRxTimeUnix();
		}

		// Position downsampling for types 1, 2, 3
		bool old_position = false;

		if (position_interval > 0)
		{
			unsigned msg_type = msg.type();
			if (msg_type == 1 || msg_type == 2 || msg_type == 3 || msg_type == 18 || msg_type == 27)
			{
				if (!position_history.check(msg.mmsi(), (uint32_t)msg.getRxTimeUnix(), position_interval))
				{
					return false;
				}
				old_position = true;
			}
		}

		if (unique_interval > 0 && !old_position)
		{
			if (!duplicate_history.check(msg.getHash(), (uint32_t)msg.getRxTimeUnix(), unique_interval))
			{
				return false;
			}
		}

		if (!on)
			return true;

		if (!AIS)
			return false;

		if (remove_empty)
		{
			if (msg.getLength() == 0)
				return false;
		}

		bool ID_ok = true;
		if (ID_allowed.size())
		{

			ID_ok = false;
			for (auto id : ID_allowed)
			{
				if (msg.getStation() == id)
				{
					ID_ok = true;
					break;
				}
			}
		}
		if (!ID_ok)
		{
			return false;
		}

		bool CH_ok = true;
		if (!allowed_channels.empty())
		{
			CH_ok = false;
			for (char c : allowed_channels)
			{
				if (msg.getChannel() == c)
				{
					CH_ok = true;
					break;
				}
			}
		}

		if (!CH_ok)
		{
			return false;
		}

		bool MMSI_ok = true;
		if (!MMSI_allowed.empty())
		{
			MMSI_ok = false;
			for (auto mmsi : MMSI_allowed)
			{
				if (msg.mmsi() == mmsi)
				{
					MMSI_ok = true;
					break;
				}
			}
		}

		if (!MMSI_ok)
		{
			return false;
		}

		if (!MMSI_blocked.empty())
		{
			for (auto mmsi : MMSI_blocked)
			{
				if (msg.mmsi() == mmsi)
				{
					return false;
				}
			}
		}

		unsigned type = msg.type() & 31;
		unsigned repeat = msg.repeat() & 3;

		bool type_ok = ((1U << type) & allow) != 0;
		bool repeat_ok = ((1U << repeat) & allow_repeat) != 0;

		if (!(type_ok && repeat_ok))
		{
			return false;
		}

		return true;
	}
}
