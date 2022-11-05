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
