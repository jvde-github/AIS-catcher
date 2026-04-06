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

#include "Common.h"
#include "JSON.h"
#include "Keys.h"

namespace JSON
{

	class StringBuilder
	{
	private:
		int dict = 0;
		bool stringify_enhanced = false;

	public:
		StringBuilder(int d = JSON_DICT_FULL) : dict(d) {}

		void to_string(std::string &json, const Value &v);
		void to_string_enhanced(std::string &json, const Value &v, int key_index);
		void stringify(const JSON &properties, std::string &json);
		static void stringify(const std::string &str, std::string &json, bool esc = true);

		static std::string stringify(const std::string &str, bool esc = true)
		{
			std::string s;
			stringify(str, s, esc);
			return s;
		}

		// dictionary to use
		void setMap(int d) { dict = d; }

		// enable/disable enhanced output with metadata
		void setStringifyEnhanced(bool enhanced) { stringify_enhanced = enhanced; }
		bool getStringifyEnhanced() const { return stringify_enhanced; }
	};

	class StringBuilderArray
	{
	private:
		int dict = 0;

		char *buf = nullptr;
		char *ptr = nullptr;
		char *end = nullptr;

		void append(char c)
		{
			if (ptr < end)
				*ptr++ = c;
		}

		void append(const char *s)
		{
			while (*s && ptr < end)
				*ptr++ = *s++;
		}

		void append(const char *s, int len)
		{
			int avail = end - ptr;
			int n = len < avail ? len : avail;
			memcpy(ptr, s, n);
			ptr += n;
		}

		void append(const std::string &s)
		{
			append(s.data(), s.size());
		}

		void append_uint(unsigned long long v)
		{
			char tmp[20];
			int len = 0;
			do
			{
				tmp[len++] = '0' + (int)(v % 10);
				v /= 10;
			} while (v);
			if (ptr + len <= end)
				for (int i = len - 1; i >= 0; i--)
					*ptr++ = tmp[i];
		}

		void append_int(long int v)
		{
			if (v < 0)
			{
				append('-');
				v = -v;
			}
			append_uint((unsigned long long)v);
		}

		void append_float(double v)
		{
			if (v != v)
			{
				append("null");
				return;
			}

			if (v < 0)
			{
				append('-');
				v = -v;
			}

			long long scaled = (long long)(v * 1000000.0 + 0.5);
			long long whole = scaled / 1000000;
			int frac = (int)(scaled % 1000000);

			append_uint((unsigned long long)whole);

			if (ptr + 7 <= end)
			{
				*ptr++ = '.';
				ptr[5] = '0' + frac % 10;
				frac /= 10;
				ptr[4] = '0' + frac % 10;
				frac /= 10;
				ptr[3] = '0' + frac % 10;
				frac /= 10;
				ptr[2] = '0' + frac % 10;
				frac /= 10;
				ptr[1] = '0' + frac % 10;
				frac /= 10;
				ptr[0] = '0' + frac;
				ptr += 6;
			}
		}

		void append_string_escaped(const std::string &str)
		{
			append('"');
			for (char c : str)
			{
				switch (c)
				{
				case '\"':
					append("\\\"");
					break;
				case '\\':
					append("\\\\");
					break;
				case '\r':
				case '\0':
					break;
				case '\n':
					append("\\n");
					break;
				default:
					append(c);
				}
			}
			append('"');
		}

		void write_value(const Value &v)
		{
			if (v.isString())
			{
				append_string_escaped(v.getString());
			}
			else if (v.isObject())
			{
				write_object(v.getObject());
			}
			else if (v.isArrayString())
			{
				const std::vector<std::string> &as = v.getStringArray();
				append('[');
				for (int i = 0; i < (int)as.size(); i++)
				{
					if (i > 0)
						append(',');
					append_string_escaped(as[i]);
				}
				append(']');
			}
			else if (v.isArray())
			{
				const std::vector<Value> &a = v.getArray();
				append('[');
				for (int i = 0; i < (int)a.size(); i++)
				{
					if (i > 0)
						append(',');
					write_value(a[i]);
				}
				append(']');
			}
			else if (v.isInt())
			{
				append_int(v.getInt());
			}
			else if (v.isFloat())
			{
				append_float(v.getFloat());
			}
			else if (v.isBool())
			{
				append(v.getBool() ? "true" : "false");
			}
			else
			{
				append("null");
			}
		}

		void write_object(const JSON &object)
		{
			bool first = true;
			append('{');
			for (const Property &p : object.getProperties())
			{
				if (p.Key() < 0 || p.Key() >= AIS::KEY_COUNT)
					continue;

				const char* key = AIS::KeyMap[p.Key()][dict];
				if (key[0] == '\0')
					continue;

				if (!first)
					append(',');
				first = false;

				append('"');
				append(key);
				append("\":");
				write_value(p.Get());
			}
			append('}');
		}

	public:
		StringBuilderArray(int d = JSON_DICT_FULL) : dict(d) {}

		int stringify(const JSON &object, char *buffer, int length, const char *suffix = nullptr)
		{
			buf = buffer;
			ptr = buffer;
			end = buffer + length - 1;

			write_object(object);

			if (suffix)
				append(suffix);

			*ptr = '\0';
			return ptr - buf;
		}

		void setMap(int d) { dict = d; }
	};
}
