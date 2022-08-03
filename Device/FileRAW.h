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

#pragma once

#include "Device.h"

namespace Device {

	enum class FileLayout { Stereo,
							Mono,
							Left,
							Right };

	class RAWFile : public Device {
		std::istream* file = NULL;

		std::string filename;
		std::vector<char> buffer;

		const int buffer_size = 16 * 16384;

		FileLayout layout = FileLayout::Stereo;
		Format format = Format::CU8;

	public:
		// Control
		void Open(uint64_t);
		void Close();

		bool isCallback() { return false; }
		bool isStreaming();

		// Device specific
		void setFormat(Format f) { format = f; }

		void Print();
		void Set(std::string option, std::string arg);
	};
}
