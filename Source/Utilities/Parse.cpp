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

#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>

#include "Parse.h"
#include "Convert.h"

namespace Util
{
	long Parse::Integer(const std::string &str, long min, long max, const std::string &setting)
	{
		long number = 0;
		size_t sz = str.length();

		try
		{
			if (!str.empty())
				number = std::stol(str, &sz);
		}
		catch (const std::exception &)
		{
			if (setting.empty())
				throw std::runtime_error("expected a number");
			else
				throw std::runtime_error("expected a number for setting " + setting);
		}

		if (str.length() > sz && (str[sz] == 'K' || str[sz] == 'k'))
			number *= 1000;

		if (min != 0 || max != 0)
			if (number < min || number > max)
			{
				if (setting.empty())
					throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "]");
				else
					throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "] for setting " + setting);
			}
		return number;
	}

	double Parse::Float(const std::string &str, double min, double max)
	{
		double number = 0;

		try
		{
			if (!str.empty())
				number = std::stod(str);
		}
		catch (const std::exception &)
		{
			throw std::runtime_error("expected a number as input.");
		}

		if (number < min || number > max)
			throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "]");

		return number;
	}

	std::string Parse::PercentDecode(const std::string &s)
	{
		std::string out;
		for (size_t i = 0; i < s.size(); i++)
		{
			if (s[i] == '%' && i + 2 < s.size() && Util::Convert::isHexDigit(s[i + 1]) && Util::Convert::isHexDigit(s[i + 2]))
			{
				out += (char)(Util::Convert::hexDigitValue(s[i + 1]) * 16 + Util::Convert::hexDigitValue(s[i + 2]));
				i += 2;
			}
			else
				out += s[i];
		}
		return out;
	}

	std::string Parse::FormDecode(const std::string &s)
	{
		std::string spaced = s;
		for (char &ch : spaced)
			if (ch == '+')
				ch = ' ';
		return PercentDecode(spaced);
	}

	std::vector<std::pair<std::string, std::string>> Parse::Query(const std::string &query)
	{
		std::vector<std::pair<std::string, std::string>> out;
		std::size_t pos = 0;
		while (pos < query.size())
		{
			std::size_t amp = query.find('&', pos);
			if (amp == std::string::npos)
				amp = query.size();
			if (amp > pos)
			{
				const std::size_t eq = query.find('=', pos);
				if (eq != std::string::npos && eq < amp)
					out.push_back(std::make_pair(FormDecode(query.substr(pos, eq - pos)), FormDecode(query.substr(eq + 1, amp - eq - 1))));
				else
					out.push_back(std::make_pair(FormDecode(query.substr(pos, amp - pos)), std::string()));
			}
			pos = amp + 1;
		}
		return out;
	}

	std::string Parse::QueryParam(const std::string &query, const std::string &name)
	{
		for (const auto &p : Query(query))
			if (p.first == name)
				return p.second;
		return std::string();
	}

	std::string Parse::URLWithoutCredentials(const std::string &url)
	{
		size_t start = url.find("://");
		start = start == std::string::npos ? 0 : start + 3;
		size_t end = url.find('/', start);
		size_t at = url.rfind('@', end == std::string::npos ? std::string::npos : end);
		if (at == std::string::npos || at < start)
			return url;
		return url.substr(0, start) + url.substr(at + 1);
	}

	std::string Parse::URLForLog(const std::string &url)
	{
		size_t start = url.find("://");
		start = start == std::string::npos ? 0 : start + 3;
		size_t end = url.find_first_of("/?#", start);
		end = end == std::string::npos ? url.size() : end;
		size_t at = url.rfind('@', end);
		if (at != std::string::npos && at >= start && at < end)
			return url.substr(0, start) + url.substr(at + 1, end - at - 1);
		return url.substr(0, end);
	}

	void Parse::URL(const std::string &url, std::string &protocol, std::string &username, std::string &password, std::string &host, std::string &port, std::string &path, bool *has_password)
	{
		std::string s = url;

		protocol.clear();
		host.clear();
		port.clear();
		path = "/";

		size_t idx = s.find("://");
		if (idx != std::string::npos)
		{
			protocol = s.substr(0, idx);
			s = s.substr(idx + 3);
		}

		idx = s.find('/');
		if (idx != std::string::npos)
		{
			path = s.substr(idx);
			s.resize(idx);
		}

		size_t at_pos = s.rfind('@');
		if (at_pos != std::string::npos)
		{
			username.clear();
			password.clear();
			if (has_password)
				*has_password = false;
			std::string userinfo = s.substr(0, at_pos);
			s = s.substr(at_pos + 1);

			size_t colon_pos = userinfo.find(':');
			if (colon_pos != std::string::npos)
			{
				username = PercentDecode(userinfo.substr(0, colon_pos));
				password = PercentDecode(userinfo.substr(colon_pos + 1));
				if (has_password)
					*has_password = true;
			}
			else
			{
				username = PercentDecode(userinfo);
			}
		}

		idx = s.find(':');
		if (idx != std::string::npos)
		{
			host = s.substr(0, idx);
			port = s.substr(idx + 1);
		}
		else
		{
			host = s;
		}
	}

	// An enum's names: the first of a value is the one written, any of them is read as it.
	template <typename E>
	struct Name
	{
		const char *text;
		E value;
	};

	static const Name<Format> FORMATS[] = {
		{"CU8", Format::CU8}, {"CF32", Format::CF32}, {"CS16", Format::CS16}, {"CS8", Format::CS8}, {"TXT", Format::TXT},
		{"BASESTATION", Format::BASESTATION}, {"BEAST", Format::BEAST}, {"RAW1090", Format::RAW1090}, {"N2K", Format::N2K},
		{"F32_FS4", Format::F32_FS4}, {"DC16H", Format::DC16H}};

	static const Name<Type> TYPES[] = {
		{"NONE", Type::NONE}, {"RTLSDR", Type::RTLSDR}, {"AIRSPY", Type::AIRSPY}, {"AIRSPYHF", Type::AIRSPYHF}, {"SDRPLAY", Type::SDRPLAY},
		{"WAVFILE", Type::WAVFILE}, {"RAWFILE", Type::RAWFILE}, {"FILE", Type::RAWFILE}, {"RTLTCP", Type::RTLTCP}, {"HACKRF", Type::HACKRF},
		{"HYDRASDR", Type::HYDRASDR}, {"SOAPYSDR", Type::SOAPYSDR}, {"ZMQ", Type::ZMQ}, {"SERIALPORT", Type::SERIALPORT}, {"UDP", Type::UDP},
		{"UDPSERVER", Type::UDP}, {"SPYSERVER", Type::SPYSERVER}, {"NMEA2000", Type::N2K}};

	static const Name<PROTOCOL> PROTOCOLS[] = {
		{"NONE", PROTOCOL::NONE}, {"RTLTCP", PROTOCOL::RTLTCP}, {"GPSD", PROTOCOL::GPSD}, {"TXT", PROTOCOL::TXT}, {"MQTT", PROTOCOL::MQTT},
		{"WS", PROTOCOL::WS}, {"WSMQTT", PROTOCOL::WSMQTT}, {"BASESTATION", PROTOCOL::BASESTATION}, {"BEAST", PROTOCOL::BEAST},
		{"RAW1090", PROTOCOL::RAW1090}, {"TLS", PROTOCOL::TLS}, {"TCP", PROTOCOL::TCP}, {"ZMQ", PROTOCOL::ZMQ}, {"MQTTS", PROTOCOL::MQTTS},
		{"WSSMQTT", PROTOCOL::WSSMQTT}, {"WSS", PROTOCOL::WSS}, {"N2K", PROTOCOL::N2K}, {"CAN", PROTOCOL::CAN}};

	static const Name<MessageFormat> MESSAGE_FORMATS[] = {
		{"NONE", MessageFormat::SILENT}, {"0", MessageFormat::SILENT}, {"NMEA", MessageFormat::NMEA}, {"1", MessageFormat::NMEA},
		{"NMEA_TAG", MessageFormat::NMEA_TAG}, {"7", MessageFormat::NMEA_TAG}, {"COMMUNITY_HUB", MessageFormat::COMMUNITY_HUB},
		{"BINARY_NMEA", MessageFormat::BINARY_NMEA}, {"NMEA_BINARY", MessageFormat::BINARY_NMEA}, {"FULL", MessageFormat::FULL},
		{"2", MessageFormat::FULL}, {"N2K", MessageFormat::N2K}, {"JSON_NMEA", MessageFormat::JSON_NMEA}, {"3", MessageFormat::JSON_NMEA},
		{"JSON_SPARSE", MessageFormat::JSON_SPARSE}, {"4", MessageFormat::JSON_SPARSE}, {"JSON_FULL", MessageFormat::JSON_FULL},
		{"5", MessageFormat::JSON_FULL}, {"JSON_ANNOTATED", MessageFormat::JSON_ANNOTATED}, {"6", MessageFormat::JSON_ANNOTATED}};

	template <typename E, std::size_t N>
	static bool read(const Name<E> (&names)[N], std::string text, E &out)
	{
		Convert::toUpper(text);
		for (const auto &n : names)
			if (text == n.text)
			{
				out = n.value;
				return true;
			}
		return false;
	}

	template <typename E, std::size_t N>
	static std::string write(const Name<E> (&names)[N], E value, const char *unknown = "")
	{
		for (const auto &n : names)
			if (n.value == value)
				return n.text;
		return unknown;
	}

	bool Parse::StreamFormat(std::string str, Format &format) { return read(FORMATS, str, format); }
	bool Parse::DeviceType(std::string str, Type &type) { return read(TYPES, str, type); }
	bool Parse::Protocol(std::string arg, PROTOCOL &protocol) { return read(PROTOCOLS, arg, protocol); }
	bool Parse::OutputFormat(std::string str, MessageFormat &out) { return read(MESSAGE_FORMATS, str, out); }
	std::string Parse::DeviceTypeString(Type type) { return write(TYPES, type); }
	// declared in Convert.h; here beside the names they share with the readers
	std::string Convert::toString(Format format) { return write(FORMATS, format, "UNKNOWN"); }
	std::string Convert::toString(PROTOCOL protocol) { return write(PROTOCOLS, protocol); }
	std::string Convert::toString(MessageFormat out) { return write(MESSAGE_FORMATS, out); }

	bool Parse::Switch(const std::string &arg)
	{
		return Switch(arg, "ON", "OFF");
	}

	bool Parse::Switch(std::string arg, const std::string &TrueString, const std::string &FalseString)
	{
		Util::Convert::toUpper(arg);
		if (arg == FalseString || arg == "FALSE")
			return false;
		if (arg != TrueString && arg != "TRUE")
			throw std::runtime_error("unknown switch \"" + arg + "\"");

		return true;
	}

	bool Parse::Digits(const std::string &s, long long &out, std::size_t most)
	{
		if (s.empty() || s.size() > most || s.size() > 18 || s.find_first_not_of("0123456789") != std::string::npos)
			return false;
		out = std::atoll(s.c_str());
		return true;
	}

	bool Parse::AutoInteger(std::string arg, int min, int max, int &val)
	{
		Util::Convert::toUpper(arg);
		if (arg == "AUTO")
			return true;

		val = Integer(arg, min, max);
		return false;
	}

	bool Parse::AutoFloat(std::string arg, double min, double max, double &val)
	{
		Util::Convert::toUpper(arg);
		if (arg == "AUTO")
			return true;

		val = Float(arg, min, max);
		return false;
	}

	bool Parse::OptionalInteger(std::string arg, int min, int max, unsigned &val, unsigned default_val)
	{
		Util::Convert::toUpper(arg);
		if (arg == "OFF" || arg == "FALSE" || arg == "NO")
		{
			val = default_val;
			return false;
		}

		val = (unsigned)Integer(arg, min, max);
		return true;
	}

	void Parse::Split(const std::string &s, char delim, std::vector<std::string> &out)
	{
		out.clear();
		std::stringstream ss(s);
		std::string tok;
		while (std::getline(ss, tok, delim))
		{
			tok.erase(0, tok.find_first_not_of(" \t"));
			auto last = tok.find_last_not_of(" \t");
			if (last != std::string::npos)
				tok.erase(last + 1);
			if (!tok.empty())
				out.push_back(tok);
		}
	}
}
