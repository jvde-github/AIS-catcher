/*
Copyright(c) 2021 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <cstring>
#include <cassert>
#include <vector>

#include "Stream.h"

namespace Util
{
	class RealPart : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	class ImaginaryPart : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	template <typename T>
	class PassThrough : public SimpleStreamInOut<T, T>
	{

	public:
		virtual void Receive(const T* data, int len) { SimpleStreamInOut<T, T>::sendOut(data, len); }
		virtual void Receive(T* data, int len) { SimpleStreamInOut<T, T>::sendOut(data, len); }

	};

	template <typename T>
	class Timer : public SimpleStreamInOut<T, T>
	{

		high_resolution_clock::time_point time_start;
		float timing = 0.0;

		void tic()
		{
			time_start = high_resolution_clock::now();
		}

		void toc()
		{
			timing += 1e-3f * duration_cast<microseconds>(high_resolution_clock::now() - time_start).count();
		}

	public:
		virtual void Receive(const T* data, int len) { tic();  SimpleStreamInOut<T, T>::sendOut(data, len);  toc(); }
		virtual void Receive(T* data, int len) { tic();  SimpleStreamInOut<T, T>::sendOut(data, len); toc(); }

		float getTotalTiming() { return timing; }
	};

	class Convert
	{
	public:
		static void toUpper(std::string& s)
		{
			for (auto& c : s) c = toupper(c);
		}

		static void toFloat(CU8* in, CFLOAT32* out, int len)
		{
			for (int i = 0; i < len; i++)
			{
				out[i].real((float)in[i].real() / 128.0f - 1.0f);
				out[i].imag((float)in[i].imag() / 128.0f - 1.0f);
			}
		}

		static void toFloat(CS8* in, CFLOAT32* out, int len)
		{
			for (int i = 0; i < len; i++)
			{
				out[i].real((float)in[i].real() / 128.0f);
				out[i].imag((float)in[i].imag() / 128.0f);
			}
		}

		static void toFloat(CS16* in, CFLOAT32* out, int len)
		{
			for (int i = 0; i < len; i++)
			{
				out[i].real((float)in[i].real() / 32768.0f);
				out[i].imag((float)in[i].imag() / 32768.0f);
			}
		}
	};

	class Parse
	{
	public:

		static int Integer(std::string str, int min, int max)
		{
			int number = 0;

			try
			{
				number = std::stoi(str);
			}
			catch (const std::exception& ) { throw "Error: expected a number on command line"; }

			if (number < min || number > max) throw "Error: Number out of range on command line";

			return number;
		}

		static FLOAT32 Float(std::string str, FLOAT32 min, FLOAT32 max)
		{
			FLOAT32 number = 0;

			try
			{
				number = std::stof(str);
			}
			catch (const std::exception& ) { throw "Error: expected a number on command line"; }

			if (number < min || number > max) throw "Error: Number out of range on command line";

			return number;
		}

		static bool Switch(std::string arg, const std::string& TrueString = "ON", const std::string& FalseString = "OFF")
		{
			if (arg == FalseString) return false;
			if (arg != TrueString) throw "Error on command line: unknown switch";

			return true;
		}

		static bool AutoInteger(std::string arg, int min, int max, int& val)
		{
			if (arg == "AUTO") return true;

			val = Integer(arg, min, max);
			return false;
		}

		static bool AutoFloat(std::string arg, FLOAT32 min, FLOAT32 max, FLOAT32& val)
		{
			if (arg == "AUTO") return true;

			val = Float(arg, min, max);
			return false;
		}
	};

	class ConvertRAW : public SimpleStreamInOut<RAW, CFLOAT32>
	{
		std::vector <CFLOAT32> output;

	public:

		Connection<CU8> outCU8;

		void Receive(const RAW* raw, int len)
		{
			assert(len == 1);

			// if CU8 connected, silence on CFLOAT32 output
			if (raw->format == Format::CU8 && outCU8.isConnected())
			{
				outCU8.Send((CU8*)raw->data, raw->size/2);
				return;
			}

			if (raw->format == Format::CF32)
			{
				out.Send((CFLOAT32*)raw->data, raw->size / sizeof(CFLOAT32) );
				return;
			}

			int size = 0;

			switch (raw->format)
			{
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
			out.Send(output.data(), size);

		}
	};
}
