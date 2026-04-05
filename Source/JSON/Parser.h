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
#include <unordered_map>

#include "JSON.h"

namespace JSON {

	class Parser {
	private:
		const std::vector<std::vector<std::string>>* keymap = nullptr;
		int dict = 0;
		bool skipUnknownKeys = false;

		std::string json;
		int ptr = 0;

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

		TokenType currentType = TokenType::End;
		std::string currentText;
		int currentPos = 0;

		void error(const std::string& err, int pos);

		// tokenizer
		void skip_whitespace();
		void next();

		// parser
		void error_parser(const std::string& err);
		bool is_match(TokenType t);
		void must_match(TokenType t, const std::string& err);
		int search(const std::string& s);
		std::shared_ptr<JSON> parse_core();
		Value parse_value(std::shared_ptr<JSON>);

		static std::unordered_map<std::string, int> keyLookups[5];
		static bool keyLookupsBuilt[5];

	public:
		static void buildKeyLookup(const std::vector<std::vector<std::string>>* keymap, int dict)
		{
			if (dict < 0 || dict >= 5 || keyLookupsBuilt[dict]) return;
			if (keymap)
			{
				for (int i = 0; i < (int)keymap->size(); i++)
					if (dict < (int)(*keymap)[i].size() && !(*keymap)[i][dict].empty())
						keyLookups[dict][(*keymap)[i][dict]] = i;
			}
			keyLookupsBuilt[dict] = true;
		}

		Parser(const std::vector<std::vector<std::string>>* map, int d) : keymap(map), dict(d) { buildKeyLookup(keymap, dict); }
		Parser(const std::vector<std::vector<std::string>>* map) : keymap(map) { buildKeyLookup(keymap, dict); }

		std::shared_ptr<JSON> parse(const std::string& j);
		void setSkipUnknown(bool b) { skipUnknownKeys = b; }
		// dictionary to use
		void setMap(int d) { dict = d; buildKeyLookup(keymap, dict); }
	};
}
