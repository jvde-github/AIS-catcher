/*
Copyright(c) 2021-2022 jvde.github@gmail.com

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
#include <cstring>

#include "FFT.h"
#include "DSP.h"

namespace DSP
{
	void SimplePLL::Receive(const FLOAT32* data, int len)
	{
		for (int i = 0; i < len; i++)
		{
			BIT bit = (data[i] > 0);

			if (bit != prev)
			{
				PLL += (0.5f - PLL) * (FastPLL ? 0.6f : 0.05f);
			}

			PLL += 0.2f;

			if (PLL >= 1.0f)
			{
				Send(&data[i], 1);
				PLL -= (int) PLL;
			}
			prev = bit;
		}
	}

	void SimplePLL::Message(const DecoderMessages& in)
	{
		switch (in)
		{
		case DecoderMessages::StartTraining: FastPLL = true; break;
		case DecoderMessages::StopTraining: FastPLL = false; break;
		default: break;
		}
	}

// helper macros for moving averages
#define MA1(idx)	r##idx = z; z += h##idx;
#define MA2(idx)	h##idx = z; z += r##idx;

	// CIC5 downsample
	void Downsample2CIC5::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 2 == 0);

		if (output.size() < len / 2) output.resize(len / 2);

		CFLOAT32 z, r0, r1, r2, r3, r4;

		for (int i = 0, j = 0; i < len; i += 2, j++)
		{
			z = data[i];
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			output[j] = z * (FLOAT32)0.03125f;
			z = data[i + 1];
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}

		Send(output.data(), len / 2);
	}

	void Decimate2::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 2 == 0);

		if (output.size() < len / 2) output.resize(len / 2);

		for (int i = 0, j = 0; i < len; i += 2, j++)
		{
			output[j] = data[i];
		}

		Send(output.data(), len / 2);
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

		Send(output.data(), len);
	}

	// Work in progress - needs performance improvement
	void DownsampleKFilter::Receive(const CFLOAT32* data, int len)
	{
		int i, j;

		int nt = (int)taps.size();

		if (output.size() < outputSize) output.resize(outputSize);
		if (buffer.size() < len + nt) buffer.resize((int) (len + nt),0.0f);

		for (i = 0, j = nt - 1; i < len; i++, j++) buffer[j] = data[i];

		while(idx_in < len)
		{
			output[idx_out] = dot(&buffer[idx_in]);

			if(++idx_out == outputSize)
			{
				Send(output.data(), outputSize);
				idx_out = 0;
			}

			idx_in += K;
		}

		idx_in -= len;


		for (j = 0, i = len - nt + 1; j < nt - 1; i++, j++)
			buffer[j] = data[i];
	}

	// Upsample with linear interpolation
	void Upsample::Receive(const CFLOAT32* data, int len)
	{
		if(output.size() < len) output.resize(len);

		for(int i = 0; i < len; i++)
		{
			const CFLOAT32 b = data[ i ];

			do
			{
				output[idx_out++] = (1-alpha) * a + alpha * b;
				alpha += increment;

				if (idx_out == len)
				{
					Send(output.data(), len);
					idx_out = 0;
				}

			} while (alpha < 1.0f);

			alpha -= 1.0f;
			a = b;
		}
	}

	// Filter Generic complex
	void FilterComplex::Receive(const CFLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = 0, ptr = (int)taps.size() - 1; j < taps.size() - 1; ptr++, j++)
		{
			buffer[ptr] = data[j];
			output[j] = dot(&buffer[j]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++)
		{
			output[j] = dot(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++)
		{
			buffer[ptr] = data[i];
		}

		Send(output.data(), len);
	}

	// Filter Generic Real
	void Filter::Receive(const FLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = 0, ptr = (int)taps.size() - 1; j < taps.size() - 1; ptr++, j++)
		{
			buffer[ptr] = data[j];
			output[j] = dot(&buffer[j]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++)
		{
			output[j] = dot(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++)
		{
			buffer[ptr] = data[i];
		}

		Send(output.data(), len);
	}

	// Rotate +/- 25K Hz
	void Rotate::Receive(const CFLOAT32* data, int len)
	{
		if (output_up.size() < len) output_up.resize(len);
		if (output_down.size() < len) output_down.resize(len);

		for (int i = 0; i < len; i++)
		{
			FLOAT32 RR = data[i].real() * rot.real(), II = data[i].imag() * rot.imag();
			FLOAT32 RI = data[i].real() * rot.imag(), IR = data[i].imag() * rot.real();

			output_up[i].real(RR-II); output_up[i].imag(IR+RI);
			output_down[i].real(RR+II); output_down[i].imag(IR-RI);

			rot *= mult;
		}

		up.Send(output_up.data(), len);
		down.Send(output_down.data(), len);

		rot /= std::abs(rot);
	}

	// square the signal, find the mid-point between two peaks
	void SquareFreqOffsetCorrection::correctFrequency()
	{
		FLOAT32 max_val = 0.0, fz = -1;
		int delta = (int)(9600.0 / 48000.0 * N);

		FFT::fft(fft_data);

		for(int i = window; i<N-window-delta; i++)
		{
			FLOAT32 h = std::abs(fft_data[(i + N / 2) % N]) + std::abs(fft_data[(i + delta + N / 2) % N]);

			if(h > max_val)
			{
				max_val = h;
				fz = (N / 2 - (i + delta / 2.0f));
			}
		}

		CFLOAT32 rot_step = std::polar(1.0f, (float)(fz / 2.0 / N * 2 * PI));

		for(int i = 0; i<N; i++)
		{
			rot *= rot_step;
			output[i] *= rot;
		}

		rot /= std::abs(rot);
	}

	void SquareFreqOffsetCorrection::setParams(int n,int w)
	{
		N = n;
		logN = FFT::log2(N);
		window = w;
	}

	void SquareFreqOffsetCorrection::Receive(const CFLOAT32* data, int len)
	{
		if(fft_data.size() < N) fft_data.resize(N);
		if(output.size() < N) output.resize(N);

		for(int i = 0; i< len; i++)
		{
			fft_data[FFT::rev(count, logN)] = data[i] * data[i];
			output[count] = data[i];

			if(++count == N)
			{
				correctFrequency();
				Send(output.data(), N);
				count = 0;
			}
		}
	}

	// ----------------------------------------------------------------------------
	// CIC5 downsampling optimized for Raspberry Pi 1
	// Idea: I and Q signals can be downsampled in parallel and, if stored
	// as int16, can be worked in parallel using int32.
	// ----------------------------------------------------------------------------

	int DS_UINT16::Run(uint32_t* data, int len, int shift)
	{
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift; mask |= mask << 16;
		uint32_t* out = data;

		len >>= 1;

		for (int i = 0; i < len; i ++)
		{
			z = *data++;
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			// divide by 2^shift and clean up contamination from I to Q
			*out++ = (z >> shift) & mask;
			z = *data++;
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}
		return len;
	}

	// special version of the above but includes the initial conversion from CU8 to avoid extra loop
	int DS_UINT16::Run(uint8_t* in, uint32_t* out, int len, int shift)
	{
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift; mask |= mask << 16;

		len >>= 1;

		for (int i = 0; i < len; i ++)
		{
			z = (uint32_t)*in++; z |= (uint32_t)*in++ << 16;
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			*out++ = (z >> shift) & mask;
			z = (uint32_t)*in++; z |= (uint32_t)*in++ << 16;
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}
		return len;
	}

	// special version of the above but includes the initial conversion from CS8 to avoid extra loop
	int DS_UINT16::Run(int8_t* in, uint32_t* out, int len, int shift)
	{
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift; mask |= mask << 16;
		uint32_t mask_uint = (1 << 7) | (1 << 23);

		len >>= 1;

		for (int i = 0; i < len; i++)
		{
			z = (uint8_t)*in++; z |= (uint32_t)((uint8_t)*in++) << 16;
			z ^= mask_uint;
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			*out++ = (z >> shift) & mask;
			z = (uint8_t)*in++; z |= (uint32_t)((uint8_t)*in++) << 16;
			z ^= mask_uint;
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}
		return len;
	}

	// special version of the above but includes the last conversion to CFLOAT32
	int DS_UINT16::Run(uint32_t* in, CFLOAT32* out, int len, int shift)
	{
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift; mask |= mask << 16;

		len >>= 1;

		for (int i = 0; i < len; i++)
		{
			z = *in++;
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			z = (z >> shift) & mask;
			out[i].real(((int32_t)(z & 0xFFFFU) - 32768) / 32768.0f);
			out[i].imag(((int32_t)(z >> 16) - 32768) / 32768.0f);
			z = *in++;
			MA2(0); MA2(1); MA2(2); MA2(3); MA2(4);
		}
		return len;
	}

	// Aggregators
	void Downsample32_CU8::Receive(const CU8* data, int len)
	{
		assert(len % 32 == 0);

		if (output.size() < len / 32) output.resize(len / 32);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5); len = DS4.Run(buf, len, 5);
		len = DS5.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};

	void Downsample32_CS8::Receive(const CS8* data, int len)
	{
		assert(len % 32 == 0);

		if (output.size() < len / 32) output.resize(len / 32);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5); len = DS4.Run(buf, len, 5);
		len = DS5.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};

	void Downsample16_CU8::Receive(const CU8* data, int len)
	{
		assert(len % 16 == 0);

		if (output.size() < len / 16) output.resize(len / 16);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5); len = DS4.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};

	void Downsample16_CS8::Receive(const CS8* data, int len)
	{
		assert(len % 16 == 0);

		if (output.size() < len / 16) output.resize(len / 16);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5); len = DS4.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};

	void Downsample8_CU8::Receive(const CU8* data, int len)
	{
		assert(len % 8 == 0);

		if (output.size() < len / 8) output.resize(len / 8);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};

	void Downsample8_CS8::Receive(const CS8* data, int len)
	{
		assert(len % 8 == 0);

		if (output.size() < len / 8) output.resize(len / 8);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3); len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len);
	};
}
