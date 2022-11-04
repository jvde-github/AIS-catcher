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

#include <string>
#include <cmath>

#include "JSON.h"

std::string JSONbuildString::jsonify(const std::string& str) {
	std::string out;
	out.reserve(str.size());
	for (char c : str) {
		if (c == '\"') out += "\\";
		out += c;
	}
	return out;
}

void JSONbuildString::Set(int p, int v) {
	if (JSONmap[p][map].empty()) return;
	json = json + delim() + "\"" + JSONmap[p][map] + "\"" + ":" + std::to_string(v);
}
void JSONbuildString::Set(int p, unsigned v) {
	if (JSONmap[p][map].empty()) return;
	json = json + delim() + "\"" + JSONmap[p][map] + "\"" + ":" + std::to_string(v);
}
void JSONbuildString::Set(int p, float v) {
	if (JSONmap[p][map].empty()) return;
	json = json + delim() + "\"" + JSONmap[p][map] + "\"" + ":" + std::to_string(v);
}
void JSONbuildString::Set(int p, bool v) {
	if (JSONmap[p][map].empty()) return;
	json = json + delim() + "\"" + JSONmap[p][map] + "\"" + ":" + (v ? "true" : "false");
}

void JSONbuildString::Set(int p, const std::string& v) {
	if (p == PROPERTY_OBJECT_START) {
		first = true;
		json = "{";
	}
	else if (p == PROPERTY_OBJECT_END) {
		if (json.size() > 1) {
			json += "}";
			Ready();
		}
	}
	else {
		if (JSONmap[p][map].empty()) return;
		json = json + delim() + "\"" + JSONmap[p][map] + "\":\"" + jsonify(v) + "\"";
	}
}

void JSONbuildString::Set(int p, const std::vector<std::string>& v) {

	if (JSONmap[p][map].empty()) return;

	json += delim() + "\"" + JSONmap[p][map] + "\":[\"" + jsonify(v[0]) + "\"";

	for (int i = 1; i < v.size(); i++)
		json += ",\"" + jsonify(v[i]) + "\"";

	json += "]";
}

const std::vector<std::vector<std::string>> JSONmap = {
	{ "", "", "", "" },
	{ "", "", "", "" },
	{ "class", "class", "class", "" },
	{ "device", "device", "device", "" },
	{ "scaled", "", "scaled", "" },
	{ "channel", "channel", "channel", "" },
	{ "signalpower", "signalpower", "signalpower", "" },
	{ "ppm", "ppm", "ppm", "" },
	{ "rxtime", "rxtime", "rxtime", "rxtime" },
	{ "nmea", "nmea", "nmea", "" },
	{ "eta", "", "eta", "" },
	{ "shiptype_text", "", "shiptype_text", "" },
	{ "aid_type_text", "", "aid_type_text", "" },
	{ "accuracy", "", "accuracy", "" },
	{ "addressed", "", "", "" },
	{ "aid_type", "", "", "" },
	{ "airtemp", "", "", "" },
	{ "ais_version", "", "", "" },
	{ "alt", "", "", "" },
	{ "assigned", "", "", "" },
	{ "band", "", "", "" },
	{ "band_a", "", "", "" },
	{ "band_b", "", "", "" },
	{ "beam", "", "", "" },
	{ "callsign", "", "callsign", "callsign" },
	{ "cdepth2", "", "", "" },
	{ "cdepth3", "", "", "" },
	{ "cdir", "", "", "" },
	{ "cdir2", "", "", "" },
	{ "cdir3", "", "", "" },
	{ "channel_a", "", "", "" },
	{ "channel_b", "", "", "" },
	{ "country", "", "", "" },
	{ "course", "", "", "course" },
	{ "course_q", "", "", "" },
	{ "cs", "", "", "" },
	{ "cspeed", "", "", "" },
	{ "cspeed2", "", "", "" },
	{ "cspeed3", "", "", "" },
	{ "dac", "", "", "" },
	{ "data", "", "", "" },
	{ "day", "", "", "" },
	{ "dest_mmsi", "", "", "" },
	{ "dest1", "", "", "" },
	{ "dest2", "", "", "" },
	{ "destination", "", "destination", "destination" },
	{ "dewpoint", "", "", "" },
	{ "display", "", "", "" },
	{ "draught", "", "", "draught" },
	{ "dsc", "", "", "" },
	{ "dte", "", "", "" },
	{ "epfd", "", "epfd", "" },
	{ "epfd_text", "", "epfd_text", "" },
	{ "fid", "", "", "" },
	{ "gnss", "", "", "" },
	{ "hazard", "", "", "" },
	{ "heading", "", "heading", "heading" },
	{ "heading_q", "", "", "" },
	{ "hour", "", "", "" },
	{ "humidity", "", "", "" },
	{ "ice", "", "", "" },
	{ "imo", "", "", "" },
	{ "increment1", "", "", "" },
	{ "increment2", "", "", "" },
	{ "increment3", "", "", "" },
	{ "increment4", "", "", "" },
	{ "interval", "", "", "" },
	{ "lat", "", "lat", "lat" },
	{ "length", "", "length", "length" },
	{ "leveltrend", "", "", "" },
	{ "loaded", "", "", "" },
	{ "lon", "", "lon", "lon" },
	{ "maneuver", "", "", "" },
	{ "minute", "", "", "" },
	{ "mmsi", "mmsi", "mmsi", "mmsi" },
	{ "mmsi1", "", "", "" },
	{ "mmsi2", "", "", "" },
	{ "mmsi3", "", "", "" },
	{ "mmsi4", "", "", "" },
	{ "mmsiseq1", "", "", "" },
	{ "mmsiseq2", "", "", "" },
	{ "mmsiseq3", "", "", "" },
	{ "mmsiseq4", "", "", "" },
	{ "model", "", "", "" },
	{ "month", "", "", "" },
	{ "mothership_mmsi", "", "", "" },
	{ "msg22", "", "", "" },
	{ "name", "", "", "" },
	{ "ne_lat", "", "", "" },
	{ "ne_lon", "", "", "" },
	{ "number1", "", "", "" },
	{ "number2", "", "", "" },
	{ "number3", "", "", "" },
	{ "number4", "", "", "" },
	{ "off_position", "", "", "" },
	{ "offset1", "", "", "" },
	{ "offset1_1", "", "", "" },
	{ "offset1_2", "", "", "" },
	{ "offset2", "", "", "" },
	{ "offset2_1", "", "", "" },
	{ "offset3", "", "", "" },
	{ "offset4", "", "", "" },
	{ "partno", "", "", "partno" },
	{ "power", "", "", "" },
	{ "preciptype", "", "", "" },
	{ "pressure", "", "", "" },
	{ "pressuretend", "", "", "" },
	{ "quiet", "", "", "" },
	{ "radio", "", "", "" },
	{ "raim", "", "", "" },
	{ "regional", "", "", "" },
	{ "repeat", "", "", "" },
	{ "reserved", "", "", "" },
	{ "retransmit", "", "", "" },
	{ "salinity", "", "", "" },
	{ "seastate", "", "", "" },
	{ "second", "", "", "" },
	{ "seqno", "", "", "" },
	{ "serial", "", "", "" },
	{ "ship_type", "", "ship_type", "" },
	{ "shipname", "", "shipname", "shipname" },
	{ "shiptype", "", "shiptype", "shiptype" },
	{ "Spare", "", "", "" },
	{ "speed", "", "speed", "speed" },
	{ "speed_q", "", "", "" },
	{ "station_type", "", "", "" },
	{ "status", "", "status", "status" },
	{ "status_text", "", "status_text", "" },
	{ "sw_lat", "", "", "" },
	{ "sw_lon", "", "", "" },
	{ "swelldir", "", "", "" },
	{ "swellheight", "", "", "" },
	{ "swellperiod", "", "", "" },
	{ "text", "", "", "" },
	{ "timeout1", "", "", "" },
	{ "timeout2", "", "", "" },
	{ "timeout3", "", "", "" },
	{ "timeout4", "", "", "" },
	{ "timestamp", "", "", "" },
	{ "to_bow", "", "to_bow", "ref_front" },
	{ "to_port", "", "to_port", "ref_left" },
	{ "to_starboard", "", "to_starboard", "" },
	{ "to_stern", "", "to_stern", "" },
	{ "turn", "", "turn", "" },
	{ "txrx", "", "", "" },
	{ "type", "", "", "msgtype" },
	{ "type1_1", "", "", "" },
	{ "type1_2", "", "", "" },
	{ "type2_1", "", "", "" },
	{ "vendorid", "", "", "vendorid" },
	{ "vin", "", "", "" },
	{ "virtual_aid", "", "", "" },
	{ "visgreater", "", "", "" },
	{ "visibility", "", "", "" },
	{ "waterlevel", "", "", "" },
	{ "watertemp", "", "", "" },
	{ "wavedir", "", "", "" },
	{ "waveheight", "", "", "" },
	{ "waveperiod", "", "", "" },
	{ "wdir", "", "", "" },
	{ "wgust", "", "", "" },
	{ "wgustdir", "", "", "" },
	{ "wspeed", "", "", "" },
	{ "year", "", "", "" },
	{ "zonesize", "", "", "" }
};