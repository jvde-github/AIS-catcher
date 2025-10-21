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

#include <iomanip>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "Utilities.h"
#include "Message.h"
#include "Logger.h"
#include <unordered_map>

namespace Util
{

	void RealPart::Receive(const CFLOAT32 *data, int len, TAG &tag)
	{
		if (output.size() < len)
			output.resize(len);

		for (int i = 0; i < len; i++)
		{
			output[i] = data[i].real();
		}

		Send(output.data(), len, tag);
	}

	void ImaginaryPart::Receive(const CFLOAT32 *data, int len, TAG &tag)
	{
		if (output.size() < len)
			output.resize(len);

		for (int i = 0; i < len; i++)
		{
			output[i] = data[i].imag();
		}

		Send(output.data(), len, tag);
	}

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
		else if (str == "NMEA_TAG")
		{
			out = MessageFormat::NMEA_TAG;
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

	std::string Convert::toTimeStr(const std::time_t &t)
	{
		std::tm *now_tm = std::gmtime(&t);
		char str[16];
		std::strftime((char *)str, 16, "%Y%m%d%H%M%S", now_tm);
		return std::string(str);
	}

	std::string Convert::toTimestampStr(const std::time_t &t)
	{
		std::tm *now_tm = std::gmtime(&t);
		char str[22];
		std::strftime((char *)str, 22, "%Y/%m/%d %H:%M:%S", now_tm);
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
		case MessageFormat::FULL:
			return "FULL";
		case MessageFormat::JSON_NMEA:
			return "JSON_NMEA";
		case MessageFormat::JSON_SPARSE:
			return "JSON_SPARSE";
		case MessageFormat::JSON_FULL:
			return "JSON_FULL";
		default:
			break;
		}
		return "";
	}

	std::string Convert::BASE64toString(const std::string &in)
	{
		const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string out;
		int val = 0, valb = -6;
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

	void ConvertRAW::Receive(const RAW *raw, int len, TAG &tag)
	{
		assert(len == 1);

		// if CU8 connected, silence on CFLOAT32 output
		if (raw->format == Format::CU8 && outCU8.isConnected())
		{
			outCU8.Send((CU8 *)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CS8 && outCS8.isConnected())
		{
			outCS8.Send((CS8 *)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CF32 && out.isConnected())
		{
			out.Send((CFLOAT32 *)raw->data, raw->size / sizeof(CFLOAT32), tag);
			return;
		}

		if (!out.isConnected())
			return;

		int size = 0;

		switch (raw->format)
		{
		case Format::CU8:

			size = raw->size / sizeof(CU8);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CU8 *)raw->data, output.data(), size);
			break;

		case Format::CS16:

			size = raw->size / sizeof(CS16);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CS16 *)raw->data, output.data(), size);
			break;

		case Format::CS8:

			size = raw->size / sizeof(CS8);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CS8 *)raw->data, output.data(), size);
			break;

		case Format::F32_FS4:
			size = raw->size / sizeof(FLOAT32);
			if (output.size() < size)
				output.resize(size);

			for (int i = 0; i < size; i += 4)
			{
				output[i] = CFLOAT32(((FLOAT32 *)raw->data)[i], 0.0f);
				output[i + 1] = CFLOAT32(0.0f, ((FLOAT32 *)raw->data)[i + 1]);
				output[i + 2] = CFLOAT32(-((FLOAT32 *)raw->data)[i + 2], 0.0f);
				output[i + 3] = CFLOAT32(0.0f, -((FLOAT32 *)raw->data)[i + 3]);
			}
			break;
		default:
			return;
		}

		out.Send(output.data(), size, tag);
	}

	void WriteWAV::Open(const std::string &filename, int sample_rate)
	{
		if (stopping)
			return;

		try
		{
			file.open(filename, std::ios::binary);
			if (!file.is_open())
			{
				throw std::runtime_error("Cannot open WAV file for writing: \"" + filename + "\"");
			}

			header.sample_rate_val = sample_rate;

			switch (format)
			{
			case Format::CU8:
			case Format::CS8:
				header.bit_depth = 8;
				break;
			case Format::CS16:
				header.bit_depth = 16;
				break;
			case Format::CF32:
				header.bit_depth = 32;
				header.audio_format = 3;
				break;
			default:
				throw std::runtime_error("Unsupported format for WAV file writing");
			}

			header.byte_rate = sample_rate * 2 * (header.bit_depth / 8);
			header.sample_alignment = 2 * (header.bit_depth / 8);

			// Write the header
			file.write(reinterpret_cast<const char *>(&header), sizeof(header));
		}
		catch (const std::exception &e)
		{
			Error() << "WAV out: " << e.what() << std::endl;
			stopping = true;
			StopRequest();
		}
	}

	WriteWAV::~WriteWAV()
	{
		if (file.is_open())
		{
			// Get current position
			std::streampos pos = file.tellp();
			std::streamoff data_size = pos - std::streamoff(sizeof(WAVHeader));
			uint32_t wav_size = data_size + 36; // Total size - 8

			// Update RIFF chunk size
			file.seekp(4);
			file.write((const char *)(&wav_size), 4);

			// Update data chunk size
			file.seekp(40);
			uint32_t data_chunk_size = (uint32_t)(data_size);
			file.write((char *)(&data_chunk_size), 4);

			file.close();
		}
	}

	void WriteWAV::Receive(const RAW *raw, int len, TAG &tag)
	{
		if (stopping)
			return;

		format = raw->format;

		if (!file.is_open())
		{
			Open(filename, sample_rate);
		}

		if (!file.is_open() || stopping)
			return;

		// Write the data directly
		try
		{
			file.write(reinterpret_cast<const char *>(raw->data), raw->size);
		}
		catch (const std::exception &e)
		{
			Error() << "WAV out: " << e.what() << std::endl;
			stopping = true;
			StopRequest();
		}
	}

	bool WriteWAV::setValue(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			filename = arg;
			return true;
		}
		else if (option == "RATE")
		{
			sample_rate = Parse::Integer(arg, 0, 1000000000, "RATE");
			return true;
		}
		return false;
	}

	void Serialize::Uint8(uint8_t i, std::vector<char> &v)
	{
		v.push_back(i);
	}

	void Serialize::Uint16(uint16_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 8));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint32(uint32_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 24));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint64(uint64_t i, std::vector<char> &v)
	{
		v.push_back((char)(i >> 56));
		v.push_back((char)((i >> 48) & 0xFF));
		v.push_back((char)((i >> 40) & 0xFF));
		v.push_back((char)((i >> 32) & 0xFF));
		v.push_back((char)((i >> 24) & 0xFF));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Int8(int8_t i, std::vector<char> &v)
	{
		v.push_back(i);
	}

	void Serialize::Int16(int16_t i, std::vector<char> &v)
	{
		Uint16((uint16_t)i, v);
	}

	void Serialize::Int32(int32_t i, std::vector<char> &v)
	{
		Uint32((uint32_t)i, v);
	}

	void Serialize::Int64(int64_t i, std::vector<char> &v)
	{
		Uint64((uint64_t)i, v);
	}

	void Serialize::String(const std::string &s, std::vector<char> &v)
	{
		Uint8((uint8_t)s.length(), v);
		v.insert(v.end(), s.begin(), s.end());
	}

	void Serialize::Float(FLOAT32 f, std::vector<char> &v)
	{
		Int16(f * 1000.0f, v);
	}

	void Serialize::FloatLow(FLOAT32 f, std::vector<char> &v)
	{
		Int16(f * 10.0f, v);
	}

	void Serialize::LatLon(FLOAT32 lat, FLOAT32 lon, std::vector<char> &v)
	{

		if (!(lat == 0 && lon == 0) && lat != 91 && lon != 181)
		{
			Util::Serialize::Int32(lat * 6000000, v);
			Util::Serialize::Int32(lon * 6000000, v);
		}
		else
		{
			Util::Serialize::Int32(91 * 6000000, v);
			Util::Serialize::Int32(181 * 6000000, v);
		}
	}

	std::string Helper::readFile(const std::string &filename)
	{
		std::ifstream file(filename);

		if (file.fail())
			throw std::runtime_error("cannot read file \"" + filename + "\"");

		std::string str, line;
		while (std::getline(file, line))
			str += line + '\n';
		return str;
	}

	int Helper::lsb(uint64_t x)
	{
		for (int i = 0; i < 64; i++)
		{
			if (x & (1ULL << i))
				return i;
		}
		return -1;
	}

	long Helper::getMemoryConsumption()
	{
		int memory = 0;
#ifdef _WIN32
		HANDLE hProcess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
		{
			memory = pmc.WorkingSetSize;
		}
#else
		std::ifstream statm("/proc/self/statm");
		if (statm.is_open())
		{
			std::string line;
			std::getline(statm, line);
			std::stringstream ss(line);
			long size, resident, shared, text, lib, data, dt;
			ss >> size >> resident >> shared >> text >> lib >> data >> dt;
			memory = resident * sysconf(_SC_PAGESIZE);
		}
#endif
		return memory;
	}

	std::string Helper::getOS()
	{
#ifdef _WIN32
		return "Windows";
#elif __ANDROID__
		return "Android";
#elif __APPLE__
		return "MacOS";
#elif __linux__
		std::string os;

		std::ifstream inFile("/etc/os-release");
		std::string line;
		while (std::getline(inFile, line))
		{
			if (line.substr(0, 11) == "PRETTY_NAME")
			{
				std::size_t start = line.find('"') + 1;
				std::size_t end = line.rfind('"');
				if (start != std::string::npos && end != std::string::npos)
				{
					os = line.substr(start, end - start);
					break;
				}
			}
		}

		if (os.empty())
		{
			os = "Linux";
		}
		return os;
#else
		return "";
#endif
	}

	std::string Helper::getHardware()
	{
#ifdef _WIN32
	
		return "Windows PC";

#elif __ANDROID__

		return "Android Device";

#elif __APPLE__

		return "Mac";

#elif __linux__
		std::string line, model_name, revision;

		// Try device-tree first (works for Raspberry Pi and other ARM boards)
		{
			std::ifstream inFile("/proc/device-tree/model");
			if (inFile.is_open() && std::getline(inFile, line))
			{
				// Remove null terminator if present
				if (!line.empty() && line[line.length() - 1] == '\0')
					line.resize(line.length() - 1);
				return line;
			}
		}

		// Try DMI information (works for x86/x64 systems and some ARM systems)
		{
			std::ifstream dmiProduct("/sys/class/dmi/id/product_name");
			if (dmiProduct.is_open())
			{
				std::string product;
				if (std::getline(dmiProduct, product) && !product.empty() &&
					product != "To be filled by O.E.M." &&
					product != "System Product Name")
				{
					std::ifstream dmiVendor("/sys/class/dmi/id/sys_vendor");
					if (dmiVendor.is_open())
					{
						std::string vendor;
						if (std::getline(dmiVendor, vendor) && !vendor.empty() &&
							vendor != "To be filled by O.E.M." &&
							vendor != "System manufacturer")
						{
							return vendor + " " + product;
						}
					}
					return product;
				}
			}
		}

		// Parse cpuinfo for Raspberry Pi revision codes and CPU model
		{
			std::ifstream inFile("/proc/cpuinfo");
			if (inFile.is_open())
			{
				while (std::getline(inFile, line))
				{
					if (line.substr(0, 10) == "model name")
					{
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos)
						{
							model_name = line.substr(pos + 2);
						}
					}
					else if (line.substr(0, 8) == "Revision")
					{
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos)
						{
							revision = line.substr(pos + 2);
						}
					}
				}
			}
		}

		// Raspberry Pi revision lookup table
		if (!revision.empty())
		{
			static const std::unordered_map<std::string, std::string> rpi_revisions = {
				{"900021", "Raspberry Pi A+ 1.1"},
				{"900032", "Raspberry Pi B+ 1.2"},
				{"900092", "Raspberry Pi Zero 1.2"},
				{"900093", "Raspberry Pi Zero 1.3"},
				{"9000c1", "Raspberry Pi Zero W 1.1"},
				{"9020e0", "Raspberry Pi 3A+ 1.0"},
				{"920092", "Raspberry Pi Zero 1.2"},
				{"920093", "Raspberry Pi Zero 1.3"},
				{"900061", "Raspberry Pi CM1 1.1"},
				{"a01040", "Raspberry Pi 2B 1.0"},
				{"a01041", "Raspberry Pi 2B 1.1"},
				{"a02082", "Raspberry Pi 3B 1.2"},
				{"a020a0", "Raspberry Pi CM3 1.0"},
				{"a020d3", "Raspberry Pi 3B+ 1.3"},
				{"a02042", "Raspberry Pi 2B (with BCM2837) 1.2"},
				{"a21041", "Raspberry Pi 2B 1.1"},
				{"a22042", "Raspberry Pi 2B (with BCM2837) 1.2"},
				{"a22082", "Raspberry Pi 3B 1.2"},
				{"a220a0", "Raspberry Pi CM3 1.0"},
				{"a32082", "Raspberry Pi 3B 1.2"},
				{"a52082", "Raspberry Pi 3B 1.2"},
				{"a22083", "Raspberry Pi 3B 1.3"},
				{"a02100", "Raspberry Pi CM3+ 1.0"},
				{"a03111", "Raspberry Pi 4B 1.1"},
				{"b03111", "Raspberry Pi 4B 1.1"},
				{"b03112", "Raspberry Pi 4B 1.2"},
				{"b03114", "Raspberry Pi 4B 1.4"},
				{"b03115", "Raspberry Pi 4B 1.5"},
				{"c03111", "Raspberry Pi 4B 1.1"},
				{"c03112", "Raspberry Pi 4B 1.2"},
				{"c03114", "Raspberry Pi 4B 1.4"},
				{"c03115", "Raspberry Pi 4B 1.5"},
				{"d03114", "Raspberry Pi 4B 1.4"},
				{"d03115", "Raspberry Pi 4B 1.5"},
				{"c03130", "Raspberry Pi 400 1.0"},
				{"a03140", "Raspberry Pi CM4 1.0"},
				{"b03140", "Raspberry Pi CM4 1.0"},
				{"c03140", "Raspberry Pi CM4 1.0"},
				{"d03140", "Raspberry Pi CM4 1.0"},
				{"902120", "Raspberry Pi Zero 2 W 1.0"},
				{"c04170", "Raspberry Pi 5 8GB"},
				{"d04170", "Raspberry Pi 5 4GB"},
				{"c04171", "Raspberry Pi 5 8GB"},
				{"d04171", "Raspberry Pi 5 4GB"},
				{"902121", "Raspberry Pi Zero 2 W 1.0"}};

			std::unordered_map<std::string, std::string>::const_iterator it = rpi_revisions.find(revision);
			if (it != rpi_revisions.end())
			{
				return it->second;
			}
		}

		// Return CPU model name if available
		if (!model_name.empty())
			return model_name;

		return "Linux System";
#endif

		return "";
	}

	std::vector<std::string> Helper::getFilesWithExtension(const std::string &directory, const std::string &extension)
	{
		std::vector<std::string> files;

#ifdef _WIN32
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		const std::string search_path = directory + "\\*" + extension;
		hFind = FindFirstFileA((LPCSTR)search_path.c_str(), &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			std::string full_path = directory + "\\" + std::string((char *)ffd.cFileName);
			files.push_back(full_path);
		} while (FindNextFileA(hFind, &ffd) != 0);
		FindClose(hFind);

#else
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(directory.c_str())) != nullptr)
		{
			while ((ent = readdir(dir)) != nullptr)
			{
				std::string file_name = ent->d_name;
				if (file_name.length() >= extension.length() &&
					file_name.compare(file_name.length() - extension.length(), extension.length(), extension) == 0)
				{
					std::string full_path = directory + "/" + file_name;
					files.push_back(full_path);
				}
			}
			closedir(dir);
		}
#endif
		return files;
	}

	void TemplateString::set(const std::string &t)
	{
		tpl = t;
	}

	std::string TemplateString::get(const TAG &tag, const AIS::Message &msg) const
	{
		std::string out;
		out.reserve(tpl.size() + 64);

		int i = 0, n = tpl.size();

		while (i < n)
		{
			if (tpl[i] != '%')
			{
				out.push_back(tpl[i++]);
			}
			else
			{
				int start = i + 1;
				int end = tpl.find('%', start);
				if (end == std::string::npos)
				{
					out.push_back('%');
					i = start;
				}
				else
				{
					std::string key = tpl.substr(start, end - start);
					i = end + 1;

					// substitutions:
					if (key == "mmsi")
					{
						out += std::to_string(msg.mmsi());
					}
					else if (key == "ppm")
					{
						out += std::to_string(tag.ppm);
					}
					else if (key == "station")
					{
						out += std::to_string(msg.getStation());
					}
					else if (key == "type")
					{
						out += std::to_string(msg.type());
					}
					else if (key == "repeat")
					{
						out += std::to_string(msg.repeat());
					}
					else if (key == "channel")
					{
						out.push_back(msg.getChannel());
					}
					else if (key == "rxtimeux")
					{
						out += std::to_string(msg.getRxTimeUnix());
					}
					else
					{
						// unknown key → re-emit literally
						out.push_back('%');
						out += key;
						out.push_back('%');
					}
				}
			}
		}
		return out;
	}
}
