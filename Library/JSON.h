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

#include <vector>
#include <iostream>

#include "Common.h"

#define JSON_DICT_FULL	  0
#define JSON_DICT_MINIMAL 1
#define JSON_DICT_SPARSE  2
#define JSON_DICT_APRS	  3

extern const std::vector<std::vector<std::string>> JSONmap;

// PropertyStream

class JSONStreamIn {
public:
	virtual void Set(int p, int v) {}
	virtual void Set(int p, unsigned v) {}
	virtual void Set(int p, float v) {}
	virtual void Set(int p, bool v) {}
	virtual void Set(int p, const std::string& v) {}
	virtual void Set(int p, const std::vector<std::string>& v) {}
};

class JSONStreamOut {
public:
	std::vector<JSONStreamIn*> connections;

	void Submit(int p, int v) {
		for (auto c : connections) c->Set(p, v);
	}
	void Submit(int p, unsigned v) {
		for (auto c : connections) c->Set(p, v);
	}
	void Submit(int p, bool v) {
		for (auto c : connections) c->Set(p, v);
	}
	void Submit(int p, float v) {
		for (auto c : connections) c->Set(p, v);
	}
	void Submit(int p, const std::string& v) {
		for (auto c : connections) c->Set(p, v);
	}
	void Submit(int p, const std::vector<std::string>& v) {
		for (auto c : connections) c->Set(p, v);
	}

	void Connect(JSONStreamIn* s) { connections.push_back(s); }
	bool isConnected() { return connections.size() > 0; }
	void Clear() { connections.resize(0); }
};

inline JSONStreamIn& operator>>(JSONStreamOut& a, JSONStreamIn& b) {
	a.Connect(&b);
	return b;
}

// takes JSON stream as input, builds a string and calls Ready() if string is available
class JSONbuildString : public JSONStreamIn {

protected:
	std::string json;

private:
	int map = JSON_DICT_FULL;
	bool first = true;

	std::string delim() {

		if (first) {
			first = false;
			return "";
		}

		return ",";
	}

	std::string jsonify(const std::string& str);
	virtual void Ready() {}

public:
	// dictionary to use
	void setMap(int m) { map = m; }

	// inherited from JSON stream
	virtual void Set(int p, int v);
	virtual void Set(int p, unsigned v);
	virtual void Set(int p, float v);
	virtual void Set(int p, bool v);
	virtual void Set(int p, const std::string& v);
	virtual void Set(int p, const std::vector<std::string>& v);
};

// JSON keys
enum JSONkeys {
	PROPERTY_OBJECT_START = 0,
	PROPERTY_OBJECT_END,
	PROPERTY_CLASS,
	PROPERTY_DEVICE,
	PROPERTY_SCALED,
	PROPERTY_CHANNEL,
	PROPERTY_SIGNAL_POWER,
	PROPERTY_PPM,
	PROPERTY_RXTIME,
	PROPERTY_NMEA,
	PROPERTY_ETA,
	PROPERTY_SHIPTYPE_TEXT,
	PROPERTY_AID_TYPE_TEXT,
	// Copy from ODS spreadsheet
	PROPERTY_ACCURACY,
	PROPERTY_ADDRESSED,
	PROPERTY_AID_TYPE,
	PROPERTY_AIRTEMP,
	PROPERTY_AIS_VERSION,
	PROPERTY_ALT,
	PROPERTY_ASSIGNED,
	PROPERTY_BAND,
	PROPERTY_BAND_A,
	PROPERTY_BAND_B,
	PROPERTY_BEAM,
	PROPERTY_CALLSIGN,
	PROPERTY_CDEPTH2,
	PROPERTY_CDEPTH3,
	PROPERTY_CDIR,
	PROPERTY_CDIR2,
	PROPERTY_CDIR3,
	PROPERTY_CHANNEL_A,
	PROPERTY_CHANNEL_B,
	PROPERTY_COURSE,
	PROPERTY_COURSE_Q,
	PROPERTY_CS,
	PROPERTY_CSPEED,
	PROPERTY_CSPEED2,
	PROPERTY_CSPEED3,
	PROPERTY_DAC,
	PROPERTY_DATA,
	PROPERTY_DAY,
	PROPERTY_DEST_MMSI,
	PROPERTY_DEST1,
	PROPERTY_DEST2,
	PROPERTY_DESTINATION,
	PROPERTY_DEWPOINT,
	PROPERTY_DISPLAY,
	PROPERTY_DRAUGHT,
	PROPERTY_DSC,
	PROPERTY_DTE,
	PROPERTY_EPFD,
	PROPERTY_EPFD_TEXT,
	PROPERTY_FID,
	PROPERTY_GNSS,
	PROPERTY_HAZARD,
	PROPERTY_HEADING,
	PROPERTY_HEADING_Q,
	PROPERTY_HOUR,
	PROPERTY_HUMIDITY,
	PROPERTY_ICE,
	PROPERTY_IMO,
	PROPERTY_INCREMENT1,
	PROPERTY_INCREMENT2,
	PROPERTY_INCREMENT3,
	PROPERTY_INCREMENT4,
	PROPERTY_INTERVAL,
	PROPERTY_LAT,
	PROPERTY_LENGTH,
	PROPERTY_LEVELTREND,
	PROPERTY_LOADED,
	PROPERTY_LON,
	PROPERTY_MANEUVER,
	PROPERTY_MINUTE,
	PROPERTY_MMSI,
	PROPERTY_MMSI1,
	PROPERTY_MMSI2,
	PROPERTY_MMSI3,
	PROPERTY_MMSI4,
	PROPERTY_MMSISEQ1,
	PROPERTY_MMSISEQ2,
	PROPERTY_MMSISEQ3,
	PROPERTY_MMSISEQ4,
	PROPERTY_MODEL,
	PROPERTY_MONTH,
	PROPERTY_MOTHERSHIP_MMSI,
	PROPERTY_MSG22,
	PROPERTY_NAME,
	PROPERTY_NE_LAT,
	PROPERTY_NE_LON,
	PROPERTY_NUMBER1,
	PROPERTY_NUMBER2,
	PROPERTY_NUMBER3,
	PROPERTY_NUMBER4,
	PROPERTY_OFF_POSITION,
	PROPERTY_OFFSET1,
	PROPERTY_OFFSET1_1,
	PROPERTY_OFFSET1_2,
	PROPERTY_OFFSET2,
	PROPERTY_OFFSET2_1,
	PROPERTY_OFFSET3,
	PROPERTY_OFFSET4,
	PROPERTY_PARTNO,
	PROPERTY_POWER,
	PROPERTY_PRECIPTYPE,
	PROPERTY_PRESSURE,
	PROPERTY_PRESSURETEND,
	PROPERTY_QUIET,
	PROPERTY_RADIO,
	PROPERTY_RAIM,
	PROPERTY_REGIONAL,
	PROPERTY_REPEAT,
	PROPERTY_RESERVED,
	PROPERTY_RETRANSMIT,
	PROPERTY_SALINITY,
	PROPERTY_SEASTATE,
	PROPERTY_SECOND,
	PROPERTY_SEQNO,
	PROPERTY_SERIAL,
	PROPERTY_SHIP_TYPE,
	PROPERTY_SHIPNAME,
	PROPERTY_SHIPTYPE,
	PROPERTY_SPARE,
	PROPERTY_SPEED,
	PROPERTY_SPEED_Q,
	PROPERTY_STATION_TYPE,
	PROPERTY_STATUS,
	PROPERTY_STATUS_TEXT,
	PROPERTY_SW_LAT,
	PROPERTY_SW_LON,
	PROPERTY_SWELLDIR,
	PROPERTY_SWELLHEIGHT,
	PROPERTY_SWELLPERIOD,
	PROPERTY_TEXT,
	PROPERTY_TIMEOUT1,
	PROPERTY_TIMEOUT2,
	PROPERTY_TIMEOUT3,
	PROPERTY_TIMEOUT4,
	PROPERTY_TIMESTAMP,
	PROPERTY_TO_BOW,
	PROPERTY_TO_PORT,
	PROPERTY_TO_STARBOARD,
	PROPERTY_TO_STERN,
	PROPERTY_TURN,
	PROPERTY_TXRX,
	PROPERTY_TYPE,
	PROPERTY_TYPE1_1,
	PROPERTY_TYPE1_2,
	PROPERTY_TYPE2_1,
	PROPERTY_VENDORID,
	PROPERTY_VIN,
	PROPERTY_VIRTUAL_AID,
	PROPERTY_VISGREATER,
	PROPERTY_VISIBILITY,
	PROPERTY_WATERLEVEL,
	PROPERTY_WATERTEMP,
	PROPERTY_WAVEDIR,
	PROPERTY_WAVEHEIGHT,
	PROPERTY_WAVEPERIOD,
	PROPERTY_WDIR,
	PROPERTY_WGUST,
	PROPERTY_WGUSTDIR,
	PROPERTY_WSPEED,
	PROPERTY_YEAR,
	PROPERTY_ZONESIZE
};
