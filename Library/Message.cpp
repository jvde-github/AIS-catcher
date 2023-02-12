/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

namespace AIS {

	int Message::ID = 0;

	unsigned Message::getUint(int start, int len) const {
		// we start 2nd part of first byte and stop first part of last byte
		int x = start >> 3, y = start & 7;
		unsigned u = data[x] & (0xFF >> y);
		int remaining = len - 8 + y;

		// first byte is last byte
		if (remaining <= 0) {
			return u >> (-remaining);
		}
		// add full bytes
		while (remaining >= 8) {
			u <<= 8;
			u |= data[++x];
			remaining -= 8;
		}
		// make room for last bits if needed
		if (remaining > 0) {
			u <<= remaining;
			u |= data[++x] >> (8 - remaining);
		}

		return u;
	}

	int Message::getInt(int start, int len) const {
		const unsigned ones = ~0;
		unsigned u = getUint(start, len);

		// extend sign bit for the full bit
		if (u & (1 << (len - 1))) u |= ones << len;
		return (int)u;
	}

	void Message::getText(int start, int len, std::string& str) const {

		int end = start + len;
		str.clear();

		while (start < end) {
			int c = getUint(start, 6);

			// 0       ->   @ and ends the string
			// 1 - 31  ->   65+ ( i.e. setting bit 6 )
			// 32 - 63 ->   32+ ( i.e. doing nothing )

			if (!c) break;
			if (!(c & 32)) c |= 64;

			str += (char)c;
			start += 6;
		}
		return;
	}

	void Message::buildNMEA(TAG& tag, int id) {
		const char comma = ',';

		const int IDX_COUNT = 7;
		const int IDX_NUMBER = 9;

		if (id >= 0 && id < 10) ID = id;

		int nAISletters = (length + 6 - 1) / 6;
		int nSentences = (nAISletters + MAX_NMEA_CHARS - 1) / MAX_NMEA_CHARS;

		line.resize(11);

		line[IDX_COUNT] = (char)(nSentences + '0');
		line[IDX_NUMBER] = '0';

		if (nSentences > 1) {
			line += (char)(ID + '0');
			ID = (ID + 1) % 10;
		}

		line += comma;
		if (channel != '?') line += channel;
		line += comma;

		int header = line.length();
		NMEA.clear();

		for (int s = 0, l = 0; s < nSentences; s++) {

			line.resize(header);
			line[IDX_NUMBER]++;

			for (int i = 0; l < nAISletters && i < MAX_NMEA_CHARS; i++, l++)
				line += getLetter(l);

			line += comma;
			line += (char)(((s == nSentences - 1) ? nAISletters * 6 - length : 0) + '0');

			int c = NMEAchecksum(line);
			line += '*';
			line += (c >> 4) < 10 ? (c >> 4) + '0' : (c >> 4) + 'A' - 10;
			line += (c & 0xF) < 10 ? (c & 0xF) + '0' : (c & 0xF) + 'A' - 10;
			NMEA.push_back(line);
		}
	}

	char Message::getLetter(int pos) const {
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;
		uint16_t w = (data[x] << 8) | data[x + 1];

		const int mask = (1 << 6) - 1;
		int l = (w >> (16 - 6 - y)) & mask;

		// zero for bits not formally set
		int overrun = pos * 6 + 6 - length;
		if (overrun > 0) l &= 0xFF << overrun;
		return l < 40 ? (char)(l + 48) : (char)(l + 56);
	}

	void Message::setLetter(int pos, char c) {
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		if (length < (pos + 1) * 6) length = (pos + 1) * 6;
		if (length > MAX_AIS_LENGTH) return;

		c = (c >= 96 ? c - 56 : c - 48) & 0b00111111;

		switch (y) {
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

	Setting& Filter::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "ALLOW_TYPE") {

			std::stringstream ss(arg);
			std::string type_str;
			allow = 0;
			while (ss.good()) {
				getline(ss, type_str, ',');
				unsigned type = Util::Parse::Integer(type_str, 1, 27);
				allow |= 1U << type;
			}
		}
		else if (option == "BLOCK_TYPE") {

			std::stringstream ss(arg);
			std::string type_str;
			unsigned block = 0;
			while (ss.good()) {
				getline(ss, type_str, ',');
				unsigned type = Util::Parse::Integer(type_str, 1, 27);
				block |= 1U << type;
			}
			allow = ~block & all_msg;
		}
		else if (option == "FILTER") {
			Util::Convert::toUpper(arg);
			on = Util::Parse::Switch(arg);
		}
		return *this;
	}

	std::string Filter::getAllowed() {
		std::string ret;
		for (unsigned i = 1; i <= 27; i++) {
			if ((allow & (1U << i)) > 0)
				ret += (!ret.empty() ? std::string(",") : std::string("")) + std::to_string(i);
		}
		return ret;
	}
}
