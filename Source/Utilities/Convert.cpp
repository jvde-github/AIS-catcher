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

#include <iomanip>
#include <sstream>
#include <algorithm>

#include "Convert.h"

namespace Util
{
	std::string Convert::toTimeStr(const std::time_t &t)
	{
		// Fast UTC breakdown without gmtime/strftime (avoids TZ locks)
		int64_t s = static_cast<int64_t>(t);
		if (s < 0) s = 0;
		int sec = s % 60; s /= 60;
		int min = s % 60; s /= 60;
		int hour = s % 24; s /= 24;

		// Days since 1970-01-01 to y/m/d (civil_from_days, Howard Hinnant)
		int z = static_cast<int>(s) + 719468;
		int era = (z >= 0 ? z : z - 146096) / 146097;
		unsigned doe = static_cast<unsigned>(z - era * 146097);
		unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		int y = static_cast<int>(yoe) + era * 400;
		unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		unsigned mp = (5 * doy + 2) / 153;
		unsigned d = doy - (153 * mp + 2) / 5 + 1;
		unsigned m = mp + (mp < 10 ? 3 : -9);
		y += (m <= 2);

		char str[16];
		auto put2 = [](char *p, int v) { p[0] = '0' + v / 10; p[1] = '0' + v % 10; };
		put2(str, y / 100);
		put2(str + 2, y % 100);
		put2(str + 4, m);
		put2(str + 6, d);
		put2(str + 8, hour);
		put2(str + 10, min);
		put2(str + 12, sec);
		str[14] = '\0';
		return std::string(str);
	}

	std::string Convert::toTimestampStr(const std::time_t &t)
	{
		int64_t s = static_cast<int64_t>(t);
		if (s < 0) s = 0;
		int sec = s % 60; s /= 60;
		int min = s % 60; s /= 60;
		int hour = s % 24; s /= 24;

		int z = static_cast<int>(s) + 719468;
		int era = (z >= 0 ? z : z - 146096) / 146097;
		unsigned doe = static_cast<unsigned>(z - era * 146097);
		unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		int y = static_cast<int>(yoe) + era * 400;
		unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		unsigned mp = (5 * doy + 2) / 153;
		unsigned d = doy - (153 * mp + 2) / 5 + 1;
		unsigned m = mp + (mp < 10 ? 3 : -9);
		y += (m <= 2);

		char str[22];
		auto put2 = [](char *p, int v) { p[0] = '0' + v / 10; p[1] = '0' + v % 10; };
		put2(str, y / 100);
		put2(str + 2, y % 100);
		str[4] = '/';
		put2(str + 5, m);
		str[7] = '/';
		put2(str + 8, d);
		str[10] = ' ';
		put2(str + 11, hour);
		str[13] = ':';
		put2(str + 14, min);
		str[16] = ':';
		put2(str + 17, sec);
		str[19] = '\0';
		return std::string(str);
	}

	std::string Convert::toHexString(uint64_t l)
	{
		std::stringstream s;
		s << std::uppercase << std::hex << std::setfill('0') << std::setw(16) << l;
		return s.str();
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
		const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
			c = toupper(c);
	}

	void Convert::toLower(std::string &s)
	{
		for (auto &c : s)
			c = tolower(c);
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
