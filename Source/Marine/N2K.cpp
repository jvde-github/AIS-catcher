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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "JSONAIS.h"
#include "N2K.h"

#define U(m, v, s, l) m.setUint(s, l, v)
#define S(m, v, s, l) m.setInt(s, l, v)
#define B(m, v, s, l) m.setUint(s, l, v ? 1 : 0)
#define T(m, v, s, l) m.setText(s, l, v)

namespace AIS
{
	// gmtime without its one shared result: NMEA 2000 inputs decode on threads of their own
	static bool utc(std::time_t t, std::tm &out)
	{
#ifdef _WIN32
		return gmtime_s(&out, &t) == 0;
#else
		return gmtime_r(&t, &out) != nullptr;
#endif
	}

	inline int ROUND(double x)
	{
		return (int)(x < 0 ? x - 0.5 : x + 0.5);
	}

	void N2KtoMessage::startMessage(const N2KMsg &N2kMsg, int &idx)
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

	N2KtoMessage::PosFix N2KtoMessage::readPosFix(const N2KMsg &N2kMsg, int &idx)
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

	void N2KtoMessage::readCogSog(const N2KMsg &N2kMsg, int &idx, int &cog, int &sog)
	{
		cog = ROUND(N2kMsg.Get2ByteUDouble(1e-04 * 180.0f / PI * 10, idx, 10 * COG_UNDEFINED));
		sog = ROUND(N2kMsg.Get2ByteUDouble(0.01 * 3600.0f / 1852.0f * 10, idx, SPEED_UNDEFINED));
	}

	int N2KtoMessage::readRadioChannel(const N2KMsg &N2kMsg, int &idx, int &channel)
	{
		int radio = N2kMsg.GetByte(idx);
		radio |= N2kMsg.GetByte(idx) << 8;
		unsigned char byte = N2kMsg.GetByte(idx);
		channel = (byte >> 3) & 7;
		radio |= (byte & 3) << 16;
		return radio;
	}

	// Text of a length of its own: the length, with its two leading bytes, and a type come
	// first. A sender that leaves them out is read to the end of the message.
	static std::string readVarStr(const N2KMsg &m, int &idx)
	{
		std::string s;
		const int left = m.DataLen - idx;
		int n = left;
		if (left >= 2 && m.Data[idx] >= 2 && m.Data[idx] <= left && m.Data[idx + 1] <= 1)
		{
			n = m.Data[idx] - 2;
			idx += 2;
		}
		while (n-- > 0)
			s += (char)m.GetByte(idx);
		return s;
	}

	void N2KtoMessage::finalize(char channel, TAG &tag)
	{
		msg.Stamp();
		msg.setChannel(channel);
		msg.buildNMEA(tag);
		if (msg.validate(tag))
			Send(&msg, 1, tag);
	}

	void N2KtoMessage::onMsg129038(const N2KMsg &N2kMsg, TAG &tag)
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
			turn_unscaled = ROUND(sqrtf(std::fabs(turn)) * 4.733f) * (turn < 0 ? -1 : 1);

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

	void N2KtoMessage::onMsg129793(const N2KMsg &N2kMsg, TAG &tag)
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
		std::tm parts;
		struct tm *timeInfo = utc(e, parts) ? &parts : nullptr;
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

	void N2KtoMessage::onMsg129794(const N2KMsg &N2kMsg, TAG &tag)
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

		// not available: hour 24 and minute 60, month and day 0, as AIS has them
		int hour = nSeconds < 0 ? 24 : (int)nSeconds / 3600;
		int minute = nSeconds < 0 ? 60 : ((int)nSeconds % 3600) / 60;

		time_t e = 24 * 3600 * (long int)nDays;
		std::tm parts;
		struct tm *timeInfo = nDays != 0xffff && utc(e, parts) ? &parts : nullptr;
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
		U(msg, 0, 423, 1);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129798(const N2KMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		int cog, sog, channel;

		startMessage(N2kMsg, idx);
		PosFix p = readPosFix(N2kMsg, idx);
		readCogSog(N2kMsg, idx, cog, sog);
		int radio = readRadioChannel(N2kMsg, idx, channel);
		int alt = ROUND(N2kMsg.Get4ByteDouble(0.01, idx, ALT_UNDEFINED));
		N2kMsg.GetByte(idx);
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

	// as 129802, with a sequence number, a destination and the retransmit flag before the text
	void N2KtoMessage::onMsg129801(const N2KMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;

		startMessage(N2kMsg, idx);
		unsigned char byte = N2kMsg.GetByte(idx);
		int channel = (byte >> 1) & 0x1f;
		int sequence = (byte >> 6) & 0x03;
		int destination = N2kMsg.Get4ByteUInt(idx);
		int retransmit = (N2kMsg.GetByte(idx) >> 6) & 0x01;
		std::string text = readVarStr(N2kMsg, idx);

		const size_t max_chars = (MAX_AIS_LENGTH - 72) / 6;
		if (text.length() > max_chars)
			text.resize(max_chars);

		U(msg, sequence, 38, 2);
		U(msg, destination, 40, 30);
		B(msg, retransmit, 70, 1);
		U(msg, 0, 71, 1);
		T(msg, text.c_str(), 72, text.length() * 6);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129802(const N2KMsg &N2kMsg, TAG &tag)
	{
		int idx = 0;
		std::string text;

		startMessage(N2kMsg, idx);
		int channel = (N2kMsg.GetByte(idx) >> 1) & 0x1f;
		text = readVarStr(N2kMsg, idx);

		// truncate to what fits after the 40-bit header, setText drops the field entirely otherwise
		const size_t max_chars = (MAX_AIS_LENGTH - 40) / 6;
		if (text.length() > max_chars)
			text.resize(max_chars);

		T(msg, text.c_str(), 40, text.length() * 6);

		finalize('A' + channel % 2, tag);
	}

	void N2KtoMessage::onMsg129039(const N2KMsg &N2kMsg, TAG &tag)
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
	void N2KtoMessage::onMsg129040(const N2KMsg &N2kMsg, TAG &tag)
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
		int channel = (byte >> 6) & 0x01;

		U(msg, 0, 38, 8);
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
		U(msg, 0, 308, 4);

		finalize('A' + channel, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129041: AIS Type 21 (Aid-to-Navigation Report)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129041(const N2KMsg &N2kMsg, TAG &tag)
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

		int epfd = (N2kMsg.GetByte(idx) >> 1) & 0x0F;
		int regional = N2kMsg.GetByte(idx);
		int transceiver = N2kMsg.GetByte(idx) & 0x1F;

		// Extract name with proper handling
		const std::string name = readVarStr(N2kMsg, idx);
		{
			int nameIdx = 0;
			for (size_t at = 0; nameIdx < 20 && at < name.size(); at++)
			{
				byte = name[at];

				if (byte >= 32 && byte <= 126)
				{
					name_buffer[nameIdx++] = byte;
				}
				else if (byte == 0)
				{
					break;
				}
				else
				{
					name_buffer[nameIdx++] = '@';
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
		U(msg, 0, 271, 1);

		finalize('A' + (transceiver & 0x01), tag);
	}

	//----------------------------------------------------------------------
	// PGN 129809: AIS Type 24, Part A (Static Data Report - Part A)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129809(const N2KMsg &N2kMsg, TAG &tag)
	{
		char shipname[21] = {0};
		int idx = 0;

		startMessage(N2kMsg, idx);
		N2kMsg.GetStr(shipname, 20, idx);
		int channel = idx < N2kMsg.DataLen ? N2kMsg.GetByte(idx) & 0x01 : 0;

		U(msg, 0, 38, 2);
		T(msg, shipname, 40, 120);

		finalize('A' + channel, tag);
	}

	//----------------------------------------------------------------------
	// PGN 129810: AIS Type 24, Part B (Static Data Report - Part B)
	//----------------------------------------------------------------------
	void N2KtoMessage::onMsg129810(const N2KMsg &N2kMsg, TAG &tag)
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

		N2kMsg.Get4ByteUInt(idx); // mothership

		int epfd = (N2kMsg.GetByte(idx) >> 4) & 0x0F;

		int channel = N2kMsg.GetByte(idx) & 0x01;

		U(msg, 1, 38, 2);
		U(msg, shiptype, 40, 8);
		T(msg, vendorid, 48, 42);
		T(msg, callsign, 90, 42);
		U(msg, to_bow, 132, 9);
		U(msg, bow_stern - to_bow, 141, 9);
		U(msg, port_starboard - to_starboard, 150, 6);
		U(msg, to_starboard, 156, 6);
		U(msg, epfd, 162, 4);
		U(msg, 0, 166, 2);

		finalize(channel ? 'B' : 'A', tag);
	}

	// A frame of a message these handlers take. Each of them spans several frames, a fast
	// packet: every frame starts with the sequence of the message and its place in it, and
	// the first holds the length of the whole.
	void N2KtoMessage::frame(uint32_t id, const unsigned char *data, int len, TAG &tag)
	{
		static const struct
		{
			uint32_t pgn;
			void (N2KtoMessage::*handler)(const N2KMsg &, TAG &);
		} handlers[] = {
			{129038, &N2KtoMessage::onMsg129038},
			{129039, &N2KtoMessage::onMsg129039},
			{129040, &N2KtoMessage::onMsg129040},
			{129041, &N2KtoMessage::onMsg129041},
			{129793, &N2KtoMessage::onMsg129793},
			{129794, &N2KtoMessage::onMsg129794},
			{129798, &N2KtoMessage::onMsg129798},
			{129801, &N2KtoMessage::onMsg129801},
			{129802, &N2KtoMessage::onMsg129802},
			{129809, &N2KtoMessage::onMsg129809},
			{129810, &N2KtoMessage::onMsg129810},
		};

		const uint32_t pf = (id >> 16) & 0xff;
		const uint32_t pgn = ((id >> 24) & 1) << 16 | pf << 8 | (pf < 240 ? 0 : (id >> 8) & 0xff);
		const int source = id & 0xff;

		const auto *h = handlers;
		const auto *end = handlers + sizeof(handlers) / sizeof(handlers[0]);
		while (h != end && h->pgn != pgn)
			h++;
		if (h == end || len < 2)
			return;

		Partial *p = nullptr;
		for (auto &q : partial)
			if (q.source == source && q.msg.PGN == pgn)
				p = &q;

		const int sequence = data[0] >> 5, place = data[0] & 0x1f;
		int skip = 1;
		if (place == 0)
		{
			if (!p)
				p = &partial[last = (last + 1) % PARTIALS];
			p->source = source;
			p->sequence = sequence;
			p->next = 0;
			p->length = data[1];
			p->msg.PGN = pgn;
			p->msg.DataLen = 0;
			skip = 2;
		}
		if (!p)
			return;
		// a frame lost or out of its place: the message is given up
		if (p->sequence != sequence || p->next != place || p->length > N2KMsg::MAX_LEN)
		{
			p->source = -1;
			return;
		}
		p->next++;
		for (int i = skip; i < len && p->msg.DataLen < p->length; i++)
			p->msg.Data[p->msg.DataLen++] = data[i];

		if (p->msg.DataLen == p->length)
		{
			p->source = -1;
			(this->*h->handler)(p->msg, tag);
		}
	}

	// "16:29:27.082 R 09F8017F 50 C3 B8 13 47 D8 2B C6". What precedes the frame may be left
	// out: a time, with its colons, and R for a frame received; one the gateway sent itself,
	// T, is not for us.
	bool parseFrameLine(const char *s, uint32_t &id, unsigned char *data, int &len)
	{
		char *e = nullptr;
		len = 0;

		for (int field = 0;; field++)
		{
			while (*s == ' ')
				s++;
			const char *t = s;
			while (*t && *t != ' ')
				t++;
			if (field < 2 && (std::memchr(s, ':', t - s) || (t - s == 1 && *s == 'R')))
			{
				s = t;
				continue;
			}
			id = (uint32_t)std::strtoul(s, &e, 16);
			if (e != t || t - s < 1 || t - s > 8)
				return false;
			s = t;
			break;
		}

		while (true)
		{
			while (*s == ' ')
				s++;
			if (!*s)
				break;
			const unsigned long b = std::strtoul(s, &e, 16);
			if (len == 8 || e - s != 2 || (*e && *e != ' '))
				return false;
			data[len++] = (unsigned char)b;
			s = e;
		}
		return len > 0;
	}

	void N2KtoMessage::processLine(TAG &tag)
	{
		uint32_t id;
		unsigned char data[8];
		int len;

		// a line cut off at the limit is not a frame
		if (line.size() < MAX_LINE && parseFrameLine(line.c_str(), id, data, len))
			frame(id, data, len, tag);
	}

	void N2KtoMessage::Receive(const RAW *data, int len, TAG &tag)
	{
		for (int j = 0; j < len; j++)
			for (int i = 0; i < data[j].size; i++)
			{
				const char c = ((const char *)data[j].data)[i];
				if (c == '\n')
				{
					processLine(tag);
					line.clear();
				}
				else if (c != '\r' && line.size() < MAX_LINE)
					line += c;
			}
	}

	// The frames of a message, each a line as a gateway takes it: "<id> <bytes>". The id
	// holds the priority, the PGN and the address of the sender.
	void MessageToN2K::emit(const N2KMsg &m)
	{
		const uint32_t id = (uint32_t)(m.Priority & 7) << 26 | m.PGN << 8 | source;
		const int order = (sequence++ & 7) << 5;
		char buf[32];

		for (int frame = 0, at = 0; at < m.DataLen; frame++)
		{
			std::snprintf(buf, sizeof(buf), "%08X %02X", (unsigned)id, order | frame);
			*lines += buf;
			int room = 7;
			if (frame == 0)
			{
				std::snprintf(buf, sizeof(buf), " %02X", m.DataLen);
				*lines += buf;
				room = 6;
			}
			for (; room > 0; room--, at++)
			{
				std::snprintf(buf, sizeof(buf), " %02X", at < m.DataLen ? m.Data[at] : 0xff);
				*lines += buf;
			}
			*lines += "\r\n";
		}
	}

	void MessageToN2K::encode(const Message &ais, const JSON::JSON &data, std::string &out)
	{
		lines = &out;
		switch (ais.type())
		{
		case 1:
		case 2:
		case 3:
			sendType123(ais, &data);
			break;
		case 4:
		case 11:
			sendType4(ais, &data);
			break;
		case 5:
			sendType5(ais, &data);
			break;
		case 9:
			sendType9(ais, &data);
			break;
		case 12:
			sendType12(ais, &data);
			break;
		case 14:
			sendType14(ais, &data);
			break;
		case 18:
			sendType18(ais, &data);
			break;
		case 19:
			sendType19(ais, &data);
			break;
		case 21:
			sendType21(ais, &data);
			break;
		case 24:
			sendType24(ais, &data);
			break;
		default:
			break;
		}
	}

	// Follows https://github.com/AK-Homberger/N2K-AIS-Gateway implementation with some small tweaks

	void MessageToN2K::sendType123(const Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED;
		int heading = HEADING_UNDEFINED, status = 0, raim = 0, accuracy = 0, maneuver = 0, radio = 0;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_STATUS:
				status = p.Get().getInt();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_MANEUVER:
				maneuver = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129038);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | (ais.type() & 0x3f));
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(ais.getUint(137, 6) << 2 | (raim & 0x01) << 1 | (accuracy & 0x01));
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		const int rot = ais.getInt(42, 8);
		N2kMsg.Add2ByteDouble(rot == -128 ? -1e9 : (rot < 0 ? -1 : 1) * (rot / 4.733) * (rot / 4.733), 3.125E-05 / PI * 180.0f * 60.0f);
		N2kMsg.AddByte((0xff << 6) | (maneuver & 3) << 4 | (status & 0x0f));
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(0xff);

		emit(N2kMsg);
	}

	// based on CANboat PGN description
	void MessageToN2K::sendType4(const Message &ais, const JSON::JSON *data)
	{
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED;
		int raim = 0, accuracy = 0, hour = 0, minute = 0, second = 0, year = 0, month = 0, day = 0, epfd = 0, days = 0, radio = 0;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_MINUTE:
				minute = p.Get().getInt();
				break;
			case AIS::KEY_HOUR:
				hour = p.Get().getInt();
				break;
			case AIS::KEY_SECOND:
				second = p.Get().getInt();
				break;
			case AIS::KEY_YEAR:
				year = p.Get().getInt();
				break;
			case AIS::KEY_MONTH:
				month = p.Get().getInt();
				break;
			case AIS::KEY_DAY:
				day = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			}
		}

		N2KMsg N2kMsg;

		std::time_t now = std::time(nullptr);
		std::tm parts;
		std::tm *utc_tm = utc(now, parts) ? &parts : nullptr;

		if (utc_tm)
		{
			utc_tm->tm_year = year - 1900;
			utc_tm->tm_mon = month - 1;
			utc_tm->tm_mday = day;
			days = std::mktime(utc_tm) / 86400;
		}

		N2kMsg.SetPGN(129793);
		N2kMsg.Priority = 7;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());

		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);

		N2kMsg.AddByte(raim << 1 | accuracy);
		N2kMsg.Add4ByteUInt((hour * 3600 + minute * 60 + second) * 10000);

		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));

		N2kMsg.Add2ByteUInt(days);
		N2kMsg.AddByte(epfd << 4 | 0xf);
		N2kMsg.AddByte(0xff);

		emit(N2kMsg);
	}

	void MessageToN2K::sendType5(const Message &ais, const JSON::JSON *data)
	{

		int ais_version = 0, IMO = 0, shiptype = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, month = 0, day = 0, hour = 0;
		int minute = 0, epfd = 0, dte = 0, nDays = 0;
		char callsign[7], shipname[20], destination[20];
		std::memset(callsign, 0, sizeof(callsign));
		std::memset(shipname, 0, sizeof(shipname));
		std::memset(destination, 0, sizeof(destination));
		float draught = 0;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_IMO:
				IMO = p.Get().getInt();
				break;
			case AIS::KEY_CALLSIGN:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(callsign, s.c_str(), std::min(sizeof(callsign), s.size()));
			}
			break;
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_MONTH:
				month = p.Get().getInt();
				break;
			case AIS::KEY_DAY:
				day = p.Get().getInt();
				break;
			case AIS::KEY_MINUTE:
				minute = p.Get().getInt();
				break;
			case AIS::KEY_HOUR:
				hour = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_AIS_VERSION:
				ais_version = p.Get().getInt();
				break;
			case AIS::KEY_DRAUGHT:
				draught = p.Get().getFloat();
				break;
			case AIS::KEY_DESTINATION:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(destination, s.c_str(), std::min(sizeof(destination), s.size()));
			}
			break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			}
		}

		N2KMsg N2kMsg;

		if (month && day)
		{
			std::time_t now = std::time(nullptr);
			std::tm parts;
			std::tm *utc_tm = utc(now, parts) ? &parts : nullptr;

			if (utc_tm)
			{
				utc_tm->tm_year += (utc_tm->tm_mon + 1 < month || (utc_tm->tm_mon + 1 == month && day < utc_tm->tm_mday)) ? 1 : 0;
				utc_tm->tm_mon = month - 1;
				utc_tm->tm_mday = day;
				nDays = std::mktime(utc_tm) / 86400;
			}
		}

		N2kMsg.SetPGN(129794L);
		N2kMsg.Priority = 6;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | (ais.type() & 0x3f));
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteUInt(IMO);
		N2kMsg.AddAISStr(callsign, 7);
		N2kMsg.AddAISStr(shipname, 20);
		N2kMsg.AddByte(shiptype);
		N2kMsg.Add2ByteDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteDouble(to_port + to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_bow, 0.1);
		N2kMsg.Add2ByteUInt(month && day ? nDays : 0xffff);
		N2kMsg.Add4ByteUInt(ais.getUint(283, 5) < 24 && ais.getUint(288, 6) < 60 ? (hour * 3600 + minute * 60) * 10000 : 0xffffffff);
		N2kMsg.Add2ByteDouble(draught, 0.01);
		N2kMsg.AddAISStr(destination, 20);
		N2kMsg.AddByte((dte & 0x01) << 6 | (epfd & 0x0f) << 2 | (ais_version & 0x03));
		N2kMsg.AddByte(0xe0 | (ais.getChannel() == 'A' ? 0 : 1));
		N2kMsg.AddByte(0xff);

		emit(N2kMsg);
	}

	void MessageToN2K::sendType9(const Message &ais, const JSON::JSON *data)
	{
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED;
		int alt = 0, raim = 0, accuracy = 0, reserved = 0, dte = 0, radio = 0, speed = 1023;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getInt();
				break;
			case AIS::KEY_ALT:
				alt = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_RESERVED:
				reserved = p.Get().getInt();
				break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129798);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());

		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);

		N2kMsg.AddByte(ais.getUint(128, 6) << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180 / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.1 * 3600.0f / 1852.0f, 1023);

		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));

		N2kMsg.Add4ByteDouble(alt, 0.01, ALT_UNDEFINED);
		N2kMsg.AddByte(reserved);
		N2kMsg.AddByte(dte | 254);

		emit(N2kMsg);
	}

	// PGN 129801: as 129802, with the sequence number in the top bits of the transceiver byte,
	// then the destination, then the retransmit flag above six reserved bits
	void MessageToN2K::sendType12(const Message &ais, const JSON::JSON *data)
	{
		std::string text;
		int sequence = 0, destination = 0, retransmit = 0;
		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_SEQNO:
				sequence = p.Get().getInt();
				break;
			case AIS::KEY_DEST_MMSI:
				destination = p.Get().getInt();
				break;
			case AIS::KEY_RETRANSMIT:
				retransmit = p.Get().getBool();
				break;
			case AIS::KEY_TEXT:
				text = p.Get().getString();
				break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129801);
		N2kMsg.Priority = 5;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.AddByte((sequence & 0x03) << 6 | (ais.getChannel() == 'A' ? 0 : 1) << 1 | 0x01);
		N2kMsg.Add4ByteUInt(destination);
		N2kMsg.AddByte((retransmit ? 1 : 0) << 6 | 0x3f);
		N2kMsg.AddVarStr(text.c_str(), 156); // up to 156 six-bit characters fit a type 12

		emit(N2kMsg);
	}

	void MessageToN2K::sendType14(const Message &ais, const JSON::JSON *data)
	{

		std::string text;
		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_TEXT:
			{
				text = p.Get().getString();
			}
			break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129802);
		N2kMsg.Priority = 5;
		N2kMsg.AddByte((ais.repeat() & 0x03) << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.AddByte((ais.getChannel() == 'A' ? 0 : 1) << 1 | 0x01);
		N2kMsg.AddVarStr(text.c_str(), 163); // safety text is STRING_LAU (max 163)

		emit(N2kMsg);
	}

	void MessageToN2K::sendType18(const Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED;
		int heading = HEADING_UNDEFINED, radio = 0, raim = 0, accuracy = 0, CS = 0, display = 0, DSC = 0, band = 0, msg22 = 0, assigned = 0 /*, maneuver = 0*/;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_RADIO:
				radio = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_CS:
				CS = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DISPLAY:
				display = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DSC:
				DSC = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_BAND:
				band = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_MSG22:
				msg22 = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		N2KMsg N2kMsg;
		N2kMsg.SetPGN(129039);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(ais.getUint(133, 6) << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(radio & 0xff);
		N2kMsg.AddByte((radio >> 8) & 0xff);
		N2kMsg.AddByte(((ais.getChannel() == 'A' ? 0 : 1) << 3) | ((radio >> 16) & 7));
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(assigned << 7 | msg22 << 6 | band << 5 | DSC << 4 | display << 3 | CS << 2);
		N2kMsg.AddByte(0xfe | (radio & 1)); // msg 18 has 20 radio bits, check whether this is correct indeed
		N2kMsg.AddByte(0xff);

		emit(N2kMsg);
	}

	void MessageToN2K::sendType19(const Message &ais, const JSON::JSON *data)
	{

		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED, cog = COG_UNDEFINED, speed = SPEED_UNDEFINED;
		int heading = HEADING_UNDEFINED, shiptype = 0, to_bow = 0, to_stern = 0, to_port = 0, to_starboard = 0, epfd = 0, accuracy = 0, raim = 0, dte = 0, assigned = 0;
		char shipname[20];
		std::memset(shipname, 0, sizeof(shipname));

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_HEADING:
				heading = p.Get().getInt();
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_DTE:
				dte = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_COURSE:
				cog = p.Get().getFloat();
				break;
			case AIS::KEY_SPEED:
				speed = p.Get().getFloat();
				break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129040);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(ais.getUint(133, 6) << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(cog, 1e-04 * 180.0f / PI, COG_UNDEFINED);
		N2kMsg.Add2ByteUDouble(speed, 0.01 * 3600.0f / 1852.0f, SPEED_UNDEFINED);
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(0xff);
		N2kMsg.AddByte(shiptype);
		N2kMsg.Add2ByteUDouble(heading, 1e-04 * 360.0f / (2.0f * PI), HEADING_UNDEFINED);
		N2kMsg.AddByte(epfd << 4);
		N2kMsg.Add2ByteDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteDouble(to_port + to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteDouble(to_bow, 0.1);
		N2kMsg.AddAISStr(shipname, 20);
		N2kMsg.AddByte(dte | assigned << 1 | (ais.getChannel() == 'A' ? 0 : 1) << 6);
		N2kMsg.AddByte(0xf8);

		emit(N2kMsg);
	}

	void MessageToN2K::sendType21(const Message &ais, const JSON::JSON *data)
	{

		int aid_type = 0, accuracy = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, epfd = 0;
		int off_position = 0, raim = 0, virtual_aid = 0, assigned = 0;
		double lat = LAT_UNDEFINED, lon = LON_UNDEFINED;

		std::string name;

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_NAME:
				name = p.Get().getString();
				break;
			case AIS::KEY_AID_TYPE:
				aid_type = p.Get().getInt();
				break;
			case AIS::KEY_LAT:
				lat = p.Get().getFloat();
				break;
			case AIS::KEY_LON:
				lon = p.Get().getFloat();
				break;
			case AIS::KEY_ACCURACY:
				accuracy = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_OFF_POSITION:
				off_position = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_RAIM:
				raim = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_VIRTUAL_AID:
				virtual_aid = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_ASSIGNED:
				assigned = p.Get().getBool() ? 1 : 0;
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_EPFD:
				epfd = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			}
		}

		N2KMsg N2kMsg;

		N2kMsg.SetPGN(129041);
		N2kMsg.Priority = 4;
		N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
		N2kMsg.Add4ByteUInt(ais.mmsi());
		N2kMsg.Add4ByteDouble(lon, 1e-07, LON_UNDEFINED);
		N2kMsg.Add4ByteDouble(lat, 1e-07, LAT_UNDEFINED);
		N2kMsg.AddByte(ais.getUint(253, 6) << 2 | raim << 1 | accuracy);
		N2kMsg.Add2ByteUDouble(to_bow + to_stern, 0.1);
		N2kMsg.Add2ByteUDouble(to_starboard + to_port, 0.1);
		N2kMsg.Add2ByteUDouble(to_starboard, 0.1);
		N2kMsg.Add2ByteUDouble(to_bow, 0.1);
		N2kMsg.AddByte(assigned << 7 | virtual_aid << 6 | off_position << 5 | aid_type);
		N2kMsg.AddByte(epfd << 1);
		N2kMsg.AddByte(ais.getUint(260, 8));
		N2kMsg.AddByte((ais.getChannel() == 'A' ? 0 : 1) | 0xe0);
		N2kMsg.AddVarStr(name.c_str(), 34); // AtoN name is STRING_LAU (max 34)

		emit(N2kMsg);
	}

	void MessageToN2K::sendType24(const Message &ais, const JSON::JSON *data)
	{

		int shiptype = 0, to_bow = 0, to_stern = 0, to_starboard = 0, to_port = 0, mothership_mmsi = 0, partno = 0;
		char shipname[20], callsign[7], vendorid[7];
		std::memset(shipname, 0, sizeof(shipname));
		std::memset(callsign, 0, sizeof(callsign));
		std::memset(vendorid, 0, sizeof(vendorid));

		for (const JSON::Member &p : data[0].getMembers())
		{
			switch (p.Key())
			{
			case AIS::KEY_SHIPNAME:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(shipname, s.c_str(), std::min(sizeof(shipname), s.size()));
			}
			break;
			case AIS::KEY_CALLSIGN:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(callsign, s.c_str(), std::min(sizeof(callsign), s.size()));
			}
			break;
			case AIS::KEY_VENDORID:
			{
				const std::string &s = p.Get().getString();
				std::memcpy(vendorid, s.c_str(), std::min(sizeof(vendorid), s.size()));
			}
			break;
			case AIS::KEY_SHIPTYPE:
				shiptype = p.Get().getInt();
				break;
			case AIS::KEY_PARTNO:
				partno = p.Get().getInt();
				break;
			case AIS::KEY_TO_BOW:
				to_bow = p.Get().getInt();
				break;
			case AIS::KEY_TO_PORT:
				to_port = p.Get().getInt();
				break;
			case AIS::KEY_TO_STARBOARD:
				to_starboard = p.Get().getInt();
				break;
			case AIS::KEY_TO_STERN:
				to_stern = p.Get().getInt();
				break;
			case AIS::KEY_MOTHERSHIP_MMSI:
				mothership_mmsi = p.Get().getInt();
				break;
			}
		}

		N2KMsg N2kMsg;

		if (partno == 0)
		{
			N2kMsg.SetPGN(129809);
			N2kMsg.Priority = 6;
			N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
			N2kMsg.Add4ByteUInt(ais.mmsi());
			N2kMsg.AddAISStr(shipname, 20);
			N2kMsg.AddByte((ais.getChannel() == 'A' ? 0 : 1) | 0xe0);
			N2kMsg.AddByte(0xff);
		}
		else
		{
			N2kMsg.SetPGN(129810);
			N2kMsg.Priority = 6;
			N2kMsg.AddByte(ais.repeat() << 6 | ais.type());
			N2kMsg.Add4ByteUInt(ais.mmsi());
			N2kMsg.AddByte(shiptype);
			N2kMsg.AddAISStr(vendorid, 7);
			N2kMsg.AddAISStr(callsign, 7);
			N2kMsg.Add2ByteUDouble(to_bow + to_stern, 0.1);
			N2kMsg.Add2ByteUDouble(to_port + to_starboard, 0.1);
			N2kMsg.Add2ByteUDouble(to_starboard, 0.1);
			N2kMsg.Add2ByteUDouble(to_bow, 0.1);
			N2kMsg.Add4ByteUInt(mothership_mmsi);
			N2kMsg.AddByte(0x03 | ais.getUint(162, 4) << 4);
			N2kMsg.AddByte((ais.getChannel() == 'A' ? 0 : 1) | 0xe0);
			N2kMsg.AddByte(0xff);
		}

		emit(N2kMsg);
	}
}
