/*
Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include <vector>
#include "Common.h"

namespace Filters
{
	static const std::vector <FLOAT32>  Receiver
	{
		0.00119025f, -0.00148464f, -0.00282428f, -0.00200561f, -0.00068852f,
		0.00343044f,  0.00902093f,  0.01367867f,  0.01147965f,  0.0027259f,
		-0.01766614f, -0.04244429f, -0.0577468f, -0.05245161f, -0.01072754f,
		0.0732564f,  0.17643278f,  0.25582214f,  0.28200453f,  0.25582214f,
		0.17643278f,  0.0732564f, -0.01072754f, -0.05245161f, -0.0577468f,
		-0.04244429f, -0.01766614f,  0.0027259f,  0.01147965f,  0.01367867f,
		0.00902093f,  0.00343044f, -0.00068852f, -0.00200561f, -0.00282428f,
		-0.00148464f,  0.00119025f
	};

	static const std::vector <FLOAT32>  Coherent
	{
		2.06995719e-06f, 3.18610148e-05f, 3.40605309e-04f, 2.52892989e-03f,
		1.30411453e-02f, 4.67076746e-02f, 1.16186141e-01f, 2.00730781e-01f,
		2.40861391e-01f, 2.00730781e-01f, 1.16186141e-01f, 4.67076746e-02f,
		1.30411453e-02f, 2.52892989e-03f, 3.40605309e-04f, 3.18610148e-05f,
		2.06995719e-06f
	};


	// 28  1/3 Blackman Harris
	static const std::vector <FLOAT32>  BlackmanHarris_28_3
	{
		6.32542387e-05f, -2.90015252e-04f, -1.54206250e-03f,
		-1.64972455e-03f,  3.12793899e-03f,  1.09494413e-02f,  9.04975801e-03f,
		-1.43685846e-02f, -4.45615933e-02f, -3.44883647e-02f,  5.53474269e-02f,
		2.01827915e-01f,  3.16534610e-01f,  3.16534610e-01f,  2.01827915e-01f,
		5.53474269e-02f, -3.44883647e-02f, -4.45615933e-02f, -1.43685846e-02f,
		9.04975801e-03f,  1.09494413e-02f,  3.12793899e-03f, -1.64972455e-03f,
		-1.54206250e-03f, -2.90015252e-04f,  6.32542387e-05f
	};

	// 32 1/5 Blackman Harris
	static const std::vector <FLOAT32>  BlackmanHarris_32_5
	{
		2.54561241e-05f,  2.98382002e-04f,  9.52682178e-04f,
		1.60068516e-03f,  1.12710642e-03f, -1.92265407e-03f, -8.11999274e-03f,
		-1.55260296e-02f, -1.88381809e-02f, -1.05762135e-02f,  1.54358355e-02f,
		5.97213525e-02f,  1.14554365e-01f,  1.65320349e-01f,  1.95946857e-01f,
		1.95946857e-01f,  1.65320349e-01f,  1.14554365e-01f,  5.97213525e-02f,
		1.54358355e-02f, -1.05762135e-02f, -1.88381809e-02f, -1.55260296e-02f,
		-8.11999274e-03f, -1.92265407e-03f,  1.12710642e-03f,  1.60068516e-03f,
		9.52682178e-04f,  2.98382002e-04f,  2.54561241e-05f
	};
}

