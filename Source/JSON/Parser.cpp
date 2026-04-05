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

#include <string>
#include <cmath>

#include "Parser.h"
#include "Parse.h"

namespace JSON
{
	std::unordered_map<size_t, int> Parser::keyLookups[5];
	bool Parser::keyLookupsBuilt[5] = {false, false, false, false, false};

	// Parser -- Build JSON object from String

	void Parser::error(const std::string &err, int pos)
	{
		const int char_limit = 40;
		int from = MAX(pos - char_limit, 0);
		int to = MIN(pos + char_limit, json.size());

		std::stringstream ss;
		for (int i = from; i < to; i++)
		{
			char c = json[i];
			char d = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
			ss << d;
		}
		ss << std::endl
		   << std::string(MIN(char_limit, pos), ' ') << "^" << std::endl
		   << std::string(MIN(char_limit, pos), ' ') << "^" << std::endl;
		throw std::runtime_error("syntax error in JSON: " + err);
	}

	// JIT Tokenizer - lexes one token per call

	void Parser::skip_whitespace()
	{
		while (ptr < (int)json.size() && std::isspace(json[ptr]))
			ptr++;
	}

	void Parser::next()
	{
		skip_whitespace();

		if (ptr >= (int)json.size())
		{
			currentType = TokenType::End;
			tokenStart = tokenEnd = ptr;
			return;
		}

		char c = json[ptr];

		// number
		if (std::isdigit(c) || c == '-')
		{
			bool floating = false;
			bool scientific = false;
			tokenStart = ptr;

			do
			{
				if (json[ptr] == '.')
				{
					if (floating || tokenStart == ptr || !std::isdigit(json[ptr - 1]))
						error("malformed number", ptr);
					else
						floating = true;
				}
				else if ((json[ptr] == 'e' || json[ptr] == 'E') && !scientific)
				{
					if (!std::isdigit(json[ptr - 1]) && json[ptr - 1] != '.')
						error("malformed number", ptr);

					scientific = floating = true;
					ptr++;
					if (ptr != (int)json.size() && (json[ptr] == '+' || json[ptr] == '-'))
						ptr++;

					if (ptr == (int)json.size() || !std::isdigit(json[ptr]))
						error("malformed number", ptr);
				}

				ptr++;

			} while (ptr != (int)json.size() && (std::isdigit(json[ptr]) || json[ptr] == '.' || json[ptr] == 'e' || json[ptr] == 'E'));

			tokenEnd = ptr;
			currentType = floating ? TokenType::FloatingPoint : TokenType::Integer;
		}
		// string
		else if (c == '\"')
		{
			ptr++;
			tokenStart = ptr;
			tokenEscaped = false;

			while (ptr != (int)json.size() && json[ptr] != '\"' && json[ptr] != '\n' && json[ptr] != '\r')
			{
				if (json[ptr] == '\\')
				{
					tokenEscaped = true;
					break;
				}
				ptr++;
			}

			if (!tokenEscaped)
			{
				// fast path - no escapes, just record position
				tokenEnd = ptr;
				if ((int)json.size() == ptr || json[ptr] != '\"')
					error("line ends in string literal", ptr);
				ptr++;
			}
			else
			{
				// slow path - build escaped string
				escapedText.clear();
				escapedText.append(json, tokenStart, ptr - tokenStart);

				while (ptr != (int)json.size() && json[ptr] != '\"' && json[ptr] != '\n' && json[ptr] != '\r')
				{
					char ch = json[ptr];
					if (ch == '\\')
					{
						if (++ptr == (int)json.size())
							error("line ends in string literal escape sequence", ptr);
						ch = json[ptr];
						switch (ch)
						{
						case '\"':
							break;
						case '\\':
							break;
						case '/':
							break;
						case 'b':
							ch = '\b';
							break;
						case 'f':
							ch = '\f';
							break;
						case 'n':
							ch = '\n';
							break;
						case 'r':
							ch = '\r';
							break;
						case 't':
							ch = '\t';
							break;
						case 'u':
						{
							if (ptr + 4 >= (int)json.size())
								error("line ends in string literal unicode escape sequence", ptr);
							std::string hex = json.substr(ptr + 1, 4);
							for (int i = 0; i < 4; i++)
								if (!std::isxdigit(hex[i]))
									error("illegal unicode escape sequence", ptr);
							ch = std::stoi(hex, nullptr, 16);
							ptr += 4;
							break;
						}
						default:
							error("illegal escape sequence " + std::to_string((int)(ch)), ptr);
						}
					}
					escapedText += ch;
					ptr++;
				}

				tokenStart = 0;
				tokenEnd = 0;

				if ((int)json.size() == ptr || json[ptr] != '\"')
					error("line ends in string literal", ptr);
				ptr++;
			}

			currentType = TokenType::String;
		}
		// keyword
		else if (isalpha(c))
		{
			tokenStart = ptr;

			while (ptr != (int)json.size() && isalpha(json[ptr]))
				ptr++;

			tokenEnd = ptr;
			int len = tokenEnd - tokenStart;

			if (len == 4 && json[tokenStart] == 't' && json[tokenStart + 1] == 'r' && json[tokenStart + 2] == 'u' && json[tokenStart + 3] == 'e')
				currentType = TokenType::True;
			else if (len == 5 && json[tokenStart] == 'f' && json[tokenStart + 1] == 'a' && json[tokenStart + 2] == 'l' && json[tokenStart + 3] == 's' && json[tokenStart + 4] == 'e')
				currentType = TokenType::False;
			else if (len == 4 && json[tokenStart] == 'n' && json[tokenStart + 1] == 'u' && json[tokenStart + 2] == 'l' && json[tokenStart + 3] == 'l')
				currentType = TokenType::Null;
			else
				error("illegal identifier : \"" + std::string(json, tokenStart, len) + "\"", ptr);
		}
		// special characters
		else
		{
			switch (c)
			{
			case '{':
				currentType = TokenType::LeftBrace;
				break;
			case '}':
				currentType = TokenType::RightBrace;
				break;
			case '[':
				currentType = TokenType::LeftBracket;
				break;
			case ']':
				currentType = TokenType::RightBracket;
				break;
			case ':':
				currentType = TokenType::Colon;
				break;
			case ',':
				currentType = TokenType::Comma;
				break;
			default:
				error("illegal character '" + std::string(1, c) + "'", ptr);
				break;
			}
			tokenStart = ptr;
			tokenEnd = ptr;
			ptr++;
		}
	}

	// Parsing functions

	void Parser::error_parser(const std::string &err)
	{
		error(err, tokenStart);
	}

	bool Parser::is_match(TokenType t)
	{
		return currentType == t;
	}

	void Parser::must_match(TokenType t, const std::string &err)
	{
		if (currentType != t)
			error_parser(err);
	}

	std::string Parser::tokenString() const
	{
		if (tokenEscaped)
			return escapedText;
		return std::string(json, tokenStart, tokenEnd - tokenStart);
	}

	int Parser::search()
	{
		size_t h;
		if (tokenEscaped)
			h = hashRange(escapedText.data(), escapedText.size());
		else
			h = hashRange(json.data() + tokenStart, tokenEnd - tokenStart);

		auto it = keyLookups[dict].find(h);
		return it != keyLookups[dict].end() ? it->second : -1;
	}

	Value Parser::parse_value(JSON *o)
	{
		Value v = Value();
		v.setNull();
		switch (currentType)
		{
		case TokenType::Integer:
			v.setInt(Util::Parse::Integer(std::string(json, tokenStart, tokenEnd - tokenStart)));
			break;
		case TokenType::FloatingPoint:
			v.setFloat(Util::Parse::Float(std::string(json, tokenStart, tokenEnd - tokenStart)));
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
			v.setString(o->addString(tokenString()));
			break;
		case TokenType::LeftBracket:
		{
			std::vector<Value> *arr = o->addArray();

			next();

			while (!is_match(TokenType::RightBracket))
			{
				arr->push_back(parse_value(o));
				next();
				if (!is_match(TokenType::Comma))
					break;
				next();
				if (is_match(TokenType::RightBracket))
					error_parser("comma cannot be followed by ']'");
			}

			must_match(TokenType::RightBracket, "expected ']'");
			v.setArray(arr);
		}

			break;
		case TokenType::End:
			error_parser("unexpected end of file");

		default:
			error_parser("value parse not implemented");
			break;
		}
		return v;
	}

	std::shared_ptr<JSON> Parser::parse_core()
	{
		std::shared_ptr<JSON> o(new JSON());

		must_match(TokenType::LeftBrace, "expected '{'");
		next();

		while (is_match(TokenType::String))
		{
			int p = search();
			if (p < 0)
			{
				if (!skipUnknownKeys)
					error_parser("\"" + tokenString() + "\" is not an allowed \"key\"");
			}
			next();

			must_match(TokenType::Colon, "expected \':\'");
			next();

			o->Add(p, parse_value(o.get()));
			next();

			if (!is_match(TokenType::Comma))
				break;
			next();
			if (!is_match(TokenType::String))
				error_parser("comma needs to be followed by property");
		}

		must_match(TokenType::RightBrace, "expected '}'");
		return o;
	}

	void Parser::parse_into_core(JSON *o)
	{
		must_match(TokenType::LeftBrace, "expected '{'");
		next();

		while (is_match(TokenType::String))
		{
			int p = search();
			if (p < 0)
			{
				if (!skipUnknownKeys)
					error_parser("\"" + tokenString() + "\" is not an allowed \"key\"");
			}
			next();

			must_match(TokenType::Colon, "expected \':\'");
			next();

			o->Add(p, parse_value(o));
			next();

			if (!is_match(TokenType::Comma))
				break;
			next();
			if (!is_match(TokenType::String))
				error_parser("comma needs to be followed by property");
		}

		must_match(TokenType::RightBrace, "expected '}'");
	}

	std::shared_ptr<JSON> Parser::parse(const std::string &j)
	{
		json = j;
		ptr = 0;
		next();
		auto result = parse_core();
		next();
		must_match(TokenType::End, "expected END");

		return result;
	}

	void Parser::parse_into(JSON &target, const std::string &j)
	{
		json = j;
		ptr = 0;
		target.clear();
		next();
		parse_into_core(&target);
		next();
		must_match(TokenType::End, "expected END");
	}
}
