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

#include "Model.h"

namespace AIS
{
	std::vector<uint32_t> ModelStandard::SupportedSampleRates()
	{
		return { 1536000, 1920000, 2304000, 768000, 384000, 288000, 96000 };
	}

	void ModelStandard::buildModel(int sample_rate, bool timerOn)
	{
		setName("Standard (non-coherent)");

		const int nSymbolsPerSample = 48000/9600;

		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DSK >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1920000:
			DSK.setParams(Filters::BlackmanHarris_32_5, 5);
			physical >> DSK >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3,3);
			physical >> DSK >> ROT;
			break;
		case 96000:
			physical >> ROT;
			break;
		default:
			throw "Internal error: sample rate not supported in standard model.";
		}

		ROT.up >> DS2_a >> F_a >> FM_a >> FR_a >> S_a;
		ROT.down >> DS2_b >> F_b >> FM_b >> FR_b >> S_b;

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

	std::vector<uint32_t> ModelBase::SupportedSampleRates()
	{
		return { 1536000, 1920000, 2304000, 768000, 384000, 288000, 96000 };
	}

	void ModelBase::buildModel(int sample_rate,bool timerOn)
	{
		setName("Base (non-coherent)");

		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		DEC_a.setChannel('A');
		DEC_b.setChannel('B');

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DSK >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1920000:
			DSK.setParams(Filters::BlackmanHarris_32_5, 5);
			physical >> DSK >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3,3);
			physical >> DSK >> ROT;
			break;
		case 96000:
			physical >> ROT;
			break;
		default:
			throw "Internal error: sample rate not supported in base model.";
		}

		ROT.up >> DS2_a >> F_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
		ROT.down >> DS2_b >> F_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;

		DEC_a.DecoderMessage.Connect(sampler_a);
		DEC_b.DecoderMessage.Connect(sampler_b);

		return;
	}


	std::vector<uint32_t> ModelCoherent::SupportedSampleRates()
	{
		return { 1536000, 1920000, 2304000, 768000, 384000, 288000, 96000 };
	}

	void ModelCoherent::buildModel(int sample_rate, bool timerOn)
	{
		setName("AIS engine v0.17");

		const int nSymbolsPerSample = 48000/9600;

		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		FC_a.setTaps(Filters::Coherent);
		FC_b.setTaps(Filters::Coherent);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		CD_a.resize(nSymbolsPerSample);
		CD_b.resize(nSymbolsPerSample);

		CGF_a.setN(512,375/2);
		CGF_b.setN(512,375/2);

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DSK >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1920000:
			DSK.setParams(Filters::BlackmanHarris_32_5, 5);
			physical >> DSK >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3,3);
			physical >> DSK >> ROT;
			break;
		case 96000:
			physical >> ROT;
			break;
		default:
			throw "Internal error: sample rate not supported in default engine.";
		}

		ROT.up >> DS2_a >> F_a >> CGF_a >> FC_a >> S_a;
		ROT.down >> DS2_b >> F_b >> CGF_b >> FC_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			DEC_a[i].setChannel('A');
			DEC_b[i].setChannel('B');

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

	std::vector<uint32_t> ModelDiscriminator::SupportedSampleRates()
	{
		return { 48000 };
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

	std::vector<uint32_t> ModelChallenger::SupportedSampleRates()
	{
		return { 1536000, 1920000, 2304000, 768000, 384000, 288000, 96000 };
	}

	void ModelChallenger::buildModel(int sample_rate, bool timerOn)
	{
		setName("Challenger model (experimental)");

		const int nSymbolsPerSample = 48000 / 9600;
		ROT.setRotation((float)(PI * 25000.0 / 48000.0));

		FR_a.setTaps(Filters::Coherent);
		FR_b.setTaps(Filters::Coherent);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		CD_a.resize(nSymbolsPerSample);
		CD_b.resize(nSymbolsPerSample);

                CGF_a.setN(512,375/2);
                CGF_b.setN(512,375/2);

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		switch (sample_rate)
		{
		case 2304000:
			DSK.setParams(Filters::BlackmanHarris_28_3, 3);
			physical >> DSK >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1920000:
			DSK.setParams(Filters::BlackmanHarris_32_5, 5);
			physical >> DSK >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1 >> ROT;
			break;
		case 288000:
			DSK.setParams(Filters::BlackmanHarris_28_3,3);
			physical >> DSK >> ROT;
			break;
		case 96000:
			physical >> ROT;
			break;
		default:
			throw "Internal error: sample rate not supported in default engine.";
		}

		ROT.up >> DS2_a >> F_a >> CGF_a >> FR_a >> S_a;
		ROT.down >> DS2_b >> F_b >> CGF_b >> FR_b >> S_b;

		for (int i = 0; i < nSymbolsPerSample; i++)
		{
			S_a.out[i] >> CD_a[i] >> DEC_a[i] >> output;
			S_b.out[i] >> CD_b[i] >> DEC_b[i] >> output;

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

			DEC_a[i].DecoderMessage.Connect(CD_a[i]);
			DEC_b[i].DecoderMessage.Connect(CD_b[i]);
		}

		return;
	}
}
