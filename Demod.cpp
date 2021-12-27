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

#include <chrono>
#include <cassert>
#include <complex>

#include "Demod.h"
#include "DSP.h"

namespace DSP
{

	void FMDemodulation::Receive(const CFLOAT32* data, int len)
	{
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++)
		{
			auto p = data[i] * std::conj(prev);
			output[i] = atan2f(p.imag(), p.real()) / PI;
			prev = data[i];
		}

		sendOut(output.data(), len);
	}

	void CoherentDemodulation::Receive(const CFLOAT32* data, int len)
	{
		for (int i = 0; i < len; i++)
		{
			FLOAT32 re, im;

			//  multiply samples with (1j) ** i, to get all points on the same line
			switch (rot)
			{
			case 0: re = data[i].real(); im = data[i].imag(); break;
			case 1: im = data[i].real(); re = -data[i].imag(); break;
			case 2: re = -data[i].real(); im = -data[i].imag(); break;
			case 3: im = -data[i].real(); re = data[i].imag(); break;
			}

			rot = (rot + 1) & 3;

			// Determining the phase is approached as a linear classification problem.
			for (int j = 0; j < nPhases / 2; j++)
			{
				FLOAT32 a = re * phase[j].real();
				FLOAT32 b = im * phase[j].imag();
				FLOAT32 t;

				bits[j] <<= 1;
				bits[nPhases - 1 - j] <<= 1;

				t = a + b;
				bits[j] |= (t > 0) & 1;
				memory[j][last] = std::abs(t);

				t = a - b;
				bits[nPhases - 1 - j] |= (t > 0) & 1;
				memory[nPhases - 1 - j][last] = std::abs(t);
			}

			last = (last + 1) % nHistory;

			FLOAT32 max_val = 0;
			int prev_max = max_idx;

			// local minmax search
			for (int p = nPhases + prev_max - nSearch; p <= nPhases + prev_max + nSearch; p++)
			{
				int j = p % nPhases;
				FLOAT32 avg = memory[j][0];

				for (int l = 1; l < nHistory; l++) avg += memory[j][l];

				if (avg > max_val)
				{
					max_val = avg; max_idx = j;
				}
			}

			// determine the bit
			bool b2 = (bits[max_idx] >> (nDelay + 1)) & 1;
			bool b1 = (bits[max_idx] >> nDelay) & 1;

			FLOAT32 b = b1 ^ b2 ? 1.0f : -1.0f;

			sendOut(&b, 1);
		}
	}

	// Same version as default but instead relying on moving average
	void DefaultFastDemodulation::Receive(const CFLOAT32* data, int len)
	{
		for (int i = 0; i < len; i++)
		{
			FLOAT32 re, im;

			//  multiply samples with (1j) ** i, to get all points on the same line
			switch (rot)
			{
			case 0: re = data[i].real(); im = data[i].imag(); break;
			case 1: im = data[i].real(); re = -data[i].imag(); break;
			case 2: re = -data[i].real(); im = -data[i].imag(); break;
			case 3: im = -data[i].real(); re = data[i].imag(); break;
			}

			rot = (rot + 1) & 3;

			// Determining the phase is approached as a linear classification problem.
			for (int j = 0; j < nPhases / 2; j++)
			{
				FLOAT32 t, a = re * phase[j].real(), b = im * phase[j].imag();

				t = a + b;
				bits[j] = (bits[j] << 1) | ((t > 0) & 1);
				ma[j] = weight * ma[j] + (1-weight) * std::abs(t);

				t = a - b;
				bits[nPhases - 1 - j] = (bits[nPhases - 1 -j] << 1) | ((t > 0) & 1);
				ma[nPhases - 1 - j] = weight * ma[nPhases - 1 - j] + (1-weight) * std::abs(t);
			}

			// we look at previous [max_idx - nSearch, max_idx + nSearch]
			int idx = (max_idx - nSearch + nPhases) & (nPhases-1);

			max_idx = idx;
			FLOAT32 max_val = ma[idx];

			for (int p = 0; p < nSearch << 1; p++)
			{
				idx = ( ++idx ) & ( nPhases -1 );

				if (ma[idx] > max_val)
				{
					max_val = ma[idx]; max_idx = idx;
				}
			}

			// determine the bit
			bool b2 = (bits[max_idx] >> (nDelay + 1)) & 1;
			bool b1 = (bits[max_idx] >> nDelay) & 1;

			FLOAT32 b = b1 ^ b2 ? 1.0f : -1.0f;

			sendOut(&b, 1);
		}
	}
}
