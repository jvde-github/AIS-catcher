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

#pragma once

#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <string>

#include "Message.h"
#include "Stream.h"

namespace AIS
{
	// A complete NMEA 2000 message. Its fields are read in order, as the NMEA2000 library's
	// N2KMsg reads them: the lowest byte first, and `def` for a field the message ends
	// before or that holds "not available".
	struct N2KMsg
	{
		static const int MAX_LEN = 223;

		uint32_t PGN = 0;
		int DataLen = 0;
		unsigned char Data[MAX_LEN];

		int Priority = 6;

		// Writing, as the library writes: a number is rounded to its steps, and one that is
		// `undef` or does not fit is written as "not available" or "out of range"
		void SetPGN(uint32_t pgn)
		{
			PGN = pgn;
			DataLen = 0;
		}
		void AddByte(unsigned char v)
		{
			if (DataLen < MAX_LEN)
				Data[DataLen++] = v;
		}
		void Add2ByteUInt(uint16_t v) { put(2, v); }
		void Add4ByteUInt(uint32_t v) { put(4, v); }
		void Add2ByteDouble(double v, double precision, double undef = -1e9)
		{
			const double d = std::round(v / precision);
			put(2, v == undef ? 0x7fff : d >= -32768 && d < 0x7ffe ? (uint16_t)(int16_t)d : 0x7ffe);
		}
		void Add2ByteUDouble(double v, double precision, double undef = -1e9)
		{
			const double d = std::round(v / precision);
			put(2, v == undef ? 0xffff : d >= 0 && d < 0xfffe ? (uint16_t)d : 0xfffe);
		}
		void Add4ByteDouble(double v, double precision, double undef = -1e9)
		{
			const double d = std::round(v / precision);
			put(4, v == undef ? 0x7fffffff : d >= -2147483648.0 && d < 0x7ffffffe ? (uint32_t)(int32_t)d : 0x7ffffffe);
		}
		// text in the characters of AIS, to `len` places, the unused ones '@'
		void AddAISStr(const char *str, int len)
		{
			for (; len > 0 && DataLen < MAX_LEN; len--)
			{
				const char c = *str ? (char)std::toupper((unsigned char)*str++) : '@';
				Data[DataLen++] = c >= 0x20 && c <= 0x5f ? c : '?';
			}
		}
		// text of a length of its own: the length with its two leading bytes, a 1 for ASCII, the text
		void AddVarStr(const char *str, int max_len)
		{
			int len = (int)std::strlen(str);
			len = MIN(len, MIN(max_len, MAX_LEN - DataLen - 2));
			if (len < 0)
				return;
			AddByte((unsigned char)(len + 2));
			AddByte(1);
			for (int i = 0; i < len; i++)
				AddByte((unsigned char)str[i] < 0x80 ? str[i] : '?');
		}

		unsigned char GetByte(int &i) const { return i < DataLen ? Data[i++] : 0xff; }
		uint16_t Get2ByteUInt(int &i, uint16_t def = 0xffff) const
		{
			uint32_t v;
			return take(2, i, v) ? (uint16_t)v : def;
		}
		uint32_t Get4ByteUInt(int &i, uint32_t def = 0xffffffff) const
		{
			uint32_t v;
			return take(4, i, v) ? v : def;
		}
		double Get2ByteDouble(double precision, int &i, double def = -1e9) const
		{
			uint32_t v;
			return take(2, i, v) && v != 0x7fff ? (int16_t)v * precision : def;
		}
		double Get2ByteUDouble(double precision, int &i, double def = -1e9) const
		{
			uint32_t v;
			return take(2, i, v) && v != 0xffff ? v * precision : def;
		}
		double Get4ByteDouble(double precision, int &i, double def = -1e9) const
		{
			uint32_t v;
			return take(4, i, v) && v != 0x7fffffff ? (int32_t)v * precision : def;
		}
		double Get4ByteUDouble(double precision, int &i, double def = -1e9) const
		{
			uint32_t v;
			return take(4, i, v) && v != 0xffffffff ? v * precision : def;
		}
		// text of `length` characters into a buffer one longer, ended at a zero or an '@'
		bool GetStr(char *buf, size_t length, int &i) const
		{
			buf[0] = 0;
			if ((size_t)i + length > (size_t)DataLen)
				return false;
			bool ended = false;
			for (size_t k = 0; k < length; k++)
			{
				const unsigned char c = Data[i++];
				ended = ended || c == 0 || c == '@';
				buf[k] = ended ? 0 : c;
				buf[k + 1] = 0;
			}
			return true;
		}

	private:
		void put(int n, uint32_t v)
		{
			for (int k = 0; k < n; k++)
				AddByte((unsigned char)(v >> (8 * k)));
		}
		bool take(int n, int &i, uint32_t &v) const
		{
			if (i + n > DataLen)
				return false;
			v = 0;
			for (int k = 0; k < n; k++)
				v |= (uint32_t)Data[i++] << (8 * k);
			return true;
		}
	};

	// a frame line into its id and bytes; false for a line that is not a frame for us
	bool parseFrameLine(const char *s, uint32_t &id, unsigned char *data, int &len);

	// CAN frames of an NMEA 2000 bus, as text lines, to AIS messages. A line is a frame as a
	// Yacht Devices gateway writes it in RAW mode, "16:29:27.082 R 09F8017F 50 C3 B8 13 47 D8 2B C6";
	// the time and the direction may be left out.
	class N2KtoMessage : public SimpleStreamInOut<RAW, Message>
	{
		Message msg;

		// the line under way, and the messages being joined from their frames
		static const int MAX_LINE = 64, PARTIALS = 4;
		std::string line;
		struct Partial
		{
			int source = -1, sequence = 0, next = 0, length = 0;
			N2KMsg msg;
		} partial[PARTIALS];
		int last = 0;

		void processLine(TAG &tag);
		void frame(uint32_t id, const unsigned char *data, int len, TAG &tag);

		struct PosFix
		{
			int lon, lat, accuracy, raim, second;
		};

		// Reads type/repeat/mmsi and writes the common AIS header (bits 0-37) into a cleared msg.
		void startMessage(const N2KMsg &N2kMsg, int &idx);
		PosFix readPosFix(const N2KMsg &N2kMsg, int &idx);
		void readCogSog(const N2KMsg &N2kMsg, int &idx, int &cog, int &sog);
		// Reads the 3-byte radio status block; returns radio, sets channel.
		int readRadioChannel(const N2KMsg &N2kMsg, int &idx, int &channel);
		void finalize(char channel, TAG &tag);

		void onMsg129038(const N2KMsg &N2kMsg, TAG &tag); // Type 1, 2, 3  - Position Report
		void onMsg129039(const N2KMsg &N2kMsg, TAG &tag); // Type 18      - Class B Position Report
		void onMsg129040(const N2KMsg &N2kMsg, TAG &tag); // Type 19      - Class B Extended Position Report
		void onMsg129041(const N2KMsg &N2kMsg, TAG &tag); // Type 21      - Aid-to-Navigation Report
		void onMsg129793(const N2KMsg &N2kMsg, TAG &tag); // Type 4, 11   - UTC/Date & Position Report
		void onMsg129794(const N2KMsg &N2kMsg, TAG &tag); // Type 5       - Static and Voyage Related Data
		void onMsg129798(const N2KMsg &N2kMsg, TAG &tag); // Type 9       - Extended Position Report
		void onMsg129801(const N2KMsg &N2kMsg, TAG &tag); // Type 12      - Addressed Safety-Related Message
		void onMsg129802(const N2KMsg &N2kMsg, TAG &tag); // Type 14      - Safety-Related Broadcast Message
		void onMsg129809(const N2KMsg &N2kMsg, TAG &tag); // Type 24 A    - Static Data Report, Part A
		void onMsg129810(const N2KMsg &N2kMsg, TAG &tag); // Type 24 B    - Static Data Report, Part B

	public:
		virtual ~N2KtoMessage() {}

		void setOwnMMSI(int mmsi) { msg.setOwnMMSI(mmsi); }
		void Receive(const RAW *data, int len, TAG &tag);
	};

	// An AIS message, by its decoded fields, to the frames of its NMEA 2000 message, as lines
	class MessageToN2K
	{
		std::string *lines = nullptr;
		int sequence = 0;
		int source = 23;

		void emit(const N2KMsg &m);

		void sendType123(const Message &ais, const JSON::JSON *data);
		void sendType4(const Message &ais, const JSON::JSON *data);
		void sendType5(const Message &ais, const JSON::JSON *data);
		void sendType9(const Message &ais, const JSON::JSON *data);
		void sendType12(const Message &ais, const JSON::JSON *data);
		void sendType14(const Message &ais, const JSON::JSON *data);
		void sendType18(const Message &ais, const JSON::JSON *data);
		void sendType19(const Message &ais, const JSON::JSON *data);
		void sendType21(const Message &ais, const JSON::JSON *data);
		void sendType24(const Message &ais, const JSON::JSON *data);

	public:
		// appends the lines to `out`; nothing for a message type NMEA 2000 has no message for
		void encode(const Message &ais, const JSON::JSON &data, std::string &out);
	};
}
