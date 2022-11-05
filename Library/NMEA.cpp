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

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

namespace AIS {

	void NMEA::reset() {
		sentence.clear();
		index = 0;
		commas.resize(0);
	}

	void NMEA::parse(TAG& tag) {
		if (commas.size() != 6) {
			std::cerr << "NMEA: Header ok but message has invalid structure: " << sentence << std::endl;
			return;
		}

		if (sentence[commas[0]] != '1') {
			// std::cerr << "NMEA: not implemented: " << sentence << std::endl;
			return;
		}

		msg.clear();
		msg.sentence.push_back(sentence);
		msg.channel = sentence[commas[3]];
		msg.Stamp();

		for (int i = 0, l = commas[4]; l < commas[5] - 1; i++, l++) {
			msg.setLetter(i, sentence[l] >= 96 ? sentence[l] - 56 : sentence[l] - 48);
		}

		Send(&msg, 1, tag);
	}

	// continue collect full NMEA line in `sentence` and store location of commas 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {
		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {
				char c = ((char*)data[j].data)[i];

				if (index >= header.size()) {
					if (c != '\n' && c != '\r') {
						sentence += c;
						index++;
						if (c == ',') commas.push_back(index);
					}
					else {
						parse(tag);
						reset();
					}
				}
				else { // we are in process of detecting "!AIVDM" header
					if (header[index] == c) {
						sentence += c;
						index++;
					}
					else
						reset();
				}
			}
		}
	}

}
