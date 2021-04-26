/*
Copyright(c) 2021 Jasper van den Eshof

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

class Model
{

protected:

	std::string name;
	Device::Control* control;
	Connection<CFLOAT32>* input;
	Timer<CFLOAT32> timer; 
	PassThrough<NMEA> output;

	int sample_rate;

public:

	Model(int s, Device::Control* c, Connection<CFLOAT32>* i)
	{
		sample_rate = s;
		control = c;
		input = i;
	}

	virtual void BuildModel(bool timerOn) {}

	StreamOut<NMEA> &Output() { return output;  }

	void setName(std::string s) { name = s; }
	std::string getName() { return name; }

	float getTotalTiming() { return timer.getTotalTiming(); }
};

// Standard demodulation model

class ModelStandard : public Model
{
	DSP::SampleCounter<CFLOAT32> statistics;

	DSP::SampleCounter<CFLOAT32> stat1;
	DSP::SampleCounter<NMEA> stat2;

	DSP::Downsample3Complex DS3;
	DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4;
	DSP::Downsample2CIC5 DS2_a, DS2_b;
	DSP::FilterCIC5 filter_cic5_a, filter_cic5_b;

	DSP::RotateUp ROT_a;
	DSP::RotateDown ROT_b;

	DSP::FMDemodulation FM_a, FM_b;

	DSP::Filter FR_a, FR_b;
	DSP::PLLSampler sampler_a, sampler_b;
	AIS::Decoder DEC_a, DEC_b;


public:
	ModelStandard(int s, Device::Control* c, Connection<CFLOAT32>* i) : Model(s, c, i) {}

	void BuildModel(bool timerOn);
};


// challenger model for development purposes

class ModelChallenge: public Model
{
	DSP::SampleCounter<CFLOAT32> statistics;

	DSP::SampleCounter<CFLOAT32> stat1;
	DSP::SampleCounter<NMEA> stat2;

	DSP::Downsample3Complex DS3;
	DSP::Downsample2CIC5 DS2_1, DS2_2, DS2_3, DS2_4;
	DSP::Downsample2CIC5 DS2_a, DS2_b;
	DSP::FilterCIC5 filter_cic5_a, filter_cic5_b;

	DSP::RotateUp ROT_a;
	DSP::RotateDown ROT_b;

	DSP::FMDemodulation FM_a, FM_b;

	DSP::Filter FR_a, FR_b;
	DSP::PLLSampler sampler_a, sampler_b;
	AIS::Decoder DEC_a, DEC_b;

	DSP::DumpFile<CFLOAT32> file_in;
	DSP::DumpFile<CFLOAT32> file_out;

public:

	ModelChallenge(int s, Device::Control* c, Connection<CFLOAT32>* i) : Model(s, c, i) {}

	void BuildModel(bool timerOn);
};
