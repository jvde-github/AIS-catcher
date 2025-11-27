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

#include "Message.h"
#include "Parse.h"
#include "Helper.h"

namespace AIS
{

	int Message::ID = 0;

	static int NMEAchecksum(const std::string &s)
	{
		int check = 0;
		for (int i = 1; i < s.length(); i++)
			check ^= s[i];
		return check;
	}

	const std::string GPS::formatLatLon(float value, bool isLatitude) const
	{
		std::stringstream ss;

		int degrees = static_cast<int>(value);
		float minutes = (value - degrees) * 60;

		ss << std::setfill('0') << std::setw(isLatitude ? 2 : 3) << std::abs(degrees);
		ss << std::setfill('0') << std::setw(5) << std::fixed << std::setprecision(2) << minutes;

		return ss.str();
	}

	const std::string GPS::getNMEA() const
	{
		if (nmea.empty())
		{
			std::string flat = formatLatLon(lat, true);
			std::string flon = formatLatLon(lon, false);

			char latDir = lat >= 0 ? 'N' : 'S';
			char lonDir = lon >= 0 ? 'E' : 'W';

			std::string line = "$GPGLL," + flat + "," + latDir + "," + flon + "," + lonDir + ",,,";

			int c = NMEAchecksum(line);

			line += '*';
			line += (c >> 4) < 10 ? (c >> 4) + '0' : (c >> 4) + 'A' - 10;
			line += (c & 0xF) < 10 ? (c & 0xF) + '0' : (c & 0xF) + 'A' - 10;

			return line;
		}
		else
			return nmea;
	}

	const std::string GPS::getJSON() const
	{
		if (json.empty())
			return "{\"class\":\"TPV\",\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) + "}";
		else
			return json;
	}

	std::string Message::getNMEAJSON(unsigned mode, float level, float ppm, int status, const std::string &hardware, int version, Type driver, bool include_ssl, uint32_t ipv4, const std::string &uuid) const
	{
		std::stringstream ss;

		ss << "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"version\":" << version << ",\"driver\":" << (int)driver << ",\"hardware\":\"" << hardware << "\",\"channel\":\"" << getChannel() << "\",\"repeat\":" << repeat();

		if (include_ssl)
			ss << ",\"ssc\":" << start_idx << ",\"sl\":" << (end_idx - start_idx);

		if (status)
		{
			ss << ",\"msg_status\":" << status;
		}

		if (mode & 2)
		{
			ss << ",\"rxuxtime\":" << getRxTimeUnix();
			ss << ",\"rxtime\":\"" << getRxTime() << "\"";
		}

		if (!uuid.empty())
			ss << ",\"uuid\":\"" << uuid << "\"";

		if (ipv4)
			ss << ",\"ipv4\":" << ipv4;

		if (mode & 1)
		{
			ss << ",\"signalpower\":";
			if (level == LEVEL_UNDEFINED)
				ss << "null";
			else
				ss << level;
			ss << ",\"ppm\":";
			if (ppm == PPM_UNDEFINED)
				ss << "null";
			else
				ss << ppm;
		}

		if (getStation())
			ss << ",\"station_id\":" << getStation();

		if (getLength() > 0)
			ss << ",\"mmsi\":" << mmsi() << ",\"type\":" << type();
		ss << ",\"nmea\":[\"" << NMEA[0] << "\"";

		for (int j = 1; j < NMEA.size(); j++)
			ss << ",\"" << NMEA[j] << "\"";

		ss << "]}";

		return ss.str();
	}

	std::string Message::getNMEATagBlock() const
	{
		static int groupId = 0;
		groupId = (groupId % 9999) + 1;  // Recycle 1-9999

		std::string result;
		int total = NMEA.size();
		int seq = 1;

		// Use station ID with 's' prefix as source
		std::string src = "s" + std::to_string(station);

		for (const auto &nmea : NMEA)
		{
			std::stringstream tb;
			tb << "s:" << src << ",c:" << rxtime << ",g:" << seq << "-" << total << "-" << groupId;

			// Calculate checksum for tag block
			std::string tbs = tb.str();
			int check = 0;
			for (char c : tbs)
				check ^= c;

			std::stringstream ss;
			ss << "\\" << tbs << "*" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << check << "\\" << nmea << "\r\n";

			result += ss.str();
			seq++;
		}

		return result;
	}

	std::string Message::getBinaryNMEA(const TAG &tag, bool crc) const
	{
		std::string packet;

		auto push_escaped = [&](unsigned char byte) -> void
		{
			if (byte == '\n')
			{
				packet.push_back((char)0xad); // ESC
				packet.push_back((char)0xae);
			}
			else if (byte == '\r')
			{
				packet.push_back((char)0xad); // ESC
				packet.push_back((char)0xaf);
			}
			else if (byte == 0xad) // Also escape ESC itself
			{
				packet.push_back((char)0xad);
				packet.push_back((char)0xad);
			}
			else
			{
				packet.push_back((char)byte);
			}
		};

		// Start byte: 0xac
		push_escaped(0xac);

		// Version: 0
		push_escaped(0x00);

		// Flags byte: bit 0 = has signal/ppm, bit 1 = has CRC
		unsigned char flags = 0;

		if (tag.level != LEVEL_UNDEFINED && tag.ppm != PPM_UNDEFINED)
			flags |= 0x01;

		flags |= crc ? 0x02 : 0x00;

		push_escaped(flags);

		// Timestamp: 8 bytes (long long)
		long long ts = (long long)rxtime;
		for (int i = 7; i >= 0; i--)
		{
			push_escaped((ts >> (i * 8)) & 0xFF);
		}

		if (flags & 0x01)
		{
			int signal_tenths = (int)(tag.level * 10.0f);

			push_escaped((signal_tenths >> 8) & 0xFF);
			push_escaped(signal_tenths & 0xFF);

			int ppm_tenths = (int)(tag.ppm * 10.0f);
			push_escaped((unsigned char)(int8_t)ppm_tenths);
		}

		push_escaped((unsigned char)channel);

		push_escaped((length >> 8) & 0xFF);
		push_escaped(length & 0xFF);

		// AIS message data
		int numBytes = (length + 7) / 8;
		for (int i = 0; i < numBytes; i++)
		{
			push_escaped(data[i]);
		}

		if (crc)
		{
			uint16_t crc_value = Util::Helper::CRC16((const uint8_t *)packet.data(), packet.size());
			push_escaped((crc_value >> 8) & 0xFF);
			push_escaped(crc_value & 0xFF);
		}

		// End character: \n (not escaped)
		packet.push_back('\n');

		return packet;
	}

	bool Message::validate()
	{
		if (getLength() == 0)
			return true;

		const int ml[27] = {149, 149, 149, 168, 418, 88, 72, 56, 168, 70, 168, 72, 40, 40, 88, 92, 80, 168, 312, 70, 271, 145, 154, 160, 72, 60, 96};

		if (type() < 1 || type() > 27)
			return false;
		if (getLength() < ml[type() - 1])
			return false;

		return true;
	}

	unsigned Message::getUint(int start, int len) const
	{
		// we start 2nd part of first byte and stop first part of last byte
		int x = start >> 3, y = start & 7;
		unsigned u = data[x] & (0xFF >> y);
		int remaining = len - 8 + y;

		// first byte is last byte
		if (remaining <= 0)
		{
			return u >> (-remaining);
		}
		// add full bytes
		while (remaining >= 8)
		{
			u <<= 8;
			u |= data[++x];
			remaining -= 8;
		}
		// make room for last bits if needed
		if (remaining > 0)
		{
			u <<= remaining;
			u |= data[++x] >> (8 - remaining);
		}

		return u;
	}

	bool Message::setUint(int start, int len, unsigned val)
	{
		if (start + len >= MAX_AIS_LENGTH)
			return false;

		int x = start >> 3, y = start & 7;
		int remaining = len;

		if (8 - y >= remaining)
		{
			uint8_t bitmask = (0xFF >> (8 - remaining)) << (8 - y - remaining);
			data[x] = (data[x] & ~bitmask) | ((val << (8 - y - remaining)) & bitmask);
			length = MAX(start + len, length);
			return true;
		}

		uint8_t bitmask = 0xFF >> y;
		data[x] = (data[x] & ~bitmask) | ((val >> (remaining - 8 + y)) & bitmask);
		remaining -= 8 - y;
		x++;

		while (remaining >= 8)
		{
			data[x++] = (val >> (remaining - 8)) & 0xFF;
			remaining -= 8;
		}

		if (remaining > 0)
		{
			uint8_t bitmask = 0xFF << (8 - remaining);
			data[x] = (data[x] & ~bitmask) | ((val << (8 - remaining)) & bitmask);
		}
		length = MAX(start + len, length);
		return true;
	}

	int Message::getInt(int start, int len) const
	{
		const unsigned ones = ~0;
		unsigned u = getUint(start, len);

		// extend sign bit for the full bit
		if (u & (1 << (len - 1)))
			u |= ones << len;
		return (int)u;
	}

	bool Message::setInt(int start, int len, int val)
	{
		return setUint(start, len, (unsigned)val);
	}

	void Message::getText(int start, int len, std::string &str) const
	{

		int end = start + len;
		str.clear();

		while (start < end)
		{
			int c = getUint(start, 6);

			// 0       ->   @ and ends the string
			// 1 - 31  ->   65+ ( i.e. setting bit 6 )
			// 32 - 63 ->   32+ ( i.e. doing nothing )

			if (!c)
				break;
			if (!(c & 32))
				c |= 64;

			str += (char)c;
			start += 6;
		}
		return;
	}

	void Message::setText(int start, int len, const char *str)
	{

		int end = start + len;
		if (end >= MAX_AIS_LENGTH)
			return;

		int idx = 0;
		bool reachedEnd = false;

		while (start < end)
		{
			unsigned c = 0;

			if (!reachedEnd)
			{
				c = str[idx++];

				if (!(c >= 32 && c <= 63) && !(c >= 65 && c <= 95))
				{
					reachedEnd = true;
					c = 0;
				}
				else if (c & 64)
					c &= 0b00111111;
			}

			setUint(start, 6, c);

			start += 6;
		}

		length = MAX(end, length);

		return;
	}

	void Message::buildNMEA(TAG &tag, int id)
	{
		const char comma = ',';

		const int IDX_COUNT = 7;
		const int IDX_NUMBER = 9;
		const int IDX_OWN_MMSI = 5;

		if (id >= 0 && id < 10)
			ID = id;

		int nAISletters = (length + 6 - 1) / 6;
		int nSentences = (nAISletters == 0) ? 1 : (nAISletters + MAX_NMEA_CHARS - 1) / MAX_NMEA_CHARS;

		line.resize(11);

		line[IDX_OWN_MMSI] = own_mmsi == mmsi() ? 'O' : 'M';
		line[IDX_COUNT] = (char)(nSentences + '0');
		line[IDX_NUMBER] = '0';

		if (nSentences > 1)
		{
			line += (char)(ID + '0');
			ID = (ID + 1) % 10;
		}

		line += comma;
		if (channel != '?')
			line += channel;
		line += comma;

		int header = line.length();
		NMEA.clear();

		for (int s = 0, l = 0; s < nSentences; s++)
		{

			line.resize(header);
			line[IDX_NUMBER]++;

			for (int i = 0; l < nAISletters && i < MAX_NMEA_CHARS; i++, l++)
				line += getLetter(l);

			line += comma;
			line += (char)(((s == nSentences - 1) ? nAISletters * 6 - length : 0) + '0');

			int c = NMEAchecksum(line);
			line += '*';
			line += (c >> 4) < 10 ? (c >> 4) + '0' : (c >> 4) + 'A' - 10;
			line += (c & 0xF) < 10 ? (c & 0xF) + '0' : (c & 0xF) + 'A' - 10;
			NMEA.push_back(line);
		}
	}

	char Message::getLetter(int pos) const
	{
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;
		uint16_t w = (data[x] << 8) | data[x + 1];

		const int mask = (1 << 6) - 1;
		int l = (w >> (16 - 6 - y)) & mask;

		// zero for bits not formally set
		int overrun = pos * 6 + 6 - length;
		if (overrun > 0)
			l &= 0xFF << overrun;
		return l < 40 ? (char)(l + 48) : (char)(l + 56);
	}

	void Message::setLetter(int pos, char c)
	{
		int x = (pos * 6) >> 3, y = (pos * 6) & 7;

		int newlength = MAX((pos + 1) * 6, length);

		if (newlength >= MAX_AIS_LENGTH)
		{
			Error() << "Message::setLetter: length exceeds maximum AIS length." << std::endl;
			return;
		}

		length = newlength;
		c = (c >= 96 ? c - 56 : c - 48) & 0b00111111;

		switch (y)
		{
		case 0:
			data[x] = (data[x] & 0b00000011) | (c << 2);
			break;
		case 2:
			data[x] = (data[x] & 0b11000000) | c;
			break;
		case 4:
			data[x] = (data[x] & 0b11110000) | (c >> 2);
			data[x + 1] = (data[x + 1] & 0b00111111) | ((c & 3) << 6);
			break;
		case 6:
			data[x] = (data[x] & 0b11111100) | (c >> 4);
			data[x + 1] = (data[x + 1] & 0b00001111) | ((c & 15) << 4);
			break;
		default:
			break;
		}
	}

	bool Filter::SetOption(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "ALLOW_TYPE")
		{

			std::stringstream ss(arg);
			std::string type_str;
			allow = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned type = Util::Parse::Integer(type_str, 1, 27);
				allow |= 1U << type;
			}
			return true;
		}
		else if (option == "ALLOW_REPEAT" || option == "SELECT_REPEAT")
		{
			std::stringstream ss(arg);
			std::string type_str;
			allow_repeat = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned r = Util::Parse::Integer(type_str, 0, 3);
				allow_repeat |= 1U << r;
			}
			return true;
		}
		else if (option == "DESC" || option == "DESCRIPTION")
			return true;
		else if (option == "BLOCK_TYPE")
		{

			std::stringstream ss(arg);
			std::string type_str;
			unsigned block = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned type = Util::Parse::Integer(type_str, 1, 27);
				block |= 1U << type;
			}
			allow = ~block & all;
			return true;
		}
		else if (option == "BLOCK_REPEAT")
		{

			std::stringstream ss(arg);
			std::string type_str;
			unsigned block = 0;
			while (getline(ss, type_str, ','))
			{
				unsigned r = Util::Parse::Integer(type_str, 0, 3);
				block |= 1U << r;
			}
			allow_repeat = ~block & all;
			return true;
		}
		else if (option == "FILTER")
		{
			Util::Convert::toUpper(arg);
			on = Util::Parse::Switch(arg);
			return true;
		}
		else if (option == "DOWNSAMPLE")
		{
			Util::Convert::toUpper(arg);
			downsample = Util::Parse::Switch(arg);
			return true;
		}
		else if (option == "GPS")
		{
			Util::Convert::toUpper(arg);
			GPS = Util::Parse::Switch(arg);
			return true;
		}
		else if (option == "AIS")
		{
			Util::Convert::toUpper(arg);
			AIS = Util::Parse::Switch(arg);
			return true;
		}
		else if (option == "ID" || option == "SELECT_ID")
		{
			std::stringstream ss(arg);
			std::string id_str;
			while (getline(ss, id_str, ','))
			{
				ID_allowed.push_back(Util::Parse::Integer(id_str, 0, 999999));
			}
			return true;
		}
		else if (option == "ALLOW_CHANNEL" || option == "SELECT_CHANNEL")
		{

			allowed_channels = arg;
			Util::Convert::toUpper(allowed_channels);

			return true;
		}
		else if (option == "ALLOW_MMSI" || option == "SELECT_MMSI")
		{
			std::stringstream ss(arg);
			std::string mmsi_str;
			while (getline(ss, mmsi_str, ','))
			{
				MMSI_allowed.push_back(Util::Parse::Integer(mmsi_str, 0, 999999999));
			}
			return true;
		}
		else if (option == "BLOCK_MMSI")
		{
			std::stringstream ss(arg);
			std::string mmsi_str;
			while (getline(ss, mmsi_str, ','))
			{
				MMSI_blocked.push_back(Util::Parse::Integer(mmsi_str, 0, 999999999));
			}
			return true;
		}
		else if (option == "REMOVE_EMPTY")
		{
			Util::Convert::toUpper(arg);
			remove_empty = Util::Parse::Switch(arg);
			return true;
		}
		return false;
	}

	std::string Filter::getAllowed()
	{

		if (allow == all)
			return std::string("ALL");

		std::string ret;
		for (unsigned i = 1; i <= 27; i++)
		{
			if ((allow & (1U << i)) > 0)
				ret += (!ret.empty() ? std::string(",") : std::string("")) + std::to_string(i);
		}
		return ret;
	}

	bool Filter::include(const Message &msg)
	{
		if (downsample)
		{
			if (msg.isOwn())
			{
				if (msg.getRxTimeUnix() - last_VDO < downsample_time)
				{
					return false;
				}
				last_VDO = msg.getRxTimeUnix();
			}
		}

		if (!on)
			return true;
		if (!AIS)
			return false;

		if (remove_empty)
		{
			if (msg.getLength() == 0)
				return false;
		}

		bool ID_ok = true;
		if (ID_allowed.size())
		{

			ID_ok = false;
			for (auto id : ID_allowed)
			{
				if (msg.getStation() == id)
				{
					ID_ok = true;
					break;
				}
			}
		}
		if (!ID_ok)
			return false;

		bool CH_ok = true;
		if (!allowed_channels.empty())
		{
			CH_ok = false;
			for (char c : allowed_channels)
			{
				if (msg.getChannel() == c)
				{
					CH_ok = true;
					break;
				}
			}
		}

		if (!CH_ok)
			return false;

		bool MMSI_ok = true;
		if (!MMSI_allowed.empty())
		{
			MMSI_ok = false;
			for (auto mmsi : MMSI_allowed)
			{
				if (msg.mmsi() == mmsi)
				{
					MMSI_ok = true;
					break;
				}
			}
		}

		if (!MMSI_ok)
			return false;

		if (!MMSI_blocked.empty())
		{
			for (auto mmsi : MMSI_blocked)
			{
				if (msg.mmsi() == mmsi)
				{
					return false;
				}
			}
		}

		unsigned type = msg.type() & 31;
		unsigned repeat = msg.repeat() & 3;

		bool type_ok = ((1U << type) & allow) != 0;
		bool repeat_ok = ((1U << repeat) & allow_repeat) != 0;

		return type_ok && repeat_ok;
	}
}
