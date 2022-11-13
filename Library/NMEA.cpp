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
		for (auto i = multiline.begin(); i != multiline.end(); ++i) {
			if (i->channel == c) {
				multiline.erase(i);
				i--;
			}
		}
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
		}
		if (aivdm.count == 1) {
			msg.clear();
			msg.Stamp();
			addline(aivdm);
			Send(&msg, 1, tag);
			return;
		}

		// multiline message, firstly check whether we can find previous lines with the same ID, channel and line count
		// we run backwards to find the last addition
		int lastNumber = 0;
		for (auto it = multiline.rbegin(); it != multiline.rend(); it++) {
			const AIVDM& p = *it;
			if (p.channel == aivdm.channel) {
				if (p.count != aivdm.count || aivdm.ID != p.ID) {
					std::cerr << "NMEA: multiline NMEA message not correct, mismatch in code and/or number of sentences [" << aivdm.sentence << " vs " << p.sentence << "]." << std::endl;
					clean(aivdm.channel);
					return;
				}
				else { // found and we store the previous sequence number
					lastNumber = p.number;
					break;
				}
			}
		}

		if (aivdm.number != lastNumber + 1) {
			std::cerr << "NMEA: missing previous line in multiline message [" << aivdm.sentence << "]." << std::endl;
			clean(aivdm.channel);
			if (aivdm.number != 1) return;
		}

		multiline.push_back(aivdm);

		if (aivdm.number != aivdm.count) return;

		// multiline messages are now complete and in the right order
		// we create a message and add the payloads to it
		msg.clear();
		msg.Stamp();
		for (auto it = multiline.begin(); it != multiline.end(); it++)
			if (it->channel == aivdm.channel)
				addline(*it);

		Send(&msg, 1, tag);
		clean(aivdm.channel);
	}

	void NMEA::addline(const AIVDM& a) {

		msg.sentence.push_back(a.sentence);
		msg.channel = a.channel;

		for (char d : a.data) msg.appendLetter(d);
		//if(a.count==a.number) msg.reduceLength(a.fillbits);
	}

	const std::string r = "!AIVDM,?,?,?,?,?,?*??";

	const int IDX_COUNT = 7;
	const int IDX_NUMBER = 9;
	const int IDX_ID = 11;
	const int IDX_CHANNEL = 13;
	const int IDX_DATA = 15;
	const int IDX_FILLBITS = 17;
	const int IDX_CRC1 = 19;
	const int IDX_CRC2 = 20;

	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {
		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {

				char c = ((char*)(data[j].data))[i];
				aivdm.sentence += c;

				bool match = false;

				switch (index) {
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
						index++;
						match = true;
						aivdm.channel = ',';
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
					match = c == r[index];
					break;
				}

				index++;
				if (!match || index >= r.size()) { reset(); }
			}
		}
	}

}
