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

	void NMEA::reset(char c) {
		state = 0;
		line.clear();
		prev = c;
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

			if (msg.validate()) {
				if (regenerate)
					msg.buildNMEA(tag);
				else
					msg.NMEA.push_back(aivdm.sentence);
				Send(&msg, 1, tag);
			}
			else
				std::cerr << "NMEA: invalid message of type " << msg.type() << " and length " << msg.getLength() << std::endl;
			return;
		}

		int result = search(aivdm);

		if (aivdm.number != result + 1 || result == -1) {
			clean(aivdm.channel, aivdm.talkerID);
			if (aivdm.number != 1) {
				std::cerr << "NMEA: missing part of multiline message [" << aivdm.sentence << "]" << std::endl;
				return;
			}
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

		if (msg.validate()) {
			if (regenerate)
				msg.buildNMEA(tag, aivdm.ID);

			Send(&msg, 1, tag);
		}
		else
			std::cerr << "NMEA: invalid message of type " << msg.type() << " and length " << msg.getLength() << std::endl;

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

	std::string NMEA::trim(const std::string& s) {
		std::string r;
		for (char c : s)
			if (c != ' ') r += c;
		return r;
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
		bool error = false;

		split(s);

		if (parts.size() != 15) return false;

		const std::string& crc = parts[14];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line)) {
			std::cerr << "NMEA: incorrect checksum [" << line << "]." << std::endl;
			if (crc_check) return false;
		}

		// no proper fix
		int fix = atoi(parts[6].c_str());
		if (fix != 1 && fix != 2) {
			std::cerr << "NMEA: no fix in GPGGA NMEA:" << parts[6] << std::endl;
			return false;
		}

		GPS gps;
		gps.lat = GpsToDecimal(trim(parts[2]).c_str(), trim(parts[3])[0], error);
		gps.lon = GpsToDecimal(trim(parts[4]).c_str(), trim(parts[5])[0], error);

		if (error) return false;

		outGPS.Send(&gps, 1, tag);
		std::cerr << "GGA: lat = " << gps.lat << ", lon = " << gps.lon << std::endl;

		return true;
	}

	bool NMEA::processRMC(const std::string& s, TAG& tag, long t) {
		bool error = false;

		split(s);

		if (parts.size() != 13 && parts.size() != 12) return false;

		const std::string& crc = parts[parts.size()-1];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line)) {
			std::cerr << "NMEA: incorrect checksum [" << line << "]." << std::endl;
			if (crc_check) return false;
		}

		GPS gps;
		gps.lat = GpsToDecimal(trim(parts[3]).c_str(), trim(parts[4])[0], error);
		gps.lon = GpsToDecimal(trim(parts[5]).c_str(), trim(parts[6])[0], error);

		if (error) return false;

		outGPS.Send(&gps, 1, tag);
		std::cerr << "RMC: lat = " << gps.lat << ", lon = " << gps.lon << std::endl;

		return true;
	}

	bool NMEA::processGLL(const std::string& s, TAG& tag, long t) {
		bool error = false;

		split(s);

		if (parts.size() != 8) return false;

		const std::string& crc = parts[7];
		int checksum = crc.size() > 2 ? (fromHEX(crc[crc.length() - 2]) << 4) | fromHEX(crc[crc.length() - 1]) : -1;

		if (checksum != NMEAchecksum(line)) {
			std::cerr << "NMEA: incorrect checksum [" << line << "]." << std::endl;
			if (crc_check) return false;
		}

		GPS gps;
		gps.lat = GpsToDecimal(parts[1].c_str(), parts[2][0], error);
		gps.lon = GpsToDecimal(parts[3].c_str(), parts[4][0], error);

		if (error) return false;

		outGPS.Send(&gps, 1, tag);
		std::cerr << "GLL: lat = " << gps.lat << ", lon = " << gps.lon << std::endl;

		return true;
	}

	bool NMEA::processAIS(const std::string& s, TAG& tag, long t) {

		split(s);
		aivdm.reset();

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
				parser.setSkipUnknown(true);
				std::shared_ptr<JSON::JSON> j = parser.parse(s);

				std::string cls = "";
				std::string dev = "";

				tag.ppm = 0;
				tag.sample_lvl = 0;
				t = 0;

				// phase 1, get the meta data in place
				for (const auto& p : j->getProperties()) {
					switch (p.Key()) {
					case AIS::KEY_CLASS:
						cls = p.Get().getString();
						break;
					case AIS::KEY_DEVICE:
						dev = p.Get().getString();
						break;
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

				if(cls == "AIS" && dev == "AIS-catcher") {
					for (const auto& p : j->getProperties()) {
						if (p.Key() == AIS::KEY_NMEA) {
							if(p.Get().isArray()) {
								for (const auto& v : p.Get().getArray()) {
									processAIS(v.getString(), tag, t);
								}
							}
						}
					}
				}

				if(cls == "TPV") {
					GPS gps;
					
					for (const auto& p : j->getProperties()) {
						if (p.Key() == AIS::KEY_LAT) {
							if(p.Get().isFloat()) {
								gps.lat = p.Get().getFloat();
							}
							else if(p.Get().isInt()) {
								gps.lat = p.Get().getInt();
							}
						}
						else if (p.Key() == AIS::KEY_LON) {
							if(p.Get().isFloat()) {
								gps.lon = p.Get().getFloat();
							}
							else if(p.Get().isInt()) {
								gps.lon = p.Get().getInt();
							}
						}
					}

					if (gps.lat != 0 || gps.lon != 0) { 

						outGPS.Send(&gps, 1, tag);
						std::cerr << "JSON: lat = " << gps.lat << ", lon = " << gps.lon << std::endl;
					}
				}


			}
			catch (std::exception const& e) {
				std::cout << "NMEA model: " << e.what() << std::endl;
			}
		}
	}
	// continue collection of full NMEA line in `sentence` and store location of commas in 'locs'
	void NMEA::Receive(const RAW* data, int len, TAG& tag) {

		long t = 0;

		for (int j = 0; j < len; j++) {
			for (int i = 0; i < data[j].size; i++) {
				char c = ((char*)(data[j].data))[i];

				// state = 0, we are looking for the start of JSON or NMEA
				if (state == 0) {
					if (c == '{' && (prev == '\n' || prev == '\r')) {
						line = c;
						state = 1;
						count = 1;
					}
					else if (c == '$' || c == '!') {
						line = c;
						state = 2;
					}
					prev = c;
					continue;
				}

				bool newline = c == '\r' || c == '\n' || c == '\t' || c == '\0';

				if (!newline) line += c;
				prev = c;

				// state = 1 (JSON) or state = 2 (NMEA)
				if (state == 1) {
					// we do not allow nested JSON, so processing until newline character or '}'
					if (c == '{') count++;
					else if (c == '}') {
						--count;
						if (!count) {
							t = 0;
							tag.clear();
							processJSONsentence(line, tag, t);
							reset(c);
						}
					}
					else if (newline) {
						std::cerr << "NMEA: newline in uncompleted JSON input not allowed";
						reset(c);
					}
				}
				else if (state == 2) {
					// end of line if we find a checksum or a newline character
					bool isNMEA = line.size() > 10 && (line[3] == 'V' && line[4] == 'D' && line[5] == 'M');
					bool checksum = isNMEA && (isHEX(line[line.size() - 1]) && isHEX(line[line.size() - 2]) && line[line.size() - 3] == '*' &&
											   ((isdigit(line[line.size() - 4]) && line[line.size() - 5] == ',') || (line[line.size() - 4] == ',')));

					if (((isNMEA && checksum) || newline) && line.size() > 6) {
						std::string type = line.substr(3, 3);
						bool noerror = true;
						tag.clear();
						t = 0;

						if (type == "VDM") noerror &= processAIS(line, tag, t);
						if (type == "GGA") noerror &= processGGA(line, tag, t);
						if (type == "RMC") noerror &= processRMC(line, tag, t);
						if (type == "GLL") noerror &= processGLL(line, tag, t);

						if (!noerror) {
							std::cerr << "NMEA: error processing NMEA line " << line << std::endl;
						}
						reset(c);
					}
				}

				if (line.size() > 1024) reset(c);
			}
		}
	}
}