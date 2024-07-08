/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "Utilities.h"

namespace Util {

	void RealPart::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++) {
			output[i] = data[i].real();
		}

		Send(output.data(), len, tag);
	}

	void ImaginaryPart::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++) {
			output[i] = data[i].imag();
		}

		Send(output.data(), len, tag);
	}

	long Parse::Integer(std::string str, long min, long max, const std::string& setting) {
		int number = 0;
		std::string::size_type sz;

		try {
			number = std::stoi(str, &sz);
		}
		catch (const std::exception&) {
			if (setting.empty())
				throw std::runtime_error("expected a number");
			else
				throw std::runtime_error("expected a number for setting " + setting);
		}

		if (str.length() > sz && (str[sz] == 'K' || str[sz] == 'k'))
			number *= 1000;

		if (min != 0 || max != 0)
			if (number < min || number > max) {
				if (setting.empty())
					throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "]");
				else
					throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "] for setting " + setting);
			}
		return number;
	}

	FLOAT32 Parse::Float(std::string str, FLOAT32 min, FLOAT32 max) {
		FLOAT32 number = 0;

		try {
			number = std::stof(str);
		}
		catch (const std::exception&) {
			throw std::runtime_error("expected a number as input.");
		}

		if (number < min || number > max) throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "]");

		return number;
	}

	void Parse::URL(const std::string& url, std::string& protocol, std::string& host, std::string& port, std::string& path) {

		int idx = url.find("://");
		if (idx != std::string::npos) {

			protocol = url.substr(0, idx);
			path = "/";

			idx += 3;
			int hostEnd = url.find(':', idx);
			if (hostEnd == std::string::npos) {
				hostEnd = url.find('/', idx);
			}

			if (hostEnd == std::string::npos) {
				host = url.substr(idx, url.length() - idx);
			}
			else {
				host = url.substr(idx, hostEnd - idx);

				int portStart = url.find(':', hostEnd);
				if (portStart != std::string::npos) {
					int portEnd = url.find('/', portStart);
					port = url.substr(portStart + 1, (portEnd != std::string::npos ? portEnd : url.length()) - portStart - 1);
				}

				int pathStart = url.find('/', hostEnd);
				if (pathStart != std::string::npos) {
					path = url.substr(pathStart);
				}
			}
		}
	}

	bool Parse::StreamFormat(std::string str, Format& format) {
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
		else
			return false;

		return true;
	}

	bool Parse::DeviceType(std::string str, Type& type) {
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
		else
			return false;

		return true;
	}
	std::string Parse::DeviceTypeString(Type type) {
		switch (type) {
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
		default:
			return "";
		}
	}

	bool Parse::Switch(std::string arg, const std::string& TrueString, const std::string& FalseString) {
		Util::Convert::toUpper(arg);
		if (arg == FalseString || arg == "FALSE") return false;
		if (arg != TrueString && arg != "TRUE") throw std::runtime_error("unknown switch \"" + arg + "\"");

		return true;
	}

	bool Parse::AutoInteger(std::string arg, int min, int max, int& val) {
		Util::Convert::toUpper(arg);
		if (arg == "AUTO") return true;

		val = Integer(arg, min, max);
		return false;
	}

	bool Parse::AutoFloat(std::string arg, FLOAT32 min, FLOAT32 max, FLOAT32& val) {
		Util::Convert::toUpper(arg);
		if (arg == "AUTO") return true;

		val = Float(arg, min, max);
		return false;
	}

	std::string Convert::toTimeStr(const std::time_t& t) {
		std::tm* now_tm = std::gmtime(&t);
		char str[16];
		std::strftime((char*)str, 16, "%Y%m%d%H%M%S", now_tm);
		return std::string(str);
	}

	std::string Convert::toTimestampStr(const std::time_t& t) {
		std::tm* now_tm = std::gmtime(&t);
		char str[22];
		std::strftime((char*)str, 22, "%Y/%m/%d %H:%M:%S", now_tm);
		return std::string(str);
	}

	std::string Convert::toHexString(uint64_t l) {
		std::stringstream s;
		s << std::uppercase << std::hex << std::setfill('0') << std::setw(16) << l;
		return s.str();
	}

	std::string Convert::toString(Format format) {
		switch (format) {
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
		default:
			break;
		}
		return "UNKNOWN";
	}

	void Convert::toUpper(std::string& s) {
		for (auto& c : s) c = toupper(c);
	}

	// not using the complex class functions to be independent of internal representation
	void Convert::toFloat(CU8* in, CFLOAT32* out, int len) {
		uint8_t* data = (uint8_t*)in;

		for (int i = 0; i < len; i++) {
			out[i].real(((int)data[2 * i] - 128) / 128.0f);
			out[i].imag(((int)data[2 * i + 1] - 128) / 128.0f);
		}
	}

	void Convert::toFloat(CS8* in, CFLOAT32* out, int len) {
		int8_t* data = (int8_t*)in;

		for (int i = 0; i < len; i++) {
			out[i].real(data[2 * i] / 128.0f);
			out[i].imag(data[2 * i + 1] / 128.0f);
		}
	}

	void Convert::toFloat(CS16* in, CFLOAT32* out, int len) {
		int16_t* data = (int16_t*)in;

		for (int i = 0; i < len; i++) {
			out[i].real(data[2 * i] / 32768.0f);
			out[i].imag(data[2 * i + 1] / 32768.0f);
		}
	}

	void ConvertRAW::Receive(const RAW* raw, int len, TAG& tag) {
		assert(len == 1);

		// if CU8 connected, silence on CFLOAT32 output
		if (raw->format == Format::CU8 && outCU8.isConnected()) {
			outCU8.Send((CU8*)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CS8 && outCS8.isConnected()) {
			outCS8.Send((CS8*)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CF32 && out.isConnected()) {
			out.Send((CFLOAT32*)raw->data, raw->size / sizeof(CFLOAT32), tag);
			return;
		}

		if (!out.isConnected()) return;

		int size = 0;

		switch (raw->format) {
		case Format::CU8:

			size = raw->size / sizeof(CU8);
			if (output.size() < size) output.resize(size);
			Util::Convert::toFloat((CU8*)raw->data, output.data(), size);
			break;

		case Format::CS16:

			size = raw->size / sizeof(CS16);
			if (output.size() < size) output.resize(size);
			Util::Convert::toFloat((CS16*)raw->data, output.data(), size);
			break;

		case Format::CS8:

			size = raw->size / sizeof(CS8);
			if (output.size() < size) output.resize(size);
			Util::Convert::toFloat((CS8*)raw->data, output.data(), size);
			break;

		default:
			return;
		}

		out.Send(output.data(), size, tag);
	}

	void Serialize::Uint8(uint8_t i, std::vector<char>& v) {
		v.push_back(i);
	}

	void Serialize::Uint16(uint16_t i, std::vector<char>& v) {
		v.push_back((char)(i >> 8));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint32(uint32_t i, std::vector<char>& v) {
		v.push_back((char)(i >> 24));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Uint64(uint64_t i, std::vector<char>& v) {
		v.push_back((char)(i >> 56));
		v.push_back((char)((i >> 48) & 0xFF));
		v.push_back((char)((i >> 40) & 0xFF));
		v.push_back((char)((i >> 32) & 0xFF));
		v.push_back((char)((i >> 24) & 0xFF));
		v.push_back((char)((i >> 16) & 0xFF));
		v.push_back((char)((i >> 8) & 0xFF));
		v.push_back((char)(i & 0xFF));
	}

	void Serialize::Int8(int8_t i, std::vector<char>& v) {
		v.push_back(i);
	}

	void Serialize::Int16(int16_t i, std::vector<char>& v) {
		Uint16((uint16_t)i, v);
	}

	void Serialize::Int32(int32_t i, std::vector<char>& v) {
		Uint32((uint32_t)i, v);
	}

	void Serialize::Int64(int64_t i, std::vector<char>& v) {
		Uint64((uint64_t)i, v);
	}

	void Serialize::String(const std::string& s, std::vector<char>& v) {
		Uint8((uint8_t)s.length(), v);
		v.insert(v.end(), s.begin(), s.end());
	}

	void Serialize::Float(FLOAT32 f, std::vector<char>& v) {
		Int16(f * 1000.0f, v);
	}

	void Serialize::FloatLow(FLOAT32 f, std::vector<char>& v) {
		Int16(f * 10.0f, v);
	}

	void Serialize::LatLon(FLOAT32 lat, FLOAT32 lon, std::vector<char>& v) {

		if (!(lat == 0 && lon == 0) && lat != 91 && lon != 181) {
			Util::Serialize::Int32(lat * 6000000, v);
			Util::Serialize::Int32(lon * 6000000, v);
		}
		else {
			Util::Serialize::Int32(91 * 6000000, v);
			Util::Serialize::Int32(181 * 6000000, v);
		}
	}

	std::string Helper::readFile(const std::string& filename) {
		std::ifstream file(filename);

		if (file.fail()) throw std::runtime_error("cannot read file \"" + filename + "\"");

		std::string str, line;
		while (std::getline(file, line)) str += line + '\n';
		return str;
	}

	int Helper::lsb(uint64_t x) {
		for (int i = 0; i < 64; i++) {
			if (x & (1ULL << i)) return i;
		}
		return -1;
	}

	long Helper::getMemoryConsumption() {
		int memory = 0;
#ifdef _WIN32
		HANDLE hProcess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
			memory = pmc.WorkingSetSize;
		}
#else
		std::ifstream statm("/proc/self/statm");
		if (statm.is_open()) {
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

	std::string Helper::getOS() {
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
		while (std::getline(inFile, line)) {
			if (line.substr(0, 11) == "PRETTY_NAME") {
				std::size_t start = line.find('"') + 1;
				std::size_t end = line.rfind('"');
				if (start != std::string::npos && end != std::string::npos) {
					os = line.substr(start, end - start);
					break;
				}
			}
		}

		if (os.empty()) {
			os = "Linux";
		}
		return os;
#else
		return "";
#endif
	}


	std::string Helper::getHardware() {
#ifdef _WIN32
		return "";
#elif ANDROID
		return "";
#elif __APPLE__
		return "";
#elif __linux__
		std::string line, model_name, revision;

		{
			std::ifstream inFile("/proc/device-tree/model");

			if (inFile.is_open() && std::getline(inFile, line))
				return line;
		}

		// For systems without a device-tree, approximation for the RPI
		{
			std::ifstream inFile("/proc/cpuinfo");
			if (inFile.is_open()) {
				while (getline(inFile, line)) {
					if (line.substr(0, 10) == "model name") {
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos) {
							model_name = line.substr(pos + 2);
						}
					}
					else if (line.substr(0, 8) == "Revision") {
						std::size_t pos = line.find(": ");
						if (pos != std::string::npos) {
							revision = line.substr(pos + 2);
						}
					}
				}
			}
		}

		if (revision == "900021") return "Raspberry Pi A+ 1.1";
		if (revision == "900032") return "Raspberry Pi B+ 1.2";
		if (revision == "900092") return "Raspberry Pi Zero 1.2";
		if (revision == "900093") return "Raspberry Pi Zero 1.3";
		if (revision == "9000c1") return "Raspberry Pi Zero W 1.1";
		if (revision == "9020e0") return "Raspberry Pi 3A+ 1.0";
		if (revision == "920092") return "Raspberry Pi Zero 1.2";
		if (revision == "920093") return "Raspberry Pi Zero 1.3";
		if (revision == "900061") return "Raspberry Pi CM1 1.1";
		if (revision == "a01040") return "Raspberry Pi 2B 1.0";
		if (revision == "a01041") return "Raspberry Pi 2B 1.1";
		if (revision == "a02082") return "Raspberry Pi 3B 1.2";
		if (revision == "a020a0") return "Raspberry Pi CM3 1.0";
		if (revision == "a020d3") return "Raspberry Pi 3B+ 1.3";
		if (revision == "a02042") return "Raspberry Pi 2B (with BCM2837) 1.2";
		if (revision == "a21041") return "Raspberry Pi 2B 1.1";
		if (revision == "a22042") return "Raspberry Pi 2B (with BCM2837) 1.2";
		if (revision == "a22082") return "Raspberry Pi 3B 1.2";
		if (revision == "a220a0") return "Raspberry Pi CM3 1.0";
		if (revision == "a32082") return "Raspberry Pi 3B 1.2";
		if (revision == "a52082") return "Raspberry Pi 3B 1.2";
		if (revision == "a22083") return "Raspberry Pi 3B 1.3";
		if (revision == "a02100") return "Raspberry Pi CM3+ 1.0";
		if (revision == "a03111") return "Raspberry Pi 4B 1.1";
		if (revision == "b03111") return "Raspberry Pi 4B 1.1";
		if (revision == "b03112") return "Raspberry Pi 4B 1.2";
		if (revision == "b03114") return "Raspberry Pi 4B 1.4";
		if (revision == "b03115") return "Raspberry Pi 4B 1.5";
		if (revision == "c03111") return "Raspberry Pi 4B 1.1";
		if (revision == "c03112") return "Raspberry Pi 4B 1.2";
		if (revision == "c03114") return "Raspberry Pi 4B 1.4";
		if (revision == "c03115") return "Raspberry Pi 4B 1.5";
		if (revision == "d03114") return "Raspberry Pi 4B 1.4";
		if (revision == "d03115") return "Raspberry Pi 4B 1.5";
		if (revision == "c03130") return "Raspberry Pi Pi 400 1.0";
		if (revision == "a03140") return "Raspberry Pi CM4 1.0";
		if (revision == "b03140") return "Raspberry Pi CM4 1.0";
		if (revision == "c03140") return "Raspberry Pi CM4 1.0";
		if (revision == "d03140") return "Raspberry Pi CM4 1.0";
		if (revision == "902120") return "Raspberry Pi Zero 2 W 1.0";
		if (revision == "c04170") return "Raspberry Pi 5";
		if (revision == "d04170") return "Raspberry Pi 5";

		return model_name;
#endif
		return "";
	}

	std::vector<std::string> Helper::getFilesWithExtension(const std::string& directory, const std::string& extension) {
		std::vector<std::string> files;

#ifdef _WIN32
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		const std::string search_path = directory + "\\*" + extension;
		hFind = FindFirstFileA((LPCSTR)search_path.c_str(), &ffd);
		if (hFind == INVALID_HANDLE_VALUE) return files;

		do {
			std::string full_path = directory + "\\" + std::string((char*)ffd.cFileName);
			files.push_back(full_path);
		} while (FindNextFileA(hFind, &ffd) != 0);
		FindClose(hFind);

#else
		DIR* dir;
		struct dirent* ent;
		if ((dir = opendir(directory.c_str())) != nullptr) {
			while ((ent = readdir(dir)) != nullptr) {
				std::string file_name = ent->d_name;
				if (file_name.length() >= extension.length() &&
					file_name.compare(file_name.length() - extension.length(), extension.length(), extension) == 0) {
					std::string full_path = directory + "/" + file_name;
					files.push_back(full_path);
				}
			}
			closedir(dir);
		}
#endif
		return files;
	}
}
