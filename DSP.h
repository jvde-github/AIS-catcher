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

#include "Stream.h"
#include "Filters.h"
#include "Signal.h"

namespace DSP
{
	class SamplerPLL : public SimpleStreamInOut<FLOAT32, FLOAT32>, public MessageIn<DecoderMessages>
	{
		std::vector<BIT> output;
		BIT prev = 0;

		float PLL;
		bool FastPLL = true;

	public:
		// StreamIn
		virtual void Receive(const FLOAT32* data, int len);

		// MessageIn
		virtual void Message(const DecoderMessages& in);
	};


	class SamplerParallel : public StreamIn<FLOAT32>
	{
		std::vector <FLOAT32> output;
		int lastSymbol = 0;
		int nBuckets = 0;

	public:
		void setBuckets(int n);

		// Streams out
		std::vector<Connection<FLOAT32>> out;

		// Streams in
		void Receive(const FLOAT32* data, int len);
	};


        class SamplerParallelComplex : public StreamIn<CFLOAT32>
        {
                std::vector <CFLOAT32> output;
                int lastSymbol = 0;
                int nBuckets = 0;

        public:
                void setBuckets(int n);

                // Streams out
                std::vector<Connection<CFLOAT32>> out;

                // Streams in
                void Receive(const CFLOAT32* data, int len);
        };


	class Downsample2CIC5 : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		CFLOAT32 h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;
		std::vector <CFLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	class Downsample3Complex : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector <CFLOAT32> output;
		std::vector <CFLOAT32> buffer;

	public:
		Downsample3Complex()
		{
			buffer.resize(42, 0.0f);
		}

		void Receive(const CFLOAT32* data, int len);
	};

	class FilterComplex : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector <CFLOAT32> output;
		std::vector <CFLOAT32> buffer;
		std::vector <FLOAT32> taps;

		inline CFLOAT32 filter(const CFLOAT32* data)
		{
			CFLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++) x += taps[i] * *data++;
			return x;
		}

	public:
		FilterComplex() { }

		void setTaps(const std::vector<FLOAT32>& t)
		{
			taps = t;
			buffer.resize(taps.size() * 2, 0.0f);
		}

		// StreamIn
		void Receive(const CFLOAT32* data, int len);
	};

	class Filter : public SimpleStreamInOut<FLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;
		std::vector <FLOAT32> buffer;
		std::vector <FLOAT32> taps;

		inline FLOAT32 filter(const FLOAT32* data)
		{
			FLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++) x += taps[i] * *data++;

			return x;
		}

	public:
		void setTaps(const std::vector<FLOAT32>& t)
		{
			taps = t;
			buffer.resize(taps.size() * 2, 0.0f);
		}

		// StreamIn
		void Receive(const FLOAT32* data, int len);
	};


	class FilterCIC5 : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		CFLOAT32 h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;
		std::vector <CFLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	class ConvertCU8ToCFLOAT32 : public SimpleStreamInOut<CU8, CFLOAT32>
	{
		std::vector <CFLOAT32> output;

	public:
		void Receive(const CU8* data, int len)
		{
			if (output.size() < len) output.resize(len);

			for (int i = 0; i < len; i++)
			{
				output[i].real((float)data[i].real()/128.0f-1.0f);
				output[i].imag((float)data[i].imag()/128.0f-1.0f);
			}
			out.Send(output.data(), len);
		}
	};

	class RotateUp : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector <CFLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	class RotateDown : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector <CFLOAT32> output;

	public:
		void Receive(const CFLOAT32* data, int len);
	};

	class FMDemodulation : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;
		CFLOAT32 prev = 0.0;
		float DC_shift = 0.0;

	public:

		void Receive(const CFLOAT32* data, int len);
		void setDCShift(float s) { DC_shift = s; }
	};

        class SquareFreqOffsetCorrection : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
        {
                std::vector <CFLOAT32> output;
                std::vector <CFLOAT32> fft_data;

                CFLOAT32 rot = 1.0f;
                int count = 0;

                void correctFrequency();

        public:
                void Receive(const CFLOAT32* data, int len);
        };

        class CoherentDemodulation : public SimpleStreamInOut<CFLOAT32, FLOAT32>
        {
                static const int nHistory = 4;
		static const int nPhases = 16;
		static const int nSearch = 2;
		static const int nUpdate = 1;

                std::vector <CFLOAT32> phase;
                FLOAT32 memory[nPhases][nHistory];
                char bits[nPhases];

		int max_idx = 0;
		int update = 0;
		int rot = 0;
                int last = 0;

		void setPhases();
        public:

                void Receive(const CFLOAT32* data, int len);
        };
}
