/*
Copyright(c) 2021-2022 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstring>

#include "FileRAW.h"

namespace Device {

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
			layout = Util::Parse::Switch(arg)?FileLayout::Stereo:FileLayout::Mono;
		}
		else
			throw " Invalid setting for FILE RAW.";
	}

	//---------------------------------------
	// RAW CU8 file

	bool RAWFile::isStreaming()
	{
		if(!Device::isStreaming()) return false;

		int len = 0;

		if (buffer.size() < buffer_size) buffer.resize(buffer_size);
		if (output.size() < buffer_size / sizeof(CU8)) output.resize(buffer_size / sizeof(CU8));

		buffer.assign(buffer.size(), 0);
		file->read((char*)buffer.data(), buffer.size());

		if (layout != FileLayout::Stereo) throw "FILE RAW: layout not implemented.";

		RAW r = { format, buffer.data(), (int)buffer.size() };
		Send(&r, 1);

		if (file->eof()) Stop();

		return true;
	}

	void RAWFile::Close()
	{
		if(file && file != &std::cin) 
		{
			delete file;
			file = NULL;
		}
	}

	void RAWFile::Open(uint64_t h)
	{
		Device::Open(h);

		if(filename == "." || filename == "stdin")
		{
			file = &std::cin;
		}
		else
		{
			file = new std::ifstream(filename, std::ios::out | std::ios::binary);
		}

		if (!file || file->fail()) throw "Error: Cannot read RAW input.";


		setSampleRate(1536000);
	}
}
