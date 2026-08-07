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

#include <cctype>

#include "Convert.h"

namespace Util
{
	namespace
	{
		struct UTC
		{
			int year, month, day, hour, min, sec;
		};

		// No gmtime/strftime: avoids the TZ lock.
		// civil_from_days: https://howardhinnant.github.io/date_algorithms.html
		UTC breakdown(const std::time_t &t)
		{
			int64_t s = static_cast<int64_t>(t);
			if (s < 0) s = 0;

			UTC u;
			u.sec = s % 60; s /= 60;
			u.min = s % 60; s /= 60;
			u.hour = s % 24; s /= 24;

			int z = static_cast<int>(s) + 719468;
			int era = (z >= 0 ? z : z - 146096) / 146097;
			unsigned doe = static_cast<unsigned>(z - era * 146097);
			unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
			unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
			unsigned mp = (5 * doy + 2) / 153;

			u.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
			u.month = static_cast<int>(mp + (mp < 10 ? 3 : -9));
			u.year = static_cast<int>(yoe) + era * 400 + (u.month <= 2);
			return u;
		}

		inline void put2(char *p, int v)
		{
			p[0] = '0' + v / 10;
			p[1] = '0' + v % 10;
		}
	}

	std::string Convert::toTimeStr(const std::time_t &t)
	{
		UTC u = breakdown(t);

		char str[16];
		put2(str, u.year / 100);
		put2(str + 2, u.year % 100);
		put2(str + 4, u.month);
		put2(str + 6, u.day);
		put2(str + 8, u.hour);
		put2(str + 10, u.min);
		put2(str + 12, u.sec);
		return std::string(str, 14);
	}

	std::string Convert::toTimestampStr(const std::time_t &t)
	{
		UTC u = breakdown(t);

		char str[24];
		put2(str, u.year / 100);
		put2(str + 2, u.year % 100);
		str[4] = '/';
		put2(str + 5, u.month);
		str[7] = '/';
		put2(str + 8, u.day);
		str[10] = ' ';
		put2(str + 11, u.hour);
		str[13] = ':';
		put2(str + 14, u.min);
		str[16] = ':';
		put2(str + 17, u.sec);
		return std::string(str, 19);
	}

	std::string Convert::toHexString(uint64_t l)
	{
		static const char hex[] = "0123456789ABCDEF";

		char str[16];
		for (int i = 15; i >= 0; i--, l >>= 4)
			str[i] = hex[l & 0xF];
		return std::string(str, 16);
	}

	std::string Convert::toString(Format format)
	{
		switch (format)
		{
		case Format::CF32:
			return "CF32";
		case Format::CS16:
			return "CS16";
		case Format::CU8:
			return "CU8";
		case Format::CS8:
			return "CS8";
		case Format::TXT:
			return "TXT";
		case Format::BASESTATION:
			return "BASESTATION";
		case Format::BEAST:
			return "BEAST";
		case Format::RAW1090:
			return "RAW1090";
		case Format::F32_FS4:
			return "F32_FS4";
		case Format::DC16H:
			return "DC16H";
		default:
			break;
		}
		return "UNKNOWN";
	}

	std::string Convert::toString(PROTOCOL protocol)
	{
		switch (protocol)
		{
		case PROTOCOL::NONE:
			return "NONE";
		case PROTOCOL::RTLTCP:
			return "RTLTCP";
		case PROTOCOL::GPSD:
			return "GPSD";
		case PROTOCOL::TXT:
			return "TXT";
		case PROTOCOL::WS:
			return "WS";
		case PROTOCOL::MQTT:
			return "MQTT";
		case PROTOCOL::WSMQTT:
			return "WSMQTT";
		case PROTOCOL::BASESTATION:
			return "BASESTATION";
		case PROTOCOL::BEAST:
			return "BEAST";
		case PROTOCOL::RAW1090:
			return "RAW1090";
		case PROTOCOL::TLS:
			return "TLS";
		case PROTOCOL::TCP:
			return "TCP";
		case PROTOCOL::MQTTS:
			return "MQTTS";
		case PROTOCOL::WSSMQTT:
			return "WSSMQTT";
		}
		return "";
	}

	std::string Convert::toString(MessageFormat out)
	{
		switch (out)
		{
		case MessageFormat::SILENT:
			return "NONE";
		case MessageFormat::NMEA:
			return "NMEA";
		case MessageFormat::NMEA_TAG:
			return "NMEA_TAG";
		case MessageFormat::COMMUNITY_HUB:
			return "COMMUNITY_HUB";
		case MessageFormat::BINARY_NMEA:
			return "BINARY_NMEA";
		case MessageFormat::FULL:
			return "FULL";
		case MessageFormat::JSON_NMEA:
			return "JSON_NMEA";
		case MessageFormat::JSON_SPARSE:
			return "JSON_SPARSE";
		case MessageFormat::JSON_FULL:
			return "JSON_FULL";
		case MessageFormat::JSON_ANNOTATED:
			return "JSON_ANNOTATED";
		default:
			break;
		}
		return "";
	}

	std::string Convert::toBase64(const std::string &in)
	{
		static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string out;
		unsigned val = 0;
		int valb = -6;
		for (uint8_t c : in)
		{
			val = (val << 8) + c;
			valb += 8;
			while (valb >= 0)
			{
				out.push_back(base64_chars[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}
		if (valb > -6)
			out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
		while (out.size() % 4)
			out.push_back('=');
		return out;
	}

	void Convert::toUpper(std::string &s)
	{
		for (auto &c : s)
			c = (char)toupper((unsigned char)c);
	}

	void Convert::toLower(std::string &s)
	{
		for (auto &c : s)
			c = (char)tolower((unsigned char)c);
	}

	// not using the complex class functions to be independent of internal representation
	void Convert::toFloat(CU8 *in, CFLOAT32 *out, int len)
	{
		uint8_t *data = (uint8_t *)in;

		for (int i = 0; i < len; i++)
		{
			out[i].real(((int)data[2 * i] - 128) / 128.0f);
			out[i].imag(((int)data[2 * i + 1] - 128) / 128.0f);
		}
	}

	void Convert::toFloat(CS8 *in, CFLOAT32 *out, int len)
	{
		int8_t *data = (int8_t *)in;

		for (int i = 0; i < len; i++)
		{
			out[i].real(data[2 * i] / 128.0f);
			out[i].imag(data[2 * i + 1] / 128.0f);
		}
	}

	void Convert::toFloat(CS16 *in, CFLOAT32 *out, int len)
	{
		int16_t *data = (int16_t *)in;

		for (int i = 0; i < len; i++)
		{
			out[i].real(data[2 * i] / 32768.0f);
			out[i].imag(data[2 * i + 1] / 32768.0f);
		}
	}

	void Convert::toFloat(DC16H *in, CFLOAT32 *out, int len)
	{
		uint32_t *words = (uint32_t *)in;

		for (int i = 0; i < len; i++)
		{
			// Extract I and Q as 16-bit signed values from 32-bit words
			int32_t i_sample = ((int32_t)(words[i * 3] << 16)) >> 16;
			int32_t q_sample = ((int32_t)(words[i * 3 + 1] << 16)) >> 16;

			out[i].real(i_sample / 32768.0f);
			out[i].imag(-q_sample / 32768.0f);
		}
	}
}
