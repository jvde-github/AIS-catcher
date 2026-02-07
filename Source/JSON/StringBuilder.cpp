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

	// StringBuilder - Build string from JSON object using JSONBuilder for efficiency

	void StringBuilder::to_string_enhanced_internal(const Value& v, int key_index) {
		builder.start();
		
		// Add value
		builder.key("value");
		to_string_internal(v);
		
		// Add metadata if available
		if (key_index >= 0 && key_index < AIS::KeyInfoMap.size()) {
			const AIS::KeyInfo& info = AIS::KeyInfoMap[key_index];
			
			// Add unit if not empty
			if (info.unit && strlen(info.unit) > 0) {
				builder.add("unit", info.unit);
			}
			
			// Add description if not empty
			if (info.description && strlen(info.description) > 0) {
				builder.add("description", info.description);
			}
			
			// Add lookup value if available
			if (info.lookup_table && (v.isInt() || v.isFloat())) {
				int numeric_value = v.isInt() ? v.getInt() : static_cast<int>(v.getFloat());
				if (numeric_value >= 0 && numeric_value < info.lookup_table->size()) {
					builder.add("text", (*info.lookup_table)[numeric_value]);
				}
			}
		}
		
		builder.end();
	}

	void StringBuilder::to_string_internal(const Value& v) {

		if (v.isString()) {
			builder.value(v.getString());
		}
		else if (v.isObject()) {
			stringify_internal(v.getObject());
		}
		else if (v.isArrayString()) {

			const std::vector<std::string>& as = v.getStringArray();
			builder.startArray();
			for (const auto& s : as) {
				builder.value(s);
			}
			builder.endArray();
		}
		else if (v.isArray()) {

			const std::vector<Value>& a = v.getArray();
			builder.startArray();
			for (const auto& val : a) {
				to_string_internal(val);
			}
			builder.endArray();
		}
		else if (v.isInt()) {
			builder.value(v.getInt());
		}
		else if (v.isFloat()) {
			builder.value(v.getFloat());
		}
		else if (v.isBool()) {
			builder.value(v.getBool());
		}
		else {
			builder.valueNull();
		}
	}
	
	void StringBuilder::to_string(std::string& json, const Value& v) {
		builder.clear();
		to_string_internal(v);
		json += builder.str();
	}

	void StringBuilder::stringify_internal(const JSON& object) {
		builder.start();
		for (const Property& p : object.getProperties()) {

			// Skip invalid keys to avoid out-of-bounds access
			if (p.Key() < 0 || p.Key() >= keymap->size()) continue;

			const std::string& key = (*keymap)[p.Key()][dict];

			if (!key.empty()) {
				builder.key(key);
				
				if (stringify_enhanced) {
					to_string_enhanced_internal(p.Get(), p.Key());
				} else {
					to_string_internal(p.Get());
				}
			}
		}
		builder.end();
	}
	
	void StringBuilder::stringify(const JSON& object, std::string& json) {
		builder.clear();
		stringify_internal(object);
		json += builder.str();
	}
}
