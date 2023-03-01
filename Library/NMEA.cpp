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

#include "NMEA.h"

namespace AIS {

	void NMEA::reset() {
		aivdm.reset();
		index = 0;
	}

	void NMEA::clean(char c, int t) {
		auto i = queue.begin();
		while (i != queue.end()) {
			if (i->channel == c && i->talkerID == t)
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
			if (it->channel == aivdm.channel && it->talkerID == aivdm.talkerID) {
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

	void NMEA::process(TAG& tag, long t) {
		if (aivdm.checksum != NMEAchecksum(aivdm.sentence)) {
			std::cerr << "NMEA: incorrect checksum [" << aivdm.sentence << "]." << std::endl;
			if (crc_check) return;
		}

		if (aivdm.count == 1) {
			msg.clear();
			msg.Stamp(t);
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
			clean(aivdm.channel, aivdm.talkerID);
			if (aivdm.number != 1) return;
		}

		queue.push_back(aivdm);
		if (aivdm.number != aivdm.count) return;

		// multiline messages are now complete and in the right order
		// we create a message and add the payloads to it
		msg.clear();
		msg.Stamp(t);
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
		clean(aivdm.channel, aivdm.talkerID);
	}

	void NMEA::addline(const AIVDM& a) {
		for (char d : a.data) msg.appendLetter(d);
		if (a.count == a.number) msg.reduceLength(a.fillbits);
	}

	void NMEA::processNMEAchar(char c, TAG& tag, long t) {
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

		aivdm.sentence += c;

		bool match = false;

		switch (index) {
		case IDX_START:
			match = c == '!' || c == '$';
			break;
		case IDX_TALKER1:
			match = c == 'A' || c == 'B' || c == 'S';
			aivdm.talkerID = c << 8;
			break;
		case IDX_TALKER2:
			match = last == 'A' && (c == 'B' || c == 'D' || c == 'I' || c == 'N' || c == 'R' || c == 'S' || c == 'T' || c == 'X');
			match = match || (c == 'S' && last == 'B') || (c == 'A' && last == 'S');
			aivdm.talkerID |= c;
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
			if (c == ',') {
				index++;
				match = true;
			}
			else {
				match = isNMEAchar(c) && aivdm.data.size() < 128;
				if (match) {
					aivdm.data += c;
					index--;
				}
			}
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
				process(tag, t);
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

	void NMEA::processJSONsentence(TAG& tag, long t) {
		if (json[0] == '{') {
			try {
				JSON::Parser parser(&AIS::KeyMap, JSON_DICT_FULL);
				std::shared_ptr<JSON::JSON> j = parser.parse(json);

				tag.ppm = 0;
				tag.sample_lvl = 0;
				t = 0;

				// phase 1, get the meta data in place
				for (const auto& p : j->getProperties()) {
					switch (p.Key()) {
					case AIS::KEY_SIGNAL_POWER:
						tag.level = p.Get().getFloat();
						break;
					case AIS::KEY_PPM:
						tag.ppm = p.Get().getFloat();
						break;
					case AIS::KEY_RXUXTIME:
						t = p.Get().getInt();
						break;
					}
				}

				for (const auto& p : j->getProperties()) {
					if (p.Key() == AIS::KEY_NMEA) {

						for (const auto& v : p.Get().getArray()) {
							for (char d : v.getString()) processNMEAchar(d, tag, t);
							reset();
						}
					}
				}
			}
			catch (std::exception const& e) {
				std::cout << "UDP server: " << e.what() << std::endl;
			}
		}
	}
	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {

		long t = 0;

		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {

				char c = ((char*)(data[j].data))[i];

				if (c == '\r' || c == '\n') {
					if (json.size() > 0 && JSON) {
						// newline and we build up JSON string -> decode
						processJSONsentence(tag, t);
						json.clear();
					}
					newline = true;
					JSON = false;
				}
				else if (newline) {
					// first non-return character
					reset();
					newline = false;
					json.clear();
					if (c == '{') JSON = true;
				}

				if (!newline) {
					if (JSON) {
						json += c;
						if (json.size() > 2048) json.clear();
					}
					else
						processNMEAchar(c, tag, 0);
				}
			}
		}
	}
}