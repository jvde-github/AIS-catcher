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

#include "FFT.h"
#include "DSP.h"

namespace DSP
{
	void SamplerPLL::Receive(const FLOAT32* data, int len)
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
                                sendOut(&data[i], 1);
                                PLL -= (int) PLL;
                        }
                        prev = bit;
                }
	}

	void SamplerPLL::Message(const DecoderMessages& in)
	{
		switch (in)
		{
		case DecoderMessages::StartTraining: FastPLL = true; break;
		case DecoderMessages::StopTraining: FastPLL = false; break;
		default: break;
		}
	}

	void SamplerParallel::setBuckets(int n)
	{
		nBuckets = n;
		out.resize(nBuckets);
	}

	void SamplerParallel::Receive(const FLOAT32* data, int len)
	{
		for (int i = 0; i < len; i++)
		{
			out[lastSymbol].Send(&data[i], 1);
			lastSymbol = (lastSymbol + 1) % nBuckets;
		}
	}

        void SamplerParallelComplex::setBuckets(int n)
        {
                nBuckets = n;
                out.resize(nBuckets);
        }

        void SamplerParallelComplex::Receive(const CFLOAT32* data, int len)
        {
                for (int i = 0; i < len; i++)
                {
                        out[lastSymbol].Send(&data[i], 1);
                        lastSymbol = (lastSymbol + 1) % nBuckets;
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

		for (int i = 0, j = 0; i < len; i += 2, j++)
		{
			z = data[i];
			MA1(0); MA1(1); MA1(2); MA1(3); MA1(4);
			output[j] = z * (FLOAT32)0.03125f;
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
		assert(len % 3 == 0);

		int ptr, i, j;

		if (output.size() < len/3) output.resize(len/3);

		for (j = i = 0, ptr = 21 - 1; i < 21 - 1; i += 3, j++)
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

			output[j] = x;
		}

		for (i = 1; i < len - 21 + 1; i += 3, j++)
		{
			CFLOAT32 x = 0.33292088503f * data[i + 10];
			x -= 0.00101073661f * (data[i + 0] + data[i + 20]);
			x += 0.00616649466f * (data[i + 2] + data[i + 18]);
			x += 0.01130778123f * (data[i + 3] + data[i + 17]);
			x -= 0.03044260089f * (data[i + 5] + data[i + 15]);
			x -= 0.04750748661f * (data[i + 6] + data[i + 14]);
			x += 0.12579695977f * (data[i + 8] + data[i + 12]);
			x += 0.26922914593f * (data[i + 9] + data[i + 11]);

			output[j] = x;
		}
		for (ptr = 0; i < len; i++, ptr++)
		{
			buffer[ptr] = data[i];
		}

		sendOut(output.data(), len / 3);
	}

	// Filter Generic

	void FilterComplex::Receive(const CFLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = i = 0, ptr = taps.size() - 1; i < taps.size() - 1; i++, ptr++, j++)
		{
			buffer[ptr] = data[i];
			output[j] = filter(&buffer[i]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++)
		{
			output[j] = filter(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++)
		{
			buffer[ptr] = data[i];
		}

		sendOut(output.data(), len);
	}

	void Filter::Receive(const FLOAT32* data, int len)
	{
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = i = 0, ptr = taps.size() - 1; i < taps.size() - 1; i++, ptr++, j++)
		{
			buffer[ptr] = data[i];
			output[j] = filter(&buffer[i]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++)
		{
			output[j] = filter(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++)
		{
			buffer[ptr] = data[i];
		}

		sendOut(output.data(), len);
	}


	void RotateDown::Receive(const CFLOAT32* data, int len)
	{
		assert(len % 4 == 0);

		if (output.size() < len) output.resize(len);

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

		if (output.size() < len) output.resize(len);

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
			auto p = data[i] * std::conj(prev);

			output[i] = (atan2f(p.imag(), p.real()) + DC_shift) / PI;
			prev = data[i];
		}

		sendOut(output.data(), len);
	}

	// square the signal, find the mid-point between two peaks
	void SquareFreqOffsetCorrection::correctFrequency()
	{
                double max_val = 0.0, fz = -1;
                int delta = 819; // 9600/48000*4096

                FFT::fft(fft_data);

                for(int i = 0; i<4096-delta; i++)
                {
                        double h = std::abs(fft_data[(i+2048)%4096])+std::abs(fft_data[(i+delta+2048) % 4096]);

                        if(h > max_val)
                        {
                                max_val = h;
                                fz = (2048-(i+delta/2.0));
                        }
                }

                CFLOAT32 rot_step = std::polar(1.0f, (float)(fz/2/4096*2*M_PI));

                for(int i = 0; i<4096; i++)
                {
                        rot *= rot_step;
                        output[i] *= rot;
                }
        }

        void SquareFreqOffsetCorrection::Receive(const CFLOAT32* data, int len)
        {
		const int logN = 12; //FFT::log2(4096);

                if(fft_data.size() < 4096) fft_data.resize(4096);
                if(output.size() < 4096) output.resize(4096);


                for(int i = 0; i< len; i++)
                {
                        fft_data[FFT::rev(count,logN)] = data[i] * data[i];
                        output[count] = data[i];

                        if(++count == 4096)
                        {
                                correctFrequency();
                                sendOut(output.data(),4096);
                                count = 0;
                        }
                }
        }

      void CoherentDemodulation::setPhases()
	{
		int np2 = nPhases/2;
 		phase.resize(np2);
		for(int i = 0; i<np2; i++)
		{
			float alpha = M_PI/2.0/np2*i+M_PI/(2.0*np2);
			phase[i] = std::polar(1.0f,alpha);
		}
	}

      void CoherentDemodulation::Receive(const CFLOAT32* data, int len)
        {
                if(phase.size() == 0) setPhases();

                for(int i = 0; i<len; i++)
                {
                        FLOAT32 re, im;

                        //  multiply samples with (1j) ** i 
                        switch(rot)
                        {
                                case 0: re = data[i].real(); im = data[i].imag(); break;
                                case 1: im = data[i].real(); re = -data[i].imag(); break;
                                case 2: re = -data[i].real(); im = -data[i].imag(); break;
                                case 3: im = -data[i].real(); re = data[i].imag(); break;
                        }

                        rot = (rot+1) % 4;

                        // the points are on one line with unknown phase, approach as linear classification w 
			// problem where we maximize the margin, i.e. the minimum distance to zero on the reak line
			// first we calculate the this for all nPhases
                        for(int j=0; j<nPhases/2; j++)
                        {
                                FLOAT32 a = re*phase[j].real();
                                FLOAT32 b = -im*phase[j].imag();

				bits[j] <<= 1;
				bits[nPhases-1-j] <<= 1;

				FLOAT32 t;
				t = a+b;
                                bits[j] |= ((t)>0) & 1;
                                memory[j][last] = std::abs(t);

				t = a-b;
                                bits[nPhases-1-j] |= ((t)>0) & 1;
                                memory[nPhases-1-j][last] = std::abs(t);
                        }

			// Determine phase that maximizes minunum distance to zero on real line every nUpdate iterations
			// We only consider nSearches below and above the current maximum. This is crtiical as the global maximum
			// might not make sense if there are not sufficient both 1s and 0s. For example with a short nHistory.
			// A short nHistory will make the algorithm less sensitive to frequency offsets but more sensitive to noise.
			// For the moment I have preset nHistory at 4 which is a tradeoff that seems to work well. 
			// I have some ideas how to solve this issue but requires a bit more experimentation. This is also the reason
			// why below code is not fully optimized yet.

			update = (update + 1) % nUpdate;
			if(update == 0)
			{
                        	FLOAT32 maxval = 0;
				int prev_max = max_idx;
				
				// local minmax search
                                for(int m = nPhases-nSearch; m <= nPhases+nSearch; m++)
                                {
                                        int j = (m + prev_max) % nPhases;
                                        FLOAT32 min_abs = memory[j][0];

                                	for(int l = 1; l<nHistory; l++)
                                	{
                                        	FLOAT32 v = memory[j][l];
                                        	if(v < min_abs) min_abs = v;
                                	}

                                	if(min_abs > maxval)
                                	{
                                        	maxval = min_abs;
                                        	max_idx = j;
                                	}
                        	}
			}

			// determine the bit
			bool b2 = (bits[max_idx] & 2) >> 1;
			bool b1 = bits[max_idx] & 1;

                        FLOAT32 b = b1 ^ b2 ? 1.0f:-1.0f;

                        sendOut(&b,1);
                        last = (last+1) % nHistory;
                }
        }
}
