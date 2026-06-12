/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

	struct RIFFHeader {
		uint32_t groupID;  // "RIFF"
		uint32_t size;
		uint32_t RIFFtype; // "WAVE"
	};

	struct FormatChunk {
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
		struct RIFFHeader riff;
		struct FormatChunk fmt = { 0, 0, 0, 0, 0, 0 };
		struct WaveChunk chunk = { 0, 0 };
		bool fmt_seen = false;

		file.open(filename, std::ios::in | std::ios::binary);
		if(!file) throw std::runtime_error("cannot open WAV file: \"" + filename + "\"");
		file.read((char*)&riff, sizeof(struct RIFFHeader));
		if (!file) throw std::runtime_error("cannot read header from WAV file: \"" + filename + "\"");

		if (riff.groupID != 0x46464952 || riff.RIFFtype != 0x45564157)
			throw std::runtime_error("not a supported WAV-file.");

		// Walk chunks until "data"; "fmt " is not necessarily first - recorders
		// commonly insert LIST/JUNK/bext chunks ahead of it.
		while (file.read((char*)&chunk, sizeof(struct WaveChunk))) {
			if (chunk.ID == 0x20746d66) { // "fmt "
				if (chunk.size < sizeof(struct FormatChunk))
					throw std::runtime_error("invalid fmt chunk in WAV-file.");
				file.read((char*)&fmt, sizeof(struct FormatChunk));
				if (!file) throw std::runtime_error("cannot read fmt chunk from WAV-file.");
				file.ignore(chunk.size - sizeof(struct FormatChunk) + (chunk.size & 1));
				fmt_seen = true;
			}
			else if (chunk.ID == 0x61746164) // "data"
				break;
			else
				file.ignore((std::streamsize)chunk.size + (chunk.size & 1)); // chunks are word-aligned
		}

		if (chunk.ID != 0x61746164 || !fmt_seen)
			throw std::runtime_error("no data in WAV-file.");

		if (fmt.wChannels != 2)
			throw std::runtime_error("WAV-file must have 2 channels (I/Q).");

		if (fmt.wFormatTag == 3 && fmt.wBitsPerSample == 32)
			Device::setFormat(Format::CF32);
		else if (fmt.wFormatTag == 1 && fmt.wBitsPerSample == 8)
			Device::setFormat(Format::CU8);
		else if (fmt.wFormatTag == 1 && fmt.wBitsPerSample == 16)
			Device::setFormat(Format::CS16);
		else
			throw std::runtime_error("format not supported");

		Device::setSampleRate(fmt.dwSamplesPerSec);
	}

	void WAVFile::Close() {
		Device::Close();

		file.close();
	}

	// Unlike other devices, WAVFile has no read thread (isCallback() is false):
	// each isStreaming() poll from the main loop reads and pushes the next block.
	bool WAVFile::isStreaming() {
		if (file.eof() || !Device::isStreaming()) return false;

		if (buffer.size() != buffer_size) buffer.resize(buffer_size);

		file.read((char*)buffer.data(), buffer_size);

		int n = (int)file.gcount();
		if (n <= 0) return false;

		RAW r = { getFormat(), buffer.data(), n };
		Send(&r, 1, tag);

		return true;
	}

	Setting& WAVFile::SetKey(AIS::Keys key, const std::string &arg) {
		switch (key) {
		case AIS::KEY_SETTING_FILE:
			filename = arg;
			break;
		default:
			Device::SetKey(key, arg);
			break;
		}
		return *this;
	}

	std::string WAVFile::Get() {
		return Device::Get() + " file " + filename;
	}
}
