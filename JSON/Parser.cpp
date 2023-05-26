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

#include <string>
#include <cmath>

#include "Parser.h"

namespace JSON {

	// Parser -- Build JSON object from String

	void Parser::error(const std::string& err, int pos) {
		const int char_limit = 40;
		int from = MAX(pos - char_limit, 0);
		int to = MIN(pos + char_limit, json.size());

		for (int i = from; i < to; i++) {
			char c = json[i];
			char d = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
			std::cerr << d;
		}
		std::cerr << std::endl
				  << std::string(MIN(char_limit, pos), ' ') << "^" << std::endl
				  << std::string(MIN(char_limit, pos), ' ') << "^" << std::endl;
		throw std::runtime_error("syntax error in JSON: " + err);
	}

	// Lex analysis

	void Parser::skip_whitespace(int& ptr) {
		while (ptr < json.size() && std::isspace(json[ptr])) ptr++;
	}

	void Parser::tokenizer() {

		tokens.clear();
		std::string s;
		int ptr = 0;

		while (ptr < json.size()) {

			skip_whitespace(ptr);
			if (ptr == json.size()) break;

			char c = json[ptr];

			// number
			if (std::isdigit(c) || c == '-') {
				bool floating = false;
				int start_idx = ptr;

				s.clear();

				do {
					if (json[ptr] == '.') {
						if (floating || start_idx == ptr || !std::isdigit(json[ptr - 1]))
							error("malformed number", ptr);
						else
							floating = true;
					}
					s += json[ptr++];

				} while (ptr != json.size() && (std::isdigit(json[ptr]) || json[ptr] == '.'));

				tokens.push_back(Token(floating ? TokenType::FloatingPoint : TokenType::Integer, s, ptr));
			}
			// string
			else if (c == '\"') {
				s.clear();
				ptr++;

				while (ptr != json.size() && json[ptr] != '\"' && json[ptr] != '\n' && json[ptr] != '\r') {
					if (json[ptr] == '\\') {
						if (++ptr == json.size()) error("line ends in string literal escape sequence", ptr);
						if (json[ptr] != '\"') error("escape sequence not supported\\allowed", ptr);
					}
					s += json[ptr++];
				};

				if (json.size() == ptr || json[ptr] != '\"') error("line ends in string literal", ptr);

				tokens.push_back(Token(TokenType::String, s, ptr));
				ptr++;
			}
			// keyword
			else if (isalpha(c)) {

				s.clear();

				while (ptr != json.size() && isalpha(json[ptr])) s += json[ptr++];

				if (s == "true")
					tokens.push_back(Token(TokenType::True, "", ptr));
				else if (s == "false")
					tokens.push_back(Token(TokenType::False, "", ptr));
				else if (s == "null")
					tokens.push_back(Token(TokenType::Null, "", ptr));
				else
					error("illegal identifier : \"" + s + "\"", ptr);
			}
			// special characters
			else {
				switch (c) {
				case '{':
					tokens.push_back(Token(TokenType::LeftBrace, "", ptr));
					break;
				case '}':
					tokens.push_back(Token(TokenType::RightBrace, "", ptr));
					break;
				case '[':
					tokens.push_back(Token(TokenType::LeftBracket, "", ptr));
					break;
				case ']':
					tokens.push_back(Token(TokenType::RightBracket, "", ptr));
					break;
				case ':':
					tokens.push_back(Token(TokenType::Colon, "", ptr));
					break;
				case ',':
					tokens.push_back(Token(TokenType::Comma, "", ptr));
					break;
				default:
					error("illegal character '" + std::string(1, c) + "'", ptr);
					break;
				}
				ptr++;
			}
		}
		tokens.push_back(Token(TokenType::End, "", ptr));
	}

	// Parsing functions

	void Parser::error_parser(const std::string& err) {
		error(err, tokens[MIN(tokens.size() - 1, idx)].pos);
	}

	bool Parser::is_match(TokenType t) {
		if (idx >= tokens.size()) error_parser("unexpected end in input");
		return tokens[idx].type == t;
	}

	void Parser::must_match(TokenType t, const std::string& err) {
		if (!is_match(t)) error_parser(err);
	}

	void Parser::next() {
		idx++;
		if (idx >= tokens.size()) error_parser("unexpected end in input");
	}

	// search for keyword in "map", returns index in map or -1 if not found
	int Parser::search(const std::string& s) {
		int p = -1;
		for (int i = 0; i < keymap->size(); i++)
			if (dict < (*keymap)[i].size() && (*keymap)[i][dict] == s) {
				p = i;
				break;
			}
		return p;
	}

	Value Parser::parse_value(std::shared_ptr<JSON> o) {
		Value v = Value();
		v.setNull();
		switch (tokens[idx].type) {
		case TokenType::Integer:
			v.setInt(Util::Parse::Integer(tokens[idx].text));
			break;
		case TokenType::FloatingPoint:
			v.setFloat(Util::Parse::Float(tokens[idx].text));
			break;
		case TokenType::True:
			v.setBool(true);
			break;
		case TokenType::LeftBrace:
			o->objects.push_back(parse_core());
			v.setObject(o->objects.back().get());
			break;
		case TokenType::False:
			v.setBool(false);
			break;
		case TokenType::Null:
			v.setNull();
			break;
		case TokenType::String:

			o->strings.push_back(std::shared_ptr<std::string>(new std::string(tokens[idx].text)));
			v.setString(o->strings.back().get());

			break;
		case TokenType::LeftBracket:
			o->arrays.push_back(std::shared_ptr<std::vector<Value>>(new std::vector<Value>()));

			next();

			while (!is_match(TokenType::RightBracket)) {
				o->arrays.back()->push_back(parse_value(o));
				next();
				if (!is_match(TokenType::Comma)) break;
				next();
				if (is_match(TokenType::RightBracket))
					error_parser("comma cannot be followed by ']'");
			}

			must_match(TokenType::RightBracket, "expected ']'");
			v.setArray(o->arrays.back().get());

			break;
		case TokenType::End:
			error_parser("unexpected end of file");

		default:
			error_parser("value parse not implemented");
			break;
		}
		return v;
	}

	std::shared_ptr<JSON> Parser::parse_core() {
		std::shared_ptr<JSON> o = std::shared_ptr<JSON>(new JSON(keymap));

		must_match(TokenType::LeftBrace, "expected '{'");
		next();

		while (is_match(TokenType::String)) {
			int p = search(tokens[idx].text);
			if (p < 0) {
				if(!skipUnknownKeys)
					error_parser("\"" + tokens[idx].text + "\" is not an allowed \"key\"");
			}
			next();

			must_match(TokenType::Colon, "expected \':\'");
			next();

			o->Add(p, parse_value(o));
			next();

			if (!is_match(TokenType::Comma)) break;
			next();
			if (!is_match(TokenType::String))
				error_parser("comma needs to be followed by property");
		}

		must_match(TokenType::RightBrace, "expected '}'");
		return o;
	}
	std::shared_ptr<JSON> Parser::parse(const std::string& j) {
		json = j;
		idx = 0;
		tokenizer();
		auto result = parse_core();
		next();
		must_match(TokenType::End, "expected END");

		return result;
	}
}
