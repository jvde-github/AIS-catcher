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

#include <vector>
#include "Common.h"

namespace Filters
{
	static const std::vector <FLOAT32>  Receiver
	{
		0.00119025, -0.00148464, -0.00282428, -0.00200561, -0.00068852,
		0.00343044,  0.00902093,  0.01367867,  0.01147965,  0.0027259 ,
		-0.01766614, -0.04244429, -0.0577468 , -0.05245161, -0.01072754,
		0.0732564 ,  0.17643278,  0.25582214,  0.28200453,  0.25582214,
		0.17643278,  0.0732564 , -0.01072754, -0.05245161, -0.0577468 ,
		-0.04244429, -0.01766614,  0.0027259 ,  0.01147965,  0.01367867,
		0.00902093,  0.00343044, -0.00068852, -0.00200561, -0.00282428,
		-0.00148464,  0.00119025
	};

	static const std::vector <FLOAT32>  Coherent
	{
		2.06995719e-06, 3.18610148e-05, 3.40605309e-04, 2.52892989e-03,
		1.30411453e-02, 4.67076746e-02, 1.16186141e-01, 2.00730781e-01,
		2.40861391e-01, 2.00730781e-01, 1.16186141e-01, 4.67076746e-02,
		1.30411453e-02, 2.52892989e-03, 3.40605309e-04, 3.18610148e-05,
		2.06995719e-06
	};


	// 28  1/3 Blackman Harris
	static const std::vector <FLOAT32>  BlackmanHarris_28_3
	{
		6.32542387e-05, -2.90015252e-04, -1.54206250e-03,
		-1.64972455e-03,  3.12793899e-03,  1.09494413e-02,  9.04975801e-03,
		-1.43685846e-02, -4.45615933e-02, -3.44883647e-02,  5.53474269e-02,
		2.01827915e-01,  3.16534610e-01,  3.16534610e-01,  2.01827915e-01,
		5.53474269e-02, -3.44883647e-02, -4.45615933e-02, -1.43685846e-02,
		9.04975801e-03,  1.09494413e-02,  3.12793899e-03, -1.64972455e-03,
		-1.54206250e-03, -2.90015252e-04,  6.32542387e-05
	};

	// 32 1/5 Blackman Harris
	static const std::vector <FLOAT32>  BlackmanHarris_32_5
	{
		2.54561241e-05,  2.98382002e-04,  9.52682178e-04,
		1.60068516e-03,  1.12710642e-03, -1.92265407e-03, -8.11999274e-03,
		-1.55260296e-02, -1.88381809e-02, -1.05762135e-02,  1.54358355e-02,
		5.97213525e-02,  1.14554365e-01,  1.65320349e-01,  1.95946857e-01,
		1.95946857e-01,  1.65320349e-01,  1.14554365e-01,  5.97213525e-02,
		1.54358355e-02, -1.05762135e-02, -1.88381809e-02, -1.55260296e-02,
		-8.11999274e-03, -1.92265407e-03,  1.12710642e-03,  1.60068516e-03,
		9.52682178e-04,  2.98382002e-04,  2.54561241e-05
	};
}

