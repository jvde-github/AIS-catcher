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

#include "JSON.h"
#include "Keys.h"

namespace JSON
{
	// Open-addressing hash table for key lookup: no heap allocation, no pointer chasing.
	// Power-of-2 sized, linear probing. Built once per dict, shared across all Parsers.
	struct KeyHashTable
	{
		static const int CAPACITY = 1024; // must be power of 2
		static const int MASK = CAPACITY - 1;

		struct Slot
		{
			size_t hash;
			int value; // Keys enum value, or -1 if empty
		};

		Slot slots[CAPACITY];
		bool built;

		KeyHashTable() : built(false)
		{
			for (int i = 0; i < CAPACITY; i++)
				slots[i].value = -1;
		}

		void insert(size_t h, int value)
		{
			int idx = (int)(h & MASK);
			while (slots[idx].value != -1)
			{
				if (slots[idx].hash == h)
					throw std::runtime_error("JSON Parser: hash collision in key lookup");
				idx = (idx + 1) & MASK;
			}
			slots[idx].hash = h;
			slots[idx].value = value;
		}

		int find(size_t h) const
		{
			int idx = (int)(h & MASK);
			while (slots[idx].value != -1)
			{
				if (slots[idx].hash == h)
					return slots[idx].value;
				idx = (idx + 1) & MASK;
			}
			return -1;
		}
	};

	class Parser
	{
	private:
		int dict = 0;
		bool skipUnknownKeys = false;

		std::string json;
		int ptr = 0;

		enum class TokenType
		{
			LeftBrace,
			RightBrace,
			LeftBracket,
			RightBracket,
			Integer,
			FloatingPoint,
			Colon,
			Comma,
			True,
			False,
			Null,
			String,
			End
		};

		TokenType currentType = TokenType::End;
		int tokenStart = 0;
		int tokenEnd = 0;
		bool tokenEscaped = false;
		std::string escapedText;

		static size_t hashRange(const char *data, int len)
		{
			size_t h = 2166136261u;
			for (int i = 0; i < len; i++)
				h = (h ^ (unsigned char)data[i]) * 16777619u;
			return h;
		}

		void error(const std::string &err, int pos);

		// tokenizer
		void skip_whitespace();
		void next();

		// parser
		void error_parser(const std::string &err);
		bool is_match(TokenType t);
		void must_match(TokenType t, const std::string &err);
		int search();
		std::string tokenString() const;
		std::shared_ptr<JSON> parse_core();
		void parse_into_core(JSON *o);
		Value parse_value(JSON *);

		static KeyHashTable keyLookups[5];

	public:
		static void buildKeyLookup(int dict)
		{
			if (dict < 0 || dict >= 5 || keyLookups[dict].built)
				return;

			for (int i = 0; i < AIS::KEY_COUNT; i++)
				if (AIS::KeyMap[i][dict][0] != '\0')
				{
					const char* key = AIS::KeyMap[i][dict];
					keyLookups[dict].insert(hashRange(key, strlen(key)), i);
				}

			keyLookups[dict].built = true;
		}

		Parser(int d = JSON_DICT_FULL) : dict(d) { buildKeyLookup(dict); }

		std::shared_ptr<JSON> parse(const std::string &j);
		void parse_into(JSON &target, const std::string &j);
		void setSkipUnknown(bool b) { skipUnknownKeys = b; }
		void setMap(int d)
		{
			dict = d;
			buildKeyLookup(dict);
		}
	};
}
