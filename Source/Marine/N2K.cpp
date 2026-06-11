/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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
#define B(m, v, s, l) m.setUint(s, l, v ? 1 : 0)
#define T(m, v, s, l) m.setText(s, l, v)

namespace AIS
{

	inline int ROUND(double x)
	{
		if (x < 0)
			return (int)(-0.5f + x);
		return (int)(0.5f + x);
	}

	void N2KtoMessage::startMessage(const tN2kMsg &N2kMsg, int &idx)
	{
		unsigned char byte = N2kMsg.GetByte(idx);
		int type = byte & 0x3f;
		int repeat = (byte >> 6) & 0x03;
		int mmsi = N2kMsg.Get4ByteUInt(idx);

		msg.clear();
		U(msg, type, 0, 6);
		U(msg, repeat, 6, 2);
		U(msg, mmsi, 8, 30);
	}

	N2KtoMessage::PosFix N2KtoMessage::readPosFix(const tN2kMsg &N2kMsg, int &idx)
	{
		PosFix p;
		p.lon = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LON_UNDEFINED * 600000));
		p.lat = ROUND(N2kMsg.Get4ByteDouble(1e-07 * 600000.0f, idx, LAT_UNDEFINED * 600000));
		unsigned char byte = N2kMsg.GetByte(idx);
		p.accuracy = byte & 0x01;
		p.raim = (byte >> 1) & 0x01;
		p.second = (byte >> 2) & 0x3f;
		return p;
	}

	void N2KtoMessage::readCogSog(const tN2kMsg &N2kMsg, int &idx, int &cog, int &sog)
	{
		cog = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 180.0f / PI * 10, idx, 10 * COG_UNDEFINED));
		sog = ROUND(N2kMsg.Get2ByteUDouble(0.01 * 3600.0f / 1852.0f * 10, idx, SPEED_UNDEFINED));
	}

	int N2KtoMessage::readRadioChannel(const tN2kMsg &N2kMsg, int &idx, int &channel)
	{
		int radio = N2kMsg.GetByte(idx);
		radio |= N2kMsg.GetByte(idx) << 8;
		unsigned char byte = N2kMsg.GetByte(idx);
		channel = (byte >> 3) & 7;
		radio |= (byte & 3) << 16;
		return radio;
	}

	void N2KtoMessage::finalize(char channel, TAG &tag)
	{
		msg.Stamp();
		msg.setChannel(channel);
		msg.buildNMEA(tag);
		Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129038(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		int cog, sog, channel, turn_unscaled;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);
		readCogSog(N2kMsg, idx, cog, sog);
		int radio = readRadioChannel(N2kMsg, idx, channel);
		int heading = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 360.0f / (2.0f * PI), idx, HEADING_UNDEFINED));

		double turn = N2kMsg.Get2ByteDouble(3.125E-05 / PI * 180.0f * 60.0f, idx, 0xfffff);
		if (turn == 0xfffff)
			turn_unscaled = -128;
		else
			turn_unscaled = ROUND(sqrtf(fabs(turn)) * 4.733f) * (turn < 0 ? -1 : 1);

		unsigned char byte = N2kMsg.GetByte(idx);
		int status = byte & 15;
		int maneuver = (byte >> 4) & 3;

		U(msg, status, 38, 4);
		S(msg, turn_unscaled, 42, 8);
		U(msg, sog, 50, 10);
		U(msg, p.accuracy, 60, 1);
		S(msg, p.lon, 61, 28);
		S(msg, p.lat, 89, 27);
		U(msg, cog, 116, 12);
		U(msg, heading, 128, 9);
		U(msg, p.second, 137, 6);
		U(msg, maneuver, 143, 2);
		U(msg, 0, 145, 3);
		U(msg, p.raim, 148, 1);
		U(msg, radio, 149, 19);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129793(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		int year = 0, month = 0, day = 0, channel;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);

		uint32_t u = N2kMsg.Get4ByteUInt(idx) / 10000;
		int hour = u / 3600;
		u %= 3600;
		int minute = u / 60;
		u %= 60;
		int second = u;

		int radio = readRadioChannel(N2kMsg, idx, channel);

		uint32_t days = N2kMsg.Get2ByteUInt(idx);
		time_t e = 24 * 3600 * (long int)days;
		struct tm *timeInfo = gmtime(&e);
		if (timeInfo)
		{
			year = timeInfo->tm_year + 1900;
			day = timeInfo->tm_mday;
			month = timeInfo->tm_mon + 1;
		}
		int epfd = N2kMsg.GetByte(idx) >> 4;

		U(msg, year, 38, 14);
		U(msg, month, 52, 4);
		U(msg, day, 56, 5);
		U(msg, hour, 61, 5);
		U(msg, minute, 66, 6);
		U(msg, second, 72, 6);
		B(msg, p.accuracy, 78, 1);
		S(msg, p.lon, 79, 28);
		S(msg, p.lat, 107, 27);
		U(msg, epfd, 134, 4);
		U(msg, 0, 138, 10);
		U(msg, p.raim, 148, 1);
		U(msg, radio, 149, 19);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129794(const tN2kMsg &N2kMsg, TAG &tag)
	{
		char callsign[8] = {0}, shipname[21] = {0}, destination[21] = {0};
		int day = 0, month = 0;

		int idx = 0;
		unsigned char byte;

		startMessage(N2kMsg, idx);
		int IMO = N2kMsg.Get4ByteUInt(idx);
		N2kMsg.GetStr(callsign, 7, idx);
		N2kMsg.GetStr(shipname, 20, idx);
		int shiptype = N2kMsg.GetByte(idx);
		int length = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int beam = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_bow = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int nDays = N2kMsg.Get2ByteUInt(idx);
		double nSeconds = N2kMsg.Get4ByteUDouble(0.0001, idx);
		int draught = ROUND(N2kMsg.Get2ByteDouble(0.01 * 10, idx));
		N2kMsg.GetStr(destination, 20, idx);
		byte = N2kMsg.GetByte(idx);
		int ais_version = (byte & 0x03);
		int epfd = (byte >> 2 & 0x0f);
		int dte = (byte >> 6 & 0x01);
		int channel = N2kMsg.GetByte(idx) & 0x1f;

		int hour = (int)nSeconds / 3600;
		int minute = ((int)nSeconds % 3600) / 60;

		time_t e = 24 * 3600 * (long int)nDays;
		struct tm *timeInfo = gmtime(&e);
		if (timeInfo)
		{
			day = timeInfo->tm_mday;
			month = timeInfo->tm_mon + 1;
		}

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

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129798(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		int cog, sog, channel;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);
		readCogSog(N2kMsg, idx, cog, sog);
		int radio = readRadioChannel(N2kMsg, idx, channel);
		int alt = ROUND(N2kMsg.Get4ByteDouble(0.01, idx, ALT_UNDEFINED));
		N2kMsg.GetByte(idx); // reserved
		int dte = N2kMsg.GetByte(idx) & 1;

		U(msg, alt, 38, 12);
		U(msg, sog, 50, 10);
		B(msg, p.accuracy, 60, 1);
		S(msg, p.lon, 61, 28);
		S(msg, p.lat, 89, 27);
		U(msg, cog, 116, 12);
		U(msg, p.second, 128, 6);
		U(msg, 0, 134, 8);
		U(msg, dte, 142, 1);
		U(msg, 0, 146, 1);
		U(msg, p.raim, 147, 1);
		U(msg, radio, 148, 20);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129802(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		std::string text;

		startMessage(N2kMsg, idx);
		int channel = N2kMsg.GetByte(idx) & 0x3;
		while (idx < N2kMsg.DataLen)
			text += N2kMsg.GetByte(idx);

		// truncate to what fits after the 40-bit header, setText drops the field entirely otherwise
		const size_t max_chars = (MAX_AIS_LENGTH - 40) / 6;
		if (text.length() > max_chars)
			text.resize(max_chars);

		T(msg, text.c_str(), 40, text.length() * 6);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129039(const tN2kMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		int cog, sog, channel;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);
		readCogSog(N2kMsg, idx, cog, sog);
		int radio = readRadioChannel(N2kMsg, idx, channel);
		int heading = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 360.0f / (2.0f * PI), idx, HEADING_UNDEFINED));
		int regional = N2kMsg.GetByte(idx);
		unsigned char byte = N2kMsg.GetByte(idx);
		int assigned = (byte >> 7) & 1;
		int msg22 = (byte >> 6) & 1;
		int band = (byte >> 5) & 1;
		int DSC = (byte >> 4) & 1;
		int display = (byte >> 3) & 1;
		int CS = (byte >> 2) & 1;
		// radiobit = N2kMsg.GetByte(idx) & 1;

		U(msg, 0, 38, 8);
		U(msg, sog, 46, 10);
		U(msg, p.accuracy, 56, 1);
		S(msg, p.lon, 57, 28);
		S(msg, p.lat, 85, 27);
		U(msg, cog, 112, 12);
		U(msg, heading, 124, 9);
		U(msg, p.second, 133, 6);
		U(msg, regional, 139, 2);
		B(msg, CS, 141, 1);
		B(msg, display, 142, 1);
		B(msg, DSC, 143, 1);
		B(msg, band, 144, 1);
		B(msg, msg22, 145, 1);
		B(msg, assigned, 146, 1);
		B(msg, p.raim, 147, 1);
		U(msg, radio, 148, 20); // needs fix for radiobit

		finalize('A' + channel % 2, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129040: AIS Type 19 (Class B Extended Position Report)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129040(const tN2kMsg &N2kMsg, TAG &tag)
	{
		char shipname[21] = {0};
		int idx = 0;
		int cog, speed;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);
		readCogSog(N2kMsg, idx, cog, speed);

		// Skip two spare bytes (commonly set to 0xff)
		N2kMsg.GetByte(idx);
		N2kMsg.GetByte(idx);

		int shiptype = N2kMsg.GetByte(idx);
		int heading = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 360.0f / (2.0f * PI), idx, HEADING_UNDEFINED));

		// 1 byte: EPFD (encoded as epfd << 4) and regional
		unsigned char byte = N2kMsg.GetByte(idx);
		int epfd = (byte >> 4) & 0x0F;
		int regional = byte & 0x0F;

		// 2 bytes each: dimensions (to_bow+to_stern, to_port+to_starboard, to_starboard, to_bow)
		int bow_stern = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int port_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_bow = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		N2kMsg.GetStr(shipname, 20, idx);

		// 1 byte: combined DTE and assigned flag
		byte = N2kMsg.GetByte(idx);
		int dte = byte & 0x01;
		int assigned = (byte >> 1) & 0x01;

		U(msg, 0, 38, 8);			// Reserved
		U(msg, speed, 46, 10);
		B(msg, p.accuracy, 56, 1);
		S(msg, p.lon, 57, 28);
		S(msg, p.lat, 85, 27);
		U(msg, cog, 112, 12);
		U(msg, heading, 124, 9);
		U(msg, p.second, 133, 6);
		U(msg, regional, 139, 4);
		T(msg, shipname, 143, 120);
		U(msg, shiptype, 263, 8);
		U(msg, to_bow, 271, 9);
		U(msg, bow_stern - to_bow, 280, 9);
		U(msg, port_starboard - to_starboard, 289, 6);
		U(msg, to_starboard, 295, 6);
		U(msg, epfd, 301, 4);
		B(msg, p.raim, 305, 1);
		B(msg, dte, 306, 1);
		B(msg, assigned, 307, 1);
		U(msg, 0, 308, 4);			// Spare

		finalize('A', tag); // PGN carries no channel info
	}

	//----------------------------------------------------------------------
	// PGN 129041: AIS Type 21 (Aid-to-Navigation Report)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129041(const tN2kMsg &N2kMsg, TAG &tag)
	{
		char name_buffer[21] = {0};
		int idx = 0;
		unsigned char byte;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);

		int length = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int beam = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_bow = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		byte = N2kMsg.GetByte(idx);
		int aid_type = byte & 0x1F;
		int off_position = (byte >> 5) & 0x01;
		int virtual_aid = (byte >> 6) & 0x01;
		int assigned = (byte >> 7) & 0x01;

		int epfd = N2kMsg.GetByte(idx) & 0x0F;

		N2kMsg.GetByte(idx);
		int regional = N2kMsg.GetByte(idx) & 0x7F;

		int transceiver = N2kMsg.GetByte(idx) & 0x1F;

		// Extract name with proper handling
		if (idx < N2kMsg.DataLen)
		{
			int nameIdx = 0;
			while (nameIdx < 20 && idx < N2kMsg.DataLen)
			{
				byte = N2kMsg.GetByte(idx);

				if (byte >= 32 && byte <= 126)
				{
					name_buffer[nameIdx++] = byte;
				}
				else if (byte == 0)
				{
					break; // End of string
				}
				else
				{
					name_buffer[nameIdx++] = '@'; // Replace invalid chars with @
				}
			}

			// Trim trailing spaces
			while (nameIdx > 0 && name_buffer[nameIdx - 1] == ' ')
			{
				name_buffer[--nameIdx] = 0;
			}
		}

		U(msg, aid_type, 38, 5);
		T(msg, name_buffer, 43, 120);
		B(msg, p.accuracy, 163, 1);
		S(msg, p.lon, 164, 28);
		S(msg, p.lat, 192, 27);
		U(msg, to_bow, 219, 9);
		U(msg, length - to_bow, 228, 9);
		U(msg, beam - to_starboard, 237, 6);
		U(msg, to_starboard, 243, 6);
		U(msg, epfd, 249, 4);
		U(msg, p.second, 253, 6);
		B(msg, off_position, 259, 1);
		U(msg, regional, 260, 8);
		B(msg, p.raim, 268, 1);
		B(msg, virtual_aid, 269, 1);
		B(msg, assigned, 270, 1);
		U(msg, 0, 271, 1); // Spare

		finalize('A' + (transceiver & 0x01), tag);
	}

	//----------------------------------------------------------------------
	// PGN 129809: AIS Type 24, Part A (Static Data Report - Part A)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129809(const tN2kMsg &N2kMsg, TAG &tag)
	{
		char shipname[21] = {0};
		int idx = 0;

		startMessage(N2kMsg, idx);
		N2kMsg.GetStr(shipname, 20, idx);

		U(msg, 0, 38, 2); // part number A
		T(msg, shipname, 40, 120);

		finalize('A', tag); // PGN carries no channel info
	}

	//----------------------------------------------------------------------
	// PGN 129810: AIS Type 24, Part B (Static Data Report - Part B)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129810(const tN2kMsg &N2kMsg, TAG &tag)
	{
		char vendorid[8] = {0}, callsign[8] = {0};
		int idx = 0;

		startMessage(N2kMsg, idx);
		int shiptype = N2kMsg.GetByte(idx);

		N2kMsg.GetStr(vendorid, 7, idx);
		N2kMsg.GetStr(callsign, 7, idx);

		// Dimensions: bow+stern, port+starboard, starboard, bow
		int bow_stern = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int port_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_starboard = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));
		int to_bow = ROUND(N2kMsg.Get2ByteDouble(0.1, idx));

		int mothership_mmsi = N2kMsg.Get4ByteUInt(idx);

		// Skip spare byte
		N2kMsg.GetByte(idx);

		int channel = N2kMsg.GetByte(idx) & 0x01;

		U(msg, 1, 38, 2); // part number B
		U(msg, shiptype, 40, 8);
		T(msg, vendorid, 48, 42);
		T(msg, callsign, 90, 42);
		U(msg, to_bow, 132, 9);
		U(msg, bow_stern - to_bow, 141, 9);
		U(msg, port_starboard - to_starboard, 150, 6);
		U(msg, to_starboard, 156, 6);
		U(msg, mothership_mmsi, 162, 30);

		finalize(channel ? 'B' : 'A', tag);
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
			break;
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