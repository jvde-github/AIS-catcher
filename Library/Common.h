/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include "AIS-catcher.h"

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

#ifdef DEBUG
#define DBG(msg) std::cerr << "**** " << msg << std::endl;
#else
#define DBG(msg)
#endif

class tN2kMsg;

void StopRequest();

#define GROUPS_ALL 0xFFFFFFFFFFFFFFFF
#define GROUP_OUT_UNDEFINED (1ULL << 63)

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

enum class Format
{
	CU8,
	CF32,
	CS16,
	CS8,
	TXT,
	N2K,
	BASESTATION,
	BEAST,
	RAW1090,
	UNKNOWN
};

enum class PROTOCOL
{
	NONE,
	RTLTCP,
	GPSD,
	TXT,
	MQTT,
	WS,
	WSMQTT,
	BASESTATION,
	BEAST,
	RAW1090
};

enum class Type
{
	NONE = 0,
	RTLSDR = 1,
	AIRSPYHF = 2,
	SERIALPORT = 3,
	AIRSPY = 4,
	SDRPLAY = 5,
	WAVFILE = 6,
	RAWFILE = 7,
	RTLTCP = 8,
	UDP = 9,
	HACKRF = 10,
	SOAPYSDR = 11,
	ZMQ = 12,
	SPYSERVER = 13,
	N2K = 14
};

enum class MessageFormat
{
	UNDEFINED,
	SILENT,
	NMEA,
	NMEA_TAG,
	FULL,
	JSON_NMEA,
	JSON_SPARSE,
	JSON_FULL
};

enum ShippingClass
{
	CLASS_OTHER = 0,
	CLASS_UNKNOWN = 1,
	CLASS_CARGO = 2,
	CLASS_B = 3,
	CLASS_PASSENGER = 4,
	CLASS_SPECIAL = 5,
	CLASS_TANKER = 6,
	CLASS_HIGHSPEED = 7,
	CLASS_FISHING = 8,
	CLASS_PLANE = 9,
	CLASS_HELICOPTER = 10,
	CLASS_STATION = 11,
	CLASS_ATON = 12,
	CLASS_SARTEPIRB = 13
};

enum MMSI_Class
{
	MMSI_OTHER = 0,
	MMSI_CLASS_A = 1,
	MMSI_CLASS_B = 2,
	MMSI_BASESTATION = 3,
	MMSI_SAR = 4,
	MMSI_SARTEPIRB = 5,
	MMSI_ATON = 6
};

const float DISTANCE_UNDEFINED = -1;
const float LAT_UNDEFINED = 91;
const float LON_UNDEFINED = 181;
const float COG_UNDEFINED = 360;
const float SPEED_UNDEFINED = -1;
const float DRAUGHT_UNDEFINED = -1;

const int HEADING_UNDEFINED = 511;
const int STATUS_UNDEFINED = 15;
const int DIMENSION_UNDEFINED = -1;
const int ALT_UNDEFINED = 4095;
const int ETA_DAY_UNDEFINED = 0;
const int ETA_MONTH_UNDEFINED = 0;
const int ETA_HOUR_UNDEFINED = 24;
const int ETA_MINUTE_UNDEFINED = 60;
const int IMO_UNDEFINED = 0;
const int ANGLE_UNDEFINED = -1;

const float LEVEL_UNDEFINED = 1024;
const float PPM_UNDEFINED = 1024;

const int STATUS_OK = 0;
const int STATUS_ERROR = 1;

const int HEXINDENT_DIRECT = 1;
const int HEXINDENT_IMPLIED_FROM_CRC = 2;

const int MSG_TYPE_UNDEFINED = -1;
const int DF_UNDEFINED = -1;
const std::time_t TIME_UNDEFINED = (std::time_t)0L;
const uint32_t HEXIDENT_UNDEFINED = 0;
const int ALTITUDE_UNDEFINED = -1000000;
const int VERT_RATE_UNDEFINED = -10000;
const int SQUAWK_UNDEFINED = -1;
const int AIRBORNE_UNDEFINED = 2;
const int CRC_UNDEFINED = -1;
const int CPR_POSITION_UNDEFINED = -1;

struct TAG
{
	unsigned mode = 3;
	float sample_lvl = 0;
	float level = 0;
	float ppm = 0;
	uint64_t group = 0;

	// some data flowing from DB downstream
	int version = VERSION_NUMBER;
	int status = STATUS_OK;
	int angle = -1;
	float distance = -1;
	float lat = 0, lon = 0;
	bool validated = false;
	std::time_t previous_signal = (std::time_t)0;
	int shipclass = CLASS_UNKNOWN;
	float speed = SPEED_UNDEFINED;
	std::string hardware;
	Type driver = Type::NONE;

	void clear()
	{
		driver = Type::NONE;
		hardware.clear();
		version = 0;
		group = GROUP_OUT_UNDEFINED;
		status = STATUS_OK;
		sample_lvl = LEVEL_UNDEFINED;
		level = LEVEL_UNDEFINED;
		ppm = PPM_UNDEFINED;
		lat = LAT_UNDEFINED;
		lon = LON_UNDEFINED;
		distance = DISTANCE_UNDEFINED;
		speed = SPEED_UNDEFINED;
		angle = ANGLE_UNDEFINED;
		validated = false;
		previous_signal = (std::time_t)0;
		shipclass = CLASS_UNKNOWN;
	}
};

struct RAW
{
	Format format;
	void *data;
	int size;
};

struct Setting
{
	virtual ~Setting() {}
	virtual Setting &Set(std::string option, std::string arg) { return *this; }
	virtual std::string Get() { return ""; }
};

template <typename T>
class Callback
{
public:
	virtual ~Callback() {}
	virtual void onMsg(const T &) {}
};

using namespace std::chrono;

const FLOAT32 PI = 3.14159265358979323846f;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "Logger.h"
