/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include <chrono>
#include <cassert>
#include <complex>

#include "Demod.h"
#include "DSP.h"

namespace Demod {

	void FM::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++) {
			auto p = data[i] * std::conj(prev);
			output[i] = atan2f(p.imag(), p.real()) / PI;
			prev = data[i];
		}

		Send(output.data(), len, tag);
	}

	// Same version as above but instead relying on moving average to speed up
	void PhaseSearchEMA::Receive(const CFLOAT32* data, int len, TAG& tag) {
		for (int i = 0; i < len; i++) {
			FLOAT32 re, im;

			//  multiply samples with (1j) ** i, to get all points on the same line
			switch (rot) {
			case 0:
				re = data[i].real();
				im = data[i].imag();
				break;
			case 1:
				im = data[i].real();
				re = -data[i].imag();
				break;
			case 2:
				re = -data[i].real();
				im = -data[i].imag();
				break;
			case 3:
				im = -data[i].real();
				re = data[i].imag();
				break;
			}

			rot = (rot + 1) & 3;

			// Determining the phase is approached as a linear classification problem.
			for (int j = 0; j < nPhases / 2; j++) {
				FLOAT32 t, a = re * phase[j].real(), b = im * phase[j].imag();

				t = a + b;
				bits[j] = (bits[j] << 1) | (t > 0);
				ma[j] = weight * ma[j] + (1 - weight) * std::abs(t);

				t = a - b;
				bits[nPhases - 1 - j] = (bits[nPhases - 1 - j] << 1) | (t > 0);
				ma[nPhases - 1 - j] = weight * ma[nPhases - 1 - j] + (1 - weight) * std::abs(t);

				// prevent error propagation of inf and nan in input
				if (std::isinf(ma[j]) || std::isnan(ma[j])) ma[j] = 0;
				if (std::isinf(ma[nPhases - 1 - j]) || std::isnan(ma[nPhases - 1 - j])) ma[nPhases - 1 - j] = 0;
			}

			// we look at previous [max_idx - nSearch, max_idx + nSearch]
			int idx = (max_idx - nSearch + nPhases) & (nPhases - 1);
			FLOAT32 max_val = ma[idx];
			max_idx = idx;

			for (int p = 0; p < nSearch << 1; p++) {
				idx = (++idx) & (nPhases - 1);

				if (ma[idx] > max_val) {
					max_val = ma[idx];
					max_idx = idx;
				}
			}

			// determine the bit
			bool b2 = (bits[max_idx] >> (nDelay + 1)) & 1;
			bool b1 = (bits[max_idx] >> nDelay) & 1;

			FLOAT32 b = b1 ^ b2 ? 1.0f : -1.0f;

			Send(&b, 1, tag);
		}
	}

	void PhaseSearch::Receive(const CFLOAT32* data, int len, TAG& tag) {
		for (int i = 0; i < len; i++) {
			FLOAT32 re, im;

			//  multiply samples with (1j) ** i, to get all points on the same line
			switch (rot) {
			case 0:
				re = data[i].real();
				im = data[i].imag();
				break;
			case 1:
				im = data[i].real();
				re = -data[i].imag();
				break;
			case 2:
				re = -data[i].real();
				im = -data[i].imag();
				break;
			case 3:
				im = -data[i].real();
				re = data[i].imag();
				break;
			}

			rot = (rot + 1) & 3;

			// Determining the phase is approached as a linear classification problem.
			for (int j = 0; j < nPhases / 2; j++) {
				FLOAT32 a = re * phase[j].real();
				FLOAT32 b = im * phase[j].imag();
				FLOAT32 t;

				t = a + b;
				bits[j] = (bits[j] << 1) | (t > 0);
				memory[j][last] = std::abs(t);

				t = a - b;
				bits[nPhases - 1 - j] = (bits[nPhases - 1 - j] << 1) | (t > 0);
				memory[nPhases - 1 - j][last] = std::abs(t);
			}

			last = (last + 1) % nHistory;

			FLOAT32 max_val = 0;
			int prev_max = max_idx;

			// local minmax search
			for (int p = nPhases + prev_max - nSearch; p <= nPhases + prev_max + nSearch; p++) {
				int j = p % nPhases;
				FLOAT32 avg = memory[j][0];

				for (int l = 1; l < nHistory; l++) avg += memory[j][l];

				if (avg > max_val) {
					max_val = avg;
					max_idx = j;
				}
			}

			// determine the bit
			bool b2 = (bits[max_idx] >> (nDelay + 1)) & 1;
			bool b1 = (bits[max_idx] >> nDelay) & 1;

			FLOAT32 b = b1 ^ b2 ? 1.0f : -1.0f;

			Send(&b, 1, tag);
		}
	}
}
