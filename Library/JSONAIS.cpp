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

#include "AIS.h"

// Sources:
//	https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-0-199811-S!!PDF-E.pdf
//	https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
// 	https://gpsd.gitlab.io/gpsd/AIVDM.html

#include "JSONAIS.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

// Below is a direct translation (more or less) of https://gpsd.gitlab.io/gpsd/AIVDM.html

namespace AIS {

	struct COUNTRY {
		uint32_t MID;
		std::string country;
		std::string code;
	};

	extern const std::vector<std::string> JSON_MAP_STATUS;
	extern const std::vector<std::string> JSON_MAP_EPFD;
	extern const std::vector<std::string> JSON_MAP_SHIPTYPE;
	extern const std::vector<std::string> JSON_MAP_AID_TYPE;
	extern const std::vector<COUNTRY> JSON_MAP_MID;

	void JSONAIS::U(const AIS::Message& msg, int p, int start, int len, unsigned undefined) {
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			json.Add(p, (int)u);
	}

	void JSONAIS::UL(const AIS::Message& msg, int p, int start, int len, float a, float b, unsigned undefined) {
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			json.Add(p, u * a + b);
	}

	void JSONAIS::S(const AIS::Message& msg, int p, int start, int len, int undefined) {
		int u = msg.getInt(start, len);
		if (u != undefined)
			json.Add(p, u);
	}

	void JSONAIS::SL(const AIS::Message& msg, int p, int start, int len, float a, float b, int undefined) {
		int s = msg.getInt(start, len);
		if (s != undefined)
			json.Add(p, s * a + b);
	}

	void JSONAIS::E(const AIS::Message& msg, int p, int start, int len, int pmap, const std::vector<std::string>* map) {
		unsigned u = msg.getUint(start, len);
		json.Add(p, (int)u);
		if (map) {
			if (u < map->size())
				json.Add(pmap, &(*map)[u]);
			else
				json.Add(pmap, &undefined);
		}
	}

	void JSONAIS::TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined) {
		int u = msg.getInt(start, len);
		json.Add(AIS::KEY_TURN_UNSCALED, u);

		if (u == -128)
			// json.Add(p, &nan);
			json.Add(p, (int)-128);
		else if (u == -127)
			// json.Add(p, &fastleft);
			json.Add(p, (int)-127);
		else if (u == 127)
			// json.Add(p, &fastright);
			json.Add(p, (int)127);
		else {
			double rot = u / 4.733;
			rot = (u < 0) ? -rot * rot : rot * rot;
			json.Add(p, (int)(rot + 0.5));
		}
	}

	void JSONAIS::B(const AIS::Message& msg, int p, int start, int len) {
		unsigned u = msg.getUint(start, len);
		json.Add(p, (bool)u);
	}

	void JSONAIS::TIMESTAMP(const AIS::Message& msg, int p, int start, int len, std::string& str) {
		if (len != 40) return;

		std::stringstream s;
		s << std::setfill('0') << std::setw(4) << msg.getUint(start, 14) << "-" << std::setw(2) << msg.getUint(start + 14, 4) << "-" << std::setw(2) << msg.getUint(start + 18, 5) << "T"
		  << std::setw(2) << msg.getUint(start + 23, 5) << ":" << std::setw(2) << msg.getUint(start + 28, 6) << ":" << std::setw(2) << msg.getUint(start + 34, 6) << "Z";
		str = std::string(s.str());
		json.Add(p, &str);
	}

	void JSONAIS::ETA(const AIS::Message& msg, int p, int start, int len, std::string& str) {
		if (len != 20) return;

		std::stringstream s;
		s << std::setfill('0') << std::setw(2) << msg.getUint(start, 4) << "-" << std::setw(2) << msg.getUint(start + 4, 5) << "T"
		  << std::setw(2) << msg.getUint(start + 9, 5) << ":" << std::setw(2) << msg.getUint(start + 14, 6) << "Z";
		str = std::string(s.str());
		json.Add(p, &str);
	}


	void JSONAIS::T(const AIS::Message& msg, int p, int start, int len, std::string& str) {
		msg.getText(start, len, str);
		while (!str.empty() && str[str.length() - 1] == ' ') str.resize(str.length() - 1);
		json.Add(p, &str);
	}

	void JSONAIS::D(const AIS::Message& msg, int p, int start, int len, std::string& str) {
		str = std::to_string(len) + ':';
		for (int i = start; i < start + len; i += 4) {
			char c = msg.getUint(i, 4);
			str += (char)(c < 10 ? c + '0' : c + 'a' - 10);
		}
		json.Add(p, &str);
	}

	// Refernce: https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.585-9-202205-I!!PDF-E.pdf
	void JSONAIS::COUNTRY(const AIS::Message& msg) {

		uint32_t mid = msg.mmsi();
		while (mid > 1000) mid /= 10;

		if (mid == 111) {
			mid = msg.mmsi();
			while (mid > 1000000) mid /= 10;
			mid %= 1000;
		}
		else if (mid / 10 == 99) {
			mid = msg.mmsi();
			while (mid > 100000) mid /= 10;
			mid %= 1000;
		}

		if (mid > 100) {
			int l = 0, r = JSON_MAP_MID.size() - 1;
			while (l != r) {
				int m = (l + r + 1) / 2;
				if (JSON_MAP_MID[m].MID > mid)
					r = m - 1;
				else
					l = m;
			}
			if (JSON_MAP_MID[l].MID == mid) {
				json.Add(AIS::KEY_COUNTRY, &JSON_MAP_MID[l].country);
				json.Add(AIS::KEY_COUNTRY_CODE, &JSON_MAP_MID[l].code);
			}
		}
	}

	void JSONAIS::Receive(const AIS::Message* data, int len, TAG& tag) {
		for (int i = 0; i < len; i++) {
			json.clear();
			ProcessMsg(data[i], tag);
			json.binary = (void*)&data[i];
			Send(&json, 1, tag);
		}
	}

	void JSONAIS::ProcessMsg6Data(const AIS::Message& msg) {
		int dac = msg.getUint(72, 10);
		int fid = msg.getUint(82, 6);

		if (dac == 235 && fid == 10) {
			UL(msg, AIS::KEY_ANA_INT, 88, 10, 0.05, 0);
			UL(msg, AIS::KEY_ANA_EXT1, 98, 10, 0.05, 0);
			UL(msg, AIS::KEY_ANA_EXT2, 108, 10, 0.05, 0);
			U(msg, AIS::KEY_RACON, 118, 2);
			U(msg, AIS::KEY_HEALTH, 122, 1);
			U(msg, AIS::KEY_STAT_EXT, 123, 8);
			B(msg, AIS::KEY_OFF_POSITION, 131, 1);
		}
		else
			D(msg, AIS::KEY_DATA, 88, MIN(920, msg.getLength() - 88), datastring);
	}

	void JSONAIS::ProcessMsg8Data(const AIS::Message& msg) {
		int dac = msg.getUint(40, 10);
		int fid = msg.getUint(50, 6);

		// ECE/TRANS/SC.3/176
		if (dac == 200 && fid == 10) {
			T(msg, AIS::KEY_VIN, 56, 48, text);
			UL(msg, AIS::KEY_LENGTH, 104, 13, 0.1, 0);
			UL(msg, AIS::KEY_BEAM, 117, 10, 0.1, 0);
			E(msg, AIS::KEY_SHIPTYPE, 127, 14);
			E(msg, AIS::KEY_HAZARD, 141, 3);
			UL(msg, AIS::KEY_DRAUGHT, 144, 11, 0.01, 0);
			E(msg, AIS::KEY_LOADED, 155, 2);
			B(msg, AIS::KEY_SPEED_Q, 157, 1);
			B(msg, AIS::KEY_COURSE_Q, 158, 1);
			B(msg, AIS::KEY_HEADING_Q, 159, 1);
		}
		else if (dac == 1 && fid == 31) {
			// Sources: http://vislab-ccom.unh.edu/~schwehr/papers/2010-IMO-SN.1-Circ.289.pdf, GPSDECODE
			SL(msg, AIS::KEY_LON, 56, 25, 1 / 60000.0, 0);
			SL(msg, AIS::KEY_LAT, 81, 24, 1 / 60000.0, 0);
			B(msg, AIS::KEY_ACCURACY, 105, 1);
			U(msg, AIS::KEY_DAY, 106, 5, 0);
			U(msg, AIS::KEY_HOUR, 111, 5, 24);
			U(msg, AIS::KEY_MINUTE, 116, 6, 60);
			U(msg, AIS::KEY_WSPEED, 122, 7, 127);
			U(msg, AIS::KEY_WGUST, 129, 7, 127);
			U(msg, AIS::KEY_WDIR, 136, 9, 360);
			U(msg, AIS::KEY_WGUSTDIR, 145, 9, 360);
			SL(msg, AIS::KEY_AIRTEMP, 154, 11, 0.1, 0, -1024);
			U(msg, AIS::KEY_HUMIDITY, 165, 7, 101);
			SL(msg, AIS::KEY_DEWPOINT, 172, 10, 0.1, 0, 501);
			UL(msg, AIS::KEY_PRESSURE, 182, 9, 1, 799, 511);
			U(msg, AIS::KEY_PRESSURETEND, 191, 2, 3);
			U(msg, AIS::KEY_VISGREATER, 193, 1);
			UL(msg, AIS::KEY_VISIBILITY, 194, 7, 0.1, 0);
			UL(msg, AIS::KEY_WATERLEVEL, 201, 12, 0.01, -10, 4002);
			U(msg, AIS::KEY_LEVELTREND, 213, 2, 3);
			UL(msg, AIS::KEY_CSPEED, 215, 8, 0.1, 0, 255);
			U(msg, AIS::KEY_CDIR, 223, 9, 360);
			UL(msg, AIS::KEY_CSPEED2, 232, 8, 0.1, 0);
			U(msg, AIS::KEY_CDIR2, 240, 9);
			U(msg, AIS::KEY_CDEPTH2, 249, 5);
			UL(msg, AIS::KEY_CSPEED3, 254, 8, 0.1, 0);
			U(msg, AIS::KEY_CDIR3, 262, 9);
			U(msg, AIS::KEY_CDEPTH3, 271, 5);
			UL(msg, AIS::KEY_WAVEHEIGHT, 276, 8, 0.1, 0);
			U(msg, AIS::KEY_WAVEPERIOD, 284, 6);
			U(msg, AIS::KEY_WAVEDIR, 290, 9);
			UL(msg, AIS::KEY_SWELLHEIGHT, 299, 8, 0.1, 0);
			U(msg, AIS::KEY_SWELLPERIOD, 307, 6);
			U(msg, AIS::KEY_SWELLDIR, 313, 9);
			U(msg, AIS::KEY_SEASTATE, 322, 4);
			//SL(msg, AIS::KEY_WATERTEMP, 326, 10, 0.1, 0, 501);
			U(msg, AIS::KEY_PRECIPTYPE, 336, 3, 7);
			U(msg, AIS::KEY_SALINITY, 339, 9, 510);
			U(msg, AIS::KEY_ICE, 348, 2, 3);
		}
	}

	void JSONAIS::ProcessMsg(const AIS::Message& msg, TAG& tag) {

		rxtime = msg.getRxTime();
		channel = std::string(1, msg.getChannel());

		json.Add(AIS::KEY_CLASS, &class_str);
		json.Add(AIS::KEY_DEVICE, &device);

		if (tag.mode & 2)
			json.Add(AIS::KEY_RXTIME, &rxtime);

		json.Add(AIS::KEY_SCALED, true);
		json.Add(AIS::KEY_CHANNEL, &channel);
		json.Add(AIS::KEY_NMEA, &msg.NMEA);

		if (tag.mode & 1) {
			json.Add(AIS::KEY_SIGNAL_POWER, tag.level);
			json.Add(AIS::KEY_PPM, tag.ppm);
		}

		U(msg, AIS::KEY_TYPE, 0, 6);
		U(msg, AIS::KEY_REPEAT, 6, 2);
		U(msg, AIS::KEY_MMSI, 8, 30);

		if (tag.mode & 4) {
			COUNTRY(msg);
		}

		switch (msg.type()) {
		case 1:
		case 2:
		case 3:
			E(msg, AIS::KEY_STATUS, 38, 4, AIS::KEY_STATUS_TEXT, &JSON_MAP_STATUS);
			TURN(msg, AIS::KEY_TURN, 42, 8);
			UL(msg, AIS::KEY_SPEED, 50, 10, 0.1, 0, 1023);
			B(msg, AIS::KEY_ACCURACY, 60, 1);
			SL(msg, AIS::KEY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 89, 27, 1 / 600000.0, 0);
			UL(msg, AIS::KEY_COURSE, 116, 12, 0.1, 0);
			U(msg, AIS::KEY_HEADING, 128, 9 /*, 511*/);
			U(msg, AIS::KEY_SECOND, 137, 6);
			E(msg, AIS::KEY_MANEUVER, 143, 2);
			X(msg, AIS::KEY_SPARE, 145, 3);
			B(msg, AIS::KEY_RAIM, 148, 1);
			U(msg, AIS::KEY_RADIO, 149, MIN(19, MAX(msg.getLength() - 149, 0)));
			break;
		case 4:
		case 11:
			TIMESTAMP(msg, AIS::KEY_TIMESTAMP, 38, 40, timestamp);
			U(msg, AIS::KEY_YEAR, 38, 14, 0);
			U(msg, AIS::KEY_MONTH, 52, 4, 0);
			U(msg, AIS::KEY_DAY, 56, 5, 0);
			U(msg, AIS::KEY_HOUR, 61, 5, 24);
			U(msg, AIS::KEY_MINUTE, 66, 6, 60);
			U(msg, AIS::KEY_SECOND, 72, 6, 60);
			B(msg, AIS::KEY_ACCURACY, 78, 1);
			SL(msg, AIS::KEY_LON, 79, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 107, 27, 1 / 600000.0, 0);
			E(msg, AIS::KEY_EPFD, 134, 4, AIS::KEY_EPFD_TEXT, &JSON_MAP_EPFD);
			X(msg, AIS::KEY_SPARE, 138, 10);
			B(msg, AIS::KEY_RAIM, 148, 1);
			U(msg, AIS::KEY_RADIO, 149, 19);
			break;
		case 5:
			U(msg, AIS::KEY_AIS_VERSION, 38, 2);
			U(msg, AIS::KEY_IMO, 40, 30);
			T(msg, AIS::KEY_CALLSIGN, 70, 42, callsign);
			T(msg, AIS::KEY_SHIPNAME, 112, 120, shipname);
			E(msg, AIS::KEY_SHIPTYPE, 232, 8, AIS::KEY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
			U(msg, AIS::KEY_TO_BOW, 240, 9);
			U(msg, AIS::KEY_TO_STERN, 249, 9);
			U(msg, AIS::KEY_TO_PORT, 258, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 264, 6);
			E(msg, AIS::KEY_EPFD, 270, 4, AIS::KEY_EPFD_TEXT, &JSON_MAP_EPFD);
			ETA(msg, AIS::KEY_ETA, 274, 20, eta);
			U(msg, AIS::KEY_MONTH, 274, 4, 0);
			U(msg, AIS::KEY_DAY, 278, 5, 0);
			U(msg, AIS::KEY_HOUR, 283, 5, 24);
			U(msg, AIS::KEY_MINUTE, 288, 6, 60);
			UL(msg, AIS::KEY_DRAUGHT, 294, 8, 0.1, 0);
			T(msg, AIS::KEY_DESTINATION, 302, 120, destination);
			B(msg, AIS::KEY_DTE, 422, 1);
			X(msg, AIS::KEY_SPARE, 423, 1);
			break;
		case 6:
			U(msg, AIS::KEY_SEQNO, 38, 2);
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			B(msg, AIS::KEY_RETRANSMIT, 70, 1);
			X(msg, AIS::KEY_SPARE, 71, 1);
			U(msg, AIS::KEY_DAC, 72, 10);
			U(msg, AIS::KEY_FID, 82, 6);
			ProcessMsg6Data(msg);
			break;
		case 7:
		case 13:
			X(msg, AIS::KEY_SPARE, 38, 2);
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_MMSISEQ1, 70, 2);
			if (msg.getLength() <= 72) break;
			U(msg, AIS::KEY_MMSI2, 72, 30);
			U(msg, AIS::KEY_MMSISEQ2, 102, 2);
			if (msg.getLength() <= 104) break;
			U(msg, AIS::KEY_MMSI3, 104, 30);
			U(msg, AIS::KEY_MMSISEQ3, 134, 2);
			if (msg.getLength() <= 136) break;
			U(msg, AIS::KEY_MMSI4, 136, 30);
			U(msg, AIS::KEY_MMSISEQ4, 166, 2);
			break;
		case 8:
			U(msg, AIS::KEY_DAC, 40, 10);
			U(msg, AIS::KEY_FID, 50, 6);
			ProcessMsg8Data(msg);
			break;
		case 9:
			U(msg, AIS::KEY_ALT, 38, 12);
			U(msg, AIS::KEY_SPEED, 50, 10);
			B(msg, AIS::KEY_ACCURACY, 60, 1);
			SL(msg, AIS::KEY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 89, 27, 1 / 600000.0, 0);
			UL(msg, AIS::KEY_COURSE, 116, 12, 0.1, 0);
			U(msg, AIS::KEY_SECOND, 128, 6);
			U(msg, AIS::KEY_REGIONAL, 134, 8);
			B(msg, AIS::KEY_DTE, 142, 1);
			B(msg, AIS::KEY_ASSIGNED, 146, 1);
			B(msg, AIS::KEY_RAIM, 147, 1);
			U(msg, AIS::KEY_RADIO, 148, 20);
			break;
		case 10:
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			break;
		case 12:
			U(msg, AIS::KEY_SEQNO, 38, 2);
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			B(msg, AIS::KEY_RETRANSMIT, 70, 1);
			T(msg, AIS::KEY_TEXT, 72, 936, text);
			break;
		case 14:
			T(msg, AIS::KEY_TEXT, 40, 968, text);
			break;
		case 15:
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_TYPE1_1, 70, 6);
			U(msg, AIS::KEY_OFFSET1_1, 76, 12);
			if (msg.getLength() <= 90) break;
			U(msg, AIS::KEY_TYPE1_2, 90, 6);
			U(msg, AIS::KEY_OFFSET1_2, 96, 12);
			if (msg.getLength() <= 110) break;
			U(msg, AIS::KEY_MMSI2, 110, 30);
			U(msg, AIS::KEY_TYPE2_1, 140, 6);
			U(msg, AIS::KEY_OFFSET2_1, 146, 12);
			break;
		case 16:
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_OFFSET1, 70, 12);
			U(msg, AIS::KEY_INCREMENT1, 82, 10);
			if (msg.getLength() == 92) break;
			U(msg, AIS::KEY_MMSI2, 92, 30);
			U(msg, AIS::KEY_OFFSET2, 122, 12);
			U(msg, AIS::KEY_INCREMENT2, 134, 10);
			break;
		case 17:
			SL(msg, AIS::KEY_LON, 40, 18, 1 / 600.0, 0);
			SL(msg, AIS::KEY_LAT, 58, 17, 1 / 600.0, 0);
			D(msg, AIS::KEY_DATA, 80, MIN(736, msg.getLength() - 80), datastring);
			break;
		case 18:
			UL(msg, AIS::KEY_SPEED, 46, 10, 0.1, 0);
			B(msg, AIS::KEY_ACCURACY, 56, 1);
			SL(msg, AIS::KEY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, AIS::KEY_COURSE, 112, 12, 0.1, 0);
			U(msg, AIS::KEY_HEADING, 124, 9);
			U(msg, AIS::KEY_RESERVED, 38, 8);
			U(msg, AIS::KEY_SECOND, 133, 6);
			U(msg, AIS::KEY_REGIONAL, 139, 2);
			B(msg, AIS::KEY_CS, 141, 1);
			B(msg, AIS::KEY_DISPLAY, 142, 1);
			B(msg, AIS::KEY_DSC, 143, 1);
			B(msg, AIS::KEY_BAND, 144, 1);
			B(msg, AIS::KEY_MSG22, 145, 1);
			B(msg, AIS::KEY_ASSIGNED, 146, 1);
			B(msg, AIS::KEY_RAIM, 147, 1);
			U(msg, AIS::KEY_RADIO, 148, 20);
			break;
		case 19:
			UL(msg, AIS::KEY_SPEED, 46, 10, 0.1, 0);
			SL(msg, AIS::KEY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, AIS::KEY_COURSE, 112, 12, 0.1, 0);
			U(msg, AIS::KEY_HEADING, 124, 9);
			T(msg, AIS::KEY_SHIPNAME, 143, 120, shipname);
			E(msg, AIS::KEY_SHIPTYPE, 263, 8, AIS::KEY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
			U(msg, AIS::KEY_TO_BOW, 271, 9);
			U(msg, AIS::KEY_TO_STERN, 280, 9);
			U(msg, AIS::KEY_TO_PORT, 289, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 295, 6);
			E(msg, AIS::KEY_EPFD, 301, 4, AIS::KEY_EPFD_TEXT, &JSON_MAP_EPFD);
			B(msg, AIS::KEY_ACCURACY, 56, 1);
			U(msg, AIS::KEY_RESERVED, 38, 8);
			U(msg, AIS::KEY_SECOND, 133, 6);
			U(msg, AIS::KEY_REGIONAL, 139, 4);
			B(msg, AIS::KEY_RAIM, 305, 1);
			B(msg, AIS::KEY_DTE, 306, 1);
			B(msg, AIS::KEY_ASSIGNED, 307, 1);
			X(msg, AIS::KEY_SPARE, 308, 4);
			break;
		case 20:
			U(msg, AIS::KEY_OFFSET1, 40, 12);
			U(msg, AIS::KEY_NUMBER1, 52, 4);
			U(msg, AIS::KEY_TIMEOUT1, 56, 3);
			U(msg, AIS::KEY_INCREMENT1, 59, 11);
			if (msg.getLength() <= 99) break;
			U(msg, AIS::KEY_OFFSET2, 70, 12);
			U(msg, AIS::KEY_NUMBER2, 82, 4);
			U(msg, AIS::KEY_TIMEOUT2, 86, 3);
			U(msg, AIS::KEY_INCREMENT2, 89, 11);
			if (msg.getLength() <= 129) break;
			U(msg, AIS::KEY_OFFSET3, 100, 12);
			U(msg, AIS::KEY_NUMBER3, 112, 4);
			U(msg, AIS::KEY_TIMEOUT3, 116, 3);
			U(msg, AIS::KEY_INCREMENT3, 119, 11);
			if (msg.getLength() <= 159) break;
			U(msg, AIS::KEY_OFFSET4, 130, 12);
			U(msg, AIS::KEY_NUMBER4, 142, 4);
			U(msg, AIS::KEY_TIMEOUT4, 146, 3);
			U(msg, AIS::KEY_INCREMENT4, 149, 11);
			break;
		case 21:
			E(msg, AIS::KEY_AID_TYPE, 38, 5, AIS::KEY_AID_TYPE_TEXT, &JSON_MAP_AID_TYPE);
			T(msg, AIS::KEY_NAME, 43, 120, name);
			B(msg, AIS::KEY_ACCURACY, 163, 1);
			SL(msg, AIS::KEY_LON, 164, 28, 1 / 600000.0, 0);
			SL(msg, AIS::KEY_LAT, 192, 27, 1 / 600000.0, 0);
			U(msg, AIS::KEY_TO_BOW, 219, 9);
			U(msg, AIS::KEY_TO_STERN, 228, 9);
			U(msg, AIS::KEY_TO_PORT, 237, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 243, 6);
			E(msg, AIS::KEY_EPFD, 249, 4, AIS::KEY_EPFD_TEXT, &JSON_MAP_EPFD);
			U(msg, AIS::KEY_SECOND, 253, 6);
			B(msg, AIS::KEY_OFF_POSITION, 259, 1);
			U(msg, AIS::KEY_REGIONAL, 260, 8);
			B(msg, AIS::KEY_RAIM, 268, 1);
			B(msg, AIS::KEY_VIRTUAL_AID, 269, 1);
			B(msg, AIS::KEY_ASSIGNED, 270, 1);
			break;
		case 22:
			U(msg, AIS::KEY_CHANNEL_A, 40, 12);
			U(msg, AIS::KEY_CHANNEL_B, 52, 12);
			U(msg, AIS::KEY_TXRX, 64, 4);
			B(msg, AIS::KEY_POWER, 68, 1);
			if (msg.getUint(139, 1)) {
				U(msg, AIS::KEY_DEST1, 69, 30);	 // check // if addressed is 1
				U(msg, AIS::KEY_DEST2, 104, 30); // check
			}
			else {
				SL(msg, AIS::KEY_NE_LON, 69, 18, 1.0 / 600.0, 0);
				SL(msg, AIS::KEY_NE_LAT, 87, 17, 1.0 / 600.0, 0);
				SL(msg, AIS::KEY_SW_LON, 104, 18, 1.0 / 600.0, 0);
				SL(msg, AIS::KEY_SW_LAT, 122, 17, 1.0 / 600.0, 0);
			}
			B(msg, AIS::KEY_ADDRESSED, 139, 1);
			B(msg, AIS::KEY_BAND_A, 140, 1);
			B(msg, AIS::KEY_BAND_B, 141, 1);
			U(msg, AIS::KEY_ZONESIZE, 142, 3);
			break;
		case 23:
			U(msg, AIS::KEY_NE_LON, 40, 18);
			U(msg, AIS::KEY_NE_LAT, 58, 17);
			U(msg, AIS::KEY_SW_LON, 75, 18);
			U(msg, AIS::KEY_SW_LAT, 93, 17);
			E(msg, AIS::KEY_STATION_TYPE, 110, 4);
			E(msg, AIS::KEY_SHIP_TYPE, 114, 8);
			U(msg, AIS::KEY_TXRX, 144, 2);
			E(msg, AIS::KEY_INTERVAL, 146, 4);
			U(msg, AIS::KEY_QUIET, 150, 4);
			break;
		case 24:
			U(msg, AIS::KEY_PARTNO, 38, 2);

			if (msg.getUint(38, 2) == 0) {
				T(msg, AIS::KEY_SHIPNAME, 40, 120, shipname);
			}
			else {
				E(msg, AIS::KEY_SHIPTYPE, 40, 8, AIS::KEY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
				T(msg, AIS::KEY_VENDORID, 48, 18, vendorid);
				U(msg, AIS::KEY_MODEL, 66, 4);
				U(msg, AIS::KEY_SERIAL, 70, 20);
				T(msg, AIS::KEY_CALLSIGN, 90, 42, callsign);
				if (msg.mmsi() / 10000000 == 98) {
					U(msg, AIS::KEY_MOTHERSHIP_MMSI, 132, 30);
				}
				else {
					U(msg, AIS::KEY_TO_BOW, 132, 9);
					U(msg, AIS::KEY_TO_STERN, 141, 9);
					U(msg, AIS::KEY_TO_PORT, 150, 6);
					U(msg, AIS::KEY_TO_STARBOARD, 156, 6);
				}
			}
			break;
		case 27:
			U(msg, AIS::KEY_ACCURACY, 38, 1);
			U(msg, AIS::KEY_RAIM, 39, 1);
			E(msg, AIS::KEY_STATUS, 40, 4, AIS::KEY_STATUS_TEXT, &JSON_MAP_STATUS);
			SL(msg, AIS::KEY_LON, 44, 18, 1 / 600.0, 0);
			SL(msg, AIS::KEY_LAT, 62, 17, 1 / 600.0, 0);
			U(msg, AIS::KEY_SPEED, 79, 6);
			U(msg, AIS::KEY_COURSE, 85, 9);
			U(msg, AIS::KEY_GNSS, 94, 1);
			break;
		default:
			break;
		}
	}

	// Below is a direct translation (more or less) of https://gpsd.gitlab.io/gpsd/AIVDM.html

	const std::vector<std::string> JSON_MAP_STATUS = {
		"Under way using engine",
		"At anchor",
		"Not under command",
		"Restricted manoeuverability",
		"Constrained by her draught",
		"Moored",
		"Aground",
		"Engaged in Fishing",
		"Under way sailing",
		"Reserved for HSC",
		"Reserved for WIG",
		"Reserved",
		"Reserved",
		"Reserved",
		"AIS-SART is active",
		"Not defined"
	};

	const std::vector<std::string> JSON_MAP_EPFD = {
		"Undefined",
		"GPS",
		"GLONASS",
		"Combined GPS/GLONASS",
		"Loran-C",
		"Chayka",
		"Integrated navigation system",
		"Surveyed",
		"Galileo"
	};

	const std::vector<std::string> JSON_MAP_SHIPTYPE = {
		"Not available",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Wing in ground (WIG) - all ships of this type",
		"Wing in ground (WIG) - Hazardous category A",
		"Wing in ground (WIG) - Hazardous category B",
		"Wing in ground (WIG) - Hazardous category C",
		"Wing in ground (WIG) - Hazardous category D",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Fishing",
		"Towing",
		"Towing: length exceeds 200m or breadth exceeds 25m",
		"Dredging or underwater ops",
		"Diving ops",
		"Military ops",
		"Sailing",
		"Pleasure Craft",
		"Reserved",
		"Reserved",
		"High speed craft (HSC) - all ships of this type",
		"High speed craft (HSC) - Hazardous category A",
		"High speed craft (HSC) - Hazardous category B",
		"High speed craft (HSC) - Hazardous category C",
		"High speed craft (HSC) - Hazardous category D",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - No additional information",
		"Pilot Vessel",
		"Search and Rescue vessel",
		"Tug",
		"Port Tender",
		"Anti-pollution equipment",
		"Law Enforcement",
		"Spare - Local Vessel",
		"Spare - Local Vessel",
		"Medical Transport",
		"Noncombatant ship according to RR Resolution No. 18",
		"Passenger - all ships of this type",
		"Passenger - Hazardous category A",
		"Passenger - Hazardous category B",
		"Passenger - Hazardous category C",
		"Passenger - Hazardous category D",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - No additional information",
		"Cargo - all ships of this type",
		"Cargo - Hazardous category A",
		"Cargo - Hazardous category B",
		"Cargo - Hazardous category C",
		"Cargo - Hazardous category D",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - No additional information",
		"Tanker - all ships of this type",
		"Tanker - Hazardous category A",
		"Tanker - Hazardous category B",
		"Tanker - Hazardous category C",
		"Tanker - Hazardous category D",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - No additional information",
		"Other Type - all ships of this type",
		"Other Type - Hazardous category A",
		"Other Type - Hazardous category B",
		"Other Type - Hazardous category C",
		"Other Type - Hazardous category D",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - no additional information"
	};

	const std::vector<std::string> JSON_MAP_AID_TYPE = {
		"Default, Type of Aid to Navigation not specified",
		"Reference point",
		"RACON (radar transponder marking a navigation hazard)",
		"Fixed offshore structure",
		"Spare, Reserved for future use.",
		"Light, without sectors",
		"Light, with sectors",
		"Leading Light Front",
		"Leading Light Rear",
		"Beacon, Cardinal N",
		"Beacon, Cardinal E",
		"Beacon, Cardinal S",
		"Beacon, Cardinal W",
		"Beacon, Port hand",
		"Beacon, Starboard hand",
		"Beacon, Preferred Channel port hand",
		"Beacon, Preferred Channel starboard hand",
		"Beacon, Isolated danger",
		"Beacon, Safe water",
		"Beacon, Special mark",
		"Cardinal Mark N",
		"Cardinal Mark E",
		"Cardinal Mark S",
		"Cardinal Mark W",
		"Port hand Mark",
		"Starboard hand Mark",
		"Preferred Channel Port hand",
		"Preferred Channel Starboard hand",
		"Isolated danger",
		"Safe Water",
		"Special Mark",
		"Light Vessel / LANBY / Rigs"
	};

	// Source: https://help.marinetraffic.com/hc/en-us/articles/360018392858-How-does-MarineTraffic-identify-a-vessel-s-country-and-flag-
	const std::vector<COUNTRY> JSON_MAP_MID = {
		{ 201, "Albania", "AL" },
		{ 202, "Andorra", "AD" },
		{ 203, "Austria", "AT" },
		{ 204, "Portugal", "PT" },
		{ 205, "Belgium", "BE" },
		{ 206, "Belarus", "BY" },
		{ 207, "Bulgaria", "BG" },
		{ 208, "Vatican", "VA" },
		{ 209, "Cyprus", "CY" },
		{ 210, "Cyprus", "CY" },
		{ 211, "Germany", "DE" },
		{ 212, "Cyprus", "CY" },
		{ 213, "Georgia", "GE" },
		{ 214, "Moldova", "MD" },
		{ 215, "Malta", "MT" },
		{ 216, "Armenia", "AM" },
		{ 218, "Germany", "DE" },
		{ 219, "Denmark", "DK" },
		{ 220, "Denmark", "DK" },
		{ 224, "Spain", "ES" },
		{ 225, "Spain", "ES" },
		{ 226, "France", "FR" },
		{ 227, "France", "FR" },
		{ 228, "France", "FR" },
		{ 229, "Malta", "MT" },
		{ 230, "Finland", "FI" },
		{ 231, "Faroe Is", "FO" },
		{ 232, "United Kingdom", "GB" },
		{ 233, "United Kingdom", "GB" },
		{ 234, "United Kingdom", "GB" },
		{ 235, "United Kingdom", "GB" },
		{ 236, "Gibraltar", "GI" },
		{ 237, "Greece", "GR" },
		{ 238, "Croatia", "HR" },
		{ 239, "Greece", "GR" },
		{ 240, "Greece", "GR" },
		{ 241, "Greece", "GR" },
		{ 242, "Morocco", "MA" },
		{ 243, "Hungary", "HU" },
		{ 244, "Netherlands", "NL" },
		{ 245, "Netherlands", "NL" },
		{ 246, "Netherlands", "NL" },
		{ 247, "Italy", "IT" },
		{ 248, "Malta", "MT" },
		{ 249, "Malta", "MT" },
		{ 250, "Ireland", "IE" },
		{ 251, "Iceland", "IS" },
		{ 252, "Liechtenstein", "LI" },
		{ 253, "Luxembourg", "LU" },
		{ 254, "Monaco", "MC" },
		{ 255, "Portugal", "PT" },
		{ 256, "Malta", "MT" },
		{ 257, "Norway", "NO" },
		{ 258, "Norway", "NO" },
		{ 259, "Norway", "NO" },
		{ 261, "Poland", "PL" },
		{ 262, "Montenegro", "ME" },
		{ 263, "Portugal", "PT" },
		{ 264, "Romania", "RO" },
		{ 265, "Sweden", "SE" },
		{ 266, "Sweden", "SE" },
		{ 267, "Slovakia", "SK" },
		{ 268, "San Marino", "SM" },
		{ 269, "Switzerland", "CH" },
		{ 270, "Czech Republic", "CZ" },
		{ 271, "Turkey", "TR" },
		{ 272, "Ukraine", "UA" },
		{ 273, "Russia", "RU" },
		{ 274, "FYR Macedonia", "MK" },
		{ 275, "Latvia", "LV" },
		{ 276, "Estonia", "EE" },
		{ 277, "Lithuania", "LT" },
		{ 278, "Slovenia", "SI" },
		{ 279, "Serbia", "RS" },
		{ 301, "Anguilla", "AI" },
		{ 303, "USA", "US" },
		{ 304, "Antigua Barbuda", "AG" },
		{ 305, "Antigua Barbuda", "AG" },
		{ 306, "Curacao", "CW" },
		{ 307, "Aruba", "AW" },
		{ 308, "Bahamas", "BS" },
		{ 309, "Bahamas", "BS" },
		{ 310, "Bermuda", "BM" },
		{ 311, "Bahamas", "BS" },
		{ 312, "Belize", "BZ" },
		{ 314, "Barbados", "BB" },
		{ 316, "Canada", "CA" },
		{ 319, "Cayman Is", "KY" },
		{ 321, "Costa Rica", "CR" },
		{ 323, "Cuba", "CU" },
		{ 325, "Dominica", "DM" },
		{ 327, "Dominican Rep", "DO" },
		{ 329, "Guadeloupe", "GP" },
		{ 330, "Grenada", "GD" },
		{ 331, "Greenland", "GL" },
		{ 332, "Guatemala", "GT" },
		{ 334, "Honduras", "HN" },
		{ 336, "Haiti", "HT" },
		{ 338, "USA", "US" },
		{ 339, "Jamaica", "JM" },
		{ 341, "St Kitts Nevis", "KN" },
		{ 343, "St Lucia", "LC" },
		{ 345, "Mexico", "MX" },
		{ 347, "Martinique", "MQ" },
		{ 348, "Montserrat", "MS" },
		{ 350, "Nicaragua", "NI" },
		{ 351, "Panama", "PA" },
		{ 352, "Panama", "PA" },
		{ 353, "Panama", "PA" },
		{ 354, "Panama", "PA" },
		{ 355, "Panama", "PA" },
		{ 356, "Panama", "PA" },
		{ 357, "Panama", "PA" },
		{ 358, "Puerto Rico", "PR" },
		{ 359, "El Salvador", "SV" },
		{ 361, "St Pierre Miquelon", "PM" },
		{ 362, "Trinidad Tobago", "TT" },
		{ 364, "Turks Caicos Is", "TC" },
		{ 366, "USA", "US" },
		{ 367, "USA", "US" },
		{ 368, "USA", "US" },
		{ 369, "USA", "US" },
		{ 370, "Panama", "PA" },
		{ 371, "Panama", "PA" },
		{ 372, "Panama", "PA" },
		{ 373, "Panama", "PA" },
		{ 374, "Panama", "PA" },
		{ 375, "St Vincent Grenadines", "VC" },
		{ 376, "St Vincent Grenadines", "VC" },
		{ 377, "St Vincent Grenadines", "VC" },
		{ 378, "British Virgin Is", "VG" },
		{ 379, "US Virgin Is", "VI" },
		{ 401, "Afghanistan", "AF" },
		{ 403, "Saudi Arabia", "SA" },
		{ 405, "Bangladesh", "BD" },
		{ 408, "Bahrain", "BH" },
		{ 410, "Bhutan", "BT" },
		{ 412, "China", "CN" },
		{ 413, "China", "CN" },
		{ 414, "China", "CN" },
		{ 416, "Taiwan", "TW" },
		{ 417, "Sri Lanka", "LK" },
		{ 419, "India", "IN" },
		{ 422, "Iran", "IR" },
		{ 423, "Azerbaijan", "AZ" },
		{ 425, "Iraq", "IQ" },
		{ 428, "Israel", "IL" },
		{ 431, "Japan", "JP" },
		{ 432, "Japan", "JP" },
		{ 434, "Turkmenistan", "TM" },
		{ 436, "Kazakhstan", "KZ" },
		{ 437, "Uzbekistan", "UZ" },
		{ 438, "Jordan", "JO" },
		{ 440, "Korea", "KR" },
		{ 441, "Korea", "KR" },
		{ 443, "Palestine", "PS" },
		{ 445, "DPR Korea", "KP" },
		{ 447, "Kuwait", "KW" },
		{ 450, "Lebanon", "LB" },
		{ 451, "Kyrgyz Republic", "KG" },
		{ 453, "Macao", "MO" },
		{ 455, "Maldives", "MV" },
		{ 457, "Mongolia", "MN" },
		{ 459, "Nepal", "NP" },
		{ 461, "Oman", "OM" },
		{ 463, "Pakistan", "PK" },
		{ 466, "Qatar", "QA" },
		{ 468, "Syria", "SY" },
		{ 470, "UAE", "AE" },
		{ 471, "UAE", "AE" },
		{ 472, "Tajikistan", "TJ" },
		{ 473, "Yemen", "YE" },
		{ 475, "Yemen", "YE" },
		{ 477, "Hong Kong", "HK" },
		{ 478, "Bosnia and Herzegovina", "BA" },
		{ 501, "Antarctica", "AQ" },
		{ 503, "Australia", "AU" },
		{ 506, "Myanmar", "MM" },
		{ 508, "Brunei", "BN" },
		{ 510, "Micronesia", "FM" },
		{ 511, "Palau", "PW" },
		{ 512, "New Zealand", "NZ" },
		{ 514, "Cambodia", "KH" },
		{ 515, "Cambodia", "KH" },
		{ 516, "Christmas Is", "CX" },
		{ 518, "Cook Is", "CK" },
		{ 520, "Fiji", "FJ" },
		{ 523, "Cocos Is", "CC" },
		{ 525, "Indonesia", "ID" },
		{ 529, "Kiribati", "KI" },
		{ 531, "Laos", "LA" },
		{ 533, "Malaysia", "MY" },
		{ 536, "N Mariana Is", "MP" },
		{ 538, "Marshall Is", "MH" },
		{ 540, "New Caledonia", "NC" },
		{ 542, "Niue", "NU" },
		{ 544, "Nauru", "NR" },
		{ 546, "French Polynesia", "PF" },
		{ 548, "Philippines", "PH" },
		{ 553, "Papua New Guinea", "PG" },
		{ 555, "Pitcairn Is", "PN" },
		{ 557, "Solomon Is", "SB" },
		{ 559, "American Samoa", "AS" },
		{ 561, "Samoa", "WS" },
		{ 563, "Singapore", "SG" },
		{ 564, "Singapore", "SG" },
		{ 565, "Singapore", "SG" },
		{ 566, "Singapore", "SG" },
		{ 567, "Thailand", "TH" },
		{ 570, "Tonga", "TO" },
		{ 572, "Tuvalu", "TV" },
		{ 574, "Vietnam", "VN" },
		{ 576, "Vanuatu", "VU" },
		{ 577, "Vanuatu", "VU" },
		{ 578, "Wallis Futuna Is", "WF" },
		{ 601, "South Africa", "ZA" },
		{ 603, "Angola", "AO" },
		{ 605, "Algeria", "DZ" },
		{ 607, "St Paul Amsterdam Is", "TF" },
		{ 608, "Ascension Is", "IO" },
		{ 609, "Burundi", "BI" },
		{ 610, "Benin", "BJ" },
		{ 611, "Botswana", "BW" },
		{ 612, "Cen Afr Rep", "CF" },
		{ 613, "Cameroon", "CM" },
		{ 615, "Congo", "CG" },
		{ 616, "Comoros", "KM" },
		{ 617, "Cape Verde", "CV" },
		{ 618, "Antarctica", "AQ" },
		{ 619, "Ivory Coast", "CI" },
		{ 620, "Comoros", "KM" },
		{ 621, "Djibouti", "DJ" },
		{ 622, "Egypt", "EG" },
		{ 624, "Ethiopia", "ET" },
		{ 625, "Eritrea", "ER" },
		{ 626, "Gabon", "GA" },
		{ 627, "Ghana", "GH" },
		{ 629, "Gambia", "GM" },
		{ 630, "Guinea-Bissau", "GW" },
		{ 631, "Equ. Guinea", "GQ" },
		{ 632, "Guinea", "GN" },
		{ 633, "Burkina Faso", "BF" },
		{ 634, "Kenya", "KE" },
		{ 635, "Antarctica", "AQ" },
		{ 636, "Liberia", "LR" },
		{ 637, "Liberia", "LR" },
		{ 642, "Libya", "LY" },
		{ 644, "Lesotho", "LS" },
		{ 645, "Mauritius", "MU" },
		{ 647, "Madagascar", "MG" },
		{ 649, "Mali", "ML" },
		{ 650, "Mozambique", "MZ" },
		{ 654, "Mauritania", "MR" },
		{ 655, "Malawi", "MW" },
		{ 656, "Niger", "NE" },
		{ 657, "Nigeria", "NG" },
		{ 659, "Namibia", "NA" },
		{ 660, "Reunion", "RE" },
		{ 661, "Rwanda", "RW" },
		{ 662, "Sudan", "SD" },
		{ 663, "Senegal", "SN" },
		{ 664, "Seychelles", "SC" },
		{ 665, "St Helena", "SH" },
		{ 666, "Somalia", "SO" },
		{ 667, "Sierra Leone", "SL" },
		{ 668, "Sao Tome Principe", "ST" },
		{ 669, "Swaziland", "SZ" },
		{ 670, "Chad", "TD" },
		{ 671, "Togo", "TG" },
		{ 672, "Tunisia", "TN" },
		{ 674, "Tanzania", "TZ" },
		{ 675, "Uganda", "UG" },
		{ 676, "DR Congo", "CD" },
		{ 677, "Tanzania", "TZ" },
		{ 678, "Zambia", "ZM" },
		{ 679, "Zimbabwe", "ZW" },
		{ 701, "Argentina", "AR" },
		{ 710, "Brazil", "BR" },
		{ 720, "Bolivia", "BO" },
		{ 725, "Chile", "CL" },
		{ 730, "Colombia", "CO" },
		{ 735, "Ecuador", "EC" },
		{ 740, "UK", "UK" },
		{ 745, "Guiana", "GF" },
		{ 750, "Guyana", "GY" },
		{ 755, "Paraguay", "PY" },
		{ 760, "Peru", "PE" },
		{ 765, "Suriname", "SR" },
		{ 770, "Uruguay", "UY" },
		{ 775, "Venezuela", "VE" }
	};
}
