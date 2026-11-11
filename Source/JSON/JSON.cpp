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

#include "JSON.h"

namespace JSON
{

	void Value::to_string(std::string &str) const
	{
		switch (type)
		{
		case Value::Type::STRING:
			str += *data.s;
			break;
		case Value::Type::CSTRING:
			if (data.cs)
				str += data.cs;
			break;
		case Value::Type::BOOL:
			str += data.b ? "true" : "false";
			break;
		case Value::Type::INT:
			str += std::to_string(data.i);
			break;
		case Value::Type::FLOAT:
			str += std::to_string(data.f);
			break;
		case Value::Type::EMPTY:
			str += "null";
			break;
		case Value::Type::OBJECT:
			str += "object";
			break;
		case Value::Type::ARRAY_STRING:
		case Value::Type::ARRAY:
			str += "array";
			break;
		default:
			str += "error";
			break;
		}
	}

	// one key to a line, so the reader's line numbers mean something
	std::string indented(const std::string &json)
	{
		std::string out;
		out.reserve(json.size() * 2);
		int depth = 0;
		bool in_string = false;
		const auto newline = [&] {
			out += '\n';
			out.append(depth * 2, ' ');
		};
		for (std::size_t i = 0; i < json.size(); i++)
		{
			const char c = json[i];
			if (in_string)
			{
				out += c;
				if (c == '\\' && i + 1 < json.size())
					out += json[++i];
				else if (c == '"')
					in_string = false;
				continue;
			}
			switch (c)
			{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				break;
			case '"':
				in_string = true;
				out += c;
				break;
			case '{':
			case '[':
			{
				out += c;
				std::size_t next = json.find_first_not_of(" \t\r\n", i + 1);
				if (next != std::string::npos && (json[next] == '}' || json[next] == ']'))
				{
					out += json[next];
					i = next;
				}
				else
				{
					depth++;
					newline();
				}
				break;
			}
			case '}':
			case ']':
				depth--;
				newline();
				out += c;
				break;
			case ',':
				out += c;
				newline();
				break;
			case ':':
				out += ": ";
				break;
			default:
				out += c;
			}
		}
		return out;
	}

	std::string text(const Value &v)
	{
		std::string joined;
		if (v.isArray())
			for (const auto &item : v.getArray())
				joined += (joined.empty() ? "" : ",") + item.to_string();
		else if (v.isArrayString())
			for (const auto &item : v.getStringArray())
				joined += (joined.empty() ? "" : ",") + item;
		else
			joined = v.to_string();
		return joined;
	}
}
