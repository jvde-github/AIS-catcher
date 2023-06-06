/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include <chrono>
#include <cassert>
#include <complex>
#include <cstring>

#include "FFT.h"
#include "DSP.h"

namespace DSP {
	void SimplePLL::Receive(const FLOAT32* data, int len, TAG& tag) {
		for (int i = 0; i < len; i++) {
			BIT bit = (data[i] > 0);

			if (bit != prev) {
				PLL += (0.5f - PLL) * (FastPLL ? 0.6f : 0.05f);
			}

			PLL += 0.2f;

			if (PLL >= 1.0f) {
				Send(&data[i], 1, tag);
				PLL -= (int)PLL;
			}
			prev = bit;
		}
	}

	void SimplePLL::Signal(const DecoderSignals& in) {
		switch (in) {
		case DecoderSignals::StartTraining:
			FastPLL = true;
			break;
		case DecoderSignals::StopTraining:
			FastPLL = false;
			break;
		default:
			break;
		}
	}

	// Downsample moving average
	void DownsampleMovingAverage::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < BLOCK_SIZE) output.resize(BLOCK_SIZE);

		for(int i = 0; i < len; i++) {
			D += data[i];
			df++;

			idx_out = idx_out + out_rate;

			if(idx_out >= in_rate) {
				idx_out %= in_rate;

				output[idx_in] = D / (FLOAT32) df;
				D = 0;
				df = 0;

				if(++idx_in == BLOCK_SIZE) {
					Send(output.data(), BLOCK_SIZE, tag);
					idx_in = 0;
				}
			}
		}
	}

// helper macros for moving averages
#define MA1(idx) \
	r##idx = z;  \
	z += h##idx;
#define MA2(idx) \
	h##idx = z;  \
	z += r##idx;

	// CIC5 downsample
	void Downsample2CIC5::Receive(const CFLOAT32* data, int len, TAG& tag) {
		assert(len % 2 == 0);

		if (output.size() < len / 2) output.resize(len / 2);

		CFLOAT32 z, r0, r1, r2, r3, r4;

		for (int i = 0, j = 0; i < len; i += 2, j++) {
			z = data[i];
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			output[j] = z * (FLOAT32)0.03125f;
			z = data[i + 1];
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
		}

		Send(output.data(), len / 2, tag);
	}

	void Decimate2::Receive(const CFLOAT32* data, int len, TAG& tag) {
		assert(len % 2 == 0);

		if (output.size() < len / 2) output.resize(len / 2);

		for (int i = 0, j = 0; i < len; i += 2, j++) {
			output[j] = data[i];
		}

		Send(output.data(), len / 2, tag);
	}

	// FilterCIC5
	void FilterCIC5::Receive(const CFLOAT32* data, int len, TAG& tag) {
		CFLOAT32 z, r0, r1, r2, r3, r4;

		assert(len % 2 == 0);

		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i += 2) {
			z = data[i];
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			output[i] = z * (FLOAT32)0.03125f;
			z = data[i + 1];
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
			output[i + 1] = z * (FLOAT32)0.03125f;
		}

		Send(output.data(), len, tag);
	}

	// Work in progress - needs performance improvement
	void DownsampleKFilter::Receive(const CFLOAT32* data, int len, TAG& tag) {
		int i, j;

		int nt = (int)taps.size();

		if (output.size() < outputSize) output.resize(outputSize);
		if (buffer.size() < len + nt) buffer.resize((int)(len + nt), 0.0f);

		for (i = 0, j = nt - 1; i < len; i++, j++) buffer[j] = data[i];

		while (idx_in < len) {
			output[idx_out] = dot(&buffer[idx_in]);

			if (++idx_out == outputSize) {
				Send(output.data(), outputSize, tag);
				idx_out = 0;
			}

			idx_in += K;
		}

		idx_in -= len;


		for (j = 0, i = len - nt + 1; j < nt - 1; i++, j++)
			buffer[j] = data[i];
	}

	// Upsample with linear interpolation
	void Upsample::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++) {
			const CFLOAT32 b = data[i];

			do {
				output[idx_out++] = (1 - alpha) * a + alpha * b;
				alpha += increment;

				if (idx_out == len) {
					Send(output.data(), len, tag);
					idx_out = 0;
				}

			} while (alpha < 1.0f);

			alpha -= 1.0f;
			a = b;
		}
	}

	// Filter Generic complex
	void FilterComplex::Receive(const CFLOAT32* data, int len, TAG& tag) {
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = 0, ptr = (int)taps.size() - 1; j < taps.size() - 1; ptr++, j++) {
			buffer[ptr] = data[j];
			output[j] = dot(&buffer[j]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++) {
			output[j] = dot(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++) {
			buffer[ptr] = data[i];
		}

		Send(output.data(), len, tag);
	}

	// Filter Generic Real
	void Filter::Receive(const FLOAT32* data, int len, TAG& tag) {
		int ptr, i, j;

		if (output.size() < len) output.resize(len);

		for (j = 0, ptr = (int)taps.size() - 1; j < taps.size() - 1; ptr++, j++) {
			buffer[ptr] = data[j];
			output[j] = dot(&buffer[j]);
		}

		for (i = 0; i < len - taps.size() + 1; i++, j++) {
			output[j] = dot(&data[i]);
		}

		for (ptr = 0; i < len; i++, ptr++) {
			buffer[ptr] = data[i];
		}

		Send(output.data(), len, tag);
	}

	// Filter Generic complex 3 Taps
	void FilterComplex3Tap::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output.size() < len) output.resize(len);

		for (int i = 0; i < len; i++) {
			output[i] = alpha * (h1 + data[i]) + h2 * beta;
			h1 = h2;
			h2 = data[i];
		}

		Send(output.data(), len, tag);
	}

	// Rotate +/- 25K Hz
	void Rotate::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (output_up.size() < len) output_up.resize(len);
		if (output_down.size() < len) output_down.resize(len);

		for (int i = 0; i < len; i++) {
			FLOAT32 RR = data[i].real() * rot.real(), II = data[i].imag() * rot.imag();
			FLOAT32 RI = data[i].real() * rot.imag(), IR = data[i].imag() * rot.real();

			output_up[i].real(RR - II);
			output_up[i].imag(IR + RI);
			output_down[i].real(RR + II);
			output_down[i].imag(IR - RI);

			rot *= mult;
		}

		up.Send(output_up.data(), len, tag);
		down.Send(output_down.data(), len, tag);

		rot /= std::abs(rot);
	}

	void SOXR::setParams(int sample_rate, int out_rate) {
#ifdef HASSOXR
		soxr_error_t error;

		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
		soxr_quality_spec_t quality_spec = soxr_quality_spec((SOXR_VHQ | SOXR_LINEAR_PHASE), 0);
		soxr_runtime_spec_t runtime_spec = soxr_runtime_spec(1);

		m_soxr = soxr_create(sample_rate, out_rate, 2, &error, &io_spec, &quality_spec, &runtime_spec);

		if (error) {
			soxr_delete(m_soxr);
			throw std::runtime_error("Model: error opening SOX");
		}

		output.resize(N);
#else
		throw std::runtime_error("SOXR not included in this distribution. Please recompile with SOXR support.");
#endif
	}

	// Downsample using libsoxr
	void SOXR::Receive(const CFLOAT32* data, int len, TAG& tag) {
#ifdef HASSOXR
		if (out_soxr.size() < len) out_soxr.resize(len);

		size_t sz = 0;
		soxr_error_t error = soxr_process(m_soxr, data, len, nullptr, out_soxr.data(), out_soxr.size(), &sz);

		if (error) {
			soxr_delete(m_soxr);
			std::cerr << "Error: SOX processing returns error." << std::endl;
		}

		for (int i = 0; i < sz; i++) {
			output[count++] = out_soxr[i];
			if (count == N) {
				Send(output.data(), N, tag);
				count = 0;
			}
		}
#endif
	}

	void SRC::setParams(int sample_rate, int out_rate) {
#ifdef HASSAMPLERATE

		state = src_new(SRC_SINC_FASTEST, 2, &src_error);

		if (!state) {
			throw std::runtime_error("Model: cannot open libsamplerate");
		}

		ratio = (double)out_rate / (double)sample_rate;
		output.resize(N);
#else
		throw std::runtime_error("libsamplerate support not included in this distribution. Please recompile with libsamplerate support.");
#endif
	}

	// Downsample using libsoxr
	void SRC::Receive(const CFLOAT32* data, int len, TAG& tag) {
#ifdef HASSAMPLERATE

		if (out_src.size() < len) out_src.resize(len);

		SRC_DATA sd;

		sd.data_in = (float*)data;
		sd.data_out = (float*)out_src.data();
		sd.input_frames = len;
		sd.output_frames = len;
		sd.src_ratio = ratio;
		sd.end_of_input = 0;

		int ret = src_process(state, &sd);

		if (ret) {
			std::cerr << "Error: libsamplerate processing returns error: " << src_strerror(ret) << std::endl;
		}
		else {
			for (int i = 0; i < sd.output_frames_gen; i++) {
				output[count++] = out_src[i];
				if (count == N) {
					Send(output.data(), N, tag);
					count = 0;
				}
			}
		}
#endif
	}

	// square the signal, find the mid-point between two peaks
	FLOAT32 SquareFreqOffsetCorrection::correctFrequency() {
		FLOAT32 max_val = 0.0, fz = -1;
		int delta = (int)(9600.0 / 48000.0 * N);
		int wi = 0;

		FFT::fft(fft_data);

		if(wide) {
			if(cumsum.size() < N) cumsum.resize(N);

			int M = (int)(12500.0 / 48000.0 * N);
			FLOAT32 wm = -1;

			cumsum[0] = 0;
			for(int i = 1; i < N; i++) {
				cumsum[i] = cumsum[i-1] + std::abs(fft_data[(i + N / 2) % N]); //* std::abs(fft_data[(i + N / 2) % N]);
			}

			for(int i = 0; i < N - M; i++) {

				if(cumsum[i+M] - cumsum[i] > wm) {
					wm = cumsum[i+M] - cumsum[i];
					wi = i;
				}
			}
			wi = (wi + M/2 - N/2);  
		}

		for (int i = wi +  window; i < wi + N - window - delta; i++) {
			FLOAT32 h = std::abs(fft_data[(i + N / 2) % N]) + std::abs(fft_data[(i + delta + N / 2) % N]);

			if (h > max_val) {
				max_val = h;
				fz = (N / 2 - (i + delta / 2.0f));
			}
		}

		FLOAT32 f = fz / 2.0 / N;
		CFLOAT32 rot_step = std::polar(1.0f, (float)(f * 2 * PI));

		for (int i = 0; i < N; i++) {
			rot *= rot_step;
			output[i] *= rot;
		}

		rot /= std::abs(rot);
		return f * 48000.0 / 162.0f;
	}

	void SquareFreqOffsetCorrection::setParams(int n, int w) {
		N = n;
		logN = FFT::log2(N);
		window = w;
	}

	void SquareFreqOffsetCorrection::Receive(const CFLOAT32* data, int len, TAG& tag) {
		if (fft_data.size() < N) fft_data.resize(N);
		if (output.size() < N) output.resize(N);

		for (int i = 0; i < len; i++) {
			fft_data[FFT::rev(count, logN)] = data[i] * data[i];
			output[count] = data[i];

			if (++count == N) {
				tag.ppm = correctFrequency();
				Send(output.data(), N, tag);
				count = 0;
			}
		}
	}

	// ----------------------------------------------------------------------------
	// CIC5 downsampling optimized for Raspberry Pi 1
	// Idea: I and Q signals can be downsampled in parallel and, if stored
	// as int16, can be worked in parallel with int32 operations.
	// Self invented so might be more clever approaches
	// ----------------------------------------------------------------------------

	int DS_UINT16::Run(uint32_t* data, int len, int shift) {
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift;
		mask |= mask << 16;
		uint32_t* out = data;

		len >>= 1;

		for (int i = 0; i < len; i++) {
			z = *data++;
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			// divide by 2^shift and clean up contamination from I to Q
			*out++ = (z >> shift) & mask;
			z = *data++;
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
		}
		return len;
	}

	// special version of the above but includes the initial conversion from CU8 to avoid extra loop
	int DS_UINT16::Run(uint8_t* in, uint32_t* out, int len, int shift) {
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift;
		mask |= mask << 16;

		len >>= 1;

		for (int i = 0; i < len; i++) {
			z = (uint32_t)*in++;
			z |= (uint32_t)*in++ << 16;
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			*out++ = (z >> shift) & mask;
			z = (uint32_t)*in++;
			z |= (uint32_t)*in++ << 16;
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
		}
		return len;
	}

	// special version of the above but includes the initial conversion from CS8 to avoid extra loop
	int DS_UINT16::Run(int8_t* in, uint32_t* out, int len, int shift) {
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift;
		mask |= mask << 16;
		const uint32_t mask_uint = (1 << 7) | (1 << 23);

		len >>= 1;

		for (int i = 0; i < len; i++) {
			z = (uint8_t)*in++;
			z |= (uint32_t)((uint8_t)*in++) << 16;
			z ^= mask_uint; // from int to uint in parallel by flipping sign bits
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			*out++ = (z >> shift) & mask;
			z = (uint8_t)*in++;
			z |= (uint32_t)((uint8_t)*in++) << 16;
			z ^= mask_uint;
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
		}
		return len;
	}

	// special version of the above but includes the last conversion to CFLOAT32
	int DS_UINT16::Run(uint32_t* in, CFLOAT32* out, int len, int shift) {
		uint32_t z, r0, r1, r2, r3, r4;
		uint32_t mask = 0xFFFFU >> shift;
		mask |= mask << 16;
		const uint32_t mask_uint = (1 << 15) | (1 << 31);
		len >>= 1;

		for (int i = 0; i < len; i++) {
			z = *in++;
			MA1(0);
			MA1(1);
			MA1(2);
			MA1(3);
			MA1(4);
			z = (z >> shift) & mask;

			z ^= mask_uint; // uint to int in parallel by flipping sign bit
			out[i].real(((int16_t)(z & 0xFFFFU)) / 32768.0f);
			out[i].imag(((int16_t)(z >> 16)) / 32768.0f);

			z = *in++;
			MA2(0);
			MA2(1);
			MA2(2);
			MA2(3);
			MA2(4);
		}
		return len;
	}

	// Multi-pass aggregators
	void Downsample32_CU8::Receive(const CU8* data, int len, TAG& tag) {
		assert(len % 32 == 0);

		if (output.size() < len / 32) output.resize(len / 32);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5);
		len = DS4.Run(buf, len, 5);
		len = DS5.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}

	void Downsample32_CS8::Receive(const CS8* data, int len, TAG& tag) {
		assert(len % 32 == 0);

		if (output.size() < len / 32) output.resize(len / 32);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5);
		len = DS4.Run(buf, len, 5);
		len = DS5.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}

	void Downsample16_CU8::Receive(const CU8* data, int len, TAG& tag) {
		assert(len % 16 == 0);

		if (output.size() < len / 16) output.resize(len / 16);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5);
		len = DS4.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}

	void Downsample16_CS8::Receive(const CS8* data, int len, TAG& tag) {
		assert(len % 16 == 0);

		if (output.size() < len / 16) output.resize(len / 16);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, len, 5);
		len = DS4.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}

	void Downsample8_CU8::Receive(const CU8* data, int len, TAG& tag) {
		assert(len % 8 == 0);

		if (output.size() < len / 8) output.resize(len / 8);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((uint8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}

	void Downsample8_CS8::Receive(const CS8* data, int len, TAG& tag) {
		assert(len % 8 == 0);

		if (output.size() < len / 8) output.resize(len / 8);
		if (buffer.size() < len / 2) buffer.resize(len / 2);

		uint32_t* buf = buffer.data();

		len = DS1.Run((int8_t*)data, buf, len, 3);
		len = DS2.Run(buf, len, 4);
		len = DS3.Run(buf, output.data(), len, 0);

		out.Send(output.data(), len, tag);
	}
}
