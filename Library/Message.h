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

#pragma once

#include <string>
#include <time.h>
#include <vector>

#include "Utilities.h"

namespace AIS {

#define MAX_AIS_LENGTH (128 * 8)

	class Message {
	protected:
		static int ID;

		int NMEAchecksum(const std::string& s) {
			int check = 0;
			for (char c : s) check ^= c;
			return check;
		}

	public:
		std::time_t rxtime;
		std::vector<std::string> sentence;
		char channel;
		uint8_t data[128];
		int length;

		void Stamp() {
			std::time(&rxtime);
		}

		std::string getRxTime() const {
			return Util::Convert::toTimeStr(rxtime);
		}

		void clear() {
			length = 0;
			sentence.resize(0);
			std::memset(data, 0, 128);
		}

		unsigned type() const {
			return data[0] >> 2;
		}

		unsigned repeat() const {
			return data[0] & 3;
		}

		unsigned mmsi() const {
			return (data[1] << 22) | (data[2] << 14) | (data[3] << 6) | (data[4] >> 2);
		}

		unsigned getUint(int start, int len) const;
		int getInt(int start, int len) const;
		std::string getText(int start, int len) const;

		void setBit(int i, bool b) {
			if (b)
				data[i >> 3] |= (1 << (i & 7));
			else
				data[i >> 3] &= ~(1 << (i & 7));
		}

		bool getBit(int i) const {
			return data[i >> 3] & (1 << (i & 7));
		}

		char getLetter(int pos, int nBytes) const;
		void setLetter(int pos, char c);
		void appendLetter(char c) { setLetter(length / 6, c); }
		void reduceLength(int l) { length -= l; }
		void setID(int i) { ID = i; }

		void buildNMEA(TAG& tag);
	};
}
