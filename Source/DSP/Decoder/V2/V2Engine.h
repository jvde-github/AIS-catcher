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

#include <cstdint>
#include <vector>

#include "AIS.h"
#include "Common.h"
#include "Stream.h"
#include "FFT.h"

namespace V2
{
	const int BLOCK_SIZE = 512;

	struct FreqOffset
	{
		CFLOAT32 rot = CFLOAT32(1.0f, 0.0f);
		float last_f = 0.0f;
		float prominence = 0.0f;

		float Estimate(const CFLOAT32 *window);
		void Derotate(float f, const CFLOAT32 *src, CFLOAT32 *dst, int len);

	private:
		std::vector<CFLOAT32> fft_data = std::vector<CFLOAT32>(BLOCK_SIZE);
		FFT::Plan<FLOAT32> fft_plan;
		float magnitude[BLOCK_SIZE];
	};

	struct FilterFL17
	{
		CFLOAT32 buffer[32];
		const float *taps;

		explicit FilterFL17(const float *t);
		void Run(const CFLOAT32 *input, CFLOAT32 *output);
	};

	struct FilterFL37
	{
		float buffer[72];
		const float *taps;

		explicit FilterFL37(const float *t);
		void Run(const float *input, float *output);
	};

	struct PhaseTracker
	{
		unsigned rot = 0;
		CFLOAT32 s = CFLOAT32(0.0f, 0.0f);
		int prev_decision = 0;
		float weight = 0.86f;
		float weight_train = 0.75f;

		int Run(CFLOAT32 z, bool training);

	private:
		CFLOAT32 Rotate90(CFLOAT32 z);
	};

	struct FMDemod
	{
		CFLOAT32 prev = CFLOAT32(1.0f, 0.0f);

		void Run(const CFLOAT32 *input, float *output);
	};

	struct BitPLL
	{
		float phase = 0.0f;
		int last_bit = 0;

		bool Run(float sample, bool training);
	};

	class Engine : public StreamIn<CFLOAT32>
	{
	public:
		static const int N_DECODERS = 6; // 5 strobe decoders + 1 FM
		static const int FM_DEC = 5;

	private:
		static const int SLOT = 1280; // SOTDMA slot at 48 kHz
		static const int PRE = 155;	  // start-flag anchor -> burst start

		FreqOffset fo;
		FilterFL17 filter17;
		PhaseTracker trk[5];
		FMDemod fm_demod;
		FilterFL37 filter37;

		AIS::Decoder dec[N_DECODERS]; // [0..4] strobe decoders, [FM_DEC] the FM decoder

		CFLOAT32 raw[2 * BLOCK_SIZE]; // [0,BLOCK_SIZE) decoded this call, [BLOCK_SIZE,2*BLOCK_SIZE) lookahead
		CFLOAT32 freq_corrected[BLOCK_SIZE];
		CFLOAT32 coh_filtered[BLOCK_SIZE];

		float fm_demodulated[BLOCK_SIZE];
		// The level the FM decoder slices at: a carrier offset shifts the discriminator output, so the
		// centre between the two tones is its average over two periods of the training pattern,
		// ending ten samples back so that the start flag is not in it, held from the start flag on.
		// fm_filtered starts with the FM_HIST samples before the block.
		static const int FM_WINDOW = 40, FM_HIST = FM_WINDOW + 9;
		float fm_filtered[FM_HIST + BLOCK_SIZE] = {};
		float fm_centre = 0.0f;
		BitPLL fm_pll;

		CFLOAT32 slot_ema = CFLOAT32(0.0f, 0.0f);
		int slot_phase = 0;
		int di = 0;
		long long sample_idx = -BLOCK_SIZE; // of the sample being decoded, one block behind what has arrived
		int fill = 0;
		float ppm = 0.0f, ppm_prev = 0.0f;
		int ppm_split = 0;

		bool midWins(const CFLOAT32 *input) const;
		void CGF(const CFLOAT32 *input, CFLOAT32 *output, bool busy);
		void learnSlotPhase(const AIS::Decoder &d);
		void resetDecoders();
		void processBlock(TAG &tag);

	public:
		Engine();

		void setWeights(float train, float track)
		{
			for (int i = 0; i < 5; i++)
			{
				trk[i].weight_train = train;
				trk[i].weight = track;
			}
		}

		void setOrigin(char channel, int station, int own_mmsi)
		{
			for (int i = 0; i < N_DECODERS; i++)
				dec[i].setOrigin(channel, station, own_mmsi);
		}

		AIS::Decoder &getDecoder(int i) { return dec[i]; }

		void Receive(const CFLOAT32 *data, int len, TAG &tag);
	};
}
