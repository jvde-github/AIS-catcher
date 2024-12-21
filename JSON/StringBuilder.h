/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include "Utilities.h"
#include "Common.h"
#include "JSON.h"

namespace JSON {

	class StringBuilder {
	private:
		const std::vector<std::vector<std::string>>* keymap = nullptr;
		int dict = 0;


	public:
		StringBuilder(const std::vector<std::vector<std::string>>* map, int d) : keymap(map), dict(d) {}
		StringBuilder(const std::vector<std::vector<std::string>>* map) : keymap(map) {}

		void to_string(std::string& json, const Value& v);
		void stringify(const JSON& properties, std::string& json);
		static void stringify(const std::string& str, std::string& json, bool esc = true);
		std::string stringify(const JSON& properties) {
			std::string j;
			stringify(properties, j);
			return j;
		}

		// dictionary to use
		void setMap(int d) { dict = d; }
	};
}
