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

#include <cstring>

#include "IO.h"

namespace IO {

	void MessageToScreen::Receive(const AIS::Message* data, int len, TAG& tag) {

		if (level == OutputLevel::NONE) return;

		for (int i = 0; i < len; i++) {

			if (level == OutputLevel::FULL || level == OutputLevel::SPARSE)
				for (auto s : data[i].NMEA) {

					std::cout << s;

					if (level == OutputLevel::FULL) {
						std::cout << " ( MSG: " << data[i].type() << ", REPEAT: " << data[i].repeat() << ", MMSI: " << data[i].mmsi();
						if (tag.mode & 1) std::cout << ", signalpower: " << tag.level << ", ppm: " << tag.ppm;
						if (tag.mode & 2) std::cout << ", timestamp: " << data[i].getRxTime();

						std::cout << ")";
					}
					std::cout << std::endl;
				}
			else if (level == OutputLevel::JSON_NMEA) {

				std::cout << "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"channel\":\"" << data[i].getChannel() << "\"";

				if (tag.mode & 2) std::cout << ",\"rxtime\":\"" << data[i].getRxTime() << "\"";
				if (tag.mode & 1) std::cout << ",\"signalpower\":" << tag.level << ",\"ppm\":" << tag.ppm;

				std::cout << ",\"mmsi\":" << data[i].mmsi() << ",\"type\":" << data[i].type()
						  << ",\"nmea\":[\"" << data[i].NMEA[0] << "\"";

				for (int j = 1; j < data[i].NMEA.size(); j++)
					std::cout << ",\"" << data[i].NMEA[j] << "\"";

				std::cout << "]}" << std::endl;
			}
		}
	}

	void JSONtoScreen::Ready() {
		std::cout << json << std::endl;
	}
}
