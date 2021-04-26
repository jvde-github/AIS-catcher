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

#include <vector>
#include "Common.h"

namespace Filters
{
	const std::vector<FLOAT32> CIC5 
	{ 
		0.03125, 0.15625, 0.3125 , 0.3125 , 0.15625, 0.03125 
	};

	const std::vector <FLOAT32>  Receiver
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
}

