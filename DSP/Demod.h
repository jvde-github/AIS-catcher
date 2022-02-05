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

#pragma once

#include <cstring>

#include "Filters.h"

#include "Stream.h"
#include "Signals.h"

namespace Demod
{
	// needs to be a power of two for the Fast version
	static const int nPhases = 16;

	static const CFLOAT32 phase[nPhases/2] =
	{
		{ 9.9518472640441780e-01f, 9.8017143048367339e-02f }, { 9.5694033335306883e-01f, 2.9028468509743588e-01f }, { 8.8192125790916542e-01f, 4.7139674887287397e-01f },
		{ 7.7301044123076901e-01f, 6.3439329894649099e-01f }, { 6.3439326515712957e-01f, 7.7301046896098113e-01f }, { 4.7139671032286945e-01f, 8.8192127851457169e-01f },
		{ 2.9028464326824349e-01f, 9.5694034604181499e-01f }, { 9.8017099547459546e-02f, 9.9518473068888236e-01f }
	};

	class FM : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector <FLOAT32> output;
		CFLOAT32 prev = 0.0;

	public:

		void Receive(const CFLOAT32* data, int len);
	};

	class PhaseSearch : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		int nHistory = 8;
		int nDelay = 0;

		static const int maxHistory = 14;
		static const int nSearch = 2;

		FLOAT32 memory[nPhases][maxHistory];
		char bits[nPhases];

		int max_idx = 0;
		int rot = 0;
		int last = 0;

	public:

		void Receive(const CFLOAT32* data, int len);
		void setParams(int h, int d) { assert(nHistory <= maxHistory); assert(nDelay <= nHistory); nHistory = h; nDelay = d; }
	};

	class PhaseSearchEMA : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		int nDelay = 0;

		static const int nSearch = 1;

		const FLOAT32 weight = 0.85f;

		FLOAT32 ma[nPhases];
		char bits[nPhases];

		int max_idx = 0, rot = 0;

	public:

		PhaseSearchEMA() { std::memset(ma, 0, nPhases * sizeof(FLOAT32)); }
		void Receive(const CFLOAT32* data, int len);
		void setParams(int d) { nDelay = d; }
	};
}
