/*
Copyright(c) 2021-2022 jvde.github@gmail.com

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

#pragma once

#include "DSP.h"
#include "Demod.h"

#include "AIS-catcher.h"

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "Utilities.h"

#include "Device/Device.h"
#include "IO.h"

namespace AIS
{
	// Abstract demodulation model
	class Model: public Setting
	{
	protected:
		std::string name = "";

		Device::Device* device;
		Util::Timer<RAW> timer;
		Util::PassThrough<NMEA> output;

	public:

		virtual void buildModel(int, bool, Device::Device* d) { device = d;  }

		StreamOut<NMEA>& Output() { return output; }

		void setName(std::string s) { name = s; }
		std::string getName() { return name; }

		float getTotalTiming() { return timer.getTotalTiming(); }
	};


	// Common front-end downsampling
	class ModelFrontend : public Model
	{
	private:
		DSP::SOXR sox;
		DSP::DownsampleKFilter DSK;
		DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4, DS2_5, DS2_6, DS2_7;
		DSP::Downsample2CIC5 DS2_a, DS2_b;
		DSP::Upsample US;
		DSP::FilterCIC5 FCIC5_a, FCIC5_b;

		// fixed point downsamplers
		DSP::Downsample32_CU8 DS32_CU8;
		DSP::Downsample16_CU8 DS16_CU8;
		DSP::Downsample8_CU8 DS8_CU8;
		DSP::Downsample32_CS8 DS32_CS8;
		DSP::Downsample16_CS8 DS16_CS8;
		DSP::Downsample8_CS8 DS8_CS8;

		Util::ConvertRAW convert;

		bool fixedpointDS = false;
		bool SOXR_DS = false;

	protected:
		const int nSymbolsPerSample = 48000 / 9600;

		Connection<CFLOAT32> *C_a = NULL, *C_b = NULL;
		DSP::Rotate ROT;
	public:
		void buildModel(int, bool, Device::Device*);

		virtual void Set(std::string option, std::string arg);

	};

	// Standard demodulation model, FM with brute-force timing recovery
	class ModelStandard : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::Deinterleave<FLOAT32> S_a, S_b;

	public:
		void buildModel(int, bool, Device::Device*);
	};


	// Base model for development purposes, simplest and fastest
	class ModelBase : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;
		DSP::Filter FR_a, FR_b;
		DSP::SimplePLL sampler_a, sampler_b;
		AIS::Decoder DEC_a, DEC_b;

	public:
		void buildModel(int, bool, Device::Device*);
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelDefault : public ModelFrontend
	{
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		std::vector<Demod::PhaseSearch> CD_a, CD_b;
		std::vector<Demod::PhaseSearchEMA> CD_EMA_a, CD_EMA_b;

		DSP::FilterComplex FC_a, FC_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::Deinterleave<CFLOAT32> S_a, S_b;

	protected:

		int nHistory = 12;
		int nDelay = 3;

		bool PS_EMA = true;

	public:

		void buildModel(int, bool, Device::Device*);
		void Set(std::string option, std::string arg);
	};

	// Challenger model
	class ModelChallenger : public ModelDefault
	{
	public:
		ModelChallenger(int h = 4, int d = 2)
		{
			nDelay = d;
			nHistory = h;
			setName("Challenger model");
			PS_EMA = false;
		}
	};

	// Standard demodulation model for FM discriminator input
	class ModelDiscriminator : public Model
	{
		Util::RealPart RP;
		Util::ImaginaryPart IP;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::Deinterleave<FLOAT32> S_a, S_b;

		Util::ConvertRAW convert;

	public:
		void buildModel(int, bool, Device::Device*);
	};
}
