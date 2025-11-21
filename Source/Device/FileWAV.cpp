/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

	struct WAVHeader {

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

	struct WaveChunk {
		uint32_t ID;
		uint32_t size;
	};

	void WAVFile::Open(uint64_t h) {
		struct WAVHeader header;
		struct WaveChunk chunk = { 0, 0 };

		file.open(filename, std::ios::in | std::ios::binary);
		if(!file) throw std::runtime_error("cannot open WAV file: \"" + filename + "\"");	
		file.read((char*)&header, sizeof(struct WAVHeader));
		if (!file) throw std::runtime_error("cannot read header from WAV file: \"" + filename + "\"");

		// process header and format chunk
		bool valid = true;

		valid &= header.wChannels == 2;
		valid &= header.chunkSize >= 16;

		valid &= header.groupID == 0x46464952;
		valid &= header.RIFFtype == 0x45564157;

		if (!valid) throw std::runtime_error("not a supported WAV-file.");

		if (header.wFormatTag == 3 && header.wBitsPerSample == 32)
			Device::setFormat(Format::CF32);
		else if (header.wFormatTag == 1 && header.wBitsPerSample == 8)
			Device::setFormat(Format::CU8);
		else if (header.wFormatTag == 1 && header.wBitsPerSample == 16)
			Device::setFormat(Format::CS16);
		else
			throw std::runtime_error("format not supported");


		file.ignore(header.chunkSize - 16);

		// process wave chunks until start of data
		while (!file.eof()) {
			file.read((char*)&chunk, sizeof(struct WaveChunk));
			if (chunk.ID == 0x74636166 || chunk.ID == 0x69787561)
				file.ignore(chunk.size);
			else if (chunk.ID == 0x61746164)
				break;
			else
				throw std::runtime_error("unrecognized chunk in WAV-file.");
		}

		if (chunk.ID != 0x61746164) throw std::runtime_error("no Data in WAV-file.");

		Device::setSampleRate(header.dwSamplesPerSec);
	}

	void WAVFile::Close() {
		Device::Close();

		file.close();
	}

	bool WAVFile::isStreaming() {
		if (file.eof() || !Device::isStreaming()) return false;

		if (buffer.size() != buffer_size) buffer.resize(buffer_size);

		buffer.assign(buffer_size, 0);
		file.read((char*)buffer.data(), buffer_size);

		RAW r = { getFormat(), buffer.data(), (int)buffer.size() };
		Send(&r, 1, tag);

		return true;
	}

	Setting& WAVFile::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "FILE")
			filename = arg;
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string WAVFile::Get() {
		return Device::Get() + " file " + filename;
	}
}
