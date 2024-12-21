/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstring>

#include "Filters.h"
#include "Stream.h"

namespace Demod {
	// needs to be a power of two for the Fast version
	static const int nPhases = 16;

	static const CFLOAT32 phase[nPhases / 2] = {
		{ 9.9518472640441780e-01f, 9.8017143048367339e-02f }, { 9.5694033335306883e-01f, 2.9028468509743588e-01f }, { 8.8192125790916542e-01f, 4.7139674887287397e-01f }, { 7.7301044123076901e-01f, 6.3439329894649099e-01f }, { 6.3439326515712957e-01f, 7.7301046896098113e-01f }, { 4.7139671032286945e-01f, 8.8192127851457169e-01f }, { 2.9028464326824349e-01f, 9.5694034604181499e-01f }, { 9.8017099547459546e-02f, 9.9518473068888236e-01f }
	};

	class FM : public SimpleStreamInOut<CFLOAT32, FLOAT32> {
		std::vector<FLOAT32> output;
		CFLOAT32 prev = 0.0;

	public:
		void Receive(const CFLOAT32* data, int len, TAG& tag);
	};

	class PhaseSearch : public SimpleStreamInOut<CFLOAT32, FLOAT32> {
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
		virtual ~PhaseSearch() {}

		void Receive(const CFLOAT32* data, int len, TAG& tag);

		void setParams(int h, int d) {
			assert(nHistory <= maxHistory);
			assert(nDelay <= nHistory);
			nHistory = h;
			nDelay = d;
		}
	};

	class PhaseSearchEMA : public SimpleStreamInOut<CFLOAT32, FLOAT32> {
		int nDelay = 0;

		static const int nSearch = 1;

		FLOAT32 weight = 0.85f;

		FLOAT32 ma[nPhases] = { 0 };
		char bits[nPhases] = { 0 };

		int max_idx = 0, rot = 0;

	public:
		virtual ~PhaseSearchEMA() {}

		void Receive(const CFLOAT32* data, int len, TAG& tag);
		void setParams(int d) { nDelay = d; }
		void setWeight(FLOAT32 w) { weight = w; }
	};
}
