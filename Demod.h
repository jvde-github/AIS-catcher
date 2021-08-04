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
	class FMDemodulation : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;
		CFLOAT32 prev = 0.0;
		float DC_shift = 0.0;

	public:

		void Receive(const CFLOAT32* data, int len);
		void setDCShift(float s) { DC_shift = s; }
	};

	class CoherentDemodulation : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		static const int nHistory = 10;
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

	class ChallengerDemodulation : public SimpleStreamInOut<CFLOAT32, FLOAT32>, public MessageIn<DecoderMessages>
	{
		static const int nHistory = 8;
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

		// MessageIn
		virtual void Message(const DecoderMessages& in);
		void Receive(const CFLOAT32* data, int len);
	};
}
