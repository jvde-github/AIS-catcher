/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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
		static std::string toTimeStr(const std::time_t& t) {
			std::tm* now_tm = std::gmtime(&t);
			char str[16];
			std::strftime((char*)str, 16, "%Y%m%d%H%M%S", now_tm);
			return std::string(str);
		}

		static void toUpper(std::string& s) {
			for (auto& c : s) c = toupper(c);
		}

		// not using the complex class functions to be independent of internal representation
		static void toFloat(CU8* in, CFLOAT32* out, int len) {
			uint8_t* data = (uint8_t*)in;

			for (int i = 0; i < len; i++) {
				out[i].real(((int)data[2 * i] - 128) / 128.0f);
				out[i].imag(((int)data[2 * i + 1] - 128) / 128.0f);
			}
		}

		static void toFloat(CS8* in, CFLOAT32* out, int len) {
			int8_t* data = (int8_t*)in;

			for (int i = 0; i < len; i++) {
				out[i].real(data[2 * i] / 128.0f);
				out[i].imag(data[2 * i + 1] / 128.0f);
			}
		}

		static void toFloat(CS16* in, CFLOAT32* out, int len) {
			int16_t* data = (int16_t*)in;

			for (int i = 0; i < len; i++) {
				out[i].real(data[2 * i] / 32768.0f);
				out[i].imag(data[2 * i + 1] / 32768.0f);
			}
		}
	};

	class Parse {
	public:
		static int Integer(std::string str, int min, int max) {
			int number = 0;
			std::string::size_type sz;

			try {
				number = std::stoi(str, &sz);
			}
			catch (const std::exception&) {
				throw "Error: expected a number on command line";
			}

			if (str.length() > sz && (str[sz] == 'K' || str[sz] == 'k'))
				number *= 1000;

			if (number < min || number > max) throw "Error: input parameter out of range.";

			return number;
		}

		static FLOAT32 Float(std::string str, FLOAT32 min = -1e6, FLOAT32 max = +1e6) {
			FLOAT32 number = 0;

			try {
				number = std::stof(str);
			}
			catch (const std::exception&) {
				throw "Error: expected a number as input.";
			}

			if (number < min || number > max) throw "Error: input parameter out of range.";

			return number;
		}

		static bool StreamFormat(std::string str, Format& format) {
			if (str == "CU8")
				format = Format::CU8;
			else if (str == "CF32")
				format = Format::CF32;
			else if (str == "CS16")
				format = Format::CS16;
			else if (str == "CS8")
				format = Format::CS8;
			else
				return false;

			return true;
		}

		static bool Switch(std::string arg, const std::string& TrueString = "ON", const std::string& FalseString = "OFF") {
			if (arg == FalseString) return false;
			if (arg != TrueString) throw "Error on input: unknown switch";

			return true;
		}

		static bool AutoInteger(std::string arg, int min, int max, int& val) {
			if (arg == "AUTO") return true;

			val = Integer(arg, min, max);
			return false;
		}

		static bool AutoFloat(std::string arg, FLOAT32 min, FLOAT32 max, FLOAT32& val) {
			if (arg == "AUTO") return true;

			val = Float(arg, min, max);
			return false;
		}
	};

	class ConvertRAW : public SimpleStreamInOut<RAW, CFLOAT32> {
		std::vector<CFLOAT32> output;

	public:
		Connection<CU8> outCU8;
		Connection<CS8> outCS8;

		void Receive(const RAW* raw, int len, TAG& tag) {
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
				throw "Internal error: unexpected format";
			}
			out.Send(output.data(), size, tag);
		}
	};
}
