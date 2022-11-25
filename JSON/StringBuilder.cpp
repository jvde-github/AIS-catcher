/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include "StringBuilder.h"

namespace JSON {

	// StringBuilder - Build string from JSON object

	void StringBuilder::jsonify(const std::string& str, std::string& json) {
		json += '\"';
		for (char c : str) {
			switch (c) {
			case '\"':
				json += "\\\"";
				break;
			case '\\':
				json += "\\\\";
				break;
			case '\r':
			case '\n':
				break;
			default:
				json += c;
			}
		}
		json += '\"';
	}

	void StringBuilder::to_string(std::string& json, const Value& v, int& idx) {

		bool first;

		switch (v.getType()) {
		case Value::Type::BOOL:
		case Value::Type::INT:
		case Value::Type::FLOAT:
		case Value::Type::EMPTY:
			v.to_string(json);
			break;
		case Value::Type::STRING:
			jsonify(v.getString(), json);
			break;
		case Value::Type::OBJECT:
			build(*v.getObject(), json);
			break;
		case Value::Type::ARRAY_STRING: {

			const std::vector<std::string>& as = v.getStringArray();
			json += '[';

			if (as.size()) {
				jsonify(as[0], json);

				for (int i = 1; i < as.size(); i++) {
					json += ',';
					jsonify(as[i], json);
				}
			}

			json += ']';

		} break;
		case Value::Type::ARRAY: {

			const std::vector<Value>& a = v.getArray();
			json += '[';

			first = true;
			for (const auto val : a) {

				if (!first) json += ',';
				first = false;

				to_string(json, val, idx);
			}

			json += ']';
		} break;
		default:
			std::cerr << "JSON: not implemented!" << std::endl;
		}
	}

	void StringBuilder::build(const JSON& object, std::string& json) {
		bool first = true;
		json += '{';
		int idx = 0;
		for (const Property& p : object.properties) {

			const std::string& key = (*keymap)[p.getKey()][dict];

			if (!key.empty()) {

				if (!first) json += ',';
				first = false;

				json += "\"" + key + "\":";
				to_string(json, p.Get(), idx);
			}
		}
		json += '}';
	}
}
