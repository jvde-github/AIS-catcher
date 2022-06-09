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

#include <chrono>
#include <complex>

#ifdef _WIN32
#include <windows.h>
#define SleepSystem(x) Sleep(x)
#else
#include <unistd.h>
#include <signal.h>
#define SleepSystem(x) usleep(x*1000)
#endif

typedef float FLOAT32;
typedef double FLOAT64;
typedef std::complex <FLOAT32> CFLOAT32;
typedef std::complex <FLOAT64> CFLOAT64;
typedef int16_t S16;
typedef std::complex <int32_t> CS32;
typedef std::complex <int16_t> CS16;
typedef std::complex <uint8_t> CU8;
typedef std::complex <int8_t> CS8;
typedef char BIT;
enum class Format { CU8, CF32, CS16, CS8, UNKNOWN };

typedef struct { std::vector<std::string> sentence; char channel; unsigned msg; unsigned mmsi; unsigned repeat; uint8_t *data; int length; } NMEA;
typedef struct { Format format; void *data; int size; } RAW;

using namespace std::chrono;

const FLOAT32 PI = 3.14159265358979323846f;

#define MAX(a,b) (a)>(b)?(a):(b)

class Setting
{
public:
	// Settings
	virtual void Print(void) {}
	virtual void Set(std::string option, std::string arg) { }
};
