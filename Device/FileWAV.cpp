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

		file.open(filename, std::ios::in | std::ios::binary);
		file.read((char*)&header, sizeof(struct WAVHeader));

		if (!file) throw "Error: Cannot read from WAV file.";

		// process header and format chunk
		bool valid = true;

		valid &= header.wChannels == 2;
		valid &= header.chunkSize >= 16;

		valid &= header.groupID == 0x46464952;
		valid &= header.RIFFtype == 0x45564157;

		if (!valid) throw "Eror: Not a supported WAV-file.";

		if (header.wFormatTag == 3 && header.wBitsPerSample == 32)
			format = Format::CF32;
		else if (header.wFormatTag == 1 && header.wBitsPerSample == 8)
			format = Format::CU8;
		else if (header.wFormatTag == 1 && header.wBitsPerSample == 16)
			format = Format::CS16;
		else throw "Error: not supported format.";


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

		RAW r = { format, buffer.data(), (int)buffer.size() };
		Send(&r, 1, tag);

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

