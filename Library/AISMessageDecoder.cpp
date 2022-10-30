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

#include "AISMessageDecoder.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

// Below is a direct translation (more or less) of https://gpsd.gitlab.io/gpsd/AIVDM.html

namespace AIS {

	void AISMessageDecoder::ProcessMsg8Data(const AIS::Message& msg, int len) {
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

	void AISMessageDecoder::Receive(const AIS::Message* data, int len, TAG& tag) {
		Submit(PROPERTY_OBJECT_START, std::string(""));

		const AIS::Message& msg = data[0];

		if (!sparse) Submit(PROPERTY_CLASS, std::string("AIS"));
		if (!sparse) Submit(PROPERTY_DEVICE, std::string("AIS-catcher"));

		if (tag.mode & 2) {
			Submit(PROPERTY_RXTIME, msg.getRxTime());
		}

		if (!sparse) Submit(PROPERTY_SCALED, true);
		Submit(PROPERTY_CHANNEL, std::string(1, msg.channel));
		if (!sparse) Submit(PROPERTY_NMEA, msg.sentence);

		if (tag.mode & 1) {
			Submit(PROPERTY_SIGNAL_POWER, tag.level);
			Submit(PROPERTY_PPM, tag.ppm);
		}

		switch (msg.type()) {
		case 1:
		case 2:
		case 3:
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			E(msg, PROPERTY_STATUS, 38, 4, PROPERTY_STATUS_TEXT, &PROPERTY_MAP_STATUS);
			TURN(msg, PROPERTY_TURN, 42, 8);
			UL(msg, PROPERTY_SPEED, 50, 10, 0.1, 0, 1023);
			B(msg, PROPERTY_ACCURACY, 60, 1);
			SL(msg, PROPERTY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 89, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 116, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 128, 9, 511);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_SECOND, 137, 6);
			E(msg, PROPERTY_MANEUVER, 143, 2);
			X(msg, PROPERTY_SPARE, 145, 3);
			B(msg, PROPERTY_RAIM, 148, 1);
			U(msg, PROPERTY_RADIO, 149, 19);
			break;
		case 4:
		case 11:
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			SL(msg, PROPERTY_LON, 79, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 107, 27, 1 / 600000.0, 0);
			E(msg, PROPERTY_EPFD, 134, 4, PROPERTY_EPFD_TEXT, &PROPERTY_MAP_EPFD);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
			TIMESTAMP(msg, PROPERTY_TIMESTAMP, 38, 40);
			U(msg, PROPERTY_YEAR, 38, 14, 0);
			U(msg, PROPERTY_MONTH, 52, 4, 0);
			U(msg, PROPERTY_DAY, 56, 5, 0);
			U(msg, PROPERTY_HOUR, 61, 5, 24);
			U(msg, PROPERTY_MINUTE, 66, 6, 60);
			U(msg, PROPERTY_SECOND, 72, 6, 60);
			B(msg, PROPERTY_ACCURACY, 78, 1);
			X(msg, PROPERTY_SPARE, 138, 10);
			B(msg, PROPERTY_RAIM, 148, 1);
			U(msg, PROPERTY_RADIO, 149, 19);
			break;
		case 5:
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_AIS_VERSION, 38, 2);
			U(msg, PROPERTY_IMO, 40, 30);
			T(msg, PROPERTY_CALLSIGN, 70, 42);
			T(msg, PROPERTY_SHIPNAME, 112, 120);
			E(msg, PROPERTY_SHIPTYPE, 232, 8, PROPERTY_SHIPTYPE_TEXT, &PROPERTY_MAP_SHIPTYPE);
			U(msg, PROPERTY_TO_BOW, 240, 9);
			U(msg, PROPERTY_TO_STERN, 249, 9);
			U(msg, PROPERTY_TO_PORT, 258, 6);
			U(msg, PROPERTY_TO_STARBOARD, 264, 6);
			E(msg, PROPERTY_EPFD, 270, 4, PROPERTY_EPFD_TEXT, &PROPERTY_MAP_EPFD);
			ETA(msg, PROPERTY_ETA, 274, 20);
			T(msg, PROPERTY_DESTINATION, 302, 120);
			UL(msg, PROPERTY_DRAUGHT, 294, 8, 0.1, 0);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MONTH, 274, 4, 0);
			U(msg, PROPERTY_DAY, 278, 5, 0);
			U(msg, PROPERTY_HOUR, 283, 5, 0);
			U(msg, PROPERTY_MINUTE, 288, 6, 0);
			B(msg, PROPERTY_DTE, 422, 1);
			X(msg, PROPERTY_SPARE, 423, 1);
			break;
		case 6:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_SEQNO, 38, 2);
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			B(msg, PROPERTY_RETRANSMIT, 70, 1);
			X(msg, PROPERTY_SPARE, 71, 1);
			U(msg, PROPERTY_DAC, 72, 10);
			U(msg, PROPERTY_FID, 82, 6);
			break;
		case 7:
		case 13:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_DAC, 40, 10);
			U(msg, PROPERTY_FID, 50, 6);
			ProcessMsg8Data(msg, len);
			break;
		case 9:
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_ALT, 38, 12);
			U(msg, PROPERTY_SPEED, 50, 10);
			B(msg, PROPERTY_ACCURACY, 60, 1);
			SL(msg, PROPERTY_LON, 61, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 89, 27, 1 / 600000.0, 0);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
			UL(msg, PROPERTY_COURSE, 116, 12, 0.1, 0);
			U(msg, PROPERTY_SECOND, 128, 6);
			X(msg, PROPERTY_REGIONAL, 134, 8);
			B(msg, PROPERTY_DTE, 142, 1);
			B(msg, PROPERTY_ASSIGNED, 146, 1);
			B(msg, PROPERTY_RAIM, 147, 1);
			U(msg, PROPERTY_RADIO, 148, 20);
			break;
		case 10:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			break;
		case 12:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_SEQNO, 38, 2);
			U(msg, PROPERTY_DEST_MMSI, 40, 30);
			B(msg, PROPERTY_RETRANSMIT, 70, 1);
			T(msg, PROPERTY_TEXT, 72, 936);
			break;
		case 14:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			T(msg, PROPERTY_TEXT, 40, 968);
			break;
		case 15:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_MMSI1, 40, 30);
			U(msg, PROPERTY_OFFSET1, 70, 12);
			U(msg, PROPERTY_INCREMENT1, 82, 10);
			break;
		case 17:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			SL(msg, PROPERTY_LON, 40, 18, 1 / 600.0, 0);
			SL(msg, PROPERTY_LAT, 58, 17, 1 / 600.0, 0);
			D(msg, PROPERTY_DATA, 80, 736);
			break;
		case 18:
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			UL(msg, PROPERTY_SPEED, 46, 10, 0.1, 0);
			B(msg, PROPERTY_ACCURACY, 56, 1);
			SL(msg, PROPERTY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 112, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 124, 9);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
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
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_MMSI, 8, 30);
			UL(msg, PROPERTY_SPEED, 46, 10, 0.1, 0);
			SL(msg, PROPERTY_LON, 57, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 85, 27, 1 / 600000.0, 0);
			UL(msg, PROPERTY_COURSE, 112, 12, 0.1, 0);
			U(msg, PROPERTY_HEADING, 124, 9);
			T(msg, PROPERTY_SHIPNAME, 143, 120);
			E(msg, PROPERTY_SHIPTYPE, 263, 8, PROPERTY_SHIPTYPE_TEXT, &PROPERTY_MAP_SHIPTYPE);
			U(msg, PROPERTY_TO_BOW, 271, 9);
			U(msg, PROPERTY_TO_STERN, 280, 9);
			U(msg, PROPERTY_TO_PORT, 289, 6);
			U(msg, PROPERTY_TO_STARBOARD, 295, 6);
			E(msg, PROPERTY_EPFD, 301, 4, PROPERTY_EPFD_TEXT, &PROPERTY_MAP_EPFD);
			if (sparse) break;
			U(msg, PROPERTY_REPEAT, 6, 2);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			E(msg, PROPERTY_AID_TYPE, 38, 5, PROPERTY_AID_TYPE_TEXT, &PROPERTY_MAP_AID_TYPE);
			T(msg, PROPERTY_NAME, 43, 120);
			B(msg, PROPERTY_ACCURACY, 163, 1);
			SL(msg, PROPERTY_LON, 164, 28, 1 / 600000.0, 0);
			SL(msg, PROPERTY_LAT, 192, 27, 1 / 600000.0, 0);
			U(msg, PROPERTY_TO_BOW, 219, 9);
			U(msg, PROPERTY_TO_STERN, 228, 9);
			U(msg, PROPERTY_TO_PORT, 237, 6);
			U(msg, PROPERTY_TO_STARBOARD, 243, 6);
			E(msg, PROPERTY_EPFD, 249, 4, PROPERTY_EPFD_TEXT, &PROPERTY_MAP_EPFD);
			U(msg, PROPERTY_SECOND, 253, 6);
			B(msg, PROPERTY_OFF_POSITION, 259, 1);
			U(msg, PROPERTY_REGIONAL, 260, 8);
			B(msg, PROPERTY_RAIM, 268, 1);
			B(msg, PROPERTY_VIRTUAL_AID, 269, 1);
			B(msg, PROPERTY_ASSIGNED, 270, 1);
			break;
		case 22:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_CHANNEL_A, 40, 12);
			U(msg, PROPERTY_CHANNEL_B, 52, 12);
			U(msg, PROPERTY_TXRX, 64, 4);
			B(msg, PROPERTY_POWER, 68, 1);
			SL(msg, PROPERTY_NE_LON, 69, 18, 0.1, 0);
			SL(msg, PROPERTY_NE_LAT, 87, 17, 0.1, 0);
			SL(msg, PROPERTY_SW_LON, 104, 18, 0.1, 0);
			SL(msg, PROPERTY_SW_LAT, 122, 17, 0.1, 0);
			U(msg, PROPERTY_DEST1, 69, 30);
			U(msg, PROPERTY_DEST2, 104, 30);
			B(msg, PROPERTY_ADDRESSED, 139, 1);
			B(msg, PROPERTY_BAND_A, 140, 1);
			B(msg, PROPERTY_BAND_B, 141, 1);
			U(msg, PROPERTY_ZONESIZE, 142, 3);
			break;
		case 23:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			U(msg, PROPERTY_PARTNO, 38, 2);

			if (msg.getUint(38, 2) == 0) {
				T(msg, PROPERTY_SHIPNAME, 40, 120);
			}
			else {
				E(msg, PROPERTY_SHIPTYPE, 40, 8, PROPERTY_SHIPTYPE_TEXT, &PROPERTY_MAP_SHIPTYPE);
				T(msg, PROPERTY_VENDORID, 48, 18);
				U(msg, PROPERTY_MODEL, 66, 4);
				U(msg, PROPERTY_SERIAL, 70, 20);
				T(msg, PROPERTY_CALLSIGN, 90, 42);
				U(msg, PROPERTY_TO_BOW, 132, 9);
				U(msg, PROPERTY_TO_STERN, 141, 9);
				U(msg, PROPERTY_TO_PORT, 150, 6);
				U(msg, PROPERTY_TO_STARBOARD, 156, 6);
				U(msg, PROPERTY_MOTHERSHIP_MMSI, 132, 30);
			}
			break;
		case 27:
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
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
			if (sparse) break;
			U(msg, PROPERTY_TYPE, 0, 6);
			U(msg, PROPERTY_REPEAT, 6, 2);
			U(msg, PROPERTY_MMSI, 8, 30);
			break;
		}
		Submit(PROPERTY_OBJECT_END, std::string(""));
	}

	// Below is a direct translation (more or less) of https://gpsd.gitlab.io/gpsd/AIVDM.html

	const std::vector<std::string> PROPERTY_MAP_STATUS = {
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
		"Not defined (default)"
	};

	const std::vector<std::string> PROPERTY_MAP_EPFD = {
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

	const std::vector<std::string> PROPERTY_MAP_SHIPTYPE = {
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

	const std::vector<std::string> PROPERTY_MAP_AID_TYPE = {
		"Default, Type of Aid to Navigation not specified",
		"Reference point",
		"RACON (radar transponder marking a navigation hazard)",
		"Fixed structure off shore, such as oil platforms, wind farms, rigs.",
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
}
