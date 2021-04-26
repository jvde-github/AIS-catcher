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

#include "Model.h"

namespace AIS
{
	void ModelStandard::BuildModel(bool timerOn)
	{
		setName("AIS Catcher v0.01");

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
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 288000:
			physical >> DS3;
			DS3 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS3 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		default:
			throw "Internal error: sample rate not supported in standard model.";
		}

		DEC_a.DecoderStateMessage.Connect(sampler_a);
		DEC_b.DecoderStateMessage.Connect(sampler_b);

		return;
	}

	void ModelChallenge::BuildModel(bool timerOn)
	{
		setName("Challenger model");

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
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 768000:
			physical >> DS2_3 >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 384000:
			physical >> DS2_2 >> DS2_1;
			DS2_1 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS2_1 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		case 288000:
			physical >> DS3;
			DS3 >> ROT_a >> DS2_a >> filter_cic5_a >> FM_a >> FR_a >> sampler_a >> DEC_a >> output;
			DS3 >> ROT_b >> DS2_b >> filter_cic5_b >> FM_b >> FR_b >> sampler_b >> DEC_b >> output;
			break;
		default:
			throw "Internal error: sample rate not supported in challenger model.";
		}


		DEC_a.DecoderStateMessage.Connect(sampler_a);
		DEC_b.DecoderStateMessage.Connect(sampler_b);

		return;
	}
}