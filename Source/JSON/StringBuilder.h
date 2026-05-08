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
#include <iostream>
#include <memory>
#include <cstring>
#include <cstdio>
#include <type_traits>

#include "Common.h"
#include "JSON.h"
#include "Keys.h"
#include "../Library/SWAR.h"

namespace JSON
{

	void stringify(const char *str, size_t len, std::string &json, bool esc = true);

	inline void stringify(const std::string &str, std::string &json, bool esc = true)
	{
		stringify(str.data(), str.size(), json, esc);
	}

	inline std::string stringify(const std::string &str, bool esc = true)
	{
		std::string s;
		stringify(str.data(), str.size(), s, esc);
		return s;
	}

	// Fast writer that owns raw ptr/end cursors into a std::string sink and
	// grows the backing string on demand. Hot path is a single-branch byte
	// write — same speed as a fixed char buffer, but with no truncation risk.
	// Provides both low-level primitives (append, append_int, append_string_escaped,
	// ...) and a fluent JSON object builder (kv, val, beginObject, endArray, ...).
	//
	// On destruction (or explicit finish()) the backing string is resized to
	// the actually-written content; capacity persists across calls when the
	// same target string is reused.
	class Writer
	{
	public:
		// String reference — captures pointer + length at the call site.
		// Unifies the (const char*, size_t), literal array, std::string, and
		// C-string entry points into a single overload per method.
		// Literals fold to compile-time length; char arrays use bounded
		// strnlen; std::string uses size().
		struct StringRef
		{
			const char *p;
			size_t n;
			StringRef(const char *s) : p(s), n(strlen(s)) {}
			StringRef(const char *s, size_t len) : p(s), n(len) {}
			template <size_t N>
			StringRef(const char (&s)[N]) : p(s), n(strnlen(s, N - 1)) {}
			StringRef(const std::string &s) : p(s.data()), n(s.size()) {}
		};

		// Key reference — captures pointer + length at the call site.
		// Key parameter type. Aliases AIS::KeyStr (defined in Keys.h) so the
		// same {ptr, length} struct serves both the static KeyMap table and
		// the Writer's key/kv API. Literal keys deduce length at compile
		// time via KeyStr's array ctor — no runtime strlen on the hot path.
		typedef AIS::KeyStr KeyRef;

	private:
		std::string *target;
		size_t start_off;
		char *ptr;
		char *end;
		bool need_sep = false;
		// Scratch for put_uint_raw / put_int_raw. Member (not local) so the
		// function's frame has no stack array — eliminates the canary check.
		char int_tmp[20];

		// Grow the backing string, preserving the write offset.
		void grow(size_t extra)
		{
			size_t used = (size_t)(ptr - &(*target)[0]);
			size_t cur = target->size();
			size_t need = used + extra;
			size_t nsz = cur * 2;
			if (nsz < need)
				nsz = need + 64;
			AISC_STRING_RESIZE_UNINIT(*target, nsz);
			ptr = &(*target)[0] + used;
			end = &(*target)[0] + nsz;
		}

		// ---- Unchecked write primitives.
		// Caller is responsible for ensuring enough capacity has been reserved.
		// Used to avoid per-byte capacity checks in the hot path: composite
		// methods reserve their worst case once, then call only put_* helpers. ----

		inline void put_char(char c) { *ptr++ = c; }

		inline void put_bytes(const char *s, size_t n)
		{
			memcpy(ptr, s, n);
			ptr += n;
		}

		// Two-digit ASCII table: digits100()[2*n .. 2*n+1] = the two decimal
		// digits of n for n in [0, 99]. Used by put_uint_raw and the
		// fractional block of put_float_raw to emit two digits per iteration
		// (one div/mod by 100 instead of two by 10). Function-local static
		// keeps this header-only under C++11 (no inline variables).
		static const char *digits100()
		{
			static constexpr char t[201] =
				"0001020304050607080910111213141516171819"
				"2021222324252627282930313233343536373839"
				"4041424344454647484950515253545556575859"
				"6061626364656667686970717273747576777879"
				"8081828384858687888990919293949596979899";
			return t;
		}

		// Up to 20 decimal digits. Caller must have reserved >= 20.
		inline void put_uint_raw(unsigned long long v)
		{
			char *const end_tmp = int_tmp + 20;
			char *p = end_tmp;
			while (v >= 100)
			{
				unsigned two = (unsigned)(v % 100);
				v /= 100;
				p -= 2;
				p[0] = digits100()[two * 2];
				p[1] = digits100()[two * 2 + 1];
			}
			if (v >= 10)
			{
				p -= 2;
				p[0] = digits100()[v * 2];
				p[1] = digits100()[v * 2 + 1];
			}
			else
			{
				*--p = (char)('0' + (int)v);
			}
			size_t n = (size_t)(end_tmp - p);
			memcpy(ptr, p, n);
			ptr += n;
		}

		// Optional sign + up to 20 digits. Caller must have reserved >= 21.
		inline void put_int_raw(long long v)
		{
			unsigned long long uv;
			if (v < 0)
			{
				*ptr++ = '-';
				// `0ULL - (unsigned long long)v` is well-defined unsigned
				// wrap and produces |v| for all v, including LLONG_MIN
				// (where `v = -v` would be UB).
				uv = 0ULL - (unsigned long long)v;
			}
			else
			{
				uv = (unsigned long long)v;
			}
			put_uint_raw(uv);
		}

		// Optional sign + integer + '.' + 6 fractional digits.
		// Caller must have reserved >= 28 and pre-checked NaN.
		// Uses round-half-to-even (banker's rounding) to match printf %f
		// and std::to_string exactly.
		inline void put_float_raw(double v)
		{
			if (v < 0)
			{
				*ptr++ = '-';
				v = -v;
			}

			long long whole = (long long)v;
			double frac_d = (v - (double)whole) * 1000000.0;
			int frac = (int)(frac_d + 0.5);
			// If we landed exactly on a .5 case and rounded UP to an odd value,
			// snap back down so the result is even (banker's rounding).
			if ((double)frac - frac_d == 0.5 && (frac & 1))
				frac--;
			if (frac >= 1000000)
			{
				whole++;
				frac -= 1000000;
			}

			put_uint_raw((unsigned long long)whole);

			// Integer value → no decimal point, no fractional digits.
			if (frac == 0)
				return;

			*ptr++ = '.';
			// frac in [1, 999999]: emit as three 2-digit pairs via table.
			unsigned f = (unsigned)frac;
			unsigned a = f / 10000;        // 00..99
			unsigned b = (f / 100) % 100;  // 00..99
			unsigned c = f % 100;          // 00..99
			ptr[0] = digits100()[a * 2];
			ptr[1] = digits100()[a * 2 + 1];
			ptr[2] = digits100()[b * 2];
			ptr[3] = digits100()[b * 2 + 1];
			ptr[4] = digits100()[c * 2];
			ptr[5] = digits100()[c * 2 + 1];
			ptr += 6;
			// Trim trailing zeros. Safe: the frac == 0 early-return
			// guarantees at least one non-zero digit remains.
			while (ptr[-1] == '0')
				ptr--;
		}

		// Writes ',' if a separator is pending. Caller must have reserved >= 1.
		inline void put_sep_raw()
		{
			if (need_sep)
				*ptr++ = ',';
			need_sep = false;
		}

		// Short-run memcpy that lets the compiler emit fixed-size ldr/str pairs
		// instead of a _memcpy stub call. Hot on JSON key/value writes.
		// [1..8]: size-specialized. [9..16]: two overlapping 8-byte copies
		// (branch-free). Default: fall back to memcpy.
		inline void put_bytes_short(const char *s, size_t n)
		{
			switch (n)
			{
			case 0: break;
			case 1: ptr[0] = s[0]; break;
			case 2: std::memcpy(ptr, s, 2); break;
			case 3: std::memcpy(ptr, s, 3); break;
			case 4: std::memcpy(ptr, s, 4); break;
			case 5: std::memcpy(ptr, s, 5); break;
			case 6: std::memcpy(ptr, s, 6); break;
			case 7: std::memcpy(ptr, s, 7); break;
			case 8: std::memcpy(ptr, s, 8); break;
			case 9:  std::memcpy(ptr, s, 9);  break;
			case 10: std::memcpy(ptr, s, 10); break;
			case 11: std::memcpy(ptr, s, 11); break;
			case 12: std::memcpy(ptr, s, 12); break;
			case 13: std::memcpy(ptr, s, 13); break;
			case 14: std::memcpy(ptr, s, 14); break;
			case 15: std::memcpy(ptr, s, 15); break;
			case 16: std::memcpy(ptr, s, 16); break;
			default: std::memcpy(ptr, s, n); break;
			}
			ptr += n;
		}

		// Writes "key":. Caller must have reserved >= klen + 3.
		inline void put_kvkey_raw(const char *k, size_t klen)
		{
			*ptr++ = '"';
			put_bytes_short(k, klen);
			*ptr++ = '"';
			*ptr++ = ':';
		}

		// JSON-escaped string body (no surrounding quotes).
		// Caller must have reserved >= len*6.
		void put_string_escaped_body_raw(const char *str, size_t len)
		{
			const char *s = str;
			const char *sEnd = s + len;
			const char *start = s;

			constexpr size_t m_quote = SWAR::mask('"');
			constexpr size_t m_bslash = SWAR::mask('\\');

			while (s < sEnd)
			{
				// Fast-scan the next run of safe bytes (>= 0x20, not '"' or '\\').
				SWAR_SKIP_PTR(s, sEnd,
					SWAR::has_byte(word, m_quote)
					| SWAR::has_byte(word, m_bslash)
					| SWAR::has_byte_lt(word, 0x20));
				if (s >= sEnd) break;

				unsigned char c = (unsigned char)*s;
				if (c >= 0x20 && c != '"' && c != '\\')
				{
					s++;
					continue;
				}
				if (s > start)
				{
					put_bytes_short(start, (size_t)(s - start));
				}
				switch (c)
				{
				case '"':
					memcpy(ptr, "\\\"", 2);
					ptr += 2;
					break;
				case '\\':
					memcpy(ptr, "\\\\", 2);
					ptr += 2;
					break;
				case '\b':
					memcpy(ptr, "\\b", 2);
					ptr += 2;
					break;
				case '\f':
					memcpy(ptr, "\\f", 2);
					ptr += 2;
					break;
				case '\n':
					memcpy(ptr, "\\n", 2);
					ptr += 2;
					break;
				case '\r':
					memcpy(ptr, "\\r", 2);
					ptr += 2;
					break;
				case '\t':
					memcpy(ptr, "\\t", 2);
					ptr += 2;
					break;
				default:
				{
					static const char hex[] = "0123456789abcdef";
					ptr[0] = '\\';
					ptr[1] = 'u';
					ptr[2] = '0';
					ptr[3] = '0';
					ptr[4] = hex[(c >> 4) & 0xF];
					ptr[5] = hex[c & 0xF];
					ptr += 6;
					break;
				}
				}
				start = ++s;
			}
			if (s > start)
			{
				size_t n = (size_t)(s - start);
				memcpy(ptr, start, n);
				ptr += n;
			}
		}

		// JSON-escaped string with surrounding quotes.
		// Caller must have reserved >= len*6 + 2.
		void put_string_escaped_raw(const char *str, size_t len)
		{
			*ptr++ = '"';
			put_string_escaped_body_raw(str, len);
			*ptr++ = '"';
		}

	public:
		explicit Writer(std::string &dst, size_t headroom = 1024)
		{
			target = &dst;
			start_off = dst.size();
			if (dst.capacity() < start_off + headroom)
				dst.reserve(start_off + headroom);
			AISC_STRING_RESIZE_UNINIT(dst, start_off + headroom);
			ptr = &dst[0] + start_off;
			end = &dst[0] + dst.size();
		}

		~Writer() { finish(); }

		// Trim backing string down to written content. Idempotent.
		// Returns total bytes written since construction.
		int finish()
		{
			if (!target)
				return 0;
			size_t written = (size_t)(ptr - (&(*target)[0] + start_off));
			target->resize(start_off + written);
			target = nullptr;
			return (int)written;
		}

		inline void reserve_more(size_t n)
		{
			if ((size_t)(end - ptr) < n)
				grow(n);
		}

		// ---- Low-level primitives ----

		inline void append(char c)
		{
			reserve_more(1);
			*ptr++ = c;
		}

		inline void append(const char *s, size_t len)
		{
			reserve_more(len);
			put_bytes(s, len);
		}

		// Compile-time-length overload: preferred for string literals,
		// avoids the runtime strlen() call entirely.
		template <size_t N>
		inline void append_lit(const char (&s)[N]) { append(s, N - 1); }

		// Runtime overload: only used when length is genuinely unknown.
		inline void append_lit(const char *s) { append(s, strlen(s)); }

		void append_int(long long v)
		{
			reserve_more(21);
			put_int_raw(v);
		}

		void append_float(double v)
		{
			// Single range-check catches NaN, ±Inf, and |v| >= 1e18 in one
			// branch: NaN comparisons are always false, ±Inf fails one side,
			// and 1e18 keeps |whole| comfortably under LLONG_MAX so the
			// (long long)v cast in put_float_raw is well-defined and the
			// post-rounding `whole++` cannot overflow.
			if (!(v > -1e18 && v < 1e18))
			{
				reserve_more(4);
				put_bytes("null", 4);
				return;
			}
			reserve_more(28);
			put_float_raw(v);
		}

		void append_string_escaped(const char *str, size_t len)
		{
			// Worst case: 6x expansion for \uXXXX + 2 quotes
			reserve_more(len * 6 + 2);
			put_string_escaped_raw(str, len);
		}

		void append_string_escaped(const std::string &s)
		{
			append_string_escaped(s.data(), s.size());
		}

		// Returns pointer to the start of content written by this Writer.
		// Valid only until the next grow() / append. Useful for in-place
		// post-processing such as CRC computation over the just-written bytes.
		char *buffer_start() { return &(*target)[0] + start_off; }
		size_t written() const { return (size_t)(ptr - (&(*target)[0] + start_off)); }

		// ---- High-level JSON-builder methods (auto-separator) ----
		// Each reserves its worst case once, then writes via put_*_raw helpers.
		// Reservation budgets:
		//   sep:  1
		//   int:  21 (sign + 20 digits)
		//   uint: 20
		//   float:28 (sign + 20 whole + '.' + 6 frac); NaN is "null" = 4
		//   key:  klen + 3 ('"' + key + '":')
		//   esc:  n*6 + 2 (worst-case string escaping)

		Writer &beginObject()
		{
			reserve_more(2);
			put_sep_raw();
			put_char('{');
			return *this;
		}
		Writer &endObject()
		{
			reserve_more(1);
			put_char('}');
			need_sep = true;
			return *this;
		}
		Writer &beginArray()
		{
			reserve_more(2);
			put_sep_raw();
			put_char('[');
			return *this;
		}
		Writer &endArray()
		{
			reserve_more(1);
			put_char(']');
			need_sep = true;
			return *this;
		}

		// Smaller-integer overloads exist only to block ambiguous resolution
		// against val(double). They widen and forward to the 64-bit workhorse.
		Writer &val(long long v)
		{
			reserve_more(22);
			put_sep_raw();
			put_int_raw(v);
			need_sep = true;
			return *this;
		}
		Writer &val(int v) { return val((long long)v); }
		Writer &val(long v) { return val((long long)v); }
		Writer &val(unsigned long long v)
		{
			reserve_more(21);
			put_sep_raw();
			put_uint_raw(v);
			need_sep = true;
			return *this;
		}
		Writer &val(unsigned v) { return val((unsigned long long)v); }
		Writer &val(unsigned long v) { return val((unsigned long long)v); }
		Writer &val(double v)
		{
			// Single range-check catches NaN, ±Inf, and |v| >= 1e18 in one
			// branch — see append_float() for rationale.
			if (!(v > -1e18 && v < 1e18))
			{
				reserve_more(5);
				put_sep_raw();
				put_bytes("null", 4);
				need_sep = true;
				return *this;
			}
			reserve_more(29);
			put_sep_raw();
			put_float_raw(v);
			need_sep = true;
			return *this;
		}
		// Exact-match for float so callers don't need to write (double)x.
		// Without this overload float would reach val(double) via Floating
		// Promotion — works, but the explicit overload removes the noise.
		Writer &val(float v) { return val((double)v); }
		// Constrained to exact bool to avoid pointer->bool conversions stealing
		// calls from val(StringRef).
		template <typename B, typename = typename std::enable_if<std::is_same<B, bool>::value>::type>
		Writer &val(B v)
		{
			reserve_more(6); // sep + "false"
			put_sep_raw();
			if (v)
				put_bytes("true", 4);
			else
				put_bytes("false", 5);
			need_sep = true;
			return *this;
		}
		Writer &val(StringRef v)
		{
			reserve_more(1 + v.n * 6 + 2);
			put_sep_raw();
			put_string_escaped_raw(v.p, v.n);
			need_sep = true;
			return *this;
		}
		// Writes one JSON string value built from two pieces, no temporary
		// allocation. Both pieces are escaped into the same "..." literal.
		Writer &val(StringRef s1, StringRef s2)
		{
			reserve_more(1 + (s1.n + s2.n) * 6 + 2);
			put_sep_raw();
			*ptr++ = '"';
			put_string_escaped_body_raw(s1.p, s1.n);
			put_string_escaped_body_raw(s2.p, s2.n);
			*ptr++ = '"';
			need_sep = true;
			return *this;
		}
		Writer &val_null()
		{
			reserve_more(5);
			put_sep_raw();
			put_bytes("null", 4);
			need_sep = true;
			return *this;
		}
		// Writes val(v) unless v equals sentinel, in which case writes null.
		// Two type params so e.g. char vs. int sentinels deduce independently.
		template <typename T, typename U>
		Writer &val_unless(T v, U sentinel)
		{
			return v == sentinel ? val_null() : val(v);
		}

		Writer &kv(KeyRef k, long long v)
		{
			reserve_more(k.n + 25); // sep + key + sign + 20 digits
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			put_int_raw(v);
			need_sep = true;
			return *this;
		}
		Writer &kv(KeyRef k, int v) { return kv(k, (long long)v); }
		Writer &kv(KeyRef k, long v) { return kv(k, (long long)v); }
		Writer &kv(KeyRef k, unsigned long long v)
		{
			reserve_more(k.n + 24); // sep + key + 20 digits
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			put_uint_raw(v);
			need_sep = true;
			return *this;
		}
		Writer &kv(KeyRef k, unsigned v) { return kv(k, (unsigned long long)v); }
		Writer &kv(KeyRef k, unsigned long v) { return kv(k, (unsigned long long)v); }
		Writer &kv(KeyRef k, double v)
		{
			reserve_more(k.n + 32); // sep + key + sign + 20 + '.' + 6
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			// Single range-check catches NaN, ±Inf, and |v| >= 1e18 in one
			// branch — see append_float() for rationale.
			if (!(v > -1e18 && v < 1e18))
				put_bytes("null", 4);
			else
				put_float_raw(v);
			need_sep = true;
			return *this;
		}
		Writer &kv(KeyRef k, float v) { return kv(k, (double)v); }
		Writer &kv(KeyRef k, StringRef v)
		{
			reserve_more(k.n + 4 + 1 + v.n * 6 + 2); // key + sep + escaped value
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			put_string_escaped_raw(v.p, v.n);
			need_sep = true;
			return *this;
		}
		// Two-piece string value, no temporary allocation. Both pieces are
		// escaped into the same "..." literal.
		Writer &kv(KeyRef k, StringRef s1, StringRef s2)
		{
			reserve_more(k.n + 4 + 1 + (s1.n + s2.n) * 6 + 2);
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			*ptr++ = '"';
			put_string_escaped_body_raw(s1.p, s1.n);
			put_string_escaped_body_raw(s2.p, s2.n);
			*ptr++ = '"';
			need_sep = true;
			return *this;
		}
		template <typename B, typename = typename std::enable_if<std::is_same<B, bool>::value>::type>
		Writer &kv(KeyRef k, B v)
		{
			reserve_more(k.n + 9); // sep + key + "false"
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			if (v)
				put_bytes("true", 4);
			else
				put_bytes("false", 5);
			need_sep = true;
			return *this;
		}
		Writer &kv_null(KeyRef k)
		{
			reserve_more(k.n + 8); // sep + key + 'null'
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			put_bytes("null", 4);
			need_sep = true;
			return *this;
		}
		// Writes kv(k, v) unless v equals sentinel, in which case writes
		// kv_null(k). Mirror of val_unless for keyed objects.
		template <typename T, typename U>
		Writer &kv_unless(KeyRef k, T v, U sentinel)
		{
			return v == sentinel ? kv_null(k) : kv(k, v);
		}

		// Writes "key":raw  — caller-supplied raw must be valid JSON.
		Writer &kv_raw(KeyRef k, const char *raw, size_t rawlen)
		{
			reserve_more(k.n + 4 + rawlen);
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			put_bytes(raw, rawlen);
			need_sep = true;
			return *this;
		}
		Writer &kv_raw(KeyRef k, const std::string &raw)
		{
			return kv_raw(k, raw.data(), raw.size());
		}

		// Writes "key": and leaves cursor positioned for the next val/raw write.
		// need_sep is cleared so the following val() does not emit a comma.
		Writer &key(KeyRef k)
		{
			reserve_more(k.n + 4);
			put_sep_raw();
			put_kvkey_raw(k.p, k.n);
			need_sep = false;
			return *this;
		}

		// Explicit (ptr, len) overload for runtime-built keys (e.g. snprintf
		// into a stack buffer). Avoids the KeyRef array ctor, which captures
		// the declared array size N-1 — correct for literals, wrong for
		// partially-filled buffers.
		Writer &key(const char *s, size_t n)
		{
			reserve_more(n + 4);
			put_sep_raw();
			put_kvkey_raw(s, n);
			need_sep = false;
			return *this;
		}

		// Like val() but for already-serialised JSON fragments. Honors need_sep.
		Writer &raw_val(const char *raw, size_t rawlen)
		{
			reserve_more(1 + rawlen);
			put_sep_raw();
			put_bytes(raw, rawlen);
			need_sep = true;
			return *this;
		}
		Writer &raw_val(const std::string &raw)
		{
			return raw_val(raw.data(), raw.size());
		}

		// Escape hatch for pre-built JSON fragments (no escaping, no separator)
		Writer &raw(const char *s)
		{
			append_lit(s);
			return *this;
		}
		Writer &raw(const std::string &s)
		{
			append(s.data(), s.size());
			return *this;
		}
	};

	// Serializes a JSON object using a Writer sink. Stateless w.r.t. the
	// destination — each stringify() call constructs a Writer.
	class Serializer
	{
	private:
		int dict = 0;
		bool stringify_enhanced = false;

		void write_value(const Value &v, Writer &w)
		{
			switch (v.getType())
			{
			case Value::Type::STRING:
				w.val(v.getStringRef());
				break;
			case Value::Type::INT:
				w.val((long long)v.getInt());
				break;
			case Value::Type::FLOAT:
				w.val(v.getFloat());
				break;
			case Value::Type::BOOL:
				w.val(v.getBool());
				break;
			case Value::Type::OBJECT:
				write_object(v.getObject(), w);
				break;
			case Value::Type::ARRAY_STRING:
			{
				const std::vector<std::string> &as = v.getStringArray();
				w.beginArray();
				for (const std::string &s : as)
					w.val(s);
				w.endArray();
				break;
			}
			case Value::Type::ARRAY:
			{
				const std::vector<Value> &a = v.getArray();
				w.beginArray();
				for (const Value &e : a)
					write_value(e, w);
				w.endArray();
				break;
			}
			default:
				w.val_null();
				break;
			}
		}

		void write_value_enhanced(const Value &v, int key_index, Writer &w)
		{
			w.beginObject().key("value");
			write_value(v, w);

			if (key_index >= 0 && key_index < AIS::KEY_COUNT)
			{
				const AIS::KeyInfo &info = AIS::KeyInfoMap[key_index];

				if (info.unit && info.unit[0] != '\0')
					w.kv("unit", info.unit);
				if (info.description && info.description[0] != '\0')
					w.kv("description", info.description);
				if (info.lookup_table && (v.isInt() || v.isFloat()))
				{
					int nv = v.isInt() ? v.getInt() : static_cast<int>(v.getFloat());
					if (nv >= 0 && nv < (int)info.lookup_table->size())
						w.kv("text", (*info.lookup_table)[nv]);
				}
			}
			w.endObject();
		}

		void write_object(const JSON &object, Writer &w)
		{
			w.beginObject();
			for (const Member &p : object.getMembers())
			{
				if (p.Key() < 0 || p.Key() >= AIS::KEY_COUNT)
					continue;
				const AIS::KeyStr &key = AIS::KeyMap[p.Key()][dict];
				if (key.empty())
					continue;

				w.key(key);

				if (stringify_enhanced)
					write_value_enhanced(p.Get(), p.Key(), w);
				else
					write_value(p.Get(), w);
			}
			w.endObject();
		}

	public:
		Serializer(int d = JSON_DICT_FULL) : dict(d) {}

		// Appends the serialized object (plus optional suffix) to dst.
		// Returns number of bytes appended.
		int stringify(const JSON &object, std::string &dst, const char *suffix = nullptr)
		{
			Writer w(dst);
			write_object(object, w);
			if (suffix)
				w.append_lit(suffix);
			return w.finish();
		}

		// Serializes object into an existing Writer. Honors the Writer's current
		// separator state, so callers can interleave serialized objects with
		// other Writer operations (e.g. inside a beginArray()/endArray() frame).
		void stringify(const JSON &object, Writer &w)
		{
			write_object(object, w);
		}

		void setMap(int d) { dict = d; }
		void setStringifyEnhanced(bool enhanced) { stringify_enhanced = enhanced; }
		bool getStringifyEnhanced() const { return stringify_enhanced; }
	};
}
