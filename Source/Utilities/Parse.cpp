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

#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>

#include "Parse.h"
#include "Convert.h"

namespace Util
{
	long Parse::Integer(std::string str, long min, long max, const std::string &setting)
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

	double Parse::Float(std::string str, double min, double max)
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

	void Parse::URL(const std::string &url, std::string &protocol, std::string &username, std::string &password, std::string &host, std::string &port, std::string &path)
	{
		std::string s = url;

		protocol.clear();
		username.clear();
		password.clear();
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
			s = s.substr(0, idx);
		}

		size_t at_pos = s.find('@');
		if (at_pos != std::string::npos)
		{
			std::string userinfo = s.substr(0, at_pos);
			s = s.substr(at_pos + 1);

			size_t colon_pos = userinfo.find(':');
			if (colon_pos != std::string::npos)
			{
				username = userinfo.substr(0, colon_pos);
				password = userinfo.substr(colon_pos + 1);
			}
			else
			{
				username = userinfo;
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

	void Parse::HTTP_URL(const std::string &url, std::string &protocol, std::string &host, std::string &port, std::string &path)
	{

		int idx = url.find("://");
		if (idx != std::string::npos)
		{

			protocol = url.substr(0, idx);
			path = "/";

			idx += 3;
			int hostEnd = url.find(':', idx);
			if (hostEnd == std::string::npos)
			{
				hostEnd = url.find('/', idx);
			}

			if (hostEnd == std::string::npos)
			{
				host = url.substr(idx, url.length() - idx);
			}
			else
			{
				host = url.substr(idx, hostEnd - idx);

				int portStart = url.find(':', hostEnd);
				if (portStart != std::string::npos)
				{
					int portEnd = url.find('/', portStart);
					port = url.substr(portStart + 1, (portEnd != std::string::npos ? portEnd : url.length()) - portStart - 1);
				}

				int pathStart = url.find('/', hostEnd);
				if (pathStart != std::string::npos)
				{
					path = url.substr(pathStart);
				}
			}
		}
	}

	bool Parse::StreamFormat(std::string str, Format &format)
	{
		Convert::toUpper(str);
		if (str == "CU8")
			format = Format::CU8;
		else if (str == "CF32")
			format = Format::CF32;
		else if (str == "CS16")
			format = Format::CS16;
		else if (str == "CS8")
			format = Format::CS8;
		else if (str == "TXT")
			format = Format::TXT;
		else if (str == "BASESTATION")
			format = Format::BASESTATION;
		else if (str == "BEAST")
			format = Format::BEAST;
		else if (str == "RAW1090")
			format = Format::RAW1090;
		else if (str == "F32_FS4")
			format = Format::F32_FS4;
		else
			return false;

		return true;
	}

	bool Parse::DeviceType(std::string str, Type &type)
	{
		Convert::toUpper(str);
		if (str == "NONE")
			type = Type::NONE;
		else if (str == "RTLSDR")
			type = Type::RTLSDR;
		else if (str == "AIRSPY")
			type = Type::AIRSPY;
		else if (str == "AIRSPYHF")
			type = Type::AIRSPYHF;
		else if (str == "SDRPLAY")
			type = Type::SDRPLAY;
		else if (str == "WAVFILE")
			type = Type::WAVFILE;
		else if (str == "RAWFILE" || str == "FILE")
			type = Type::RAWFILE;
		else if (str == "RTLTCP")
			type = Type::RTLTCP;
		else if (str == "HACKRF")
			type = Type::HACKRF;
		else if (str == "SOAPYSDR")
			type = Type::SOAPYSDR;
		else if (str == "ZMQ")
			type = Type::ZMQ;
		else if (str == "SERIALPORT")
			type = Type::SERIALPORT;
		else if (str == "UDP" || str == "UDPSERVER")
			type = Type::UDP;
		else if (str == "SPYSERVER")
			type = Type::SPYSERVER;
		else if (str == "NMEA2000")
			type = Type::N2K;
		else
			return false;

		return true;
	}

	bool Parse::Protocol(std::string arg, PROTOCOL &protocol)
	{
		Util::Convert::toUpper(arg);
		if (arg == "NONE")
		{
			protocol = PROTOCOL::NONE;
		}
		else if (arg == "RTLTCP")
		{
			protocol = PROTOCOL::RTLTCP;
		}
		else if (arg == "GPSD")
		{
			protocol = PROTOCOL::GPSD;
		}
		else if (arg == "TXT")
		{
			protocol = PROTOCOL::TXT;
		}
		else if (arg == "MQTT")
		{
			protocol = PROTOCOL::MQTT;
		}
		else if (arg == "WS")
		{
			protocol = PROTOCOL::WS;
		}
		else if (arg == "WSMQTT")
		{
			protocol = PROTOCOL::WSMQTT;
		}
		else if (arg == "BASESTATION")
		{
			protocol = PROTOCOL::BASESTATION;
		}
		else if (arg == "BEAST")
		{
			protocol = PROTOCOL::BEAST;
		}
		else if (arg == "RAW1090")
		{
			protocol = PROTOCOL::RAW1090;
		}
		else if (arg == "TLS")
		{
			protocol = PROTOCOL::TLS;
		}
		else if (arg == "TCP")
		{
			protocol = PROTOCOL::TCP;
		}
		else if (arg == "MQTTS")
		{
			protocol = PROTOCOL::MQTTS;
		}
		else if (arg == "WSSMQTT")
		{
			protocol = PROTOCOL::WSSMQTT;
		}
		else
			return false;

		return true;
	}

	bool Parse::OutputFormat(std::string str, MessageFormat &out)
	{
		Convert::toUpper(str);
		if (str == "NONE" || str == "0")
		{
			out = MessageFormat::SILENT;
		}
		else if (str == "NMEA" || str == "1")
		{
			out = MessageFormat::NMEA;
		}
		else if (str == "NMEA_TAG" || str == "7")
		{
			out = MessageFormat::NMEA_TAG;
		}
		else if (str == "COMMUNITY_HUB")
		{
			out = MessageFormat::COMMUNITY_HUB;
		}
		else if (str == "BINARY_NMEA" || str == "NMEA_BINARY")
		{
			out = MessageFormat::BINARY_NMEA;
		}
		else if (str == "FULL" || str == "2")
		{
			out = MessageFormat::FULL;
		}
		else if (str == "JSON_NMEA" || str == "3")
		{
			out = MessageFormat::JSON_NMEA;
		}
		else if (str == "JSON_SPARSE" || str == "4")
		{
			out = MessageFormat::JSON_SPARSE;
		}
		else if (str == "JSON_FULL" || str == "5")
		{
			out = MessageFormat::JSON_FULL;
		}
		else if (str == "JSON_ANNOTATED" || str == "6")
		{
			out = MessageFormat::JSON_ANNOTATED;
		}
		else
			return false;

		return true;
	}

	std::string Parse::DeviceTypeString(Type type)
	{
		switch (type)
		{
		case Type::NONE:
			return "NONE";
		case Type::RTLSDR:
			return "RTLSDR";
		case Type::AIRSPY:
			return "AIRSPY";
		case Type::AIRSPYHF:
			return "AIRSPYHF";
		case Type::SDRPLAY:
			return "SDRPLAY";
		case Type::WAVFILE:
			return "WAVFILE";
		case Type::RAWFILE:
			return "RAWFILE";
		case Type::RTLTCP:
			return "RTLTCP";
		case Type::HACKRF:
			return "HACKRF";
		case Type::SOAPYSDR:
			return "SOAPYSDR";
		case Type::ZMQ:
			return "ZMQ";
		case Type::SERIALPORT:
			return "SERIALPORT";
		case Type::UDP:
			return "UDP";
		case Type::SPYSERVER:
			return "SPYSERVER";
		case Type::N2K:
			return "NMEA2000";
		default:
			return "";
		}
	}

#ifdef _WIN32
	std::time_t Parse::DateTime(const std::string &datetime)
	{
		std::tm tm = {};
		std::istringstream ss(datetime);
		ss >> std::get_time(&tm, "%Y/%m/%d %H:%M:%S");
		if (ss.fail())
			return 0;
		return _mkgmtime(&tm);
	}
#else
	std::time_t Parse::DateTime(const std::string &datetime)
	{
		std::tm tm = {};
		strptime(datetime.c_str(), "%Y/%m/%d %H:%M:%S", &tm);
		return timegm(&tm);
	}
#endif

	bool Parse::Switch(std::string arg, const std::string &TrueString, const std::string &FalseString)
	{
		Util::Convert::toUpper(arg);
		if (arg == FalseString || arg == "FALSE")
			return false;
		if (arg != TrueString && arg != "TRUE")
			throw std::runtime_error("unknown switch \"" + arg + "\"");

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

	bool Parse::OptionalInteger(std::string arg, int min, int max, unsigned &val)
	{
		Util::Convert::toUpper(arg);
		if (arg == "OFF" || arg == "FALSE" || arg == "NO")
		{
			val = 0;
			return false;
		}

		val = (unsigned)Integer(arg, min, max);
		return true;
	}
}
