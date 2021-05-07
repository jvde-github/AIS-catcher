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

#include "Stream.h"
#include "DSP.h"
#include "Device.h"
#include "AIS.h"
#include "Utilities.h"
#include "IO.h"

namespace AIS
{
	class Model
	{
	protected:

		std::string name;
		Device::Control* control;
		Connection<CFLOAT32>* input;
		Util::Timer<CFLOAT32> timer;
		Util::PassThrough<NMEA> output;

	public:

		Model(Device::Control* ctrl, Connection<CFLOAT32>* in)
		{
			control = ctrl;
			input = in;
		}

		virtual void buildModel(int, bool) {}
		virtual std::vector<uint32_t> SupportedSampleRates() { return std::vector<uint32_t>(); }

		StreamOut<NMEA>& Output() { return output; }

		void setName(std::string s) { name = s; }
		std::string getName() { return name; }

		float getTotalTiming() { return timer.getTotalTiming(); }
	};

	// Standard demodulation model

	class ModelStandard : public Model
	{
		DSP::Downsample3Complex DS3;
		DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4;
		DSP::Downsample2CIC5 DS2_a, DS2_b;
		DSP::FilterCIC5 F_a, F_b;

		DSP::RotateUp ROT_a;
		DSP::RotateDown ROT_b;

		Util::RealPart RP;
		Util::ImaginaryPart IP;

		DSP::FMDemodulation FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		std::vector<AIS::Decoder> DEC_a, DEC_b;
		DSP::SamplerParallel S_a, S_b;

	public:
		ModelStandard(Device::Control* c, Connection<CFLOAT32>* i) : Model(c, i) {}
		std::vector<uint32_t> SupportedSampleRates();

		void buildModel(int,bool);
	};


	// challenger model for development purposes

	class ModelChallenge : public Model
	{
		DSP::Downsample3Complex DS3;
		DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4;
		DSP::Downsample2CIC5 DS2_a, DS2_b;
		DSP::FilterCIC5 F_a, F_b;

		DSP::RotateUp ROT_a;
		DSP::RotateDown ROT_b;

		Util::RealPart RP;
		Util::ImaginaryPart IP;

		DSP::FMDemodulation FM_a, FM_b;

		DSP::Filter FR_a, FR_b;
		DSP::SamplerPLL sampler_a, sampler_b;
		AIS::Decoder DEC_a, DEC_b;

	public:

		ModelChallenge(Device::Control* c, Connection<CFLOAT32>* i) : Model(c, i) {}
		std::vector<uint32_t> SupportedSampleRates();

		void buildModel(int, bool);
	};
}
