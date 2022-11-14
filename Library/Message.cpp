/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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
		// max unsigned integers are 30 bits in AIS standard
		const uint8_t ones = 0xFF;
		const uint8_t start_mask[] = { ones, ones >> 1, ones >> 2, ones >> 3, ones >> 4, ones >> 5, ones >> 6, ones >> 7 };

		// we start 2nd part of first byte and stop first part of last byte
		int i = start >> 3;
		unsigned u = data[i] & start_mask[start & 7];
		int remaining = len - 8 + (start & 7);

		// first byte is last byte
		if (remaining <= 0) {
			return u >> (-remaining);
		}
		// add full bytes
		while (remaining >= 8) {
			u <<= 8;
			u |= data[++i];
			remaining -= 8;
		}
		// make room for last bits if needed
		if (remaining > 0) {
			u <<= remaining;
			u |= data[++i] >> (8 - remaining);
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

	std::string Message::getText(int start, int len) const {

		int end = start + len;
		std::string text = "";
		text.reserve((len + 5) / 6 + 2); // reserve 2 extra for special characters

		while (start < end) {
			int c = getUint(start, 6);

			// 0       ->   @ and ends the string
			// 1 - 31  ->   65+ ( i.e. setting bit 6 )
			// 32 - 63 ->   32+ ( i.e. doing nothing )

			if (!c) break;
			if (!(c & 32)) c |= 64;

			text += (char)c;
			start += 6;
		}
		return text;
	}

	void Message::buildNMEA(TAG& tag) {
		std::string line;
		const std::string comma = ",";

		sentence.resize(0);
		int nAISletters = (length + 6 - 1) / 6;
		int nSentences = (nAISletters + 56 - 1) / 56;

		for (int s = 0, l = 0; s < nSentences; s++) {
			line = std::string("AIVDM,") + std::to_string(nSentences) + comma + std::to_string(s + 1) + comma;
			line += (nSentences > 1 ? std::to_string(ID) : "") + comma + channel + comma;

			for (int i = 0; l < nAISletters && i < 56; i++, l++)
				line += getLetter(l, length);

			line += comma + std::to_string((s == nSentences - 1) ? nAISletters * 6 - length : 0);

			char hex[3];
			sprintf(hex, "%02X", NMEAchecksum(line));
			line += std::string("*") + hex;

			sentence.push_back("!" + line);
		}

		if (tag.mode & 2) Stamp();
		ID = (ID + 1) % 10;
	}
	// dealing with 6 bit letters
	char Message::getLetter(int pos, int nBytes) const {
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		// zero padding
		uint8_t b0 = x < nBytes ? data[x] : 0;
		uint8_t b1 = x + 1 < nBytes ? data[(int)(x + 1)] : 0;
		uint16_t w = (b0 << 8) | b1;

		const int mask = (1 << 6) - 1;
		int l = (w >> (16 - 6 - y)) & mask;

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
}
