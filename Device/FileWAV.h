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

namespace Device{

	class WAVFile : public Device
	{
		std::ifstream file;
		std::string filename;

		std::vector<uint8_t> buffer;
		const int buffer_size = 16 * 16384;

		Format format = Format::CF32;

	public:

		// Control
		void Close();
		void Open(uint64_t);
		bool isCallback() { return false; }
		bool isStreaming();

		// Settings
		void Print();
		void Set(std::string option, std::string arg);
	};
}
