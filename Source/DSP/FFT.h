/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include <vector>
#include <complex>
#include <stdexcept>

#include "Common.h"

namespace FFT
{
	static inline int log2(int x)
	{
		int y = 0;
		while (x >>= 1)
			y++;
		return y;
	}

	static inline int rev(int x, int logN)
	{
#if defined(HASRBIT) && (defined(__aarch64__) || defined(__arm__))
		unsigned int r;
#if defined(__aarch64__)
		__asm__("rbit %w0, %w1" : "=r"(r) : "r"((unsigned)x));
#else
		__asm__("rbit %0, %1" : "=r"(r) : "r"((unsigned)x));
#endif
		return (int)(r >> (32 - logN));
#else
		static const int rev4[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

		int y = 0, j;

		// 4 bits in one go
		for (j = 0; j < logN >> 2; j++)
		{
			y <<= 4;
			y |= rev4[x & 15];
			x >>= 4;
		}

		// remaining bits
		for (int i = j << 2; i < logN; i++)
		{
			y <<= 1;
			y |= (x & 1);
			x >>= 1;
		}

		return y;
#endif
	}

	template <typename T>
	static void calcOmega(std::vector<std::complex<T>> &Omega, int N)
	{
		int logN = log2(N);

		for (int i = 0; i < logN; i++)
			if (rev(1 << i, logN) != (1 << (logN - 1 - i)))
				throw std::runtime_error("FFT: bit reversal check failed");

		Omega.resize(N);

		for (int s = 0; s < N; s++)
			Omega[s] = std::polar(T(1), T(-2.0 * PI) * T(s) / T(N));
	}

	template <typename T>
	class Plan
	{
		std::vector<std::complex<T>> Omega;
		int N = 0, logN = 0;

	public:
		void fft(std::vector<std::complex<T>> &x)
		{
			std::complex<T> t;

			if (N != (int)x.size())
			{
				N = (int)x.size();
				logN = log2(N);
				calcOmega(Omega, N);
			}

			int m = 2, m2 = 1;
			int w, r = N;

			for (int s = 0; s < logN; s++)
			{
				w = 0;
				r >>= 1;

				for (int j = 0; j < m2; j++)
				{
					const std::complex<T> &o = Omega[w];

					for (int k = 0; k < N; k += m)
					{
						t = o * x[k + j + m2];

						x[k + j + m2] = x[k + j] - t;
						x[k + j] += t;
					}

					w += r;
				}

				m2 = m;
				m <<= 1;
			}
		}
	};
}
