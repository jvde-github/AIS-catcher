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

#pragma once

#include <vector>
#include <iostream>
#include <memory>

#include "JSON.h"

namespace JSON {

	class Parser {
	private:
		struct Token;

		const std::vector<std::vector<std::string>>* keymap = nullptr;
		int dict = 0;
		bool skipUnknownKeys = false;

		std::string json;
		std::vector<Token> tokens;

		enum class TokenType {
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

		struct Token {
			int pos = 0;
			TokenType type;
			std::string text;

			Token(TokenType t, const std::string& x, int p) {
				pos = p;
				type = t;
				text = x;
			}
		};

		void error(const std::string& err, int pos);

		// tokenizer
		void skip_whitespace(int&);
		void tokenizer();

		// parser
		int idx = 0;

		void error_parser(const std::string& err);
		bool is_match(TokenType t);
		void must_match(TokenType t, const std::string& err);
		int search(const std::string& s);
		void next();
		std::shared_ptr<JSON> parse_core();
		Value parse_value(std::shared_ptr<JSON>);

	public:
		Parser(const std::vector<std::vector<std::string>>* map, int d) : keymap(map), dict(d) {}
		Parser(const std::vector<std::vector<std::string>>* map) : keymap(map) {}

		std::shared_ptr<JSON> parse(const std::string& j);
		void setSkipUnknown(bool b) { skipUnknownKeys = b; }
		// dictionary to use
		void setMap(int d) { dict = d; }
	};
}
