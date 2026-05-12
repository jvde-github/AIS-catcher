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
#include <cstring>
#include <cmath>

#include <cstdlib>

#include "Parser.h"
#include "Keys.h"
#include "SWAR.h"
#include "Convert.h"

// Verification mode: also run the authoritative linear search and assert the
// fast path returns the same value. Turn off for production after validation.
#define VERIFY_KEY_LOOKUP 0

namespace JSON
{
	// Generated from KeyDefs.h — one branch per X() entry whose input dict
	// field is non-empty. The `sizeof(input) > 1` guard lets the compiler
	// dead-code-eliminate the ~470 empty-input branches at compile time.
	// Each remaining memcmp is folded into constant-size integer compares.
	static inline int lookupInputKey(const char *s, int len)
	{
#define X(name, full, min, sparse, aprs, setting, input, unit, desc, lookup) \
	if (sizeof(input) > 1 && sizeof(input) - 1 == (size_t)len &&             \
		memcmp(s, input, sizeof(input) - 1) == 0)                            \
		return AIS::name;
#include "KeyDefs.h"
#undef X
		return -1;
	}

	// Parser -- Build JSON object from String

	[[noreturn]] void Parser::error(const std::string &err, int pos)
	{
		const int char_limit = 40;
		int sz = (int)(pend - p_start);
		int from = MAX(pos - char_limit, 0);
		int to = MIN(pos + char_limit, sz);

		std::stringstream ss;
		for (int i = from; i < to; i++)
		{
			char c = p_start[i];
			char d = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
			ss << d;
		}

		ss << std::endl
		   << std::string(MIN(char_limit, pos - from), ' ') << "^" << std::endl
		   << std::string(MIN(char_limit, pos - from), ' ') << "^" << std::endl;

		throw std::runtime_error("syntax error in JSON: " + err);
	}

	[[noreturn]] void Parser::error(const char *err, int pos)
	{
		error(std::string(err), pos);
	}

	// JIT Tokenizer - lexes one token per call

	void Parser::next()
	{
		skip_whitespace();

		if (p >= pend)
		{
			currentType = TokenType::End;
			tokenStart = tokenEnd = p;
			return;
		}

		char c = *p;

		switch (c)
		{
		case '{':
			currentType = TokenType::LeftBrace;
			p++;
			break;
		case '}':
			currentType = TokenType::RightBrace;
			p++;
			break;
		case '[':
			currentType = TokenType::LeftBracket;
			p++;
			break;
		case ']':
			currentType = TokenType::RightBracket;
			p++;
			break;
		case ':':
			currentType = TokenType::Colon;
			p++;
			break;
		case ',':
			currentType = TokenType::Comma;
			p++;
			break;
		case 't':
			if (p + 4 <= pend && memcmp(p, "true", 4) == 0)
			{
				currentType = TokenType::True;
				tokenStart = p;
				tokenEnd = p + 4;
				p += 4;
			}
			else
				error("illegal identifier", (int)(p - p_start));
			break;
		case 'f':
			if (p + 5 <= pend && memcmp(p, "false", 5) == 0)
			{
				currentType = TokenType::False;
				tokenStart = p;
				tokenEnd = p + 5;
				p += 5;
			}
			else
				error("illegal identifier", (int)(p - p_start));
			break;
		case 'n':
			if (p + 4 <= pend && memcmp(p, "null", 4) == 0)
			{
				currentType = TokenType::Null;
				tokenStart = p;
				tokenEnd = p + 4;
				p += 4;
			}
			else
				error("illegal identifier", (int)(p - p_start));
			break;

		case '\"':
		{
			p++;
			tokenStart = p;

			// Word-sized fast scan: stop on '"' (end of string), '\\' (escape),
			// or any byte < 0x20 (control char, illegal unescaped per JSON spec).
			constexpr size_t m_quote = SWAR::mask('"');
			constexpr size_t m_bslash = SWAR::mask('\\');

			SWAR_SKIP_PTR(p, pend,
				SWAR::has_byte(word, m_quote)
				| SWAR::has_byte(word, m_bslash)
				| SWAR::has_byte_lt(word, 0x20));
			while (p < pend && *p != '\"' && *p != '\\' && (unsigned char)*p >= 0x20)
				p++;

			if (p < pend && *p == '\"')
			{
				tokenEnd = p;
				p++;
			}
			else
			{
				parse_string_escaped(); // handles '\\', control chars, EOF
			}

			currentType = TokenType::String;
			break;
		}

		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		{
			bool floating = false;
			bool scientific = false;
			tokenStart = p;

			do
			{
				if (*p == '.')
				{
					if (floating || tokenStart == p || (unsigned char)(p[-1] - '0') >= 10)
						error("malformed number", (int)(p - p_start));
					else
						floating = true;
				}
				else if ((*p == 'e' || *p == 'E') && !scientific)
				{
					if ((unsigned char)(p[-1] - '0') >= 10 && p[-1] != '.')
						error("malformed number", (int)(p - p_start));

					scientific = floating = true;
					p++;
					if (p != pend && (*p == '+' || *p == '-'))
						p++;

					if (p == pend || (unsigned char)(*p - '0') >= 10)
						error("malformed number", (int)(p - p_start));
				}

				p++;

			} while (p != pend && ((unsigned char)(*p - '0') < 10 || *p == '.' || *p == 'e' || *p == 'E'));

			tokenEnd = p;
			currentType = floating ? TokenType::FloatingPoint : TokenType::Integer;
			break;
		}

		default:
			error("illegal character", (int)(p - p_start));
			break;
		}
	}

	// Cold path: decode a string literal that contains escape sequences.
	// Entered with p pointing at the first '\\' inside the string, tokenStart
	// at the opening-quote+1. On return, escapedText holds the decoded value,
	// p is past the closing quote, and tokenStart/tokenEnd are nulled.
	void Parser::parse_string_escaped()
	{
		escapedText.clear();
		escapedText.append(tokenStart, p - tokenStart);

		while (p != pend && *p != '\"' && *p != '\n' && *p != '\r')
		{
			char ch = *p;
			if (ch == '\\')
			{
				if (++p == pend)
					error("line ends in string literal escape sequence", (int)(p - p_start));
				ch = *p;
				switch (ch)
				{
				case '\"':
				case '\\':
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
					if (p + 4 >= pend)
						error("line ends in string literal unicode escape sequence", (int)(p - p_start));
					int cp = 0;
					for (int i = 1; i <= 4; i++)
					{
						int d = Util::Convert::hexValue(p[i]);
						if (d < 0)
							error("illegal unicode escape sequence", (int)(p - p_start));
						cp = (cp << 4) | d;
					}
					ch = (char)cp;
					p += 4;
					break;
				}
				default:
					error("illegal escape sequence", (int)(p - p_start));
				}
			}
			escapedText += ch;
			p++;
		}

		tokenStart = nullptr;
		tokenEnd = nullptr;

		if (p == pend || *p != '\"')
			error("line ends in string literal", (int)(p - p_start));
		p++;
	}

	// Specialized tokenizer for object-member position: the next token must be
	// a string key or '}'. Skips escape detection — keys never contain escapes
	// in this protocol — so the SWAR scan only watches for the closing quote.
	void Parser::next_key()
	{
		skip_whitespace();

		if (p >= pend)
		{
			currentType = TokenType::End;
			tokenStart = tokenEnd = p;
			return;
		}

		char c = *p;
		if (c == '}')
		{
			currentType = TokenType::RightBrace;
			p++;
			return;
		}
		if (c != '\"')
		{
			next();
			return;
		}

		p++;
		tokenStart = p;
		tokenEscaped = false;

		constexpr size_t m_quote = SWAR::mask('"');
		SWAR_SKIP_PTR(p, pend, SWAR::has_byte(word, m_quote));
		while (p < pend && *p != '\"')
			p++;

		tokenEnd = p;
		if (p == pend)
			error("line ends in string literal", (int)(p - p_start));
		p++;
		currentType = TokenType::String;
	}

	// Parsing functions

	[[noreturn]] void Parser::error_parser(const std::string &err)
	{
		error(err, (int)(tokenStart - p_start));
	}

	std::string Parser::tokenString() const
	{
		if (tokenEscaped)
			return escapedText;
		return std::string(tokenStart, tokenEnd - tokenStart);
	}

	int Parser::search()
	{
		if (dict == JSON_DICT_INPUT)
		{
			const char *str = tokenEscaped ? escapedText.data() : tokenStart;
			int len = tokenEscaped ? (int)escapedText.size() : (int)(tokenEnd - tokenStart);

			int fast = lookupInputKey(str, len);
#if VERIFY_KEY_LOOKUP
			int slow = linearSearch();
			if (fast != slow)
			{
				throw std::runtime_error(
					"key lookup mismatch: fast=" + std::to_string(fast) +
					" slow=" + std::to_string(slow) +
					" key=\"" + std::string(str, (size_t)len) + "\"");
			}
#endif
			return fast;
		}

		return linearSearch();
	}

	void Parser::skip_value()
	{
		switch (currentType)
		{
		case TokenType::LeftBrace:
		{
			next();
			while (is_match(TokenType::String))
			{
				next();
				must_match(TokenType::Colon, "expected ':'");
				next();
				skip_value();
				next();
				if (!is_match(TokenType::Comma))
					break;
				next();
			}
			must_match(TokenType::RightBrace, "expected '}'");
			break;
		}
		case TokenType::LeftBracket:
		{
			next();
			while (!is_match(TokenType::RightBracket))
			{
				skip_value();
				next();
				if (!is_match(TokenType::Comma))
					break;
				next();
			}
			must_match(TokenType::RightBracket, "expected ']'");
			break;
		}
		case TokenType::String:
		case TokenType::Integer:
		case TokenType::FloatingPoint:
		case TokenType::True:
		case TokenType::False:
		case TokenType::Null:
			break;
		default:
			error_parser("unexpected token while skipping value");
			break;
		}
	}

	Value Parser::parse_value(Pool *pool)
	{
		Value v = Value();
		v.setNull();
		switch (currentType)
		{
		case TokenType::Integer:
		{
			int64_t val = 0;
			bool neg = (*tokenStart == '-');
			for (const char *s = tokenStart + neg; s < tokenEnd; s++)
				val = val * 10 + (*s - '0');
			v.setInt(neg ? -val : val);
			break;
		}
		case TokenType::FloatingPoint:
		{
			double val = 0.0;
			const char *s = tokenStart;
			bool neg = (*s == '-');
			if (neg)
				s++;

			int64_t int_part = 0;
			while (s < tokenEnd && *s >= '0' && *s <= '9')
			{
				int_part = int_part * 10 + (*s - '0');
				s++;
			}
			val = (double)int_part;

			if (s < tokenEnd && *s == '.')
			{
				s++;
				double frac = 0.0, div = 1.0;
				while (s < tokenEnd && *s >= '0' && *s <= '9')
				{
					frac = frac * 10 + (*s - '0');
					div *= 10.0;
					s++;
				}
				val += frac * (1.0 / div);
			}

			if (s < tokenEnd && (*s == 'e' || *s == 'E'))
			{
				s++;
				bool eneg = false;
				if (s < tokenEnd && (*s == '+' || *s == '-'))
				{
					eneg = (*s == '-');
					s++;
				}

				int exp = 0;
				while (s < tokenEnd && *s >= '0' && *s <= '9')
				{
					if (exp < 400)
						exp = exp * 10 + (*s - '0');
					s++;
				}

				static const double pow10_table[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15};
				double scale = (exp <= 15) ? pow10_table[exp] : std::pow(10.0, exp);
				if (eneg)
					val /= scale;
				else
					val *= scale;
			}

			v.setFloat(neg ? -val : val);
			break;
		}
		case TokenType::True:
			v.setBool(true);
			break;
		case TokenType::LeftBrace:
		{
			JSON *child = pool->addObject();
			parse_into_core(child, pool);
			v.setObject(child);
			break;
		}
		case TokenType::False:
			v.setBool(false);
			break;
		case TokenType::Null:
			v.setNull();
			break;
		case TokenType::String:
			if (tokenEscaped)
				v.setString(pool->addString(escapedText.data(), escapedText.size()));
			else
				v.setString(pool->addString(tokenStart, tokenEnd - tokenStart));
			break;
		case TokenType::LeftBracket:
		{
			std::vector<Value> *arr = pool->addArray();

			next();

			while (!is_match(TokenType::RightBracket))
			{
				arr->push_back(parse_value(pool));
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

	void Parser::parse_into_core(JSON *o, Pool *pool)
	{
		must_match(TokenType::LeftBrace, "expected '{'");
		next_key();

		while (is_match(TokenType::String))
		{
			int idx = search();
			if (idx < 0 && !skipUnknownKeys)
				error_parser("\"" + tokenString() + "\" is not an allowed \"key\"");

			next();
			must_match(TokenType::Colon, "expected \':\'");
			next();

			if (idx < 0)
				skip_value();
			else
				o->Add(idx, parse_value(pool));

			next();

			if (!is_match(TokenType::Comma))
				break;
			next_key();
			if (!is_match(TokenType::String))
				error_parser("comma needs to be followed by property");
		}

		must_match(TokenType::RightBrace, "expected '}'");
	}

	Document Parser::parse(const std::string &j)
	{
		Document doc;
		p_start = j.data();
		p = p_start;
		pend = p_start + j.size();
		next();
		parse_into_core(&doc.root, &doc.pool);
		next();
		must_match(TokenType::End, "expected END");
		return doc;
	}

	void Parser::parse_into(JSON &target, Pool &pool, const std::string &j)
	{
		p_start = j.data();
		p = p_start;
		pend = p_start + j.size();
		target.clear();
		pool.clear();
		next();
		parse_into_core(&target, &pool);
		next();
		must_match(TokenType::End, "expected END");
	}
}
