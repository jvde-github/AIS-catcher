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

	void NMEA::split(std::string str, std::vector<std::string>& tokens) {
		int at;
		while (((at = str.find(',')) != std::string::npos)) {
			tokens.push_back(str.substr(0, at));
			str = str.substr(at + 1, str.size() - (at + 1));
		}
		tokens.push_back(str);
	}

	void NMEA::parse(TAG& tag) {
		std::vector<std::string> tokens;
		split(sentence, tokens);

		if (tokens.size() != 7) {
			std::cerr << "NMEA: Header ok but message has invalid structure: " << sentence << std::endl;
			return;
		}

		if (tokens[1] != "1") {
			// std::cerr << "NMEA: not implemented: " << sentence << std::endl;
			return;
		}

		msg.clear();
		msg.sentence.push_back(sentence);
		msg.channel = tokens[4][0];
		msg.Stamp();

		const std::string& data = tokens[5];
		for (int i = 0; i < data.size(); i++)
			msg.setLetter(i, data[i] >= 96 ? data[i] - 56 : data[i] - 48);

		Send(&msg, 1, tag);
	}

	void NMEA::Receive(const RAW* data, int len, TAG& tag) {
		for (int i = 0; i < data->size; i++) {
			char c = ((char*)data->data)[i];

			if (index == header.size() - 1) {
				if (c != '\n' && c != '\r')
					sentence += c;
				else {
					parse(tag);
					sentence.clear();
					index = 0;
				}
			}
			else {
				if (header[index] == c) {
					sentence += c;
					index++;
				}
				else
					sentence.clear();
			}
		}
	}

}
