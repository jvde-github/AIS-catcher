/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#ifdef HASNMEA2000
#include <N2kMsg.h>
#include "N2K.h"


#define U(m, v, s, l) m.setUint(s, l, v)
#define S(m, v, s, l) m.setInt(s, l, v)

#define X(m, v, s, l)				m.setUint(s, l, v)
#define UL(m, v, s, l, alpha, beta) m.setUint(s, l, (unsigned int)(0.5f + ((v) - (beta)) / (alpha)))
#define SL(m, v, s, l, alpha, beta) m.setInt(s, l, (int)(0.5f + ((v) - (beta)) / (alpha)))
#define E(m, v, s, l)				m.setUint(s, l, v)
#define B(m, v, s, l)				m.setUint(s, l, v ? 1 : 0)
#define TURN(m, v, s, l)			m.setInt(s, l, v == 10000 ? -128 : (unsigned int)(0.5f + v / 5.0f))
#define T(m, v, s, l)				m.setText(s, l, v)

#define ROUND(x)					((int)(0.5f + (x)))

namespace AIS {

	void N2KtoMessage::onMsg129038(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, status, type, second, mmsi, accuracy, raim, radio, maneuver, channel, heading;
		double lat, lon;
		int cog, sog, rot;

		int idx = 0;
		unsigned char byte;

		// Based on CANboat description, NMEA2000 library
		// TO-DO: deal with undefined values
		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED));
		byte = N2kMsg.GetByte(idx);
		accuracy = (byte & 0x01);
		raim = (byte >> 1) & 0x01;
		second = (byte >> 2) & 0x3f;
		cog = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 180.0f / PI * 10, idx, 10 * COG_UNDEFINED));
		sog = ROUND(N2kMsg.Get2ByteUDouble(0.01 * 3600.0f / 1852.0f * 10, idx, SPEED_UNDEFINED));
		radio = N2kMsg.GetByte(idx);
		radio |= N2kMsg.GetByte(idx) << 8;
		byte = N2kMsg.GetByte(idx);
		channel = (byte >> 3) & 7;
		radio |= (byte & 3) << 16;
		heading = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 360.0f / (2.0f * PI), idx, HEADING_UNDEFINED));
		rot = N2kMsg.Get2ByteDouble(3.125E-05 / PI * 180.0f * 60.0f, idx, 10000);
		byte = N2kMsg.GetByte(idx);
		status = byte & 15;
		maneuver = (byte >> 4) & 3;

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		E(msg, status, 38, 4);
		TURN(msg, rot, 42, 8);
		U(msg, sog, 50, 10);
		B(msg, accuracy, 60, 1);
		S(msg, lon, 61, 28);
		S(msg, lat, 89, 27);
		U(msg, cog, 89 + 27, 12);
		U(msg, heading, 128, 9);
		U(msg, second, 137, 6);
		E(msg, maneuver, 143, 2);
		X(msg, 0, 145, 3);
		B(msg, raim, 148, 1);
		U(msg, radio, 149, 19);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129793(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, type, mmsi, channel;
		int year = 0, month = 0, day = 0, hour, minute, second, accuracy, epfd, raim, radio;
		double lat, lon;

		int idx = 0;
		unsigned char byte;
		uint32_t u, days;

		// Based on CANboat description, NMEA2000 library
		// TO-DO: deal with undefined values
		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED));
		byte = N2kMsg.GetByte(idx);
		accuracy = (byte & 0x01);
		raim = (byte >> 1) & 0x01;
		u = N2kMsg.Get4ByteUInt(idx) / 10000;
		hour = u / 3600;
		u %= 3600;
		minute = u / 60;
		u %= 60;
		second = u;
		radio = N2kMsg.GetByte(idx);
		radio |= N2kMsg.GetByte(idx) << 8;
		byte = N2kMsg.GetByte(idx);
		channel = (byte >> 3) & 7;
		radio |= (byte & 3) << 16;
		days = N2kMsg.Get2ByteUInt(idx);
		time_t e = 24 * 3600 * (long int)days;
		struct tm* timeInfo = gmtime(&e);
		if (timeInfo) {
			year = timeInfo->tm_year + 1900;
			day = timeInfo->tm_mday;
			month = timeInfo->tm_mon + 1;
		}
		byte = N2kMsg.GetByte(idx);
		epfd = byte >> 4;

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, year, 38, 14);
		U(msg, month, 52, 4);
		U(msg, day, 56, 5);
		U(msg, hour, 61, 5);
		U(msg, minute, 66, 6);
		U(msg, second, 72, 6);
		B(msg, accuracy, 78, 1);
		S(msg, lon, 79, 28);
		S(msg, lat, 107, 27);
		E(msg, epfd, 134, 4);
		X(msg, 0, 138, 10);
		B(msg, raim, 148, 1);
		U(msg, radio, 149, 19);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129794(const tN2kMsg& N2kMsg, TAG& tag) {

		uint32_t mmsi, IMO;
		int hour, minute, day = 0, month = 0;
		char callsign[8] = { 0 }, shipname[21] = { 0 }, destination[21] = { 0 };
		double length, beam, posrefstbd, posrefbow, draught;
		uint8_t type, vesseltype;
		tN2kAISRepeat repeat;
		tN2kAISVersion AISversion;
		tN2kGNSStype GNSStype;
		tN2kAISDTE DTE;
		tN2kAISTransceiverInformation AISinfo;
		uint16_t ETAdate;
		double ETAtime;

		if (!ParseN2kPGN129794(N2kMsg, type, repeat, mmsi, IMO, callsign, shipname, vesseltype, length, beam, posrefstbd, posrefbow, ETAdate, ETAtime, draught, destination, AISversion, GNSStype, DTE, AISinfo))
			return;

		hour = (int)ETAtime / 3600;
		minute = (int)ETAtime % 60;

		time_t e = 24 * 3600 * (long int)ETAdate;
		struct tm* timeInfo = gmtime(&e);
		if (timeInfo) {
			day = timeInfo->tm_mday;
			month = timeInfo->tm_mon + 1;
		}

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, AISversion, 38, 2);
		U(msg, IMO, 40, 30);
		T(msg, callsign, 70, 42);
		T(msg, shipname, 112, 120);
		E(msg, vesseltype, 232, 8);
		U(msg, posrefbow, 240, 9);
		U(msg, length - posrefbow, 249, 9);
		U(msg, beam - posrefstbd, 258, 6);
		U(msg, posrefstbd, 264, 6);
		E(msg, GNSStype, 270, 4);
		U(msg, month, 274, 4);
		U(msg, day, 278, 5);
		U(msg, hour, 283, 5);
		U(msg, minute, 288, 6);
		UL(msg, draught, 294, 8, 0.1f, 0);
		T(msg, destination, 302, 120);
		B(msg, DTE, 422, 1);
		X(msg, 0, 423, 1); // spare

		msg.Stamp();
		msg.setChannel('A');
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::Receive(const RAW* data, int len, TAG& tag) {
		if (len == 0) return;
		tN2kMsg N2kMsg = *((tN2kMsg*)data->data);

		switch (N2kMsg.PGN) {
		case 129038:
			onMsg129038(N2kMsg, tag);
			break;
		case 129793:
			onMsg129793(N2kMsg, tag);
		case 129794:
			onMsg129794(N2kMsg, tag);
			break;
		default:
			break;
		}
	}
}

#endif