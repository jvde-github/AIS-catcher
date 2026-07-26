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

#include "AIS-catcher.h"

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "NMEA.h"
#include "N2K.h"
#include "Basestation.h"
#include "Beast.h"

#include "DSP.h"
#include "Demod.h"
#include "Decoder/V2/Engine.h"
#include "StreamHelpers.h"

#include "Device/Device.h"

constexpr int N_SAMPLES_PER_SYMBOL = 5;

namespace AIS
{
	enum class Mode
	{
		AB,
		CD,
		ABCD,
		X
	};

	enum class ModelClass
	{
		IQ,
		FM,
		TXT,
		N2K,
		BASESTATION
	};


	// Abstract demodulation model
	class Model : public Setting
	{
	protected:
		std::string name = "";
		int station = 0;
		int own_mmsi = -1;

		Mode mode = Mode::AB;
		std::string designation = "AB";

		Device::Device *device = nullptr;
		Util::Timer<RAW> timer;
		Util::PassThrough<Message> output;
		Util::PassThrough<Plane::ADSB> outputADSB;
		Util::PassThrough<GPS> output_gps;

	public:
		Model() : Setting("Model") {}
		virtual ~Model() {}
		virtual void buildModel(char, char, int, bool, Device::Device *d) { device = d; }

		StreamOut<Message> &Output() { return output; }
		StreamOut<GPS> &OutputGPS() { return output_gps; }
		StreamOut<Plane::ADSB> &OutputADSB() { return outputADSB; }

		void setName(const std::string &s) { name = s; }
		const std::string &getName() { return name; }

		float getTotalTiming() { return timer.getTotalTiming(); }

		void setMode(Mode m) { mode = m; }
		void setOwnMMSI(int m) { own_mmsi = m; }
		void setDesignation(const std::string &s) { designation = s; }
		virtual Setting &SetKey(AIS::Keys key, const std::string &arg)
		{
			switch (key)
			{
			case AIS::KEY_SETTING_STATION_ID:
			case AIS::KEY_SETTING_ID:
				station = Util::Parse::Integer(arg);
				break;
			case AIS::KEY_SETTING_OWN_MMSI:
				own_mmsi = Util::Parse::Integer(arg);
				break;
			default:
				throw std::runtime_error("Model: unknown setting.");
			}
			return *this;
		}

		virtual std::string Get() { return ""; }
		virtual ModelClass getClass() { return ModelClass::IQ; }
	};

	// Common front-end downsampling
	class ModelFrontend : public Model
	{
	private:
		DSP::SOXR sox;
		DSP::SRC src;
		DSP::DownsampleKFilter DSK;
		DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4, DS2_5, DS2_6, DS2_7;
		DSP::Downsample2CIC5 DS2_a, DS2_b;
		DSP::Upsample US;
		DSP::FilterCIC5 FCIC5_a, FCIC5_b;
		DSP::FilterComplex3Tap FDC;
		DSP::DownsampleMovingAverage DS_MA;
		// fixed point downsamplers
		DSP::Downsample16_CU8 DS16_CU8;

		Util::ConvertRAW convert;

	protected:
		bool fixedpointDS = false;
		bool droop_compensation = true;
		bool SOXR_DS = false;
		bool SAMPLERATE_DS = false;
		bool MA_DS = false;
		bool allowDSK = false;

		Connection<CFLOAT32> *C_a = nullptr, *C_b = nullptr;
		DSP::Rotate ROT;

		// dump 48K channels to WAV files
		Util::WriteWAV wavA, wavB;
		Util::ConvertToRAW convertA, convertB;
		bool dump = false;

	public:
		void buildModel(char, char, int, bool, Device::Device *);

		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
	};

	// Standard demodulation model, FM with brute-force timing recovery
	class ModelStandard : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		AIS::Decoder DEC_a[N_SAMPLES_PER_SYMBOL], DEC_b[N_SAMPLES_PER_SYMBOL];
		DSP::Deinterleave<FLOAT32> S_a, S_b;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
	};

	// Base model for development purposes, simplest and fastest
	class ModelBase : public ModelFrontend
	{
		Demod::FM FM_a, FM_b;
		DSP::Filter FR_a, FR_b;
		DSP::SimplePLL sampler_a, sampler_b;
		AIS::Decoder DEC_a, DEC_b;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelDefault : public ModelFrontend
	{
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		Demod::PhaseSearch CD_a[N_SAMPLES_PER_SYMBOL], CD_b[N_SAMPLES_PER_SYMBOL];
		Demod::PhaseSearchEMA CD_EMA_a[N_SAMPLES_PER_SYMBOL], CD_EMA_b[N_SAMPLES_PER_SYMBOL];

		DSP::FilterComplex FC_a, FC_b;
		AIS::Decoder DEC_a[N_SAMPLES_PER_SYMBOL], DEC_b[N_SAMPLES_PER_SYMBOL];
		DSP::ScatterPLL S_a, S_b;

	protected:
		int nHistory = 12;
		int nDelay = 3;

		bool PS_EMA = true;
		bool CGF_wide = true;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelChallenger : public ModelFrontend
	{
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;

		DSP::FilterComplex FC_a, FC_b;
		DSP::Filter FR_af, FR_bf;

		Demod::PhaseSearchEMA CD_EMA_a[N_SAMPLES_PER_SYMBOL], CD_EMA_b[N_SAMPLES_PER_SYMBOL];
		Demod::FM FM_af, FM_bf;

		AIS::Decoder DEC_a[N_SAMPLES_PER_SYMBOL], DEC_b[N_SAMPLES_PER_SYMBOL], DEC_af[N_SAMPLES_PER_SYMBOL], DEC_bf[N_SAMPLES_PER_SYMBOL];
		DSP::ScatterPLL S_a, S_b;

		DSP::Deinterleave<CFLOAT32> throttle_a, throttle_b;
		DSP::Deinterleave<FLOAT32> S_af, S_bf;

	protected:
		int nHistory = 12;
		int nDelay = 3;

		bool PS_EMA = true;
		bool CGF_wide = true;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
	};

	// v2 engine: slot-predicted frequency correction, decision-directed tracking, FM branch
	class ModelEngineV2 : public ModelFrontend
	{
		V2::Engine V2_a, V2_b;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
	};

	// Standard demodulation model for FM discriminator input
	class ModelDiscriminator : public Model
	{
		Util::RealPart RP;
		Util::ImaginaryPart IP;
		DSP::Upsample US;

		DSP::Filter FR_a, FR_b;
		AIS::Decoder DEC_a[N_SAMPLES_PER_SYMBOL], DEC_b[N_SAMPLES_PER_SYMBOL];
		DSP::Deinterleave<FLOAT32> S_a, S_b;

		Util::ConvertRAW convert;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		ModelClass getClass() { return ModelClass::FM; }
	};

	// Standard demodulation model for FM discriminator input
	class ModelNMEA : public Model
	{
		NMEA nmea;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::TXT; }
	};

	class ModelN2K : public Model
	{
		N2KtoMessage n2k;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::N2K; }
	};

	class ModelBaseStation : public Model
	{
		Basestation model;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::BASESTATION; }
	};

	class ModelBeast : public Model
	{
		Beast model;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::BASESTATION; }
	};

	class ModelRAW1090 : public Model
	{
		RAW1090 model;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::BASESTATION; }
	};

	class ModelExport : public Model
	{
		Util::WriteWAV wav;

	public:
		void buildModel(char, char, int, bool, Device::Device *);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::IQ; }
	};
}
