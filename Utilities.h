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
}
