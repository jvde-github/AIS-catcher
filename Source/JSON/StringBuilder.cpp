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

#include "StringBuilder.h"
#include "Keys.h"

namespace JSON {

	// StringBuilder - Build string from JSON object

	void StringBuilder::stringify(const std::string& str, std::string& json, bool esc) {
		if (esc) json += '\"';
		for (char c : str) {
			switch (c) {
			case '\"':
				json += "\\\"";
				break;
			case '\\':
				json += "\\\\";
				break;
			case '\r':			
			case '\0':				
				break;
			case '\n':
				json += "\\n";
				break;
			default:
				json += c;
			}
		}
		if (esc) json += '\"';
	}

	void StringBuilder::to_string_enhanced(std::string& json, const Value& v, int key_index) {
		json += '{';
		
		// Add value
		json += "\"value\":";
		to_string(json, v);
		
		// Add metadata if available
		if (key_index >= 0 && key_index < AIS::KeyInfoMap.size()) {
			const AIS::KeyInfo& info = AIS::KeyInfoMap[key_index];
			
			// Add unit if not empty
			if (info.unit && strlen(info.unit) > 0) {
				json += ",\"unit\":";
				stringify(info.unit, json);
			}
			
			// Add description if not empty
			if (info.description && strlen(info.description) > 0) {
				json += ",\"description\":";
				stringify(info.description, json);
			}
			
			// Add lookup value if available
			if (info.lookup_table && (v.isInt() || v.isFloat())) {
				int numeric_value = v.isInt() ? v.getInt() : static_cast<int>(v.getFloat());
				if (numeric_value >= 0 && numeric_value < info.lookup_table->size()) {
					json += ",\"text\":";
					stringify((*info.lookup_table)[numeric_value], json);
				}
			}
		}
		
		json += '}';
	}

	void StringBuilder::to_string(std::string& json, const Value& v) {

		if (v.isString()) {
			stringify(v.getString(), json);
		}
		else if (v.isObject()) {
			stringify(v.getObject(), json);
		}
		else if (v.isArrayString()) {

			const std::vector<std::string>& as = v.getStringArray();

			json += '[';

			if (as.size()) {
				stringify(as[0], json);

				for (int i = 1; i < as.size(); i++) {
					json += ',';
					stringify(as[i], json);
				}
			}

			json += ']';
		}
		else if (v.isArray()) {

			const std::vector<Value>& a = v.getArray();

			json += '[';

			bool first = true;
			for (const auto& val : a) {

				if (!first) json += ',';
				first = false;

				to_string(json, val);
			}

			json += ']';
		}
		else
			v.to_string(json);
	}

	void StringBuilder::stringify(const JSON& object, std::string& json) {
		bool first = true;
		json += '{';
		for (const Property& p : object.getProperties()) {

			// Skip invalid keys to avoid out-of-bounds access
			if (p.Key() < 0 || p.Key() >= keymap->size()) continue;

			const std::string& key = (*keymap)[p.Key()][dict];

			if (!key.empty()) {

				if (!first) json += ',';
				first = false;

				json += "\"" + key + "\":";
				
				if (stringify_enhanced) {
					to_string_enhanced(json, p.Get(), p.Key());
				} else {
					to_string(json, p.Get());
				}
			}
		}
		json += '}';
	}
}
