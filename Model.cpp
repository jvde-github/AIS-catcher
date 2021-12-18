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

#include "AIS-catcher.h"
#include "Model.h"

namespace AIS
{
	void Model::setBandwidth(int w)
	{
		Bandwidth = w;
	}

	void Model::setBandwidthFilter(std::string f)
	{
		Util::Convert::toUpper(f);

		if (f == "CIC5")
			BandwidthFilter = BandwidthFilterType::CIC5;
		else if (f == "BM")
			BandwidthFilter = BandwidthFilterType::BlackmanHarris;
		else
			throw "Model: Unknown bandwidth filter type.";
	}

	void ModelFrontend::setBandwidthFilter(BandwidthFilterType t, int bw)
	{
		switch (bw)
		{
		case 10000: FFIR_a.setTaps(Filters::BM_10000); FFIR_b.setTaps(Filters::BM_10000); break;
		case 12500: FFIR_a.setTaps(Filters::BM_12500); FFIR_b.setTaps(Filters::BM_12500); break;
		case 15000: FFIR_a.setTaps(Filters::BM_15000); FFIR_b.setTaps(Filters::BM_15000); break;
		case 16000: FFIR_a.setTaps(Filters::BM_16000); FFIR_b.setTaps(Filters::BM_16000); break;
		case 25000: FFIR_a.setTaps(Filters::BM_25000); FFIR_b.setTaps(Filters::BM_25000); break;
		default: throw "Model: bandwidth not supported for this filter.";
		}
	}

	void ModelFrontend::buildModel(int sample_rate, bool timerOn)
	{
		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{

		// 2^7
		case 12288000:
			physical >> DS2_7 >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 10000000:
			US.setParams(sample_rate, 12288000);
			physical >> DS2_7 >>  DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> US >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^6
		case 6144000:
			physical >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 6000000:
			US.setParams(sample_rate, 6144000);
			physical >> DS2_6 >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> US >> DS2_1 >> ROT;
			break;

		// 2^5
		case 3072000:
			physical >> DS2_5 >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 2500000:
		case 3000000:
			US.setParams(sample_rate, 3072000);
			physical >> DS2_5 >> DS2_4 >> DS2_3 >> US >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2304K
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> DSK >> ROT;
			break;
		case 2000000:
		case 1920000:
			US.setParams(sample_rate, 2304000);
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> US >> DSK >> ROT;
			break;


		// 2^4
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^3
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 2^2
		case 384000:
			physical >> DS2_2 >> DS2_1 >> ROT;
			break;

		// 288K
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DSK >> ROT;
			break;
		case 240000:
		case 250000:
			US.setParams(sample_rate, 288000);
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> US >> DSK >> ROT;
			break;

		// 2^1
		case 192000:
			physical >> DS2_1 >> ROT;
			break;

		// 2^0
		case 96000:
			physical >> ROT;
			break;
		default:
			throw "Internal error: sample rate not supported in engine.";
		}

		ROT.up >> DS2_a;
		ROT.down >> DS2_b;

		switch (BandwidthFilter)
		{
		case BandwidthFilterType::CIC5:
			DS2_a >> FCIC5_a >> C_a;
			DS2_b >> FCIC5_b >> C_b;
			break;
		default:
			DS2_a >> FFIR_a >> C_a;
			DS2_b >> FFIR_b >> C_b;
			setBandwidthFilter(BandwidthFilter, Bandwidth);
		}
		return;
	}

	void ModelBase::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);
		setName("Base (non-coherent)");

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		DEC_a.setChannel('A');
		DEC_b.setChannel('B');

		C_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
		C_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;

		DEC_a.DecoderMessage.Connect(sampler_a);
		DEC_b.DecoderMessage.Connect(sampler_b);

		return;
	}

	void ModelStandard::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);
		setName("Standard (non-coherent)");

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		C_a >> FM_a >> FR_a >> S_a;
		C_b >> FM_b >> FR_b >> S_b;

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

	void ModelCoherent::buildModel(int sample_rate, bool timerOn)
	{
		ModelFrontend::buildModel(sample_rate, timerOn);

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

		C_a >> CGF_a >> FC_a >> S_a;
		C_b >> CGF_b >> FC_b >> S_b;

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

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{
		case 48000:
			physical >> RP >> FR_a;
			physical >> IP >> FR_b;
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
