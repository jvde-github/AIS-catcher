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

#pragma once

#include <chrono>

#include <vector>
#include <fstream>
#include <string>
#include <cassert>

#include "Common.h"
#include "Stream.h"
#include "Keys.h"

namespace Util
{
	class RealPart : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector<FLOAT32> output;

	public:
		virtual ~RealPart() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class ImaginaryPart : public SimpleStreamInOut<CFLOAT32, FLOAT32>
	{
		std::vector<FLOAT32> output;

	public:
		virtual ~ImaginaryPart() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	template <typename T>
	class PassThrough : public SimpleStreamInOut<T, T>
	{
	public:
		virtual ~PassThrough() {}
		virtual void Receive(const T *data, int len, TAG &tag) { SimpleStreamInOut<T, T>::Send(data, len, tag); }
		virtual void Receive(T *data, int len, TAG &tag) { SimpleStreamInOut<T, T>::Send(data, len, tag); }
	};

	template <typename T>
	class Timer : public SimpleStreamInOut<T, T>
	{
		std::chrono::high_resolution_clock::time_point time_start;
		float timing = 0.0;

		void tic()
		{
			time_start = std::chrono::high_resolution_clock::now();
		}

		void toc()
		{
			timing += 1e-3f * std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - time_start).count();
		}

	public:
		virtual ~Timer() {}
		virtual void Receive(const T *data, int len, TAG &tag)
		{
			tic();
			SimpleStreamInOut<T, T>::Send(data, len, tag);
			toc();
		}
		virtual void Receive(T *data, int len, TAG &tag)
		{
			tic();
			SimpleStreamInOut<T, T>::Send(data, len, tag);
			toc();
		}

		float getTotalTiming() { return timing; }
	};

	class ConvertToRAW : public SimpleStreamInOut<CFLOAT32, RAW>
	{
	public:
		void Receive(const CFLOAT32 *data, int len, TAG &tag)
		{
			if (!out.isConnected())
				return;

			RAW rawOutput;
			rawOutput.format = Format::CF32;
			rawOutput.data = (void *)data;
			rawOutput.size = len * sizeof(CFLOAT32);

			Send(&rawOutput, 1, tag);
		}
	};

	class ConvertRAW : public SimpleStreamInOut<RAW, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

	public:
		virtual ~ConvertRAW() {}
		Connection<CU8> outCU8;
		Connection<CS8> outCS8;

		void Receive(const RAW *raw, int len, TAG &tag);
	};

	class WriteWAV : public StreamIn<RAW>
	{
		struct WAVHeader
		{
			// RIFF Header
			char riff_header[4] = {'R', 'I', 'F', 'F'};
			uint32_t wav_size = 0;
			char wave_header[4] = {'W', 'A', 'V', 'E'};

			// Format Chunk
			char fmt_header[4] = {'f', 'm', 't', ' '};
			uint32_t fmt_chunk_size = 16;
			uint16_t audio_format = 1;
			uint16_t num_channels = 2;
			uint32_t sample_rate_val;
			uint32_t byte_rate;
			uint16_t sample_alignment;
			uint16_t bit_depth;

			// Data Chunk
			char data_header[4] = {'d', 'a', 't', 'a'};
			uint32_t data_chunk_size = 0;
		} header;

		std::vector<CFLOAT32> output;
		std::ofstream file;
		std::string filename;
		Format format;
		int sample_rate = -1;
		bool stopping = false;

		void Open(const std::string &filename, int sample_rate);

	public:
		virtual ~WriteWAV();
		void Receive(const RAW *raw, int len, TAG &tag);

		bool setOptionKey(AIS::Keys key, const std::string &arg);
	};
}
