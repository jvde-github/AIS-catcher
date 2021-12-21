/*
Copyright(c) 2021 jvde.github@gmail.com

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

#include "Device.h"
#include "DeviceFileRAW.h"
#include "Common.h"
#include "Utilities.h"

namespace Device {

	void SettingsRAWFile::Print()
	{
		std::cerr << "RAW file Settings: -ga";

		switch (format)
		{
		case Format::CF32:
			std::cerr << " CF32";
			break;

		case Format::CS16:
			std::cerr << " CS16";
			break;

		case Format::CU8:
			std::cerr << " CU8";
			break;

		case Format::CS8:
			std::cerr << " CS8";
			break;

		default:
			break;
		}
		std::cerr << " file " << file << std::endl;;
	}

	void SettingsRAWFile::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			file = arg;
			return;
		}

		Util::Convert::toUpper(arg);

		if (option == "FORMAT")
		{
			if (arg == "CU8")
				format = Format::CU8;
			else if (arg == "CF32")
				format = Format::CF32;
			else if (arg == "CS16")
				format = Format::CS16;
			else if (arg == "CS8")
				format = Format::CS8;
			else throw "RAW FILE: Unknown file format specification.";
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
		int len = 0;

		if (buffer.size() < buffer_size) buffer.resize(buffer_size);
		if (output.size() < buffer_size / sizeof(CU8)) output.resize(buffer_size / sizeof(CU8));

		buffer.assign(buffer_size, 0);
		file->read((char*)buffer.data(), buffer.size());

		if (layout != FileLayout::Stereo) throw "FILE RAW: layout not implemented.";

		RAW r = { format, buffer.data(), buffer.size() };
		Send(&r, 1);

		if (file->eof()) Stop();

		return streaming;
	}

	void RAWFile::Close()
	{
		if(file && file != &std::cin) 
		{
			delete file;
			file = NULL;
		}
	}

	void RAWFile::Play()
	{
		if(filename == "." || filename == "stdin")
		{
			file = &std::cin;
		}
		else
		{
			file = new std::ifstream(filename, std::ios::out | std::ios::binary);
		}

		if (!file || file->fail()) throw "Error: Cannot read RAW input.";

		DeviceBase::Play();

	}

	void RAWFile::Stop()
	{
		DeviceBase::Stop();
	}

	std::vector<uint32_t> RAWFile::SupportedSampleRates()
	{
		return { 1536000, 48000, 96000, 192000, 240000, 250000, 288000, 384000, 768000, 1920000, 2000000, 2304000, 2500000, 3000000, 6144000, 6000000, 12288000, 10000000 };
	}

	void RAWFile::Open(SettingsRAWFile& s)
	{
		format = s.format;
		filename = s.file;
		layout = s.layout;
	}
}
