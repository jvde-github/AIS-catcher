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

	//---------------------------------------
	// RAW CU8 file

	bool RAWFile::isStreaming()
	{
		int len = 0;

		if (buffer.size() < buffer_size) buffer.resize(buffer_size);
		if (output.size() < buffer_size / sizeof(CU8)) output.resize(buffer_size / sizeof(CU8));

		buffer.assign(buffer_size, 0.0f);
		file.read((char*)buffer.data(), buffer.size());
				
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

	void RAWFile::openFile(std::string filename)
	{
		file.open(filename, std::ios::out | std::ios::binary);
		if (!file) throw "Error: Cannot read from RAW file.";
	}

	std::vector<uint32_t> RAWFile::SupportedSampleRates()
	{
		return { 48000, 288000, 384000, 768000, 1536000, 1920000, 2304000 };
	}
}
