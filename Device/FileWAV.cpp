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

#include "FileWAV.h"

namespace Device {

	//---------------------------------------
	// WAV file, FLOAT only
	//
	// Source: https://www.recordingblogs.com/wiki/wave-file-format

	struct WAVHeader
	{
		
		uint32_t groupID;
		uint32_t size;
		uint32_t RIFFtype;

		// Format chunk
		uint32_t chunkID;
		uint32_t chunkSize;
		uint16_t wFormatTag;
		uint16_t wChannels;
		uint32_t dwSamplesPerSec;
		uint32_t dwAvgBytesPerSec;
		uint16_t wBlockAlign;
		uint16_t wBitsPerSample;
	};

	struct WaveChunk
	{
		uint32_t ID;
		uint32_t size;
	};

	void WAVFile::Open(uint64_t h)
	{
		struct WAVHeader header;
		struct WaveChunk chunk = { 0, 0 };

		file.open(filename, std::ios::out | std::ios::binary);
		file.read((char*)&header, sizeof(struct WAVHeader));

		if (!file) throw "Error: Cannot read from WAV file.";

		// process header and format chunk
		bool valid = true;

		valid &= header.wChannels == 2;
		valid &= header.wFormatTag == 3; // FLOAT
		valid &= header.chunkSize >= 16;

		valid &= header.groupID == 0x46464952;
		valid &= header.RIFFtype == 0x45564157;

		if (!valid) throw "Eror: Not a supported WAV-file.";
		file.ignore(header.chunkSize - 16);

		// process wave chunks until start of data
		while(!file.eof())
		{
			file.read((char*)&chunk, sizeof(struct WaveChunk));
			if (chunk.ID == 0x74636166)
				file.ignore(chunk.size);
			else if (chunk.ID == 0x61746164) break;
			else throw "Error: unrecognized chunk in WAV-file.";

		};
		
		if (chunk.ID != 0x61746164) throw "Error: no Data in WAV-file.";

		Device::setSampleRate(header.dwSamplesPerSec);
	}

	void WAVFile::Close()
	{
		Device::Close();

		file.close();
	}

	bool WAVFile::isStreaming()
	{
		if(file.eof() || !Device::isStreaming()) return false;

		if (buffer.size() != buffer_size) buffer.resize(buffer_size);

		buffer.assign(buffer_size, 0);
		file.read((char*)buffer.data(), buffer_size);

		RAW r = { Format::CF32 , buffer.data(), (int)buffer.size() };
		Send(&r, 1);

		return true;
	}

	void WAVFile::Print()
	{
		std::cerr << "WAV file Settings: -gw";
		std::cerr << " file " << filename << std::endl;;
	}

	void WAVFile::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			filename = arg;
			return;
		}
		throw " Invalid setting for FILE WAV.";
	}

}

