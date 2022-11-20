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
		const int MAX_NMEA_CHARS = 56;
		static int ID;
		std::string line = "!AIVDM,X,X,X,X," + std::string(MAX_NMEA_CHARS, '.') + ",X*XX\n\r"; // longest line

		int NMEAchecksum(const std::string& s) {
			int check = 0;
			for (int i = 1; i < s.length(); i++) check ^= s[i];
			return check;
		}

		uint8_t data[128];
		std::time_t rxtime;
		int length;
		char channel;

	public:
		std::vector<std::string> NMEA;

		void Stamp() {
			std::time(&rxtime);
		}

		std::string getRxTime() const {
			return Util::Convert::toTimeStr(rxtime);
		}

		void clear() {
			length = 0;
			NMEA.resize(0);
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
		void getText(int start, int len, std::string& str) const;

		void setBit(int i, bool b) {
			if (b)
				data[i >> 3] |= (1 << (i & 7));
			else
				data[i >> 3] &= ~(1 << (i & 7));
		}

		bool getBit(int i) const {
			return data[i >> 3] & (1 << (i & 7));
		}

		char getLetter(int pos) const;
		void setLetter(int pos, char c);
		void appendLetter(char c) { setLetter(length / 6, c); }
		void reduceLength(int l) { length -= l; }

		void setLength(int l) { length = l; }
		int getLength() const { return length; }

		void setChannel(char c) { channel = c; }
		char getChannel() const { return channel; }

		void buildNMEA(TAG& tag, int id = -1);
	};

	class Filter : public Setting {
		const uint32_t all_msg = 0b1111111111111111111111111110;
		uint32_t allow = all_msg;
		bool on = false;

	public:
		void Set(std::string option, std::string arg);
		bool isOn() { return on; }
		std::string getAllowed();
		bool include(const Message& msg) {
			if (!on) return true;
			unsigned type = msg.type() & 31;
			return ((1U << type) & allow) != 0;
		}
	};
}
