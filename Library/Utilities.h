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

#pragma once

#include <cstring>
#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif


#include "Stream.h"
#include "Common.h"

namespace Util {

	class RealPart : public SimpleStreamInOut<CFLOAT32, FLOAT32> {
		std::vector<FLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len, TAG& tag);
	};

	class ImaginaryPart : public SimpleStreamInOut<CFLOAT32, FLOAT32> {
		std::vector<FLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len, TAG& tag);
	};

	template <typename T>
	class PassThrough : public SimpleStreamInOut<T, T> {

	public:
		virtual void Receive(const T* data, int len, TAG& tag) { SimpleStreamInOut<T, T>::Send(data, len, tag); }
		virtual void Receive(T* data, int len, TAG& tag) { SimpleStreamInOut<T, T>::Send(data, len, tag); }
	};

	template <typename T>
	class Timer : public SimpleStreamInOut<T, T> {

		high_resolution_clock::time_point time_start;
		float timing = 0.0;

		void tic() {
			time_start = high_resolution_clock::now();
		}

		void toc() {
			timing += 1e-3f * duration_cast<microseconds>(high_resolution_clock::now() - time_start).count();
		}

	public:
		virtual void Receive(const T* data, int len, TAG& tag) {
			tic();
			SimpleStreamInOut<T, T>::Send(data, len, tag);
			toc();
		}
		virtual void Receive(T* data, int len, TAG& tag) {
			tic();
			SimpleStreamInOut<T, T>::Send(data, len, tag);
			toc();
		}

		float getTotalTiming() { return timing; }
	};

	class Convert {
	public:
		static std::string toTimeStr(const std::time_t& t);
		static std::string toTimestampStr(const std::time_t& t);
		static std::string toHexString(uint64_t l);
		static std::string toString(Format format);
		static std::string toString(bool b) { return b ? std::string("ON") : std::string("OFF"); };
		static std::string toString(bool b, FLOAT32 v) { return b ? std::string("AUTO") : std::to_string(v); }

		static void toUpper(std::string& s);
		static void toFloat(CU8* in, CFLOAT32* out, int len);
		static void toFloat(CS8* in, CFLOAT32* out, int len);
		static void toFloat(CS16* in, CFLOAT32* out, int len);
	};

	class Parse {
	public:
		static long Integer(std::string str, long min = 0, long max = 0);
		static FLOAT32 Float(std::string str, FLOAT32 min = -1e30, FLOAT32 max = +1e30);
		static bool StreamFormat(std::string str, Format& format);
		static bool DeviceType(std::string str, Type& type);
		static bool Switch(std::string arg, const std::string& TrueString = "ON", const std::string& FalseString = "OFF");
		static bool AutoInteger(std::string arg, int min, int max, int& val);
		static bool AutoFloat(std::string arg, FLOAT32 min, FLOAT32 max, FLOAT32& val);
	};

	class Helper {
	public:
		static std::string readFile(const std::string& filename) {
			std::ifstream file(filename);

			if (file.fail()) throw std::runtime_error("cannot open and read file: " + filename);

			std::string str, line;
			while (std::getline(file, line)) str += line + '\n';
			return str;
		}
		static std::vector<std::string> getFilesWithExtension(const std::string& directory, const std::string& extension);

		static long getMemoryConsumption() {
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

	};

	class ConvertRAW : public SimpleStreamInOut<RAW, CFLOAT32> {
		std::vector<CFLOAT32> output;

	public:
		Connection<CU8> outCU8;
		Connection<CS8> outCS8;

		void Receive(const RAW* raw, int len, TAG& tag);
	};
}
