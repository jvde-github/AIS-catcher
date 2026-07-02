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

#include <string>
#include <cstdint>
#include <cstring>

namespace Util
{
	// Self-contained SHA-256 (FIPS 180-4), used for control-server password
	// hashing so no crypto library dependency is required
	class SHA256
	{
		uint32_t h[8];
		uint8_t buffer[64];
		uint64_t total = 0;
		size_t buffered = 0;

		static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

		void processBlock(const uint8_t *p)
		{
			static const uint32_t K[64] = {
				0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
				0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
				0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
				0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
				0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
				0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
				0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
				0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

			uint32_t w[64];
			for (int i = 0; i < 16; i++)
				w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
			for (int i = 16; i < 64; i++)
			{
				uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
				uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
				w[i] = w[i - 16] + s0 + w[i - 7] + s1;
			}

			uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];

			for (int i = 0; i < 64; i++)
			{
				uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
				uint32_t ch = (e & f) ^ (~e & g);
				uint32_t t1 = hh + S1 + ch + K[i] + w[i];
				uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
				uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
				uint32_t t2 = S0 + maj;

				hh = g;
				g = f;
				f = e;
				e = d + t1;
				d = c;
				c = b;
				b = a;
				a = t1 + t2;
			}

			h[0] += a;
			h[1] += b;
			h[2] += c;
			h[3] += d;
			h[4] += e;
			h[5] += f;
			h[6] += g;
			h[7] += hh;
		}

	public:
		SHA256()
		{
			h[0] = 0x6a09e667;
			h[1] = 0xbb67ae85;
			h[2] = 0x3c6ef372;
			h[3] = 0xa54ff53a;
			h[4] = 0x510e527f;
			h[5] = 0x9b05688c;
			h[6] = 0x1f83d9ab;
			h[7] = 0x5be0cd19;
		}

		void update(const void *data, size_t len)
		{
			const uint8_t *p = (const uint8_t *)data;
			total += len;

			while (len > 0)
			{
				size_t n = 64 - buffered;
				if (n > len)
					n = len;
				std::memcpy(buffer + buffered, p, n);
				buffered += n;
				p += n;
				len -= n;

				if (buffered == 64)
				{
					processBlock(buffer);
					buffered = 0;
				}
			}
		}

		std::string finish()
		{
			uint64_t bits = total * 8;

			uint8_t pad = 0x80;
			update(&pad, 1);
			uint8_t zero = 0;
			while (buffered != 56)
				update(&zero, 1);

			uint8_t len_bytes[8];
			for (int i = 0; i < 8; i++)
				len_bytes[i] = (uint8_t)(bits >> (56 - i * 8));
			update(len_bytes, 8);

			static const char *digits = "0123456789abcdef";
			std::string out;
			out.reserve(64);
			for (int i = 0; i < 8; i++)
				for (int j = 28; j >= 0; j -= 4)
					out += digits[(h[i] >> j) & 0xF];
			return out;
		}

		static std::string hex(const std::string &data)
		{
			SHA256 s;
			s.update(data.data(), data.size());
			return s.finish();
		}
	};
}
