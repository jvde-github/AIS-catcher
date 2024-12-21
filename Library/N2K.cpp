/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

// Sources:
// 	https://github.com/canboat/canboat
// 	https://github.com/ttlappalainen/NMEA2000
//	https://github.com/AK-Homberger/NMEA2000-AIS-Gateway

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


namespace AIS {

	inline int ROUND(double x) {
		if (x < 0) return (int)(-0.5f + x);
		return (int)(0.5f + x);
	}

	void N2KtoMessage::onMsg129038(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, status, type, second, mmsi, accuracy, raim, radio, maneuver, channel, heading, lat, lon, cog, sog, turn_unscaled;
		double turn;

		int idx = 0;
		unsigned char byte;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));
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

		turn = N2kMsg.Get2ByteDouble(3.125E-05 / PI * 180.0f * 60.0f, idx, 0xfffff);
		if (turn == 0xfffff)
			turn_unscaled = -128;
		else
			turn_unscaled = ROUND(sqrtf(turn) * 4.733f) * (turn < 0 ? -1 : 1);

		byte = N2kMsg.GetByte(idx);
		status = byte & 15;
		maneuver = (byte >> 4) & 3;

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, status, 38, 4);
		S(msg, turn_unscaled, 42, 8);
		U(msg, sog, 50, 10);
		U(msg, accuracy, 60, 1);
		S(msg, lon, 61, 28);
		S(msg, lat, 89, 27);
		U(msg, cog, 89 + 27, 12);
		U(msg, heading, 128, 9);
		U(msg, second, 137, 6);
		U(msg, maneuver, 143, 2);
		U(msg, 0, 145, 3);
		U(msg, raim, 148, 1);
		U(msg, radio, 149, 19);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129793(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, type, mmsi, channel;
		int year = 0, month = 0, day = 0, hour, minute, second, accuracy, epfd, raim, radio;
		int lat, lon;

		int idx = 0;
		unsigned char byte;
		uint32_t u, days;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);

		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));

		byte = N2kMsg.GetByte(idx);
		accuracy = (byte & 1);
		raim = (byte >> 1) & 1;
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

		msg.clear();

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
		U(msg, epfd, 134, 4);
		U(msg, 0, 138, 10);
		U(msg, raim, 148, 1);
		U(msg, radio, 149, 19);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129794(const tN2kMsg& N2kMsg, TAG& tag) {

		int mmsi, IMO;
		int hour, minute, day = 0, month = 0;
		char callsign[8] = { 0 }, shipname[21] = { 0 }, destination[21] = { 0 };
		int length, beam, to_starboard, to_bow, draught;
		int type;
		int repeat, shiptype;
		int ais_version;
		int epfd;
		int dte, channel;
		int nDays;
		double nSeconds;

		int idx = 0;
		unsigned char byte;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = byte >> 6 & 3;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		IMO = N2kMsg.Get4ByteUInt(idx);
		N2kMsg.GetStr(callsign, 7, idx);
		N2kMsg.GetStr(shipname, 20, idx);
		shiptype = N2kMsg.GetByte(idx);
		length = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		beam = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_bow = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		nDays = N2kMsg.Get2ByteUInt(idx);
		nSeconds = N2kMsg.Get4ByteUDouble(0.0001, idx);
		draught = N2kMsg.Get2ByteDouble(0.01 * 10, idx);
		N2kMsg.GetStr(destination, 20, idx);
		byte = N2kMsg.GetByte(idx);
		ais_version = (byte & 0x03);
		epfd = (byte >> 2 & 0x0f);
		dte = (byte >> 6 & 0x01);
		channel = N2kMsg.GetByte(idx) & 0x1f;

		hour = (int)nSeconds / 3600;
		minute = (int)nSeconds % 60;

		time_t e = 24 * 3600 * (long int)nDays;
		struct tm* timeInfo = gmtime(&e);
		if (timeInfo) {
			day = timeInfo->tm_mday;
			month = timeInfo->tm_mon + 1;
		}

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, ais_version, 38, 2);
		U(msg, IMO, 40, 30);
		T(msg, callsign, 70, 42);
		T(msg, shipname, 112, 120);
		U(msg, shiptype, 232, 8);
		U(msg, to_bow, 240, 9);
		U(msg, length - to_bow, 249, 9);
		U(msg, beam - to_starboard, 258, 6);
		U(msg, to_starboard, 264, 6);
		U(msg, epfd, 270, 4);
		U(msg, month, 274, 4);
		U(msg, day, 278, 5);
		U(msg, hour, 283, 5);
		U(msg, minute, 288, 6);
		U(msg, draught, 294, 8);
		T(msg, destination, 302, 120);
		U(msg, dte, 422, 1);
		U(msg, 0, 423, 1); // spare

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129798(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, type, second, mmsi, accuracy, raim, radio, channel, lat, lon, cog, sog, alt, dte;

		int idx = 0;
		unsigned char byte;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));
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
		alt = ROUND(N2kMsg.Get4ByteDouble(0.01, idx, ALT_UNDEFINED));
		byte = N2kMsg.GetByte(idx);
		dte = N2kMsg.GetByte(idx) & 1;

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, alt, 38, 12);
		U(msg, sog, 50, 10);
		B(msg, accuracy, 60, 1);
		S(msg, lon, 61, 28);
		S(msg, lat, 89, 27);
		U(msg, cog, 116, 12);
		U(msg, second, 128, 6);
		U(msg, 0, 134, 8);
		U(msg, dte, 142, 1);
		U(msg, 0, 146, 1);
		U(msg, raim, 147, 1);
		U(msg, radio, 148, 20);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}


	void N2KtoMessage::onMsg129802(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, type, mmsi, channel;

		int idx = 0;
		unsigned char byte;
		std::string text;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		channel = N2kMsg.GetByte(idx) & 0x3;
		while (idx < N2kMsg.DataLen)
			text += N2kMsg.GetByte(idx);

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		T(msg, text.c_str(), 40, text.length() * 6);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);

		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129039(const tN2kMsg& N2kMsg, TAG& tag) {
		int repeat, type, second, mmsi, accuracy, raim, radio, channel, heading, lat, lon, cog, sog /*, radiobit*/;
		int regional, assigned, msg22, band, DSC, display, CS;

		int idx = 0;
		unsigned char byte;

		byte = N2kMsg.GetByte(idx);
		type = (byte & 0x3f);
		repeat = (byte >> 6) & 0x03;
		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));
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
		regional = N2kMsg.GetByte(idx);
		byte = N2kMsg.GetByte(idx);
		assigned = (byte >> 7) & 1;
		msg22 = (byte >> 6) & 1;
		band = (byte >> 5) & 1;
		DSC = (byte >> 4) & 1;
		display = (byte >> 3) & 1;
		CS = (byte >> 2) & 1;
		//radiobit = N2kMsg.GetByte(idx) & 1;

		msg.clear();

		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, sog, 46, 10);
		U(msg, accuracy, 56, 1);
		S(msg, lon, 57, 28);
		S(msg, lat, 85, 27);
		U(msg, cog, 112, 12);
		U(msg, heading, 124, 9);
		U(msg, 0, 38, 8);
		U(msg, second, 133, 6);
		U(msg, regional, 139, 2);
		B(msg, CS, 141, 1);
		B(msg, display, 142, 1);
		B(msg, DSC, 143, 1);
		B(msg, band, 144, 1);
		B(msg, msg22, 145, 1);
		B(msg, assigned, 146, 1);
		B(msg, raim, 147, 1);
		U(msg, radio, 148, 20); // needs fix for radiobit

		msg.Stamp();
		msg.setChannel('A' + channel);
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
		case 129798:
			onMsg129798(N2kMsg, tag);
			break;
		case 129802:
			onMsg129802(N2kMsg, tag);
			break;
		case 129039:
			onMsg129039(N2kMsg, tag);
			break;
		default:
			break;
		}
	}
}

#endif