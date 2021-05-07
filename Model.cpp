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
		return { 1536000, 768000, 384000, 288000, 48000 };
	}

	void ModelStandard::buildModel(int sample_rate, bool timerOn)
	{
		setName("AIS Engine v0.05");

		const int nSymbolsPerSample = 48000/9600;
		const float FrequencyShift = 2.0f * 3.141592653589793f * 1000.0f / 48000.0f;

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		S_a.setBuckets(nSymbolsPerSample);
		S_b.setBuckets(nSymbolsPerSample);

		DEC_a.resize(nSymbolsPerSample);
		DEC_b.resize(nSymbolsPerSample);

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		FM_a.setDCShift(+FrequencyShift);
		FM_b.setDCShift(-FrequencyShift);

		switch (sample_rate)
		{
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b;
			break;
		case 288000:
			physical >> DS3;
			DS3 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a;
			DS3 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b;
			break;
		case 48000:
			physical >> RP >> FR_a;
			physical >> IP >> FR_b;
			break;
		default:
			throw "Internal error: sample rate not supported in standard model.";
		}

		FR_a >> S_a;
		FR_b >> S_b;

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

	std::vector<uint32_t> ModelChallenge::SupportedSampleRates()
	{
		return { 48000, 288000, 384000, 768000, 1536000 };
	}

	void ModelChallenge::buildModel(int sample_rate,bool timerOn)
	{
		setName("Base model");

		const float FrequencyShift = 2.0f * 3.141592653589793f * 1000.0f / 48000.0f;

		FR_a.setTaps(Filters::Receiver);
		FR_b.setTaps(Filters::Receiver);

		DEC_a.setChannel('A');
		DEC_b.setChannel('B');

		Connection<CFLOAT32>& physical = timerOn ? (*input >> timer).out : *input;

		FM_a.setDCShift(+FrequencyShift);
		FM_b.setDCShift(-FrequencyShift);

		switch (sample_rate)
		{
		case 1536000:
			physical >> DS2_4 >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 288000:
			physical >> DS3;
			DS3 >> ROT_a >> DS2_a >> F_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS3 >> ROT_b >> DS2_b >> F_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 48000:
			physical >> RP >> FR_a >> sampler_a >> DEC_a >> output;
			physical >> IP >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		default:
			throw "Internal error: sample rate not supported in base model.";
		}

		DEC_a.DecoderMessage.Connect(sampler_a);
		DEC_b.DecoderMessage.Connect(sampler_b);

		return;
	}
}
