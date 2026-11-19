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

#include <cstddef>
#include <cstdint>
#include <cstring>

// Software-Within-A-Register byte scanning on a size_t word. Callers write
// their match expression in terms of `word`.

namespace SWAR
{
	static constexpr size_t ONES = ~(size_t)0 / 255;
	static constexpr size_t HIGHS = ONES * 128;

	static constexpr size_t mask(char target)
	{
		return ONES * (unsigned char)target;
	}

	static inline size_t has_byte(size_t word, size_t m)
	{
		size_t v = word ^ m;
		return (v - ONES) & ~v & HIGHS;
	}

	static inline size_t has_byte_lt(size_t word, size_t n)
	{
		return (word - ONES * n) & ~word & HIGHS;
	}

	static inline int first_byte(size_t m)
	{
#if defined(__GNUC__) || defined(__clang__)
		if (sizeof(size_t) == 8)
			return __builtin_ctzll((unsigned long long)m) >> 3;
		else
			return __builtin_ctz((unsigned int)m) >> 3;
#else
		int n = 0;
		while (!(m & 0xFF))
		{
			m >>= 8;
			n++;
		}
		return n;
#endif
	}

	static inline int first_byte_pos(size_t m) { return first_byte(m); }
}

// Scan (buf + i) .. (buf + limit) for any byte matching `expr`, a word at a
// time; the caller handles the last bytes one by one. `expr` uses `word`.
#define SWAR_SKIP_AT(buf, i, limit, expr)              \
	while ((i) + (int)sizeof(size_t) <= (limit))       \
	{                                                  \
		size_t word;                                   \
		std::memcpy(&word, (buf) + (i), sizeof(word)); \
		size_t _swar_hit = (expr);                     \
		if (_swar_hit)                                 \
		{                                              \
			(i) += SWAR::first_byte_pos(_swar_hit);    \
			break;                                     \
		}                                              \
		(i) += sizeof(size_t);                         \
	}

#define SWAR_SKIP_PTR(p, pend, expr)                \
	while ((p) + sizeof(size_t) <= (pend))          \
	{                                               \
		size_t word;                                \
		std::memcpy(&word, (p), sizeof(word));      \
		size_t _swar_hit = (expr);                  \
		if (_swar_hit)                              \
		{                                           \
			(p) += SWAR::first_byte_pos(_swar_hit); \
			break;                                  \
		}                                           \
		(p) += sizeof(size_t);                      \
	}
