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

#include "StreamHelpers.h"
#include "Logger.h"
#include "Convert.h"
#include "Parse.h"

namespace Util
{
	void RealPart::Receive(const CFLOAT32 *data, int len, TAG &tag)
	{
		if (output.size() < len)
			output.resize(len);

		for (int i = 0; i < len; i++)
		{
			output[i] = data[i].real();
		}

		Send(output.data(), len, tag);
	}

	void ImaginaryPart::Receive(const CFLOAT32 *data, int len, TAG &tag)
	{
		if (output.size() < len)
			output.resize(len);

		for (int i = 0; i < len; i++)
		{
			output[i] = data[i].imag();
		}

		Send(output.data(), len, tag);
	}

	void ConvertRAW::Receive(const RAW *raw, int len, TAG &tag)
	{
		assert(len == 1);

		// if CU8 connected, silence on CFLOAT32 output
		if (raw->format == Format::CU8 && outCU8.isConnected())
		{
			outCU8.Send((CU8 *)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CS8 && outCS8.isConnected())
		{
			outCS8.Send((CS8 *)raw->data, raw->size / 2, tag);
			return;
		}

		if (raw->format == Format::CF32 && out.isConnected())
		{
			out.Send((CFLOAT32 *)raw->data, raw->size / sizeof(CFLOAT32), tag);
			return;
		}

		if (!out.isConnected())
			return;

		int size = 0;

		switch (raw->format)
		{
		case Format::CU8:

			size = raw->size / sizeof(CU8);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CU8 *)raw->data, output.data(), size);
			break;

		case Format::CS16:

			size = raw->size / sizeof(CS16);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CS16 *)raw->data, output.data(), size);
			break;

		case Format::CS8:

			size = raw->size / sizeof(CS8);
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((CS8 *)raw->data, output.data(), size);
			break;

		case Format::F32_FS4:
			size = raw->size / sizeof(FLOAT32);
			if (output.size() < size)
				output.resize(size);

			for (int i = 0; i < size; i += 4)
			{
				output[i] = CFLOAT32(((FLOAT32 *)raw->data)[i], 0.0f);
				output[i + 1] = CFLOAT32(0.0f, ((FLOAT32 *)raw->data)[i + 1]);
				output[i + 2] = CFLOAT32(-((FLOAT32 *)raw->data)[i + 2], 0.0f);
				output[i + 3] = CFLOAT32(0.0f, -((FLOAT32 *)raw->data)[i + 3]);
			}
			break;

		case Format::DC16H:
			// Each DC16H sample is 3 uint32_t (12 bytes)
			size = raw->size / (3 * sizeof(uint32_t));
			if (output.size() < size)
				output.resize(size);
			Util::Convert::toFloat((DC16H *)raw->data, output.data(), size);
			break;

		default:
			return;
		}

		out.Send(output.data(), size, tag);
	}

	void WriteWAV::Open(const std::string &filename, int sample_rate)
	{
		if (stopping)
			return;

		try
		{
			file.open(filename, std::ios::binary);
			if (!file.is_open())
			{
				throw std::runtime_error("Cannot open WAV file for writing: \"" + filename + "\"");
			}

			header.sample_rate_val = sample_rate;

			switch (format)
			{
			case Format::CU8:
			case Format::CS8:
				header.bit_depth = 8;
				break;
			case Format::CS16:
				header.bit_depth = 16;
				break;
			case Format::CF32:
				header.bit_depth = 32;
				header.audio_format = 3;
				break;
			default:
				throw std::runtime_error("Unsupported format for WAV file writing");
			}

			header.byte_rate = sample_rate * 2 * (header.bit_depth / 8);
			header.sample_alignment = 2 * (header.bit_depth / 8);

			// Write the header
			file.write(reinterpret_cast<const char *>(&header), sizeof(header));
		}
		catch (const std::exception &e)
		{
			Error() << "WAV out: " << e.what() << std::endl;
			stopping = true;
			StopRequest();
		}
	}

	WriteWAV::~WriteWAV()
	{
		if (file.is_open())
		{
			// Get current position
			std::streampos pos = file.tellp();
			std::streamoff data_size = pos - std::streamoff(sizeof(WAVHeader));
			uint32_t wav_size = data_size + 36; // Total size - 8

			// Update RIFF chunk size
			file.seekp(4);
			file.write((const char *)(&wav_size), 4);

			// Update data chunk size
			file.seekp(40);
			uint32_t data_chunk_size = (uint32_t)(data_size);
			file.write((char *)(&data_chunk_size), 4);

			file.close();
		}
	}

	void WriteWAV::Receive(const RAW *raw, int len, TAG &tag)
	{
		if (stopping)
			return;

		format = raw->format;

		if (!file.is_open())
		{
			Open(filename, sample_rate);
		}

		if (!file.is_open() || stopping)
			return;

		// Write the data directly
		try
		{
			file.write(reinterpret_cast<const char *>(raw->data), raw->size);
		}
		catch (const std::exception &e)
		{
			Error() << "WAV out: " << e.what() << std::endl;
			stopping = true;
			StopRequest();
		}
	}

	bool WriteWAV::setValue(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "FILE")
		{
			filename = arg;
			return true;
		}
		else if (option == "RATE")
		{
			sample_rate = Util::Parse::Integer(arg, 0, 1000000000, "RATE");
			return true;
		}
		return false;
	}
}
