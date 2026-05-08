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
#include <cstdint>

#include "JSON.h"
#include "Keys.h"

namespace JSON
{
	class Parser
	{
	private:
		int dict = 0;
		bool skipUnknownKeys = false;

		const char *p_start = nullptr;
		const char *p = nullptr;
		const char *pend = nullptr;

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
		const char *tokenStart = nullptr;
		const char *tokenEnd = nullptr;
		bool tokenEscaped = false;
		std::string escapedText;

		[[noreturn]] void error(const std::string &err, int pos);
		[[noreturn]] AISC_COLD_NOINLINE void error(const char *err, int pos);

		// tokenizer
		void skip_whitespace()
		{
			while (p < pend && (unsigned char)*p <= ' ')
				p++;
		}
		void next();
		void next_key();
		AISC_COLD_NOINLINE void parse_string_escaped();

		// parser
		[[noreturn]] void error_parser(const std::string &err);
		bool is_match(TokenType t) { return currentType == t; }
		void must_match(TokenType t, const std::string &err)
		{
			if (currentType != t)
				error_parser(err);
		}
		int search();
		std::string tokenString() const;
		void parse_into_core(JSON *o, Pool *pool);
		Value parse_value(Pool *pool);
		void skip_value();

	public:
		int linearSearch() const
		{
			const char *str = tokenEscaped ? escapedText.data() : tokenStart;
			int slen = tokenEscaped ? (int)escapedText.size() : (int)(tokenEnd - tokenStart);

			for (int i = 0; i < AIS::KEY_COUNT; i++)
			{
				const AIS::KeyStr &key = AIS::KeyMap[i][dict];
				if (!key.empty() && (int)key.n == slen && memcmp(key.p, str, slen) == 0)
					return i;
			}
			return -1;
		}

		void parse_into(JSON &target, Pool &pool, const std::string &j);

	public:
		Parser(int d = JSON_DICT_FULL) : dict(d) {}

		Document parse(const std::string &j);
		void parse_into(Document &doc, const std::string &j) { parse_into(doc.root, doc.pool, j); }
		void setSkipUnknown(bool b) { skipUnknownKeys = b; }
		void setMap(int d)
		{
			dict = d;
		}
	};
}
