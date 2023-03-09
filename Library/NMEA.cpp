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

	void NMEA::submitAIS(TAG& tag, long t) {
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


	void NMEA::split(const std::string& s) {
		parts.clear();
		std::stringstream ss(s);
		std::string p;
		while (ss.good()) {
			getline(ss, p, ',');
			parts.push_back(p);
		}
	}

	// stackoverflow variant (need to find the reference)
	float NMEA::GpsToDecimal(const char* nmeaPos, char quadrant, bool& error) {
		float v = 0;
		if (strlen(nmeaPos) > 5) {
			char integerPart[3 + 1];
			int digitCount = (nmeaPos[4] == '.' ? 2 : 3);
			memcpy(integerPart, nmeaPos, digitCount);
			integerPart[digitCount] = 0;
			nmeaPos += digitCount;

			int degrees = atoi(integerPart);
			if (degrees == 0 && integerPart[0] != '0') {
				error |= true;
				return 0;
			}

			float minutes = atof(nmeaPos);
			if (minutes == 0 && nmeaPos[0] != '0') {
				error |= true;
				return 0;
			}

			v = degrees + minutes / 60.0;
			if (quadrant == 'W' || quadrant == 'S')
				v = -v;
		}
		return v;
	}

	bool NMEA::processGGA(const std::string& s, TAG& tag, long t) {

		split(s);

		GPS gps;
		bool error = false;
		gps.lat = GpsToDecimal(parts[2].c_str(), parts[3][0], error);
		gps.lon = GpsToDecimal(parts[4].c_str(), parts[5][0], error);

		if (error) return false;
		outGPS.Send(&gps, 1, tag);
		std::cerr << "GPS: lat = " << gps.lat << ", lon = " << gps.lon << std::endl;

		return true;
	}

	bool NMEA::processAIS(const std::string& s, TAG& tag, long t) {

		split(s);

		if (parts.size() != 7) return false;

		if (parts[0].size() != 6) return false;
		if (parts[0][0] != '$' && parts[0][0] != '!') return false;

		char last = parts[0][1];
		char c = parts[0][2];

		aivdm.talkerID = ((parts[0][1] << 8) | parts[0][2]);
		if (parts[1].size() != 1) return false;

		aivdm.count = parts[1][0] - '0';
		if (parts[2].size() != 1) return false;

		aivdm.number = parts[2][0] - '0';
		if (parts[3].size() > 1) return false;

		aivdm.ID = parts[3].size() > 0 ? parts[3][0] - '0' : 0;
		if (parts[4].size() > 1) return false;

		aivdm.channel = parts[4].size() > 0 ? parts[4][0] : '?';

		for (auto c : parts[5])
			if (!isNMEAchar(c)) return false;
		aivdm.data = parts[5];

		if (parts[6].size() != 4) return false;
		aivdm.fillbits = parts[6][0] - '0';
		if (!isHEX(parts[6][2]) || !isHEX(parts[6][3])) return false;
		aivdm.checksum = (fromHEX(parts[6][2]) << 4) | fromHEX(parts[6][3]);

		aivdm.sentence = s;

		submitAIS(tag, t);

		return true;
	}

	void NMEA::processJSONsentence(std::string s, TAG& tag, long t) {
		if (s[0] == '{') {
			try {
				JSON::Parser parser(&AIS::KeyMap, JSON_DICT_FULL);
				std::shared_ptr<JSON::JSON> j = parser.parse(s);

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
							processAIS(v.getString(), tag, t);
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
				if (state == 0) {
					if (c == '{' && (prev == '\0' || prev == '\n' || prev == '\r')) {
						line = c;
						state = 1;
					}
					else if (c == '$' || c == '!') {
						line = c;
						state = 2;
					}
					prev = c;
					continue;
				}

				if (line.size() > 1024 || c == '\n' || c == '\r') {
					state = 0;
					line.clear();
					prev = c;
					continue;
				}

				line += c;

				if (state == 1 && c == '}') {
					processJSONsentence(line, tag, t);
					state = 0;
					line.clear();
					prev = c;
				}
				else if (state == 2 && line.size() > 5 && isHEX(line[line.size() - 1]) && isHEX(line[line.size() - 2]) && line[line.size() - 3] == '*' &&
						 ((isdigit(line[line.size() - 4]) && line[line.size() - 5] == ',') ||
						  (line[line.size() - 4] == ','))) {
					reset();

					if (line.size() > 6) {
						std::string type = line.substr(3, 3);

						if (type == "VDM") processAIS(line, tag, t);
						if (type == "GGA") processGGA(line, tag, t);


						state = 0;
						line.clear();
					}
				}
				prev = c;
			}
		}
	}
}