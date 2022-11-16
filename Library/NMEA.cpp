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

#include "NMEA.h"

namespace AIS {

	void NMEA::reset() {
		aivdm.reset();
		index = 0;
	}

	void NMEA::clean(char c) {
		auto i = queue.begin();
		while (i != queue.end()) {
			if (i->channel == c)
				i = queue.erase(i);
			else
				i++;
		}
	}

	int NMEA::search(const AIVDM& a) {
		// multiline message, firstly check whether we can find previous lines with the same ID, channel and line count
		// we run backwards to find the previous addition
		// return: 0 = Not Found, -1: Found but inconsistent with input, >0: number of previous message
		int lastNumber = 0;
		for (auto it = queue.rbegin(); it != queue.rend(); it++) {
			if (it->channel == aivdm.channel) {
				if (it->count != aivdm.count || it->ID != aivdm.ID)
					lastNumber = -1;
				else
					lastNumber = it->number;
				break;
			}
		}
		return lastNumber;
	}

	int NMEA::NMEAchecksum(std::string s) {
		int c = 0;
		for (int i = 1; i < s.length() - 3; i++)
			c ^= s[i];
		return c;
	}

	void NMEA::process(TAG& tag) {
		if (aivdm.checksum != NMEAchecksum(aivdm.sentence)) {
			std::cerr << "NMEA: incorrect checksum [" << aivdm.sentence << "]." << std::endl;
			if (crc_check) return;
		}

		if (aivdm.count == 1) {
			msg.clear();
			msg.Stamp();
			msg.setChannel(aivdm.channel);

			addline(aivdm);
			if (regenerate)
				msg.buildNMEA(tag);
			else
				msg.NMEA.push_back(aivdm.sentence);
			Send(&msg, 1, tag);
			return;
		}

		int result = search(aivdm);

		if (aivdm.number != result + 1 || result == -1) {
			std::cerr << "NMEA: incorrect multiline messages @ [" << aivdm.sentence << "]." << std::endl;
			clean(aivdm.channel);
			if (aivdm.number != 1) return;
		}

		queue.push_back(aivdm);
		if (aivdm.number != aivdm.count) return;

		// multiline messages are now complete and in the right order
		// we create a message and add the payloads to it
		msg.clear();
		msg.Stamp();
		msg.setChannel(aivdm.channel);

		for (auto it = queue.begin(); it != queue.end(); it++) {
			if (it->channel == aivdm.channel) {
				addline(*it);
				if (!regenerate) msg.NMEA.push_back(it->sentence);
			}
		}

		if (regenerate)
			msg.buildNMEA(tag, aivdm.ID);

		Send(&msg, 1, tag);
		clean(aivdm.channel);
	}

	void NMEA::addline(const AIVDM& a) {
		for (char d : a.data) msg.appendLetter(d);
		if (a.count == a.number) msg.reduceLength(a.fillbits);
	}

	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {
		const std::string pattern = "sttVDM,c,n,i,c,d,f*cc";

		const int IDX_START = 0;
		const int IDX_TALKER1 = 1;
		const int IDX_TALKER2 = 2;
		const int IDX_COUNT = 7;
		const int IDX_NUMBER = 9;
		const int IDX_ID = 11;
		const int IDX_CHANNEL = 13;
		const int IDX_DATA = 15;
		const int IDX_FILLBITS = 17;
		const int IDX_CRC1 = 19;
		const int IDX_CRC2 = 20;

		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {

				char c = ((char*)(data[j].data))[i];
				aivdm.sentence += c;

				bool match = false;

				switch (index) {
				case IDX_START:
					match = c == '!' || c == '$';
					break;
				case IDX_TALKER1:
					match = c == 'A' || c == 'B' || c == 'S';
					break;
				case IDX_TALKER2:
					match = last == 'A' && (c == 'B' || c == 'D' || c == 'I' || c == 'N' || c == 'R' || c == 'S' || c == 'T' || c == 'X');
					match = match || (c == 'S' && last == 'B') || (c == 'A' && last == 'S');
					break;
				case IDX_COUNT:
					match = std::isdigit(c);
					if (match) { aivdm.count = c - '0'; }
					break;
				case IDX_NUMBER:
					match = std::isdigit(c);
					if (match) { aivdm.number = c - '0'; }
					break;
				case IDX_ID:
					if (c == ',') {
						index++;
						match = true;
						aivdm.ID = 0;
						break;
					}
					match = std::isdigit(c);
					if (match) { aivdm.ID = c - '0'; }
					break;

				case IDX_CHANNEL:
					if (c == ',') {
						match = true;
						aivdm.channel = '?';
						index++;
						break;
					}
					match = true;
					aivdm.channel = c;
					break;
				case IDX_DATA:
					match = true;
					if (c != ',') {
						aivdm.data += c;
						index--;
						break;
					}
					else
						index++;
					break;
				case IDX_FILLBITS:
					match = std::isdigit(c);
					if (match) { aivdm.fillbits = c - '0'; }
					break;
				case IDX_CRC1:
					match = isHEX(c);
					if (match) aivdm.checksum = fromHEX(c) << 4;
					break;
				case IDX_CRC2:
					match = isHEX(c);
					if (match) {
						aivdm.checksum |= fromHEX(c);
						process(tag);
					}
					break;
				default:
					match = c == pattern[index];
					break;
				}

				index++;
				if (!match || index >= pattern.size()) { reset(); }
				last = c;
			}
		}
	}
}
