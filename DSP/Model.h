/*
Copyright(c) 2021 jvde.github@gmail.com

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

#include "../Stream.h"
#include "DSP.h"
#include "../AIS.h"
#include "Demod.h"
#include "../Utilities.h"

#include "../Input/Device.h"
#include "../Output/IO.h"

namespace AIS
{
	// Abstract demodulation model
	class Model
	{
	protected:
		std::string name = "";

		Device::DeviceBase* device;
		Util::Timer<RAW> timer;
		Util::PassThrough<NMEA> output;

		bool fixedpointDS = false;

	public:

		Model(Device::DeviceBase* d) { device = d; }

		virtual void buildModel(int, bool) {}

		StreamOut<NMEA>& Output() { return output; }

		void setName(std::string s) { name = s; }
		std::string getName() { return name; }

		float getTotalTiming() { return timer.getTotalTiming(); }

		virtual void setFixedPointDownsampling(bool b) { fixedpointDS = b; }
	};


	// Common front-end downsampling
	class ModelFrontend : public Model
	{
	private:
		DSP::DownsampleKFilter DSK;
		DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4, DS2_5, DS2_6, DS2_7;
		DSP::Downsample2CIC5 DS2_a, DS2_b;
		DSP::Upsample US;
		DSP::FilterCIC5 FCIC5_a, FCIC5_b;
		DSP::FilterComplex FFIR_a, FFIR_b;
		DSP::Downsample16Fixed DS16_Fixed;
		Util::ConvertRAW convert;

	protected:
		const int nSymbolsPerSample = 48000 / 9600;

		Connection<CFLOAT32> *C_a = NULL, *C_b = NULL;
		DSP::Rotate ROT;
	public:
		ModelFrontend(Device::DeviceBase* c) : Model(c) {}
		void buildModel(int, bool);
	};

	// Standard demodulation model, FM with brute-force timing recovery
	class ModelStandard : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::SamplerParallel S_a, S_b;

	public:
		ModelStandard(Device::DeviceBase* c) : ModelFrontend(c) {}
		void buildModel(int, bool);
	};


	// Base model for development purposes, simplest and fastest
	class ModelBase : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;
		DSP::Filter FR_a, FR_b;
		DSP::SamplerPLL sampler_a, sampler_b;
		AIS::Decoder DEC_a, DEC_b;

	public:
		ModelBase(Device::DeviceBase* c) : ModelFrontend(c) {}
		void buildModel(int, bool);
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelPhaseSearch : public ModelFrontend
	{
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		std::vector<Demod::PhaseSearch> CD_a, CD_b;

		DSP::FilterComplex FC_a, FC_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::SamplerParallelComplex S_a, S_b;

		int nHistory = 8;
		int nDelay = 0;

	public:
		ModelPhaseSearch(Device::DeviceBase* c, int h = 12, int d = 3) : ModelFrontend(c)
		{
			setName("AIS engine " VERSION);
			nHistory = h; nDelay = d;

		}
		void buildModel(int,bool);
	};

	// As the PhaseSearch model but optimized for speed using exponential moving averages
	class ModelPhaseSearchEMA : public ModelFrontend
	{
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		std::vector<Demod::PhaseSearchEMA> CD_a, CD_b;

		DSP::FilterComplex FC_a, FC_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::SamplerParallelComplex S_a, S_b;

		int nDelay = 0;

	public:
		ModelPhaseSearchEMA(Device::DeviceBase* c, int d = 3) : ModelFrontend(c)
		{
			setName("AIS engine (speed optimized) " VERSION);
			nDelay = d;

		}
		void buildModel(int,bool);
	};

	// Challenger model
	class ModelChallenger : public ModelPhaseSearch
	{
	public:
		ModelChallenger(Device::DeviceBase* c, int h = 8, int d = 0) : ModelPhaseSearch(c, h, d) 
		{
			setName("Challenger model");
		}
	};

	// Standard demodulation model for FM discriminator input
	class ModelDiscriminator : public Model
	{
		Util::RealPart RP;
		Util::ImaginaryPart IP;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::SamplerParallel S_a, S_b;

		Util::ConvertRAW convert;

	public:
		ModelDiscriminator(Device::DeviceBase* c) : Model(c) {}
		void buildModel(int,bool);
	};
}
