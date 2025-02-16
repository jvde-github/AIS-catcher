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

#define X(m, v, s, l) m.setUint(s, l, v)
#define UL(m, v, s, l, alpha, beta) m.setUint(s, l, (unsigned int)(0.5f + ((v) - (beta)) / (alpha)))
#define SL(m, v, s, l, alpha, beta) m.setInt(s, l, (int)(0.5f + ((v) - (beta)) / (alpha)))
#define E(m, v, s, l) m.setUint(s, l, v)
#define B(m, v, s, l) m.setUint(s, l, v ? 1 : 0)
#define TURN(m, v, s, l) m.setInt(s, l, v == 10000 ? -128 : (unsigned int)(0.5f + v / 5.0f))
#define T(m, v, s, l) m.setText(s, l, v)

namespace AIS
{

	inline int ROUND(double x)
	{
		if (x < 0)
			return (int)(-0.5f + x);
		return (int)(0.5f + x);
	}

	void N2KtoMessage::onMsg129038(const tN2kMsg &N2kMsg, TAG &tag)
	{
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

	void N2KtoMessage::onMsg129793(const tN2kMsg &N2kMsg, TAG &tag)
	{
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
		struct tm *timeInfo = gmtime(&e);
		if (timeInfo)
		{
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

	void N2KtoMessage::onMsg129794(const tN2kMsg &N2kMsg, TAG &tag)
	{

		int mmsi, IMO;
		int hour, minute, day = 0, month = 0;
		char callsign[8] = {0}, shipname[21] = {0}, destination[21] = {0};
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
		struct tm *timeInfo = gmtime(&e);
		if (timeInfo)
		{
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

	void N2KtoMessage::onMsg129798(const tN2kMsg &N2kMsg, TAG &tag)
	{
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

	void N2KtoMessage::onMsg129802(const tN2kMsg &N2kMsg, TAG &tag)
	{
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

	void N2KtoMessage::onMsg129039(const tN2kMsg &N2kMsg, TAG &tag)
	{
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
		// radiobit = N2kMsg.GetByte(idx) & 1;

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

	//----------------------------------------------------------------------
	// PGN 129040: AIS Type 19 (Class B Extended Position Report)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129040(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int repeat, type, mmsi;
		int second, raim, accuracy;
		int lon, lat;
		int cog, speed;
		int shiptype, heading, epfd;
		int bow_stern, port_starboard, to_starboard, to_bow;
		int dte_assigned;
		char shipname[21] = {0};

		int idx = 0;
		unsigned char byte;

		// Extract type and repeat from the first byte
		byte = N2kMsg.GetByte(idx);
		type = byte & 0x3F;
		repeat = (byte >> 6) & 0x03;

		mmsi = N2kMsg.Get4ByteUInt(idx);

		// Extract longitude and latitude using the same scaling as other handlers
		lon = AIS::ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = AIS::ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));

		// Next byte: seconds, raim, and accuracy
		byte = N2kMsg.GetByte(idx);
		second = (byte >> 2) & 0x3F;
		raim = (byte >> 1) & 0x01;
		accuracy = byte & 0x01;

		// Two bytes: COG and SOG
		cog = AIS::ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 180.0f / PI, idx, COG_UNDEFINED));
		speed = AIS::ROUND(N2kMsg.Get2ByteUDouble(0.01 * 3600.0f / 1852.0f, idx, SPEED_UNDEFINED));

		// Skip two spare bytes (commonly set to 0xff)
		N2kMsg.GetByte(idx);
		N2kMsg.GetByte(idx);

		// 1 byte: ship type
		shiptype = N2kMsg.GetByte(idx);
		// 2 bytes: Heading
		heading = AIS::ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 360.0f / (2.0f * PI), idx, HEADING_UNDEFINED));

		// 1 byte: EPFD (encoded as epfd << 4)
		byte = N2kMsg.GetByte(idx);
		epfd = (byte >> 4) & 0x0F;

		// 2 bytes each: dimensions (to_bow+to_stern, to_port+to_starboard, to_starboard, to_bow)
		bow_stern = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		port_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_bow = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		// 20-character ship name
		N2kMsg.GetStr(shipname, 20, idx);

		// 1 byte: combined DTE and assigned flag
		dte_assigned = N2kMsg.GetByte(idx);

		// Skip two spare bytes
		N2kMsg.GetByte(idx);
		N2kMsg.GetByte(idx);

		// Build the AIS message (bit offsets and lengths are exemplary)
		msg.clear();
		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, lon, 38, 28);
		S(msg, lat, 66, 27);
		U(msg, cog, 93, 12);
		U(msg, speed, 105, 10);
		U(msg, second, 115, 6);
		U(msg, shiptype, 121, 8);
		U(msg, heading, 129, 9);
		U(msg, epfd, 138, 4);
		U(msg, bow_stern, 142, 10);
		U(msg, port_starboard, 152, 10);
		U(msg, to_starboard, 162, 9);
		U(msg, to_bow, 171, 9);
		T(msg, shipname, 180, 120);
		U(msg, dte_assigned, 300, 8);

		msg.Stamp();
		msg.setChannel('A'); // Adjust channel extraction if needed
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129041: AIS Type 21 (Aid-to-Navigation Report)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129041(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int repeat, type, mmsi;
		int second, raim, accuracy;
		int lon, lat;
		int to_bow_stern, to_port_starboard, to_starboard, to_bow;
		int combined; // Contains flags: assigned, virtual aid, off-position, and aid type
		int epfd, regional, channel;
		std::string name;

		int idx = 0;
		unsigned char byte;

		// Extract header: type and repeat
		byte = N2kMsg.GetByte(idx);
		type = byte & 0x3F;
		repeat = (byte >> 6) & 0x03;

		mmsi = N2kMsg.Get4ByteUInt(idx);
		lon = AIS::ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		lat = AIS::ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));

		// Next byte: seconds, raim, and accuracy
		byte = N2kMsg.GetByte(idx);
		second = (byte >> 2) & 0x3F;
		raim = (byte >> 1) & 0x01;
		accuracy = byte & 0x01;

		// Dimensions: to_bow+to_stern, to_port+to_starboard, to_starboard, to_bow
		to_bow_stern = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_port_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_bow = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		// Extract combined flags (assigned, virtual aid, off-position, aid type)
		combined = N2kMsg.GetByte(idx);

		// Extract EPFD and regional bits
		epfd = N2kMsg.GetByte(idx);
		regional = N2kMsg.GetByte(idx);
		// Extract channel (simplified extraction)
		byte = N2kMsg.GetByte(idx);
		channel = (byte & 0x01);

		// Variable-length Aid name: extract remaining bytes
		while (idx < N2kMsg.DataLen)
		{
			name.push_back(N2kMsg.GetByte(idx));
		}

		msg.clear();
		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, lon, 38, 28);
		S(msg, lat, 66, 27);
		U(msg, second, 93, 6);
		U(msg, to_bow_stern, 99, 10);
		U(msg, to_port_starboard, 109, 10);
		U(msg, to_starboard, 119, 9);
		U(msg, to_bow, 128, 9);
		U(msg, combined, 137, 8);
		U(msg, epfd, 145, 8);
		U(msg, regional, 153, 8);
		U(msg, channel, 161, 8);
		T(msg, name.c_str(), 169, name.size() * 6);

		msg.Stamp();
		msg.setChannel('A' + channel);
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129809: AIS Type 24, Part A (Static Data Report - Part A)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129809(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int repeat, type, mmsi;
		char shipname[21] = {0};
		int idx = 0;
		unsigned char byte;

		// Extract header: type and repeat
		byte = N2kMsg.GetByte(idx);
		type = byte & 0x3F;
		repeat = (byte >> 6) & 0x03;

		mmsi = N2kMsg.Get4ByteUInt(idx);

		// Extract the 20-character ship name
		N2kMsg.GetStr(shipname, 20, idx);

		msg.clear();
		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		T(msg, shipname, 38, 120);
		msg.Stamp();
		msg.setChannel('A');
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129810: AIS Type 24, Part B (Static Data Report - Part B)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129810(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int repeat, type, mmsi;
		int shiptype;
		char vendorid[8] = {0}, callsign[8] = {0};
		int bow_stern, port_starboard, to_starboard, to_bow;
		int mothership_mmsi;
		int idx = 0;
		unsigned char byte;

		// Extract header: type and repeat
		byte = N2kMsg.GetByte(idx);
		type = byte & 0x3F;
		repeat = (byte >> 6) & 0x03;

		mmsi = N2kMsg.Get4ByteUInt(idx);

		// Extract ship type
		shiptype = N2kMsg.GetByte(idx);

		// Extract vendor ID and callsign (7 characters each)
		N2kMsg.GetStr(vendorid, 7, idx);
		N2kMsg.GetStr(callsign, 7, idx);

		// Extract dimensions: bow+stern, port+starboard, starboard, bow
		bow_stern = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		port_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_starboard = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		to_bow = AIS::ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		// Extract mothership MMSI
		mothership_mmsi = N2kMsg.Get4ByteUInt(idx);

		// Skip spare byte
		N2kMsg.GetByte(idx);

		// Extract channel from the next byte (simplified extraction)
		byte = N2kMsg.GetByte(idx);
		int channel = (byte & 0x01);
		// Skip sequence ID byte
		N2kMsg.GetByte(idx);

		msg.clear();
		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
		U(msg, shiptype, 38, 8);
		T(msg, vendorid, 46, 42);
		T(msg, callsign, 88, 42);
		U(msg, bow_stern, 130, 10);
		U(msg, port_starboard, 140, 10);
		U(msg, to_starboard, 150, 10);
		U(msg, to_bow, 160, 10);
		U(msg, mothership_mmsi, 170, 32);
		msg.Stamp();
		msg.setChannel(channel ? 'B' : 'A');
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
	}

	void N2KtoMessage::Receive(const RAW *data, int len, TAG &tag)
	{
		if (len == 0)
			return;
		tN2kMsg N2kMsg = *((tN2kMsg *)data->data);

		switch (N2kMsg.PGN)
		{
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
		case 129040:
			onMsg129040(N2kMsg, tag);
			break;
		case 129041:
			onMsg129041(N2kMsg, tag);
			break;
		case 129809:
			onMsg129809(N2kMsg, tag);
			break;
		case 129810:
			onMsg129810(N2kMsg, tag);
			break;
		default:
			break;
		}
	}
}

#endif