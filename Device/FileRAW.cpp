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

	void RAWFile::Open(uint64_t h)
	{
		Device::Open(h);

		if (filename == "." || filename == "stdin")
		{
			file = &std::cin;
		}
		else
		{
			file = new std::ifstream(filename, std::ios::in | std::ios::binary);
		}

		if (!file || file->fail()) throw "Error: Cannot read RAW input.";

		setSampleRate(1536000);
	}

	void RAWFile::Close()
	{
		if (file && file != &std::cin)
		{
			delete file;
			file = NULL;
		}
	}

	bool RAWFile::isStreaming()
	{
		if(!file || file->eof()) {
			// if EOF, flush the buffer
			buffer.assign(buffer.size(), 0);
			RAW r = { format, buffer.data(), (int)buffer.size() };
			Send(&r, 1);
		}
		if(!file || file->eof() || !Device::isStreaming()) return false;

		if (buffer.size() < buffer_size) buffer.resize(buffer_size);
		buffer.assign(buffer.size(), 0);
		file->read((char*)buffer.data(), buffer.size());

		if (layout != FileLayout::Stereo) throw "FILE RAW: layout not implemented.";

		RAW r = { format, buffer.data(), (int)buffer.size() };
		Send(&r, 1);

		return true;
	}

	void RAWFile::Print()
	{
		std::cerr << "RAW file Settings: -ga";

		switch (format)
		{
		case Format::CF32: std::cerr << " CF32"; break;
		case Format::CS16: std::cerr << " CS16"; break;
		case Format::CU8:  std::cerr << " CU8"; break;
		case Format::CS8:  std::cerr << " CS8"; break;
		default: break;
		}
		std::cerr << " file " << filename << std::endl;
	}

	void RAWFile::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			filename = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		if (option == "FORMAT")
		{
			if (!Util::Parse::StreamFormat(arg, format))
				throw "RAW: Unknown file format specification.";
		}
		else if (option == "STEREO")
		{
			layout = Util::Parse::Switch(arg) ? FileLayout::Stereo : FileLayout::Mono;
		}
		else
			throw " Invalid setting for FILE RAW.";
	}
}
