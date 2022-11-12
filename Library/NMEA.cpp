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
			if (i->channel() == c) {
				multiline.erase(i);
				i--;
			}
		}
	}

	void NMEA::process(TAG& tag) {
		if (aivdm.commas.size() != 6) {
			std::cerr << "NMEA: Header ok but message has invalid structure (commas): " << aivdm.sentence << std::endl;
			return;
		}

		if (aivdm.count() == 1) {
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
			if (p.channel() == aivdm.channel()) {
				if (p.count() != aivdm.count() || aivdm.ID() != p.ID()) {
					std::cerr << "NMEA: multiline NMEA message not correct, mismatch in code and/or number of sentences [" << aivdm.sentence << " vs " << p.sentence << "]." << std::endl;
					clean(aivdm.channel());
					return;
				}
				else { // found and we store the previous sequence number
					lastNumber = p.number();
					break;
				}
			}
		}

		if (aivdm.number() != lastNumber + 1) {
			std::cerr << "NMEA: missing previous line in multiline message [" << aivdm.sentence << "]." << std::endl;
			clean(aivdm.channel());
			return;
		}

		multiline.push_back(aivdm);

		if (aivdm.number() != aivdm.count()) return;

		// multiline messages are now complete and in the right order
		// we create a message and add the payloads to it
		msg.clear();
		msg.Stamp();
		for (auto it = multiline.begin(); it != multiline.end(); it++)
			if (it->channel() == aivdm.channel())
				addline(*it);

		Send(&msg, 1, tag);
		clean(aivdm.channel());
	}

	void NMEA::addline(const AIVDM& a) {

		msg.sentence.push_back(a.sentence);
		msg.channel = a.sentence[a.commas[3]];

		for (int l = a.commas[4]; l < a.commas[5] - 1; l++) {
			msg.appendLetter(a.sentence[l]);
		}
	}

	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {
		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {

				char c = ((char*)data[j].data)[i];

				if (index >= header.size()) {
					if (c != '\n' && c != '\r' && !(c == ',' && aivdm.commas.size() == 6)) {
						aivdm.sentence += c;
						index++;
						if (c == ',') aivdm.commas.push_back(index);
					}
					else {
						process(tag);
						reset();
					}
				}
				else { // we are in process of detecting "!AIVDM" header
					if (header[index] == c) {
						aivdm.sentence += c;
						index++;
					}
					else
						reset();
				}
			}
		}
	}

}
