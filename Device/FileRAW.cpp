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

#include <cstring>

#include "FileRAW.h"

namespace Device {


	//---------------------------------------
	// RAW CU8 file

	void RAWFile::Open(uint64_t h) {
		Device::Open(h);

		if (filename == "." || filename == "stdin") {
			file = &std::cin;
			std::cin.sync_with_stdio(false);
		}
		else {
			file = new std::ifstream(filename, std::ios::in | std::ios::binary);
		}

		if (!file || file->fail()) throw "Error: Cannot read RAW input.";

		setSampleRate(1536000);
	}

	void RAWFile::Close() {
		if (file && file != &std::cin) {
			delete file;
			file = NULL;
		}
	}

	bool RAWFile::isStreaming() {
		if (!file || file->eof() || !Device::isStreaming()) return false;

		if (buffer.size() < buffer_size) buffer.resize(buffer_size);
		buffer.assign(buffer.size(), 0);

		int sz = buffer.size();
		if (format == Format::TXT)
			sz = MIN(sz, file->rdbuf()->in_avail());

		file->read((char*)buffer.data(), sz);

		RAW r = { format, buffer.data(), (int)sz };
		Send(&r, 1, tag);

		return true;
	}

	void RAWFile::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "FILE") {
			filename = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		if (option == "FORMAT") {
			if (!Util::Parse::StreamFormat(arg, format))
				throw "RAW: Unknown file format specification.";
		}
		else
			throw "Invalid setting for FILE RAW.";
	}

	std::string RAWFile::Get() {
		return Device::Get() + " file " + filename + " format " + Util::Convert::toString(format);
	}
}