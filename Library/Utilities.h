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
		static std::string toTimeStr(const std::time_t& t);
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
		static int Integer(std::string str, int min = (-1 << 31 - 1), int max = (1 << 31 - 1));

		static FLOAT32 Float(std::string str, FLOAT32 min = -1e6, FLOAT32 max = +1e6);
		static bool StreamFormat(std::string str, Format& format);
		static bool DeviceType(std::string str, Type& type);
		static bool Switch(std::string arg, const std::string& TrueString = "ON", const std::string& FalseString = "OFF");
		static bool AutoInteger(std::string arg, int min, int max, int& val);
		static bool AutoFloat(std::string arg, FLOAT32 min, FLOAT32 max, FLOAT32& val);
	};

	class ConvertRAW : public SimpleStreamInOut<RAW, CFLOAT32> {
		std::vector<CFLOAT32> output;
		bool error = false;

	public:
		Connection<CU8> outCU8;
		Connection<CS8> outCS8;

		void Receive(const RAW* raw, int len, TAG& tag);
	};
}
