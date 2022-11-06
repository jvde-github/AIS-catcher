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

	struct MID {
		uint32_t code;
		std::string country;
	};

	extern const std::vector<std::string> JSON_MAP_STATUS;
	extern const std::vector<std::string> JSON_MAP_EPFD;
	extern const std::vector<std::string> JSON_MAP_SHIPTYPE;
	extern const std::vector<std::string> JSON_MAP_AID_TYPE;
	extern const std::vector<MID> JSON_MAP_MID;

	void JSONAIS::U(const AIS::Message& msg, int p, int start, int len, unsigned undefined) {
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			Submit(p, u);
	}

	void JSONAIS::UL(const AIS::Message& msg, int p, int start, int len, float a, float b, unsigned undefined) {
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			Submit(p, u * a + b);
	}

	void JSONAIS::S(const AIS::Message& msg, int p, int start, int len, int undefined) {
		int u = msg.getInt(start, len);
		if (u != undefined)
			Submit(p, u);
	}

	void JSONAIS::SL(const AIS::Message& msg, int p, int start, int len, float a, float b, int undefined) {
		int s = msg.getInt(start, len);
		if (s != undefined)
			Submit(p, s * a + b);
	}

	void JSONAIS::E(const AIS::Message& msg, int p, int start, int len, int pmap, const std::vector<std::string>* map) {
		unsigned u = msg.getUint(start, len);
		Submit(p, u);
		if (map) {
			if (u < map->size())
				Submit(pmap, (*map)[u]);
			else
				Submit(pmap, std::string("Undefined"));
		}
	}

	void JSONAIS::TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined) {
		int u = msg.getInt(start, len);

		if (u == -128)
			; // Submit(p, std::string("nan"));
		else if (u == -127)
			Submit(p, std::string("fastleft"));
		else if (u == 127)
			Submit(p, std::string("fastright"));
		else {
			double rot = u / 4.733;
			rot = (u < 0) ? -rot * rot : rot * rot;
			Submit(p, (int)(rot + 0.5));
		}
	}

	void JSONAIS::TIMESTAMP(const AIS::Message& msg, int p, int start, int len) {
		if (len != 40) return;

		std::stringstream s;
		s << std::setfill('0') << std::setw(4) << msg.getUint(start, 14) << "-" << std::setw(2) << msg.getUint(start + 14, 4) << "-" << std::setw(2) << msg.getUint(start + 18, 5) << "T"
		  << std::setw(2) << msg.getUint(start + 23, 5) << ":" << std::setw(2) << msg.getUint(start + 28, 6) << ":" << std::setw(2) << msg.getUint(start + 34, 6) << "Z";
		Submit(p, std::string(s.str()));
	}

	void JSONAIS::ETA(const AIS::Message& msg, int p, int start, int len) {
		if (len != 20) return;

		std::stringstream s;
		s << std::setfill('0') << std::setw(2) << msg.getUint(start, 4) << "-" << std::setw(2) << msg.getUint(start + 4, 5) << "T"
		  << std::setw(2) << msg.getUint(start + 9, 5) << ":" << std::setw(2) << msg.getUint(start + 14, 6) << "Z";
		Submit(p, std::string(s.str()));
	}

	void JSONAIS::B(const AIS::Message& msg, int p, int start, int len) {
		unsigned u = msg.getUint(start, len);
		Submit(p, (bool)u);
	}

	void JSONAIS::T(const AIS::Message& msg, int p, int start, int len) {
		std::string text = msg.getText(start, len);
		while (!text.empty() && text[text.length() - 1] == ' ') text.resize(text.length() - 1);
		Submit(p, text);
	}

	void JSONAIS::D(const AIS::Message& msg, int p, int start, int len) {
		std::string text = msg.getText(start, len);
		Submit(p, text);
	}

	// Refernce: https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.585-9-202205-I!!PDF-E.pdf
	void JSONAIS::MMSI(const AIS::Message& msg) {

		uint32_t mid = msg.mmsi();
		while (mid > 1000) mid /= 10;
		if (mid > 100) {
			int l = 0, r = JSON_MAP_MID.size() - 1;
			while (l != r) {
				int m = (l + r + 1) / 2;
				if (JSON_MAP_MID[m].code > mid)
					r = m - 1;
				else
					l = m;
			}
			if (JSON_MAP_MID[l].code == mid)
				Submit(PROPERTY_COUNTRY, JSON_MAP_MID[l].country);
		}
	}

	void JSONAIS::ProcessMsg8Data(const AIS::Message& msg, int len) {
		int dac = msg.getUint(40, 10);
		int fid = msg.getUint(50, 6);

		if (dac == 200 && fid == 10) {
			T(msg, PROPERTY_VIN, 56, 48);
			U(msg, PROPERTY_LENGTH, 104, 13);
			U(msg, PROPERTY_BEAM, 117, 10);
			E(msg, PROPERTY_SHIPTYPE, 127, 14);
			E(msg, PROPERTY_HAZARD, 141, 3);
			U(msg, PROPERTY_DRAUGHT, 144, 11);
			E(msg, PROPERTY_LOADED, 155, 2);
			B(msg, PROPERTY_SPEED_Q, 157, 1);
			B(msg, PROPERTY_COURSE_Q, 158, 1);
			B(msg, PROPERTY_HEADING_Q, 159, 1);
		}
		else if (dac == 1 && fid == 31) {
			// Sources: http://vislab-ccom.unh.edu/~schwehr/papers/2010-IMO-SN.1-Circ.289.pdf, GPSDECODE
			SL(msg, PROPERTY_LON, 56, 25, 1 / 60000.0, 0);
			SL(msg, PROPERTY_LAT, 81, 24, 1 / 60000.0, 0);
			B(msg, PROPERTY_ACCURACY, 105, 1);
			U(msg, PROPERTY_DAY, 106, 5, 0);
			U(msg, PROPERTY_HOUR, 111, 5, 24);
			U(msg, PROPERTY_MINUTE, 116, 6, 60);
			U(msg, PROPERTY_WSPEED, 122, 7, 127);
			U(msg, PROPERTY_WGUST, 129, 7, 127);
			U(msg, PROPERTY_WDIR, 136, 9, 360);
			U(msg, PROPERTY_WGUSTDIR, 145, 9, 360);
			SL(msg, PROPERTY_AIRTEMP, 154, 11, 0.1, 0, -1024);
			U(msg, PROPERTY_HUMIDITY, 165, 7, 101);
			SL(msg, PROPERTY_DEWPOINT, 172, 10, 0.1, 0, 501);
			UL(msg, PROPERTY_PRESSURE, 182, 9, 1, 799, 511);
			U(msg, PROPERTY_PRESSURETEND, 191, 2, 3);
			U(msg, PROPERTY_VISGREATER, 193, 1);
			UL(msg, PROPERTY_VISIBILITY, 194, 7, 0.1, 0);
			UL(msg, PROPERTY_WATERLEVEL, 201, 12, 0.01, -10, 4002);
			U(msg, PROPERTY_LEVELTREND, 213, 2, 3);
			UL(msg, PROPERTY_CSPEED, 215, 8, 0.1, 0, 255);
			U(msg, PROPERTY_CDIR, 223, 9, 360);
			UL(msg, PROPERTY_CSPEED2, 232, 8, 0.1, 0);
			U(msg, PROPERTY_CDIR2, 240, 9);
			U(msg, PROPERTY_CDEPTH2, 249, 5);
			UL(msg, PROPERTY_CSPEED3, 254, 8, 0.1, 0);
			U(msg, PROPERTY_CDIR3, 262, 9);
			U(msg, PROPERTY_CDEPTH3, 271, 5);
			UL(msg, PROPERTY_WAVEHEIGHT, 276, 8, 0.1, 0);
			U(msg, PROPERTY_WAVEPERIOD, 284, 6);
			U(msg, PROPERTY_WAVEDIR, 290, 9);
			UL(msg, PROPERTY_SWELLHEIGHT, 299, 8, 0.1, 0);
			U(msg, PROPERTY_SWELLPERIOD, 307, 6);
			U(msg, PROPERTY_SWELLDIR, 313, 9);
			U(msg, PROPERTY_SEASTATE, 322, 4);
			SL(msg, PROPERTY_WATERTEMP, 326, 10, 0.1, 0);
			U(msg, PROPERTY_PRECIPTYPE, 336, 3, 7);
			U(msg, PROPERTY_SALINITY, 339, 9, 510);
			U(msg, PROPERTY_ICE, 348, 2, 3);
		}
	}

	void JSONAIS::Receive(const AIS::Message* data, int len, TAG& tag) {
		Submit(PROPERTY_OBJECT_START, std::string(""));

		const AIS::Message& msg = data[0];

		Submit(PROPERTY_CLASS, std::string("AIS"));
		Submit(PROPERTY_DEVICE, std::string("AIS-catcher"));

		if (tag.mode & 2) {
			Submit(PROPERTY_RXTIME, msg.getRxTime());
		}

		Submit(PROPERTY_SCALED, true);
		Submit(PROPERTY_CHANNEL, std::string(1, msg.channel));
		Submit(PROPERTY_NMEA, msg.sentence);

		if (tag.mode & 1) {
			Submit(PROPERTY_SIGNAL_POWER, tag.level);
			Submit(PROPERTY_PPM, tag.ppm);
		}

		U(msg, PROPERTY_TYPE, 0, 6);
		U(msg, PROPERTY_REPEAT, 6, 2);
		U(msg, PROPERTY_MMSI, 8, 30);

		if (tag.mode & 4) {
			MMSI(msg);
		}

		switch (msg.type()) {
		case 1:
		case 2:
		case 3:
			E(msg, PROPERTY_STATUS, 38, 4, PROPERTY_STATUS_TEXT, &JSON_MAP_STATUS);
			TURN(msg, PROPERTY_TURN, 42, 8);
			UL(msg, PROPERTY_SPEED, 50, 10, 0.1, 0, 1023);
			B(msg, PROPERTY_ACCURACY, 60, 1);
			SL(msg, PROPERTY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 89, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 116, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 128, 9, 511);
			U(msg, PROPERTY_SECOND, 137, 6);
			E(msg, PROPERTY_MANEUVER, 143, 2);
			X(msg, PROPERTY_SPARE, 145, 3);
			B(msg, PROPERTY_RAIM, 148, 1);
			U(msg, PROPERTY_RADIO, 149, 19);
			break;
		case 4:
		case 11:
			TIMESTAMP(msg, PROPERTY_TIMESTAMP, 38, 40);
			U(msg, PROPERTY_YEAR, 38, 14, 0);
			U(msg, PROPERTY_MONTH, 52, 4, 0);
			U(msg, PROPERTY_DAY, 56, 5, 0);
			U(msg, PROPERTY_HOUR, 61, 5, 24);
			U(msg, PROPERTY_MINUTE, 66, 6, 60);
			U(msg, PROPERTY_SECOND, 72, 6, 60);
			B(msg, PROPERTY_ACCURACY, 78, 1);
			SL(msg, PROPERTY_LON, 79, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 107, 27, 1 / 600000.0, 0);
			E(msg, PROPERTY_EPFD, 134, 4, PROPERTY_EPFD_TEXT, &JSON_MAP_EPFD);
			X(msg, PROPERTY_SPARE, 138, 10);
			B(msg, PROPERTY_RAIM, 148, 1);
			U(msg, PROPERTY_RADIO, 149, 19);
			break;
		case 5:
			U(msg, PROPERTY_AIS_VERSION, 38, 2);
			U(msg, PROPERTY_IMO, 40, 30);
			T(msg, PROPERTY_CALLSIGN, 70, 42);
			T(msg, PROPERTY_SHIPNAME, 112, 120);
			E(msg, PROPERTY_SHIPTYPE, 232, 8, PROPERTY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
			U(msg, PROPERTY_TO_BOW, 240, 9);
			U(msg, PROPERTY_TO_STERN, 249, 9);
			U(msg, PROPERTY_TO_PORT, 258, 6);
			U(msg, PROPERTY_TO_STARBOARD, 264, 6);
			E(msg, PROPERTY_EPFD, 270, 4, PROPERTY_EPFD_TEXT, &JSON_MAP_EPFD);
			ETA(msg, PROPERTY_ETA, 274, 20);
			U(msg, PROPERTY_MONTH, 274, 4, 0);
			U(msg, PROPERTY_DAY, 278, 5, 0);
			U(msg, PROPERTY_HOUR, 283, 5, 0);
			U(msg, PROPERTY_MINUTE, 288, 6, 0);
			UL(msg, PROPERTY_DRAUGHT, 294, 8, 0.1, 0);
			T(msg, PROPERTY_DESTINATION, 302, 120);
			B(msg, PROPERTY_DTE, 422, 1);
			X(msg, PROPERTY_SPARE, 423, 1);
			break;
		case 6:
			U(msg, PROPERTY_SEQNO, 38, 2);
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			B(msg, PROPERTY_RETRANSMIT, 70, 1);
			X(msg, PROPERTY_SPARE, 71, 1);
			U(msg, PROPERTY_DAC, 72, 10);
			U(msg, PROPERTY_FID, 82, 6);
			break;
		case 7:
		case 13:
			X(msg, PROPERTY_SPARE, 38, 2);
			U(msg, PROPERTY_MMSI1, 40, 30);
			U(msg, PROPERTY_MMSISEQ1, 70, 2);
			if (msg.length <= 72) break;
			U(msg, PROPERTY_MMSI2, 72, 30);
			U(msg, PROPERTY_MMSISEQ2, 102, 2);
			if (msg.length <= 104) break;
			U(msg, PROPERTY_MMSI3, 104, 30);
			U(msg, PROPERTY_MMSISEQ3, 134, 2);
			if (msg.length <= 136) break;
			U(msg, PROPERTY_MMSI4, 136, 30);
			U(msg, PROPERTY_MMSISEQ4, 166, 2);
			break;
		case 8:
			U(msg, PROPERTY_DAC, 40, 10);
			U(msg, PROPERTY_FID, 50, 6);
			ProcessMsg8Data(msg, len);
			break;
		case 9:
			U(msg, PROPERTY_ALT, 38, 12);
			U(msg, PROPERTY_SPEED, 50, 10);
			B(msg, PROPERTY_ACCURACY, 60, 1);
			SL(msg, PROPERTY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 89, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 116, 12, 0.1, 0);
			U(msg, PROPERTY_SECOND, 128, 6);
			X(msg, PROPERTY_REGIONAL, 134, 8);
			B(msg, PROPERTY_DTE, 142, 1);
			B(msg, PROPERTY_ASSIGNED, 146, 1);
			B(msg, PROPERTY_RAIM, 147, 1);
			U(msg, PROPERTY_RADIO, 148, 20);
			break;
		case 10:
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			break;
		case 12:
			U(msg, PROPERTY_SEQNO, 38, 2);
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			B(msg, PROPERTY_RETRANSMIT, 70, 1);
			T(msg, PROPERTY_TEXT, 72, 936);
			break;
		case 14:
			T(msg, PROPERTY_TEXT, 40, 968);
			break;
		case 15:
			U(msg, PROPERTY_MMSI1, 40, 30);
			U(msg, PROPERTY_TYPE1_1, 70, 6);
			U(msg, PROPERTY_OFFSET1_1, 76, 12);
			if (msg.length <= 90) break;
			U(msg, PROPERTY_TYPE1_2, 90, 6);
			U(msg, PROPERTY_OFFSET1_2, 96, 12);
			if (msg.length <= 110) break;
			U(msg, PROPERTY_MMSI2, 110, 30);
			U(msg, PROPERTY_TYPE2_1, 140, 6);
			U(msg, PROPERTY_OFFSET2_1, 146, 12);
			break;
		case 16:
			U(msg, PROPERTY_MMSI1, 40, 30);
			U(msg, PROPERTY_OFFSET1, 70, 12);
			U(msg, PROPERTY_INCREMENT1, 82, 10);
			break;
		case 17:
			SL(msg, PROPERTY_LON, 40, 18, 1 / 600.0, 0);
			SL(msg, PROPERTY_LAT, 58, 17, 1 / 600.0, 0);
			D(msg, PROPERTY_DATA, 80, 736);
			break;
		case 18:
			UL(msg, PROPERTY_SPEED, 46, 10, 0.1, 0);
			B(msg, PROPERTY_ACCURACY, 56, 1);
			SL(msg, PROPERTY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 112, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 124, 9);
			U(msg, PROPERTY_RESERVED, 38, 8);
			U(msg, PROPERTY_SECOND, 133, 6);
			U(msg, PROPERTY_REGIONAL, 139, 2);
			B(msg, PROPERTY_CS, 141, 1);
			B(msg, PROPERTY_DISPLAY, 142, 1);
			B(msg, PROPERTY_DSC, 143, 1);
			B(msg, PROPERTY_BAND, 144, 1);
			B(msg, PROPERTY_MSG22, 145, 1);
			B(msg, PROPERTY_ASSIGNED, 146, 1);
			B(msg, PROPERTY_RAIM, 147, 1);
			U(msg, PROPERTY_RADIO, 148, 20);
			break;
		case 19:
			UL(msg, PROPERTY_SPEED, 46, 10, 0.1, 0);
			SL(msg, PROPERTY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 112, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 124, 9);
			T(msg, PROPERTY_SHIPNAME, 143, 120);
			E(msg, PROPERTY_SHIPTYPE, 263, 8, PROPERTY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
			U(msg, PROPERTY_TO_BOW, 271, 9);
			U(msg, PROPERTY_TO_STERN, 280, 9);
			U(msg, PROPERTY_TO_PORT, 289, 6);
			U(msg, PROPERTY_TO_STARBOARD, 295, 6);
			E(msg, PROPERTY_EPFD, 301, 4, PROPERTY_EPFD_TEXT, &JSON_MAP_EPFD);
			B(msg, PROPERTY_ACCURACY, 56, 1);
			U(msg, PROPERTY_RESERVED, 38, 8);
			U(msg, PROPERTY_SECOND, 133, 6);
			U(msg, PROPERTY_REGIONAL, 139, 4);
			B(msg, PROPERTY_RAIM, 305, 1);
			B(msg, PROPERTY_DTE, 306, 1);
			U(msg, PROPERTY_ASSIGNED, 307, 1);
			X(msg, PROPERTY_SPARE, 308, 4);
			break;
		case 20:
			U(msg, PROPERTY_OFFSET1, 40, 12);
			U(msg, PROPERTY_NUMBER1, 52, 4);
			U(msg, PROPERTY_TIMEOUT1, 56, 3);
			U(msg, PROPERTY_INCREMENT1, 59, 11);
			if (msg.length <= 100) break;
			U(msg, PROPERTY_OFFSET2, 70, 12);
			U(msg, PROPERTY_NUMBER2, 82, 4);
			U(msg, PROPERTY_TIMEOUT2, 86, 3);
			U(msg, PROPERTY_INCREMENT2, 89, 11);
			if (msg.length <= 130) break;
			U(msg, PROPERTY_OFFSET3, 100, 12);
			U(msg, PROPERTY_NUMBER3, 112, 4);
			U(msg, PROPERTY_TIMEOUT3, 116, 3);
			U(msg, PROPERTY_INCREMENT3, 119, 11);
			if (msg.length <= 160) break;
			U(msg, PROPERTY_OFFSET4, 130, 12);
			U(msg, PROPERTY_NUMBER4, 142, 4);
			U(msg, PROPERTY_TIMEOUT4, 146, 3);
			U(msg, PROPERTY_INCREMENT4, 149, 11);
			break;
		case 21:
			E(msg, PROPERTY_AID_TYPE, 38, 5, PROPERTY_AID_TYPE_TEXT, &JSON_MAP_AID_TYPE);
			T(msg, PROPERTY_NAME, 43, 120);
			B(msg, PROPERTY_ACCURACY, 163, 1);
			SL(msg, PROPERTY_LON, 164, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 192, 27, 1 / 600000.0, 0);
			U(msg, PROPERTY_TO_BOW, 219, 9);
			U(msg, PROPERTY_TO_STERN, 228, 9);
			U(msg, PROPERTY_TO_PORT, 237, 6);
			U(msg, PROPERTY_TO_STARBOARD, 243, 6);
			E(msg, PROPERTY_EPFD, 249, 4, PROPERTY_EPFD_TEXT, &JSON_MAP_EPFD);
			U(msg, PROPERTY_SECOND, 253, 6);
			B(msg, PROPERTY_OFF_POSITION, 259, 1);
			U(msg, PROPERTY_REGIONAL, 260, 8);
			B(msg, PROPERTY_RAIM, 268, 1);
			B(msg, PROPERTY_VIRTUAL_AID, 269, 1);
			B(msg, PROPERTY_ASSIGNED, 270, 1);
			break;
		case 22:
			U(msg, PROPERTY_CHANNEL_A, 40, 12);
			U(msg, PROPERTY_CHANNEL_B, 52, 12);
			U(msg, PROPERTY_TXRX, 64, 4);
			B(msg, PROPERTY_POWER, 68, 1);
			if (msg.getUint(139, 1)) {
				U(msg, PROPERTY_DEST1, 69, 30);	 // check // if addressed is 1
				U(msg, PROPERTY_DEST2, 104, 30); // check
			}
			else {
				SL(msg, PROPERTY_NE_LON, 69, 18, 1.0/600.0, 0);
				SL(msg, PROPERTY_NE_LAT, 87, 17, 1.0/600.0, 0);
				SL(msg, PROPERTY_SW_LON, 104, 18, 1.0/600.0, 0);
				SL(msg, PROPERTY_SW_LAT, 122, 17, 1.0/600.0, 0);
			}
			B(msg, PROPERTY_ADDRESSED, 139, 1);
			B(msg, PROPERTY_BAND_A, 140, 1);
			B(msg, PROPERTY_BAND_B, 141, 1);
			U(msg, PROPERTY_ZONESIZE, 142, 3);
			break;
		case 23:
			U(msg, PROPERTY_NE_LON, 40, 18);
			U(msg, PROPERTY_NE_LAT, 58, 17);
			U(msg, PROPERTY_SW_LON, 75, 18);
			U(msg, PROPERTY_SW_LAT, 93, 17);
			E(msg, PROPERTY_STATION_TYPE, 110, 4);
			E(msg, PROPERTY_SHIP_TYPE, 114, 8);
			U(msg, PROPERTY_TXRX, 144, 2);
			E(msg, PROPERTY_INTERVAL, 146, 4);
			U(msg, PROPERTY_QUIET, 150, 4);
			break;
		case 24:
			U(msg, PROPERTY_PARTNO, 38, 2);

			if (msg.getUint(38, 2) == 0) {
				T(msg, PROPERTY_SHIPNAME, 40, 120);
			}
			else {
				E(msg, PROPERTY_SHIPTYPE, 40, 8, PROPERTY_SHIPTYPE_TEXT, &JSON_MAP_SHIPTYPE);
				T(msg, PROPERTY_VENDORID, 48, 18);
				U(msg, PROPERTY_MODEL, 66, 4);
				U(msg, PROPERTY_SERIAL, 70, 20);
				T(msg, PROPERTY_CALLSIGN, 90, 42);
				if (msg.mmsi() / 10000000 == 98) {
					U(msg, PROPERTY_MOTHERSHIP_MMSI, 132, 30);
				}
				else {
					U(msg, PROPERTY_TO_BOW, 132, 9);
					U(msg, PROPERTY_TO_STERN, 141, 9);
					U(msg, PROPERTY_TO_PORT, 150, 6);
					U(msg, PROPERTY_TO_STARBOARD, 156, 6);
				}
			}
			break;
		case 27:
			U(msg, PROPERTY_ACCURACY, 38, 1);
			U(msg, PROPERTY_RAIM, 39, 1);
			U(msg, PROPERTY_STATUS, 40, 4);
			SL(msg, PROPERTY_LON, 44, 18, 181000, 1 / 600.0, 0);
			SL(msg, PROPERTY_LAT, 62, 17, 91000, 1 / 600.0, 0);
			U(msg, PROPERTY_SPEED, 79, 6);
			U(msg, PROPERTY_COURSE, 85, 9);
			U(msg, PROPERTY_GNSS, 94, 1);
			break;
		default:
			break;
		}
		Submit(PROPERTY_OBJECT_END, std::string(""));
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
		"Reserved for future amendment of Navigational Status for HSC",
		"Reserved for future amendment of Navigational Status for WIG",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
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
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Wing in ground (WIG), all ships of this type",
		"Wing in ground (WIG), Hazardous category A",
		"Wing in ground (WIG), Hazardous category B",
		"Wing in ground (WIG), Hazardous category C",
		"Wing in ground (WIG), Hazardous category D",
		"Wing in ground (WIG), Reserved for future use",
		"Wing in ground (WIG), Reserved for future use",
		"Wing in ground (WIG), Reserved for future use",
		"Wing in ground (WIG), Reserved for future use",
		"Wing in ground (WIG), Reserved for future use",
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
		"High speed craft (HSC), all ships of this type",
		"High speed craft (HSC), Hazardous category A",
		"High speed craft (HSC), Hazardous category B",
		"High speed craft (HSC), Hazardous category C",
		"High speed craft (HSC), Hazardous category D",
		"High speed craft (HSC), Reserved for future use",
		"High speed craft (HSC), Reserved for future use",
		"High speed craft (HSC), Reserved for future use",
		"High speed craft (HSC), Reserved for future use",
		"High speed craft (HSC), No additional information",
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
		"Passenger, all ships of this type",
		"Passenger, Hazardous category A",
		"Passenger, Hazardous category B",
		"Passenger, Hazardous category C",
		"Passenger, Hazardous category D",
		"Passenger, Reserved for future use",
		"Passenger, Reserved for future use",
		"Passenger, Reserved for future use",
		"Passenger, Reserved for future use",
		"Passenger, No additional information",
		"Cargo, all ships of this type",
		"Cargo, Hazardous category A",
		"Cargo, Hazardous category B",
		"Cargo, Hazardous category C",
		"Cargo, Hazardous category D",
		"Cargo, Reserved for future use",
		"Cargo, Reserved for future use",
		"Cargo, Reserved for future use",
		"Cargo, Reserved for future use",
		"Cargo, No additional information",
		"Tanker, all ships of this type",
		"Tanker, Hazardous category A",
		"Tanker, Hazardous category B",
		"Tanker, Hazardous category C",
		"Tanker, Hazardous category D",
		"Tanker, Reserved for future use",
		"Tanker, Reserved for future use",
		"Tanker, Reserved for future use",
		"Tanker, Reserved for future use",
		"Tanker, No additional information",
		"Other Type, all ships of this type",
		"Other Type, Hazardous category A",
		"Other Type, Hazardous category B",
		"Other Type, Hazardous category C",
		"Other Type, Hazardous category D",
		"Other Type, Reserved for future use",
		"Other Type, Reserved for future use",
		"Other Type, Reserved for future use",
		"Other Type, Reserved for future use",
		"Other Type, no additional information"
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

	// Source: https://www.itu.int/en/ITU-R/terrestrial/fmd/Pages/mid.aspx
	const std::vector<MID> JSON_MAP_MID = {
		{ 201, "Albania (Republic of)" },
		{ 202, "Andorra (Principality of)" },
		{ 203, "Austria" },
		{ 204, "Portugal - Azores" },
		{ 205, "Belgium" },
		{ 206, "Belarus (Republic of)" },
		{ 207, "Bulgaria (Republic of)" },
		{ 208, "Vatican City State" },
		{ 209, "Cyprus (Republic of)" },
		{ 210, "Cyprus (Republic of)" },
		{ 211, "Germany (Federal Republic of)" },
		{ 212, "Cyprus (Republic of)" },
		{ 213, "Georgia" },
		{ 214, "Moldova (Republic of)" },
		{ 215, "Malta" },
		{ 216, "Armenia (Republic of)" },
		{ 218, "Germany (Federal Republic of)" },
		{ 219, "Denmark" },
		{ 220, "Denmark" },
		{ 224, "Spain" },
		{ 225, "Spain" },
		{ 226, "France" },
		{ 227, "France" },
		{ 228, "France" },
		{ 229, "Malta" },
		{ 230, "Finland" },
		{ 231, "Denmark - Faroe Islands" },
		{ 232, "United Kingdom of Great Britain and Northern Ireland" },
		{ 233, "United Kingdom of Great Britain and Northern Ireland" },
		{ 234, "United Kingdom of Great Britain and Northern Ireland" },
		{ 235, "United Kingdom of Great Britain and Northern Ireland" },
		{ 236, "United Kingdom of Great Britain and Northern Ireland - Gibraltar" },
		{ 237, "Greece" },
		{ 238, "Croatia (Republic of)" },
		{ 239, "Greece" },
		{ 240, "Greece" },
		{ 241, "Greece" },
		{ 242, "Morocco (Kingdom of)" },
		{ 243, "Hungary" },
		{ 244, "Netherlands (Kingdom of the)" },
		{ 245, "Netherlands (Kingdom of the)" },
		{ 246, "Netherlands (Kingdom of the)" },
		{ 247, "Italy" },
		{ 248, "Malta" },
		{ 249, "Malta" },
		{ 250, "Ireland" },
		{ 251, "Iceland" },
		{ 252, "Liechtenstein (Principality of)" },
		{ 253, "Luxembourg" },
		{ 254, "Monaco (Principality of)" },
		{ 255, "Portugal - Madeira" },
		{ 256, "Malta" },
		{ 257, "Norway" },
		{ 258, "Norway" },
		{ 259, "Norway" },
		{ 261, "Poland (Republic of)" },
		{ 262, "Montenegro" },
		{ 263, "Portugal" },
		{ 264, "Romania" },
		{ 265, "Sweden" },
		{ 266, "Sweden" },
		{ 267, "Slovak Republic" },
		{ 268, "San Marino (Republic of)" },
		{ 269, "Switzerland (Confederation of)" },
		{ 270, "Czech Republic" },
		{ 271, "Republic of Türkiye" },
		{ 272, "Ukraine" },
		{ 273, "Russian Federation" },
		{ 274, "North Macedonia (Republic of)" },
		{ 275, "Latvia (Republic of)" },
		{ 276, "Estonia (Republic of)" },
		{ 277, "Lithuania (Republic of)" },
		{ 278, "Slovenia (Republic of)" },
		{ 279, "Serbia (Republic of)" },
		{ 301, "United Kingdom of Great Britain and Northern Ireland - Anguilla" },
		{ 303, "United States of America - Alaska (State of)" },
		{ 304, "Antigua and Barbuda" },
		{ 305, "Antigua and Barbuda" },
		{ 306, "Netherlands (Kingdom of the) - Bonaire, Sint Eustatius and Saba" },
		{ 306, "Netherlands (Kingdom of the) - Curaçao" },
		{ 306, "Netherlands (Kingdom of the) - Sint Maarten (Dutch part)" },
		{ 307, "Netherlands (Kingdom of the) - Aruba" },
		{ 308, "Bahamas (Commonwealth of the)" },
		{ 309, "Bahamas (Commonwealth of the)" },
		{ 310, "United Kingdom of Great Britain and Northern Ireland - Bermuda" },
		{ 311, "Bahamas (Commonwealth of the)" },
		{ 312, "Belize" },
		{ 314, "Barbados" },
		{ 316, "Canada" },
		{ 319, "United Kingdom of Great Britain and Northern Ireland - Cayman Islands" },
		{ 321, "Costa Rica" },
		{ 323, "Cuba" },
		{ 325, "Dominica (Commonwealth of)" },
		{ 327, "Dominican Republic" },
		{ 329, "France - Guadeloupe (French Department of)" },
		{ 330, "Grenada" },
		{ 331, "Denmark - Greenland" },
		{ 332, "Guatemala (Republic of)" },
		{ 334, "Honduras (Republic of)" },
		{ 336, "Haiti (Republic of)" },
		{ 338, "United States of America" },
		{ 339, "Jamaica" },
		{ 341, "Saint Kitts and Nevis (Federation of)" },
		{ 343, "Saint Lucia" },
		{ 345, "Mexico" },
		{ 347, "France - Martinique (French Department of)" },
		{ 348, "United Kingdom of Great Britain and Northern Ireland - Montserrat" },
		{ 350, "Nicaragua" },
		{ 351, "Panama (Republic of)" },
		{ 352, "Panama (Republic of)" },
		{ 353, "Panama (Republic of)" },
		{ 354, "Panama (Republic of)" },
		{ 355, "Panama (Republic of)" },
		{ 356, "Panama (Republic of)" },
		{ 357, "Panama (Republic of)" },
		{ 358, "United States of America - Puerto Rico" },
		{ 359, "El Salvador (Republic of)" },
		{ 361, "France - Saint Pierre and Miquelon (Territorial Collectivity of)" },
		{ 362, "Trinidad and Tobago" },
		{ 364, "United Kingdom of Great Britain and Northern Ireland - Turks and Caicos Islands" },
		{ 366, "United States of America" },
		{ 367, "United States of America" },
		{ 368, "United States of America" },
		{ 369, "United States of America" },
		{ 370, "Panama (Republic of)" },
		{ 371, "Panama (Republic of)" },
		{ 372, "Panama (Republic of)" },
		{ 373, "Panama (Republic of)" },
		{ 374, "Panama (Republic of)" },
		{ 375, "Saint Vincent and the Grenadines" },
		{ 376, "Saint Vincent and the Grenadines" },
		{ 377, "Saint Vincent and the Grenadines" },
		{ 378, "United Kingdom of Great Britain and Northern Ireland - British Virgin Islands" },
		{ 379, "United States of America - United States Virgin Islands" },
		{ 401, "Afghanistan" },
		{ 403, "Saudi Arabia (Kingdom of)" },
		{ 405, "Bangladesh (People's Republic of)" },
		{ 408, "Bahrain (Kingdom of)" },
		{ 410, "Bhutan (Kingdom of)" },
		{ 412, "China (People's Republic of)" },
		{ 413, "China (People's Republic of)" },
		{ 414, "China (People's Republic of)" },
		{ 416, "China (People's Republic of) - Taiwan (Province of China)" },
		{ 417, "Sri Lanka (Democratic Socialist Republic of)" },
		{ 419, "India (Republic of)" },
		{ 422, "Iran (Islamic Republic of)" },
		{ 423, "Azerbaijan (Republic of)" },
		{ 425, "Iraq (Republic of)" },
		{ 428, "Israel (State of)" },
		{ 431, "Japan" },
		{ 432, "Japan" },
		{ 434, "Turkmenistan" },
		{ 436, "Kazakhstan (Republic of)" },
		{ 437, "Uzbekistan (Republic of)" },
		{ 438, "Jordan (Hashemite Kingdom of)" },
		{ 440, "Korea (Republic of)" },
		{ 441, "Korea (Republic of)" },
		{ 443, "State of Palestine (In accordance with Resolution 99 Rev. Dubai, 2018)" },
		{ 445, "Democratic People's Republic of Korea" },
		{ 447, "Kuwait (State of)" },
		{ 450, "Lebanon" },
		{ 451, "Kyrgyz Republic" },
		{ 453, "China (People's Republic of) - Macao (Special Administrative Region of China)" },
		{ 455, "Maldives (Republic of)" },
		{ 457, "Mongolia" },
		{ 459, "Nepal (Federal Democratic Republic of)" },
		{ 461, "Oman (Sultanate of)" },
		{ 463, "Pakistan (Islamic Republic of)" },
		{ 466, "Qatar (State of)" },
		{ 468, "Syrian Arab Republic" },
		{ 470, "United Arab Emirates" },
		{ 471, "United Arab Emirates" },
		{ 472, "Tajikistan (Republic of)" },
		{ 473, "Yemen (Republic of)" },
		{ 475, "Yemen (Republic of)" },
		{ 477, "China (People's Republic of) - Hong Kong (Special Administrative Region of China)" },
		{ 478, "Bosnia and Herzegovina" },
		{ 501, "France - Adelie Land" },
		{ 503, "Australia" },
		{ 506, "Myanmar (Union of)" },
		{ 508, "Brunei Darussalam" },
		{ 510, "Micronesia (Federated States of)" },
		{ 511, "Palau (Republic of)" },
		{ 512, "New Zealand" },
		{ 514, "Cambodia (Kingdom of)" },
		{ 515, "Cambodia (Kingdom of)" },
		{ 516, "Australia - Christmas Island (Indian Ocean)" },
		{ 518, "New Zealand - Cook Islands" },
		{ 520, "Fiji (Republic of)" },
		{ 523, "Australia - Cocos (Keeling) Islands" },
		{ 525, "Indonesia (Republic of)" },
		{ 529, "Kiribati (Republic of)" },
		{ 531, "Lao People's Democratic Republic" },
		{ 533, "Malaysia" },
		{ 536, "United States of America - Northern Mariana Islands (Commonwealth of the)" },
		{ 538, "Marshall Islands (Republic of the)" },
		{ 540, "France - New Caledonia" },
		{ 542, "New Zealand - Niue" },
		{ 544, "Nauru (Republic of)" },
		{ 546, "France - French Polynesia" },
		{ 548, "Philippines (Republic of the)" },
		{ 550, "Timor-Leste (Democratic Republic of)" },
		{ 553, "Papua New Guinea" },
		{ 555, "United Kingdom of Great Britain and Northern Ireland - Pitcairn Island" },
		{ 557, "Solomon Islands" },
		{ 559, "United States of America - American Samoa" },
		{ 561, "Samoa (Independent State of)" },
		{ 563, "Singapore (Republic of)" },
		{ 564, "Singapore (Republic of)" },
		{ 565, "Singapore (Republic of)" },
		{ 566, "Singapore (Republic of)" },
		{ 567, "Thailand" },
		{ 570, "Tonga (Kingdom of)" },
		{ 572, "Tuvalu" },
		{ 574, "Viet Nam (Socialist Republic of)" },
		{ 576, "Vanuatu (Republic of)" },
		{ 577, "Vanuatu (Republic of)" },
		{ 578, "France - Wallis and Futuna Islands" },
		{ 601, "South Africa (Republic of)" },
		{ 603, "Angola (Republic of)" },
		{ 605, "Algeria (People's Democratic Republic of)" },
		{ 607, "France - Saint Paul and Amsterdam Islands" },
		{ 608, "United Kingdom of Great Britain and Northern Ireland - Ascension Island" },
		{ 609, "Burundi (Republic of)" },
		{ 610, "Benin (Republic of)" },
		{ 611, "Botswana (Republic of)" },
		{ 612, "Central African Republic" },
		{ 613, "Cameroon (Republic of)" },
		{ 615, "Congo (Republic of the)" },
		{ 616, "Comoros (Union of the)" },
		{ 617, "Cabo Verde (Republic of)" },
		{ 618, "France - Crozet Archipelago" },
		{ 619, "Côte d'Ivoire (Republic of)" },
		{ 620, "Comoros (Union of the)" },
		{ 621, "Djibouti (Republic of)" },
		{ 622, "Egypt (Arab Republic of)" },
		{ 624, "Ethiopia (Federal Democratic Republic of)" },
		{ 625, "Eritrea" },
		{ 626, "Gabonese Republic" },
		{ 627, "Ghana" },
		{ 629, "Gambia (Republic of the)" },
		{ 630, "Guinea-Bissau (Republic of)" },
		{ 631, "Equatorial Guinea (Republic of)" },
		{ 632, "Guinea (Republic of)" },
		{ 633, "Burkina Faso" },
		{ 634, "Kenya (Republic of)" },
		{ 635, "France - Kerguelen Islands" },
		{ 636, "Liberia (Republic of)" },
		{ 637, "Liberia (Republic of)" },
		{ 638, "South Sudan (Republic of)" },
		{ 642, "Libya (State of)" },
		{ 644, "Lesotho (Kingdom of)" },
		{ 645, "Mauritius (Republic of)" },
		{ 647, "Madagascar (Republic of)" },
		{ 649, "Mali (Republic of)" },
		{ 650, "Mozambique (Republic of)" },
		{ 654, "Mauritania (Islamic Republic of)" },
		{ 655, "Malawi" },
		{ 656, "Niger (Republic of the)" },
		{ 657, "Nigeria (Federal Republic of)" },
		{ 659, "Namibia (Republic of)" },
		{ 660, "France - Reunion (French Department of)" },
		{ 661, "Rwanda (Republic of)" },
		{ 662, "Sudan (Republic of the)" },
		{ 663, "Senegal (Republic of)" },
		{ 664, "Seychelles (Republic of)" },
		{ 665, "United Kingdom of Great Britain and Northern Ireland - Saint Helena" },
		{ 666, "Somalia (Federal Republic of)" },
		{ 667, "Sierra Leone" },
		{ 668, "Sao Tome and Principe (Democratic Republic of)" },
		{ 669, "Eswatini (Kingdom of)" },
		{ 670, "Chad (Republic of)" },
		{ 671, "Togolese Republic" },
		{ 672, "Tunisia" },
		{ 674, "Tanzania (United Republic of)" },
		{ 675, "Uganda (Republic of)" },
		{ 676, "Democratic Republic of the Congo" },
		{ 677, "Tanzania (United Republic of)" },
		{ 678, "Zambia (Republic of)" },
		{ 679, "Zimbabwe (Republic of)" },
		{ 701, "Argentine Republic" },
		{ 710, "Brazil (Federative Republic of)" },
		{ 720, "Bolivia (Plurinational State of)" },
		{ 725, "Chile" },
		{ 730, "Colombia (Republic of)" },
		{ 735, "Ecuador" },
		{ 740, "United Kingdom of Great Britain and Northern Ireland - Falkland Islands (Malvinas)" },
		{ 745, "France - Guiana (French Department of)" },
		{ 750, "Guyana" },
		{ 755, "Paraguay (Republic of)" },
		{ 760, "Peru" },
		{ 765, "Suriname (Republic of)" },
		{ 770, "Uruguay (Eastern Republic of)" },
		{ 775, "Venezuela (Bolivarian Republic of)" }
	};
}
