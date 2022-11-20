/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#define SleepSystem(x) usleep(x * 1000)
#endif

typedef float FLOAT32;
typedef double FLOAT64;
typedef std::complex<FLOAT32> CFLOAT32;
typedef std::complex<FLOAT64> CFLOAT64;
typedef int16_t S16;
typedef std::complex<int32_t> CS32;
typedef std::complex<int16_t> CS16;
typedef std::complex<uint8_t> CU8;
typedef std::complex<int8_t> CS8;
typedef char BIT;

enum class Format { CU8,
					CF32,
					CS16,
					CS8,
					UNKNOWN };

enum class Type { NONE,
				  RTLSDR,
				  AIRSPYHF,
				  AIRSPY,
				  SDRPLAY,
				  WAVFILE,
				  RAWFILE,
				  RTLTCP,
				  HACKRF,
				  SOAPYSDR,
				  ZMQ,
				  SPYSERVER };

enum class OutputLevel { NONE,
						 NMEA,
						 NMEA_TAG,
						 FULL,
						 JSON_NMEA,
						 JSON_SPARSE,
						 JSON_FULL };

struct TAG {
	unsigned mode = 0;
	float sample_lvl = 0.0f;
	float level = 0.0f;
	float ppm = 0.0f;
};

struct RAW {
	Format format;
	void* data;
	int size;
};

struct Setting {
	virtual void Set(std::string option, std::string arg) {}
	virtual std::string Get() { return ""; }
};

using namespace std::chrono;

const FLOAT32 PI = 3.14159265358979323846f;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
