/*
Copyright(c) 2021 Jasper van den Eshof

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

#include <chrono>
#include <cassert>
#include <complex>

#include "DSP.h"

namespace DSP
{
	void PLLSampler::Receive(const FLOAT32* data, int len)
	{
		for (int i = 0; i < len; i++)
		{
			BIT bit = (data[i] > 0);

			if (bit != prev)
			{
				if (FastPLL)
					PLL += ((int)(MidPoint - PLL)) >> 1;
				else
					PLL += ((int)(MidPoint - PLL)) >> 4;
			}
			prev = bit;

			PLL += Increment;
			if (PLL > 0xFFFF)
			{
				sendOut(&bit, 1);
				PLL &= 0xFFFF;
			}
		}
	}

// helper macros for moving averages
#define MA1(idx)		r##idx = z; z += h##idx;
#define MA2(idx)		h##idx = z; z += r##idx;

// CIC5 downsample
	
	void Downsample2CIC5::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 2 == 0);

		if (output.size() < len / 2) output.resize(len / 2);

		CFLOAT32 z, r0, r1, r2, r3, r4;

		for (int i = 0; i < len; i += 2)
		{
			z = data[i];
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			output[i / 2] = z * (FLOAT32)0.03125f;
			z = data[i + 1];
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}

		sendOut(output.data(), len / 2);
	}
	
	// FilterCIC5

	void FilterCIC5::Receive(const CFLOAT32* data, int len)
	{
		CFLOAT32 z, r0, r1, r2, r3, r4;

		assert(len % 2 == 0);

		if (output.size() < len) output.resize(len);
		
		for (int i = 0; i < len; i += 2)
		{
			z = data[i];
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			output[i] = z * (FLOAT32)0.03125f;
			z = data[i + 1];
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
			output[i + 1] = z * (FLOAT32)0.03125f;
		}

		sendOut(output.data(), len);
	}

	void Downsample3Complex::Receive(const CFLOAT32* data, int len)
	{
		int ptr, i, j;
		assert(len % 3 == 0);

		if (output.size() < len/3) output.resize(len/3);
		
		for (j = i = 0, ptr = 21 - 1; i < 21 - 1; i += 3)
		{
			buffer[ptr++] = data[i];
			buffer[ptr++] = data[i + 1];
			buffer[ptr++] = data[i + 2];

			CFLOAT32 x = 0.33292088503f * buffer[i + 10];
			x -= 0.00101073661f * (buffer[i + 0] + buffer[i + 20]);
			x += 0.00616649466f * (buffer[i + 2] + buffer[i + 18]);
			x += 0.01130778123f * (buffer[i + 3] + buffer[i + 17]);
			x -= 0.03044260089f * (buffer[i + 5] + buffer[i + 15]);
			x -= 0.04750748661f * (buffer[i + 6] + buffer[i + 14]);
			x += 0.12579695977f * (buffer[i + 8] + buffer[i + 12]);
			x += 0.26922914593f * (buffer[i + 9] + buffer[i + 11]);

			output[j++] = x;
		}

		for (i = 1; i < len - 21 + 1; i += 3)
		{
			CFLOAT32 x = 0.33292088503f * data[i + 10];
			x -= 0.00101073661f * (data[i + 0] + data[i + 20]);
			x += 0.00616649466f * (data[i + 2] + data[i + 18]);
			x += 0.01130778123f * (data[i + 3] + data[i + 17]);
			x -= 0.03044260089f * (data[i + 5] + data[i + 15]);
			x -= 0.04750748661f * (data[i + 6] + data[i + 14]);
			x += 0.12579695977f * (data[i + 8] + data[i + 12]);
			x += 0.26922914593f * (data[i + 9] + data[i + 11]);

			output[j++] = x;
		}
		for (ptr = 0; i < len; i++)
		{
			buffer[ptr++] = data[i];
		}
		
		sendOut(output.data(), len / 3);
	}

	// Filter Generic

	void FilterComplex::Receive(const CFLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = i = 0, ptr = taps.size() - 1; i < taps.size() - 1; i++)
		{
			buffer[ptr++] = data[i];
			output[j++] = filter(&buffer[i]);
		}

		for (i = 0; i < len - taps.size() + 1; i++)
		{
			output[j++] = filter(&data[i]);
		}

		for (ptr = 0; i < len; i++)
		{
			buffer[ptr++] = data[i];
		}

		sendOut(output.data(), len);
	}

	void Filter::Receive(const FLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = i = 0, ptr = taps.size() - 1; i < taps.size() - 1; i++)
		{
			buffer[ptr++] = data[i];
			output[j++] = filter(&buffer[i]);
		}

		for (i = 0; i < len - taps.size() + 1; i++)
		{
			output[j++] = filter(&data[i]);
		}

		for (ptr = 0; i < len; i++)
		{
			buffer[ptr++] = data[i];
		}

		sendOut(output.data(), len);
	}


	void RotateDown::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 4 == 0);

		if (output.size() < len)
		{
			output.resize(len);
		}

		for (int i = 0; i < output.size(); i += 4)
		{
			output[i] = data[i];

			output[i + 1].real(data[i + 1].imag());
			output[i + 1].imag(-data[i + 1].real());

			output[i + 2].real(-data[i + 2].real());
			output[i + 2].imag(-data[i + 2].imag());

			output[i + 3].real(-data[i + 3].imag());
			output[i + 3].imag(data[i + 3].real());
		}

		sendOut(output.data(), len);
	}

	void RotateUp::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 4 == 0);

		if (output.size() < len)
		{
			output.resize(len);
		}

		for (int i = 0; i < output.size(); i += 4)
		{
			output[i] = data[i];

			output[i + 1].real(-data[i + 1].imag());
			output[i + 1].imag(data[i + 1].real());

			output[i + 2].real(-data[i + 2].real());
			output[i + 2].imag(-data[i + 2].imag());

			output[i + 3].real(data[i + 3].imag());
			output[i + 3].imag(-data[i + 3].real());
		}

		sendOut(output.data(), len);
	}

	void FMDemodulation::Receive(const CFLOAT32* data, int len)
	{
		const float PI = 3.141592653589793f;

		if (output.size() < len) output.resize(len);
		
		for (int i = 0; i < len; i++)
		{
			float re = data[i].real() * prev.real() + data[i].imag() * prev.imag();
			float im = -data[i].real() * prev.imag() + data[i].imag() * prev.real();

			output[i] = (atan2f(im, re) + DC_shift) / PI;
			prev = data[i];
		}

		sendOut(output.data(), len);
	}

}
