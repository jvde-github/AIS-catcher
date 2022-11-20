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

#include <iomanip>

#include "Message.h"
#include "Stream.h"
#include "Signals.h"

namespace AIS {


	class NMEA : public SimpleStreamInOut<RAW, Message>, public SignalIn<DecoderSignals> {
		Message msg;

		struct AIVDM {
			std::string sentence;
			std::string data;

			void reset() {
				sentence.clear();
				data.clear();
			}
			char channel;
			int count;
			int number;
			int ID;
			int checksum;
			int fillbits;
			int talkerID;
		} aivdm;

		std::vector<AIVDM> queue;
		int index = 0;
		char last = '\n';

		void process(TAG& tag);
		void addline(const AIVDM& a);
		void reset();
		void clean(char, int);
		int search(const AIVDM& a);

		bool isNMEAchar(char c) { return (c >= 40 && c < 88) || (c >= 96 && c <= 56 + 0b111111); }
		bool isHEX(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
		int fromHEX(char c) { return (c >= '0' && c <= '9') ? (c - '0') : (c - 'A' + 10); }

		int NMEAchecksum(std::string s);

		bool regenerate = false;
		bool crc_check = false;

	public:
		void Receive(const RAW* data, int len, TAG& tag);

		void setRegenerate(bool b) { regenerate = b; }
		bool getRegenerate() { return regenerate; }

		void setCRCcheck(bool b) { crc_check = b; }
		bool getCRCcheck() { return crc_check; }
	};
}
