/*
	Copyright(c) 2026-2026 jvde.github@gmail.com

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

#include <cmath>
#include <cstring>

#include "V2Engine.h"
#include "FFT.h"
#include "Filters.h"

#define SUBBIN_INTERP 0

namespace V2
{
	// tuning
	static const float PROMINENCE_GATE = 5.5f;
	static const float SLOT_LOCK = 0.64f;
	static const float LEARN_W = 0.2f;

	static inline float norm2(CFLOAT32 z)
	{
		return z.real() * z.real() + z.imag() * z.imag();
	}

	static inline CFLOAT32 dot17(const CFLOAT32 *a, const float *taps)
	{
		CFLOAT32 sum(0.0f, 0.0f);
		for (int i = 0; i < 8; i++)
			sum += (a[i] + a[16 - i]) * taps[i];

		return sum + a[8] * taps[8];
	}

	static inline float dot37(const float *a, const float *taps)
	{
		float sum = 0.0f;
		for (int i = 0; i < 18; i++)
			sum += (a[i] + a[36 - i]) * taps[i];
		return sum + a[18] * taps[18];
	}

	float FreqOffset::Estimate(const CFLOAT32 *window)
	{
		const int fft_length = BLOCK_SIZE;
		const int delta = 102; //  9600.0f / 48000.0f * fft_length
		const int M = 133;	   // 12500.0f / 48000.0f * fft_length
		const int ofs = 15;	   // (12500.0f - 9600.0f) / 48000.0f * fft_length

		for (int n = 0; n < fft_length; n++)
			fft_data[FFT::rev(n, 9)] = window[n] * window[n];

		FFT::fft(fft_data);

		// fftshift-ordered magnitudes
		for (int i = 0; i < 256; i++)
			magnitude[i] = sqrtf(norm2(fft_data[i + 256]));
		for (int i = 0; i < 256; i++)
			magnitude[i + 256] = sqrtf(norm2(fft_data[i]));

		float rolling_sum = 0.0f;
		for (int j = 0; j < M; j++)
			rolling_sum += magnitude[j];

		float wm = rolling_sum + 0.6f * (magnitude[ofs] + magnitude[ofs + delta]);
		int wi = 0;

		for (int i = 1; i <= fft_length - M; i++)
		{
			rolling_sum = rolling_sum - magnitude[i - 1] + magnitude[i + M - 1];
			const float v = rolling_sum + 0.6f * (magnitude[i + ofs] + magnitude[i + ofs + delta]);
			if (v > wm)
			{
				wm = v;
				wi = i;
			}
		}

		int fz = -1;
		float max_val = 0.0f;
		for (int i = wi; i < wi + (M - delta); i++)
		{
			const float h = magnitude[i] + magnitude[i + delta];
			if (h > max_val)
			{
				max_val = h;
				fz = i;
			}
		}

		float total = 0.0f;
		for (int i = 0; i < fft_length; i++)
			total += magnitude[i];

		prominence = total > 0.0f ? max_val * (fft_length / 2) / total : 0.0f;

		if (fz < 0)
			return 0.0f;

		float frac = 0.0f;

#if SUBBIN_INTERP
		if (fz > 0 && fz + delta + 1 < fft_length)
		{
			const float a = magnitude[fz - 1] + magnitude[fz - 1 + delta];
			const float c = magnitude[fz + 1] + magnitude[fz + 1 + delta];
			const float den = a - 2.0f * max_val + c;

			if (den < 0.0f)
			{
				frac = 0.5f * (a - c) / den;
				frac = frac > 0.5f ? 0.5f : (frac < -0.5f ? -0.5f : frac);
			}
		}
#endif

		return (fft_length / 2 - (fz + frac + delta / 2.0f)) / 2.0f / fft_length;
	}

	void FreqOffset::Derotate(float f, const CFLOAT32 *src, CFLOAT32 *dst, int len)
	{
		const CFLOAT32 rot_step = std::polar(1.0f, f * 2.0f * PI);

		CFLOAT32 r = rot;
		for (int i = 0; i < len; i++)
		{
			r *= rot_step;
			dst[i] = src[i] * r;
		}

		rot = r / std::abs(r);
		last_f = f;
	}

	FilterFL17::FilterFL17(const float *t) : taps(t)
	{
		for (int i = 0; i < 32; i++)
			buffer[i] = CFLOAT32(0.0f, 0.0f);
	}

	void FilterFL17::Run(const CFLOAT32 *input, CFLOAT32 *output)
	{
		memcpy(buffer + 16, input, 16 * sizeof(CFLOAT32));

		const CFLOAT32 *ptr = buffer;
		for (int i = 0; i < 16; i++, ptr++)
			*output++ = dot17(ptr, taps);

		ptr = input;
		for (int i = 0; i <= BLOCK_SIZE - 17; i++, ptr++)
			*output++ = dot17(ptr, taps);

		memcpy(buffer, input + (BLOCK_SIZE - 16), 16 * sizeof(CFLOAT32));
	}

	FilterFL37::FilterFL37(const float *t) : taps(t)
	{
		for (int i = 0; i < 72; i++)
			buffer[i] = 0.0f;
	}

	void FilterFL37::Run(const float *input, float *output)
	{
		memcpy(buffer + 36, input, 36 * sizeof(float));

		const float *ptr = buffer;
		for (int i = 0; i < 36; i++, ptr++)
			*output++ = dot37(ptr, taps);

		ptr = input;
		for (int i = 0; i <= BLOCK_SIZE - 37; i++, ptr++)
			*output++ = dot37(ptr, taps);

		memcpy(buffer, input + (BLOCK_SIZE - 36), 36 * sizeof(float));
	}

	CFLOAT32 PhaseTracker::Rotate90(CFLOAT32 z)
	{
		const float re_in = z.real();
		const float im_in = z.imag();

		const float src_re = (rot & 1) ? im_in : re_in;
		const float src_im = (rot & 1) ? re_in : im_in;

		const CFLOAT32 r(((rot ^ (rot >> 1)) & 1) ? -src_re : src_re,
						 (rot & 2) ? -src_im : src_im);

		rot = (rot + 1) & 3;
		return r;
	}

	int PhaseTracker::Run(CFLOAT32 z, bool training)
	{
		z = Rotate90(z);

		const float alpha = training ? weight_train : weight;
		const float beta = 1.0f - alpha;

		const float proj = z.real() * s.real() + z.imag() * s.imag();
		const float d = proj >= 0.0f ? 1.0f : -1.0f;

		s = alpha * s + (beta * d) * z;

		// differential decision (NRZI-style, cancels any 180-degree offset)
		const int decision = proj > 0.0f ? 1 : 0;
		const int bit = decision ^ prev_decision;

		prev_decision = decision;
		return bit;
	}

	bool BitPLL::Run(float sample, bool training)
	{
		const int bit = sample > 0.0f ? 1 : 0;

		if (bit != last_bit)
			phase += (0.5f - phase) * (training ? 0.6f : 0.05f);

		last_bit = bit;

		phase += 0.2f;
		if (phase < 1.0f)
			return false;

		phase -= (int)phase;
		return true;
	}

	// octant-reduced polynomial atan2, max error 2e-4 rad
	static inline float atan2_fast(float y, float x)
	{
		const float ax = fabsf(x), ay = fabsf(y);
		const float mx = ax > ay ? ax : ay;
		const float mn = ax > ay ? ay : ax;

		if (mx == 0.0f)
			return 0.0f;

		const float a = mn / mx;
		const float s = a * a;
		float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

		if (ay > ax)
			r = 1.57079637f - r;
		if (x < 0.0f)
			r = 3.14159274f - r;

		return y < 0.0f ? -r : r;
	}

	void FMDemod::Run(const CFLOAT32 *input, float *output)
	{
		for (int i = 0; i < BLOCK_SIZE; i++)
		{
			const CFLOAT32 p = input[i] * std::conj(prev);
			output[i] = atan2_fast(p.imag(), p.real()) / PI;
			prev = input[i];
		}
	}

	Engine::Engine() : filter17(Filters::Coherent.data()), filter37(Filters::Receiver.data())
	{
		for (int i = 0; i < 2 * BLOCK_SIZE; i++)
			raw[i] = CFLOAT32(0.0f, 0.0f);
	}

	bool Engine::midWins(const CFLOAT32 *input) const
	{
		float head = 0.0f, tail = 0.0f; // input[0,256) and input[512,768)

		for (int i = 0; i < BLOCK_SIZE / 2; i++)
		{
			head += norm2(input[i]);
			tail += norm2(input[BLOCK_SIZE + i]);
		}
		return tail > head;
	}

	void Engine::CGF(const CFLOAT32 *input, CFLOAT32 *output, bool busy)
	{
		const bool slot_locked = norm2(slot_ema) >= SLOT_LOCK;
		const int e = (int)(((slot_phase - sample_idx) % SLOT + SLOT) % SLOT);

		ppm_prev = ppm;

		float f;
		if (slot_locked && e < BLOCK_SIZE)
		{
			ppm_split = e; // [0, e) keeps the previous block's frequency

			fo.Derotate(fo.last_f, input, output, e);
			f = fo.Estimate(input + e);
			fo.Derotate(f, input + e, output + e, BLOCK_SIZE - e);
		}
		else
		{
			ppm_split = 0;

			const int offset = (!busy && midWins(input)) ? BLOCK_SIZE / 2 : 0;
			f = fo.Estimate(input + offset);

			if (busy && fo.prominence < PROMINENCE_GATE)
				f = fo.last_f; // tone gate: hold while a decode is in flight

			fo.Derotate(f, input, output, BLOCK_SIZE);
		}
		ppm = f * 48000.0f / 162.0f;
	}

	void Engine::learnSlotPhase(const AIS::Decoder &d)
	{
		const long long a = d.getStartIdx() - PRE;
		const float th = (float)((a % SLOT + SLOT) % SLOT) * (2.0f * PI / SLOT);

		slot_ema = (1.0f - LEARN_W) * slot_ema + LEARN_W * CFLOAT32(cosf(th), sinf(th));
	
		const float ph = atan2f(slot_ema.imag(), slot_ema.real()) * (SLOT / (2.0f * PI));
		slot_phase = (int)(ph + SLOT + 0.5f) % SLOT;
	}

	void Engine::resetDecoders()
	{
		for (int k = 0; k < N_DECODERS; k++)
			dec[k].reset();
	}

	void Engine::processBlock(TAG &tag)
	{
		slot_ema *= 0.9999f; // slot predictor forgets in ~25 s of silence

		bool busy = false;
		for (int j = 0; j < 5; j++)
			busy |= dec[j].getState() != AIS::State::TRAINING;

		CGF(raw, freq_corrected, busy);

		// FM Decoder
		filter17.Run(freq_corrected, coh_filtered);
		fm_demod.Run(raw, fm_demodulated);

		// Coherent Decoder
		filter37.Run(fm_demodulated, fm_filtered);

		tag.ppm = ppm_prev;

		for (int i = 0; i < BLOCK_SIZE; i++)
		{
			if (i == ppm_split)
				tag.ppm = ppm;

			tag.sample_idx = sample_idx++;

			// Coherent decoder
			const int bit = trk[di].Run(coh_filtered[i], dec[di].getState() == AIS::State::TRAINING);

			tag.sample_lvl = norm2(coh_filtered[i]);

			if (dec[di].Run(bit ? 1.0f : -1.0f, tag) == AIS::State::FOUNDMESSAGE)
			{
				learnSlotPhase(dec[di]);
				resetDecoders();
			}

			// FM decoder
			if (fm_pll.Run(fm_filtered[i], dec[FM_DEC].getState() == AIS::State::TRAINING))
				if (dec[FM_DEC].Run(fm_filtered[i], tag) == AIS::State::FOUNDMESSAGE)
					resetDecoders();

			di = di + 1 == 5 ? 0 : di + 1;
		}

		memmove(raw, raw + BLOCK_SIZE, BLOCK_SIZE * sizeof *raw);
	}

	void Engine::Receive(const CFLOAT32 *data, int len, TAG &tag)
	{
		while (len > 0)
		{
			const int n = MIN(BLOCK_SIZE - fill, len);

			memcpy(raw + BLOCK_SIZE + fill, data, n * sizeof(CFLOAT32));

			fill += n;
			data += n;
			len -= n;

			if (fill == BLOCK_SIZE)
			{
				processBlock(tag);
				fill = 0;
			}
		}
	}
}
