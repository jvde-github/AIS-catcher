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

#pragma once

#include <assert.h>
#ifdef HASSOXR
#include <soxr.h>
#endif
#ifdef HASSAMPLERATE
#include <samplerate.h>
#endif
#include "Filters.h"

#include "Stream.h"
#include "Signals.h"

namespace DSP
{
	class SimplePLL : public SimpleStreamInOut<FLOAT32, FLOAT32>, public SignalIn<DecoderSignals>
	{
		BIT prev = 0;

		float PLL = 0.0f;
		bool FastPLL = true;

	public:
		virtual ~SimplePLL() {}
		// StreamIn
		virtual void Receive(const FLOAT32 *data, int len, TAG &tag);

		// MessageIn
		virtual void Signal(const DecoderSignals &in);
	};

	template <typename T>
	class Deinterleave : public StreamIn<T>
	{
		int lastSymbol = 0;
		long sample_idx = 0;

	public:
		virtual ~Deinterleave() {}
		void setConnections(int n) { out.resize(n); }

		// Streams out
		std::vector<Connection<T>> out;

		// Streams in
		void Receive(const T *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
			{
				tag.sample_idx = sample_idx++;
				out[lastSymbol].Send(&data[i], 1, tag);
				lastSymbol = (lastSymbol + 1) % out.size();
			}
		}
	};

	class ScatterPLL : public StreamIn<CFLOAT32>
	{
		int lastSymbol = 0;
		std::vector<CFLOAT32> sample;
		FLOAT32 level = 0.0f;
		long sample_idx = 0;

	public:
		virtual ~ScatterPLL() {}
		void setConnections(int n)
		{
			out.resize(n);
			sample.resize(n);
		}

		// Streams out
		std::vector<Connection<CFLOAT32>> out;

		// Streams in
		void Receive(const CFLOAT32 *data, int len, TAG &tag)
		{
			for (int i = 0; i < len; i++)
			{
				sample[lastSymbol] = data[i];
				if (tag.mode & 1)
					level += std::norm(data[i]);

				if (++lastSymbol == out.size())
				{
					if (tag.mode & 1)
						tag.sample_lvl = level / out.size();

					for (int j = 0; j < out.size(); j++)
					{
						tag.sample_idx = sample_idx++;
						out[j].Send(&sample[j], 1, tag);
					}
					level = 0.0f;
					lastSymbol = 0;
				}
			}
		}
	};

	class DownsampleMovingAverage : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		CFLOAT32 D;
		int idx_out = 0;
		int idx_in = 0;
		int df = 0;
		int in_rate = 0;
		int out_rate = 0;
		const int BLOCK_SIZE = 8192;

		std::vector<CFLOAT32> output;

	public:
		virtual ~DownsampleMovingAverage() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
		void setRates(int i, int o)
		{
			in_rate = i;
			out_rate = o;
		}
	};

	class Downsample2CIC5 : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		CFLOAT32 h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;
		std::vector<CFLOAT32> output;

	public:
		virtual ~Downsample2CIC5() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class Decimate2 : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

	public:
		virtual ~Decimate2() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class Upsample : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

		FLOAT32 alpha = 0, increment = 1.0f;
		CFLOAT32 a = 0.0f;

		int idx_out = 0;

	public:
		virtual ~Upsample() {}
		void setParams(int n, int m)
		{
			assert(n <= m);
			increment = (FLOAT32)n / (FLOAT32)m;
		}
		// StreamIn
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class DownsampleKFilter : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

		std::vector<CFLOAT32> buffer;
		std::vector<FLOAT32> taps;

		int idx_in = 0;
		int idx_out = 0;

		int K = 1;

		static const int outputSize = 16384 / 2;

		inline CFLOAT32 dot(const CFLOAT32 *data)
		{
			CFLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++)
				x += taps[i] * *data++;
			return x;
		}

	public:
		virtual ~DownsampleKFilter() {}
		void setParams(const std::vector<FLOAT32> &t, int k)
		{
			taps = t;
			K = k;
		}
		void setTaps(const std::vector<FLOAT32> &t) { taps = t; }
		void setK(int k) { K = k; }

		// StreamIn
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class FilterComplex : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

		std::vector<CFLOAT32> buffer;
		std::vector<FLOAT32> taps;

		inline CFLOAT32 dot(const CFLOAT32 *data)
		{
			CFLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++)
				x += taps[i] * *data++;
			return x;
		}

	public:
		virtual ~FilterComplex() {}

		void setTaps(const std::vector<FLOAT32> &t)
		{
			taps = t;
			buffer.resize(taps.size() * 2, 0.0f);
		}

		// StreamIn
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class Filter : public SimpleStreamInOut<FLOAT32, FLOAT32>
	{
		std::vector<FLOAT32> output;
		std::vector<FLOAT32> buffer;
		std::vector<FLOAT32> taps;

		inline FLOAT32 dot(const FLOAT32 *data)
		{
			FLOAT32 x = 0.0f;
			for (int i = 0; i < taps.size(); i++)
				x += taps[i] * *data++;

			return x;
		}

	public:
		virtual ~Filter() {}
		void setTaps(const std::vector<FLOAT32> &t)
		{
			taps = t;
			buffer.resize(taps.size() * 2, 0.0f);
		}

		// StreamIn
		void Receive(const FLOAT32 *data, int len, TAG &tag);
	};

	class FilterCIC5 : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		CFLOAT32 h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;
		std::vector<CFLOAT32> output;

	public:
		virtual ~FilterCIC5() {}
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class FilterComplex3Tap : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;

		FLOAT32 alpha = 0.0f;
		FLOAT32 beta = 1.0f;
		CFLOAT32 h1 = 0.0f, h2 = 0.0f;

	public:
		virtual ~FilterComplex3Tap() {}

		void setTaps(FLOAT32 a)
		{
			alpha = a;
			beta = 1 - 2 * a;
		}

		// StreamIn
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class Rotate : public StreamIn<CFLOAT32>
	{
		std::vector<CFLOAT32> output_up, output_down;
		CFLOAT32 rot = 1.0f, mult = 1.0f;

	public:
		virtual ~Rotate() {}

		void setRotation(float angle) { mult = std::polar(1.0f, angle); }

		// Streams out
		Connection<CFLOAT32> up;
		Connection<CFLOAT32> down;

		// Streams in
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class SOXR : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
#ifdef HASSOXR
		soxr_t m_soxr;

		std::vector<CFLOAT32> output;
		std::vector<CFLOAT32> out_soxr;

		int count = 0;
		const int N = 16384;
#endif
	public:
		virtual ~SOXR() {}
		void setParams(int sample_rate, int out_rate);
		virtual void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class SRC : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
#ifdef HASSAMPLERATE
		std::vector<CFLOAT32> output;
		std::vector<CFLOAT32> out_src;

		SRC_STATE *state = nullptr;
		int count = 0;
		const int N = 16384;
		double ratio = 0.0;
		int src_error = 0;
#endif
	public:
#ifdef HASSAMPLERATE
		virtual ~SRC()
		{
			if (state)
				src_delete(state);
		}
#else
		virtual ~SRC() {}
#endif
		void setParams(int sample_rate, int out_rate);
		virtual void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class SquareFreqOffsetCorrection : public SimpleStreamInOut<CFLOAT32, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<CFLOAT32> fft_data;
		std::vector<FLOAT32> cumsum;

		CFLOAT32 rot = 1.0f;
		int N = 2048;
		int logN = 11;
		int count = 0;
		int window = 750;
		bool wide = false;

		FLOAT32 correctFrequency();

	public:
		virtual ~SquareFreqOffsetCorrection() {}
		void setWide(bool b) { wide = b; }
		void setParams(int, int);
		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};

	class DS_UINT16
	{
		uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

	public:
		int Run(uint32_t *, int, int);
		int Run(uint8_t *, uint32_t *, int, int);
		int Run(int8_t *, uint32_t *, int, int);
		int Run(uint32_t *, CFLOAT32 *, int, int);
	};

	class Downsample32_CU8 : public SimpleStreamInOut<CU8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3, DS4, DS5;

	public:
		virtual ~Downsample32_CU8() {}
		void Receive(const CU8 *data, int len, TAG &tag);
	};

	class Downsample32_CS8 : public SimpleStreamInOut<CS8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3, DS4, DS5;

	public:
		virtual ~Downsample32_CS8() {}
		void Receive(const CS8 *data, int len, TAG &tag);
	};

	class Downsample16_CU8 : public SimpleStreamInOut<CU8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3, DS4;

	public:
		virtual ~Downsample16_CU8() {}
		void Receive(const CU8 *data, int len, TAG &tag);
	};

	class Downsample16_CS8 : public SimpleStreamInOut<CS8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3, DS4;

	public:
		void Receive(const CS8 *data, int len, TAG &tag);
	};

	class Downsample8_CU8 : public SimpleStreamInOut<CU8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3;

	public:
		virtual ~Downsample8_CU8() {}
		void Receive(const CU8 *data, int len, TAG &tag);
	};

	class Downsample8_CS8 : public SimpleStreamInOut<CS8, CFLOAT32>
	{
		std::vector<CFLOAT32> output;
		std::vector<uint32_t> buffer;

		DS_UINT16 DS1, DS2, DS3;

	public:
		virtual ~Downsample8_CS8() {}
		void Receive(const CS8 *data, int len, TAG &tag);
	};
}
