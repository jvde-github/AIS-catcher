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

#include <cassert>

#include "AIS-catcher.h"
#include "Model.h"

namespace AIS
{
	void ModelFrontend::buildModel(int sample_rate, bool timerOn)
	{
		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		Connection<RAW>& physical = timerOn ? (*device >> timer).out : device->out;

		switch (sample_rate)
		{

		// 2^7
		case 12288000:
			physical >> convert >> DS2_7 >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 10000000:
			US.setParams(sample_rate, 12288000);
			physical >> convert >> DS2_7 >>  DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> US >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^6
		case 6144000:
			physical >> convert >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 6000000:
			US.setParams(sample_rate, 6144000);
			physical >> convert >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> US >> DS2_1 >> ROT;
			break;

		// 2^5
		case 3072000:
			physical >> convert >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 2500000:
		case 3000000:
			US.setParams(sample_rate, 3072000);
			physical >> convert >> DS2_5 >> DS2_4 >> DS2_3 >> US >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2304K
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> convert >> DS2_3 >> DS2_2 >> DS2_1 >> US >> DSK >> ROT;
			break;
		case 2000000:
		case 1920000:
			US.setParams(sample_rate, 2304000);
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> convert >> DS2_3 >> DS2_2 >> DS2_1 >> US >> DSK >> ROT;
			break;


		// 2^4
		case 1536000:
			physical >> convert;
			if(fixedpointDS) convert.outCU8 >> DS16_Fixed >> ROT;
			else convert >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^3
		case 768000:
			physical >> convert >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^2
		case 384000:
			physical >> convert >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 288K
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> convert >> DSK >> ROT;
			break;
		case 240000:
		case 250000:
			US.setParams(sample_rate, 288000);
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> convert >> US >> DSK >> ROT;
			break;

		// 2^1
		case 192000:
			physical >> convert >> DS2_1 >> ROT;
			break;

		// 2^0
		case 96000:
			physical >> convert >> ROT;
			break;
		default:
			std::cerr << "Sample rate for decoding model set to " << sample_rate << std::endl;
			throw "Error: sample rate not supported in engine.";

		}

		ROT.up >> DS2_a >> FCIC5_a;
		ROT.down >> DS2_b >> FCIC5_b;

		// pick up point for downstream decoders
		C_a = &FCIC5_a.out;
		C_b = &FCIC5_b.out;

		return;
	}

	void ModelBase::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);
		setName("Base (non-coherent)");

		assert(C_a != NULL && C_b != NULL);

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		DEC_a.setChannel('A');
		DEC_b.setChannel('B');

		*C_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
		*C_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;

		DEC_a.DecoderMessage.Connect(sampler_a);
		DEC_b.DecoderMessage.Connect(sampler_b);

		return;
	}

	void ModelStandard::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);
		setName("Standard (non-coherent)");

		assert(C_a != NULL && C_b != NULL);

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		*C_a >> FM_a >> FR_a >> S_a;
		*C_b >> FM_b >> FR_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			DEC_a[i].setChannel('A');
			DEC_b[i].setChannel('B');

			S_a.out[i] >> DEC_a[i] >> output;
			S_b.out[i] >> DEC_b[i] >> output;

			for (int j = 0; j < nSymbolsPerSample; j++)
			{
				if (i != j)
				{
					DEC_a[i].DecoderMessage.Connect(DEC_a[j]);
					DEC_b[i].DecoderMessage.Connect(DEC_b[j]);
				}
			}
		}

		return;
	}

	void ModelPhaseSearch::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);

		assert(C_a != NULL && C_b != NULL);

		FC_a.setTaps(Filters::Coherent);
		FC_b.setTaps(Filters::Coherent);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		CD_a.resize(nSymbolsPerSample);
		CD_b.resize(nSymbolsPerSample);

		CGF_a.setParams(512,187);
		CGF_b.setParams(512,187);

		*C_a >> CGF_a >> FC_a >> S_a;
		*C_b >> CGF_b >> FC_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			DEC_a[i].setChannel('A');
			DEC_b[i].setChannel('B');

			CD_a[i].setParams(nHistory, nDelay);
			CD_b[i].setParams(nHistory, nDelay);

			S_a.out[i] >> CD_a[i] >> DEC_a[i] >> output;
			S_b.out[i] >> CD_b[i] >> DEC_b[i] >> output;

			for (int j = 0; j < nSymbolsPerSample; j++)
			{
				if (i != j)
				{
					DEC_a[i].DecoderMessage.Connect(DEC_a[j]);
					DEC_b[i].DecoderMessage.Connect(DEC_b[j]);
				}
			}
		}

		return;
	}

	void ModelPhaseSearchEMA::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);

		assert(C_a != NULL && C_b != NULL);

		FC_a.setTaps(Filters::Coherent);
		FC_b.setTaps(Filters::Coherent);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		CD_a.resize(nSymbolsPerSample);
		CD_b.resize(nSymbolsPerSample);

		CGF_a.setParams(512,187);
		CGF_b.setParams(512,187);

		*C_a >> CGF_a >> FC_a >> S_a;
		*C_b >> CGF_b >> FC_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			DEC_a[i].setChannel('A');
			DEC_b[i].setChannel('B');

			CD_a[i].setParams(nDelay);
			CD_b[i].setParams(nDelay);

			S_a.out[i] >> CD_a[i] >> DEC_a[i] >> output;
			S_b.out[i] >> CD_b[i] >> DEC_b[i] >> output;

			for (int j = 0; j < nSymbolsPerSample; j++)
			{
				if (i != j)
				{
					DEC_a[i].DecoderMessage.Connect(DEC_a[j]);
					DEC_b[i].DecoderMessage.Connect(DEC_b[j]);
				}
			}
		}

		return;
	}

	void ModelDiscriminator::buildModel(int sample_rate, bool timerOn)
	{
		setName("FM discriminator output model");

		const int nSymbolsPerSample = 48000/9600;

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		Connection<RAW>& physical = timerOn ? (*device >> timer).out : device->out;

		switch (sample_rate)
		{
		case 48000:
			physical >> convert;
			convert >> RP >> FR_a;
			convert >> IP >> FR_b;
			break;
		default:
			throw "Internal error: sample rate not supported in FM discriminator model.";
		}

		FR_a >> S_a;
		FR_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			S_a.out[i] >> DEC_a[i] >> output;
			S_b.out[i] >> DEC_b[i] >> output;

			DEC_a[i].setChannel('A');
			DEC_b[i].setChannel('B');

			for (int j = 0; j < nSymbolsPerSample; j++)
			{
				if (i != j)
				{
					DEC_a[i].DecoderMessage.Connect(DEC_a[j]);
					DEC_b[i].DecoderMessage.Connect(DEC_b[j]);
				}
			}
		}

		return;
	}
}
