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

#include <cstddef>
#include <string>
#include <unordered_map>

namespace WebDB {
    struct FileData {
        const unsigned char* data;
        const size_t size;
        const char* mime_type;
        
        FileData(const unsigned char* d, const size_t s, const char* m) 
            : data(d), size(s), mime_type(m) {}
    };

    extern std::unordered_map<std::string, FileData> files;

}