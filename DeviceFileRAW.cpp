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
		case Device::Format::CF32:
			std::cerr << " CF32";
			break;

		case Device::Format::CS16:
			std::cerr << " CS16";
			break;

		case Device::Format::CU8:
			std::cerr << " CU8";
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
				format = Device::Format::CU8;
			else if (arg == "CF32")
				format = Device::Format::CF32;
			else if (arg == "CS16")
				format = Device::Format::CS16;
			else throw "RAW FILE: Unknown file format specification.";
		}
		else if (option == "STEREO")
		{
			stereo = Util::Parse::Switch(arg);
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

		buffer.assign(buffer_size, 0.0f);
		file.read((char*)buffer.data(), buffer.size());

		if (!stereo) throw "FILE RAW: layout not implemented.";

		switch (format)
		{
		case Format::CU8:

			len = buffer.size() / sizeof(CU8);
			Util::Convert::toFloat((CU8*)buffer.data(), output.data(), len);
			break;

		case Format::CS16:

			len = buffer.size() / sizeof(CS16);
			Util::Convert::toFloat((CS16*)buffer.data(), output.data(), len);
			break;

		case Format::CF32:

			len = buffer.size() / sizeof(CFLOAT32);
			std::memcpy(output.data(), (CFLOAT32*)buffer.data(), buffer.size());
			break;

		default:
			throw "Internal error in DeviceFileRAW: unexpected format";
		}

		Send(output.data(), len);

		if (!file) Stop();

		return streaming;
	}

	void RAWFile::Play()
	{
		file.open(filename, std::ios::out | std::ios::binary);
		if (!file) throw "Error: Cannot read from RAW file.";

		DeviceBase::Play();

	}
	void RAWFile::Stop()
	{
		file.close();

		DeviceBase::Stop();
	}

	std::vector<uint32_t> RAWFile::SupportedSampleRates()
	{
		return { 48000, 96000, 240000, 250000, 288000, 384000, 768000, 1536000, 1920000, 2304000 };
	}

	void RAWFile::setSettings(SettingsRAWFile& s)
	{
		format = s.format;
		filename = s.file;
		stereo = s.stereo;
	}
}
