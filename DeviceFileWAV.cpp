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
#include "DeviceFileWAV.h"
#include "Common.h"


namespace Device {

	void SettingsWAVFile::Print()
	{
		std::cerr << "WAV file Settings: -gw";
		std::cerr << " file " << file << std::endl;;
	}

	void SettingsWAVFile::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			file = arg;
			return;
		}
		throw " Invalid setting for FILE WAV.";
	}

	//---------------------------------------
	// WAV file, FLOAT only

	struct WAVFileFormat
	{
		uint32_t groupID;
		uint32_t size;
		uint32_t riffType;
		uint32_t chunkID;
		uint32_t chunkSize;
		uint16_t wFormatTag;
		uint16_t wChannels;
		uint32_t dwSamplesPerSec;
		uint32_t dwAvgBytesPerSec;
		uint16_t wBlockAlign;
		uint16_t wBitsPerSample;
		uint32_t dataID;
		uint32_t dataSize;
	};

	bool WAVFile::isStreaming()
	{
		if (buffer.size() != buffer_size) buffer.resize(buffer_size);

		file.read((char*)buffer.data(), buffer_size);

		if (!file) Stop();
		else
		{
			RAW r;
			r.data = buffer.data();
			r.len = file.gcount();
			r.format = Format::CF32;

			Send(&r, 1);
		}

		return streaming;
	}

	void WAVFile::Open(SettingsWAVFile& s)
	{
		filename = s.file;

		struct WAVFileFormat header;

		file.open(filename, std::ios::out | std::ios::binary);
		file.read((char*)&header, sizeof(struct WAVFileFormat));

		if (!file) throw "Error: Cannot read from WAV file.";

		bool valid = true;

		valid &= header.wChannels == 2;
		valid &= header.wFormatTag == 3; // FLOAT

		valid &= header.groupID == 0x46464952;
		valid &= header.riffType == 0x45564157;
		valid &= header.dataID == 0x61746164;

		if (!valid) throw "Eror: Not a supported WAV-file.";

		sample_rate = header.dwSamplesPerSec;
	}

	void WAVFile::Play()
	{
		DeviceBase::Play();
	}

	void WAVFile::Stop()
	{
		file.close();
		DeviceBase::Stop();
	}

	std::vector<uint32_t> WAVFile::SupportedSampleRates()
	{
		return { sample_rate };
	}

}

