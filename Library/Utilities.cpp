/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

	long Parse::Integer(std::string str, long min, long max) {
		int number = 0;
		std::string::size_type sz;

		try {
			number = std::stoi(str, &sz);
		}
		catch (const std::exception&) {
			throw std::runtime_error("expected a number");
		}

		if (str.length() > sz && (str[sz] == 'K' || str[sz] == 'k'))
			number *= 1000;

		if (min != 0 || max != 0)
			if (number < min || number > max) throw std::runtime_error("input " + std::to_string(number) + " out of range [" + std::to_string(min) + "," + std::to_string(max) + "]");

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
		else if (str == "UDP" || str == "UDPSERVER")
			type = Type::UDP;
		else if (str == "SPYSERVER")
			type = Type::SPYSERVER;
		else
			return false;

		return true;
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
		if ((dir = opendir(directory.c_str())) != NULL) {
			while ((ent = readdir(dir)) != NULL) {
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
