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

#pragma once

#include "DSP.h"
#include "Demod.h"

#include "AIS-catcher.h"

#include "Common.h"
#include "Stream.h"
#include "AIS.h"
#include "NMEA.h"
#include "Utilities.h"

#include "Device/Device.h"
#include "IO.h"

namespace AIS {
	enum class Mode {
		AB,
		CD,
		ABCD
	};

	enum class ModelClass {
		IQ,
		FM,
		TXT
	};

	// idea is to avoid that message threads from different devices cause issues downstream (e.g. with sending UDP or updating the database).
	// can also be done further downstream
	class MessageMutex : public SimpleStreamInOut<AIS::Message, AIS::Message> {
		static std::mutex mtx;

	public:
		virtual void Receive(const AIS::Message* data, int len, TAG& tag) {
			std::lock_guard<std::mutex> lock(mtx);
			Send(data, len, tag);
		}
		virtual void Receive(AIS::Message* data, int len, TAG& tag) {
			std::lock_guard<std::mutex> lock(mtx);
			Send(data, len, tag);
		}
	};

	// Abstract demodulation model
	class Model : public Setting {
	protected:
		std::string name = "";

		Device::Device* device;
		Util::Timer<RAW> timer;
		MessageMutex output;
		Util::PassThrough<GPS> output_gps;

	public:
		virtual void buildModel(char, char, int, bool, Device::Device* d) { device = d; }

		StreamOut<Message>& Output() { return output; }
		StreamOut<GPS>& OutputGPS() { return output_gps; }

		void setName(std::string s) { name = s; }
		std::string getName() { return name; }

		float getTotalTiming() { return timer.getTotalTiming(); }

		virtual Setting& Set(std::string option, std::string arg) { throw std::runtime_error("Model: unknown setting."); }
		virtual std::string Get() { return ""; }
		virtual ModelClass getClass() { return ModelClass::IQ; }
	};


	// Common front-end downsampling
	class ModelFrontend : public Model {
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

		const int nSymbolsPerSample = 48000 / 9600;

		Connection<CFLOAT32>*C_a = NULL, *C_b = NULL;
		DSP::Rotate ROT;

	public:
		void buildModel(char, char, int, bool, Device::Device*);

		Setting& Set(std::string option, std::string arg);
		std::string Get();
	};

	// Standard demodulation model, FM with brute-force timing recovery
	class ModelStandard : public ModelFrontend {
		Demod::FM FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::Deinterleave<FLOAT32> S_a, S_b;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
	};


	// Base model for development purposes, simplest and fastest
	class ModelBase : public ModelFrontend {
		Demod::FM FM_a, FM_b;
		DSP::Filter FR_a, FR_b;
		DSP::SimplePLL sampler_a, sampler_b;
		AIS::Decoder DEC_a, DEC_b;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelDefault : public ModelFrontend {
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		std::vector<Demod::PhaseSearch> CD_a, CD_b;
		std::vector<Demod::PhaseSearchEMA> CD_EMA_a, CD_EMA_b;

		DSP::FilterComplex FC_a, FC_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::ScatterPLL S_a, S_b;

	protected:
		int nHistory = 12;
		int nDelay = 3;

		bool PS_EMA = true;
		bool CGF_wide = false;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
		Setting& Set(std::string option, std::string arg);
		std::string Get();
	};

	// Simple model embedding some elements of a coherent model with local phase estimation
	class ModelChallenger : public ModelFrontend {
		DSP::SquareFreqOffsetCorrection CGF_a, CGF_b;
		std::vector<Demod::PhaseSearch> CD_a, CD_b;
		std::vector<Demod::PhaseSearchEMA> CD_EMA_a, CD_EMA_b;

		DSP::FilterComplex FC_a, FC_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::ScatterPLL S_a, S_b;

	protected:
		int nHistory = 12;
		int nDelay = 3;

		bool PS_EMA = true;
		bool CGF_wide = false;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
		Setting& Set(std::string option, std::string arg);
		std::string Get();
	};


	// Standard demodulation model for FM discriminator input
	class ModelDiscriminator : public Model {
		Util::RealPart RP;
		Util::ImaginaryPart IP;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::Deinterleave<FLOAT32> S_a, S_b;

		Util::ConvertRAW convert;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
		ModelClass getClass() { return ModelClass::FM; }
	};

	// Standard demodulation model for FM discriminator input
	class ModelNMEA : public Model {
		NMEA nmea;

	public:
		void buildModel(char, char, int, bool, Device::Device*);
		Setting& Set(std::string option, std::string arg);
		std::string Get();
		ModelClass getClass() { return ModelClass::TXT; }
	};
}
