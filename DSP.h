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


#pragma once

#include <fstream>

#include "Stream.h"
#include "Filters.h"
#include "Signal.h"

namespace DSP
{
	class PLLSampler : public SimpleStreamInOut<FLOAT32, BIT>, public MessageIn<DecoderMessage>
	{
		std::vector<BIT> output;
		BIT prev = 0;

		uint32_t PLL;
		bool FastPLL = true;

		const uint32_t FullRotation = 0x10000;
		const uint32_t MidPoint = FullRotation / 2;
		const uint32_t SamplesPerSymbol = 5;
		const uint32_t Increment = FullRotation / SamplesPerSymbol;

	public:

		// StreamIn
		virtual void Receive(const FLOAT32* data, int len);

		// MessageIn
		virtual void Message(const DecoderMessage& in)
		{
			switch (in)
			{
			case DecoderMessage::StartTraining:
				FastPLL = true;
				break;
			case DecoderMessage::StartMessage:
				FastPLL = false;
			default:
				break;
			}
		}
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

		const int buffer_size = 1024;

		inline CFLOAT32 filter(const CFLOAT32* data)
		{
			CFLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++) x += taps[i] * *data++;
			return x;
		}

	public:
		FilterComplex() { }
		FilterComplex(const std::vector <FLOAT32>& t) { setTaps(t); }


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

		const int buffer_size = 1024;

		inline FLOAT32 filter(const FLOAT32* data)
		{
			FLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++) x += taps[i] * *data++;
			return x;
		}

	public:

		Filter() { }
		Filter(const std::vector <FLOAT32>& t) { setTaps(t); }

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
				output[i].real((float)data[i].real()/255.0f - 0.5f);
				output[i].imag((float)data[i].imag()/255.0f - 0.5f);
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


}
