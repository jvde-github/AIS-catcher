/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

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

#include "AIS.h"

// Sources:
//	https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-0-199811-S!!PDF-E.pdf
//	https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
// 	https://gpsd.gitlab.io/gpsd/AIVDM.html

#include "JSONAIS.h"
#include "AIS-catcher.h"

#include <algorithm>

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

// Below is a direct translation (more or less) of https://gpsd.gitlab.io/gpsd/AIVDM.html

namespace AIS
{

	struct COUNTRY
	{
		uint32_t MID;
		std::string country;
		std::string code;
	};

	extern const std::vector<COUNTRY> JSON_MAP_MID;

	void JSONAIS::U(const AIS::Message &msg, int p, int start, int len, unsigned undefined)
	{
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			json.Add(p, (int)u);
	}

	void JSONAIS::US(const AIS::Message &msg, int p, int start, int len, int b, unsigned undefined)
	{
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			json.Add(p, (int)(u + b));
	}

	void JSONAIS::UL(const AIS::Message &msg, int p, int start, int len, float a, float b, unsigned undefined)
	{
		unsigned u = msg.getUint(start, len);
		if (u != undefined)
			json.Add(p, u * a + b);
	}

	void JSONAIS::S(const AIS::Message &msg, int p, int start, int len, int undefined)
	{
		int u = msg.getInt(start, len);
		if (u != undefined)
			json.Add(p, u);
	}

	void JSONAIS::SL(const AIS::Message &msg, int p, int start, int len, float a, float b, int undefined)
	{
		int s = msg.getInt(start, len);
		if (s != undefined)
			json.Add(p, s * a + b);
	}

	void JSONAIS::E(const AIS::Message &msg, int p, int start, int len, int pmap)
	{
		unsigned u = msg.getUint(start, len);
		json.Add(p, (int)u);
		const std::vector<std::string> *map = KeyInfoMap[p].lookup_table;
		if (map && pmap)
		{
			if (u < map->size())
				json.Add(pmap, &(*map)[u]);
			else
				json.Add(pmap, &undefined);
		}
	}

	void JSONAIS::TURN(const AIS::Message &msg, int p, int start, int len, unsigned undefined)
	{
		int u = msg.getInt(start, len);
		json.Add(AIS::KEY_TURN_UNSCALED, u);

		if (u > -127 && u < 127)
		{
			double rot = u / 4.733;
			rot = (u < 0) ? -rot * rot : rot * rot;
			json.Add(p, (int)(rot + 0.5));
		}
		else
			json.Add(p, u); // sentinel: -128, -127, or 127
	}

	void JSONAIS::B(const AIS::Message &msg, int p, int start, int len)
	{
		unsigned u = msg.getUint(start, len);
		json.Add(p, (bool)u);
	}

	void JSONAIS::TIMESTAMP(const AIS::Message &msg, int p, int start, int len, std::string &str)
	{
		if (len != 40)
			return;

		auto put2 = [](char *d, unsigned v)
		{ d[0] = '0' + v / 10; d[1] = '0' + v % 10; };
		auto put4 = [](char *d, unsigned v)
		{ d[0] = '0' + v / 1000; d[1] = '0' + (v / 100) % 10; d[2] = '0' + (v / 10) % 10; d[3] = '0' + v % 10; };

		char buf[21]; // "YYYY-MM-DDTHH:MM:SSZ"
		put4(buf, msg.getUint(start, 14));
		buf[4] = '-';
		put2(buf + 5, msg.getUint(start + 14, 4));
		buf[7] = '-';
		put2(buf + 8, msg.getUint(start + 18, 5));
		buf[10] = 'T';
		put2(buf + 11, msg.getUint(start + 23, 5));
		buf[13] = ':';
		put2(buf + 14, msg.getUint(start + 28, 6));
		buf[16] = ':';
		put2(buf + 17, msg.getUint(start + 34, 6));
		buf[19] = 'Z';
		buf[20] = '\0';
		str.assign(buf, 20);
		json.Add(p, &str);
	}

	void JSONAIS::ETA(const AIS::Message &msg, int p, int start, int len, std::string &str)
	{
		if (len != 20)
			return;

		auto put2 = [](char *d, unsigned v)
		{ d[0] = '0' + v / 10; d[1] = '0' + v % 10; };

		char buf[15]; // "MM-DDTHH:MMZ"
		put2(buf, msg.getUint(start, 4));
		buf[2] = '-';
		put2(buf + 3, msg.getUint(start + 4, 5));
		buf[5] = 'T';
		put2(buf + 6, msg.getUint(start + 9, 5));
		buf[8] = ':';
		put2(buf + 9, msg.getUint(start + 14, 6));
		buf[11] = 'Z';
		buf[12] = '\0';
		str.assign(buf, 12);
		json.Add(p, &str);
	}

	void JSONAIS::T(const AIS::Message &msg, int p, int start, int len, std::string &str)
	{
		msg.getText(start, len, str);
		json.Add(p, &str);
	}

	void JSONAIS::D(const AIS::Message &msg, int p, int start, int len, std::string &str)
	{
		str = std::to_string(len) + ':';
		for (int i = start; i < start + len; i += 4)
		{
			char c = msg.getUint(i, 4);
			str += (char)(c < 10 ? c + '0' : c + 'a' - 10);
		}
		json.Add(p, &str);
	}

	// Reference: https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.585-9-202205-I!!PDF-E.pdf
	void JSONAIS::COUNTRY(const AIS::Message &msg)
	{

		uint32_t mid = msg.mmsi();
		while (mid > 1000)
			mid /= 10;

		if (mid == 111)
		{
			mid = msg.mmsi();
			while (mid > 1000000)
				mid /= 10;
			mid %= 1000;
		}
		else if (mid / 10 == 99 || mid / 10 == 98)
		{
			mid = msg.mmsi();
			while (mid > 100000)
				mid /= 10;
			mid %= 1000;
		}

		if (mid > 100)
		{
			auto it = std::lower_bound(JSON_MAP_MID.begin(), JSON_MAP_MID.end(), mid,
									   [](const AIS::COUNTRY &c, uint32_t m)
									   { return c.MID < m; });
			if (it != JSON_MAP_MID.end() && it->MID == mid)
			{
				json.Add(AIS::KEY_COUNTRY, &it->country);
				json.Add(AIS::KEY_COUNTRY_CODE, &it->code);
			}
		}
	}

	void JSONAIS::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			json.clear();
			ProcessMsg(data[i], tag);
			json.binary = (void *)&data[i];
			Send(&json, 1, tag);
		}
	}

	// ---------- ASM decoders (payload-relative) ----------
	// Each takes `start` = bit index of the first payload bit, i.e. first bit after the
	// message header (88 for msg 6, 56 for msg 8). All internal bit positions are
	// relative to `start` so the same decoder can be invoked from either dispatcher.

	// IALA Zeni Lite Buoy Co. proprietary (DAC=0, FID=0) — aid-to-navigation monitor.
	void JSONAIS::asm_iala_fid0_buoy_monitor(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_ASM_SUB_APP_ID, start, 16);
		UL(msg, AIS::KEY_ASM_VOLTAGE_DATA, start + 16, 12, 0.1f, 0);
		UL(msg, AIS::KEY_ASM_CURRENT_DATA, start + 28, 10, 0.1f, 0);
		B(msg, AIS::KEY_ASM_POWER_SUPPLY_TYPE, start + 38, 1);
		B(msg, AIS::KEY_ASM_LIGHT_STATUS, start + 39, 1);
		B(msg, AIS::KEY_ASM_BATTERY_STATUS, start + 40, 1);
		B(msg, AIS::KEY_ASM_OFF_POSITION_STATUS, start + 41, 1);
		X(msg, AIS::KEY_SPARE, start + 42, 6);
	}

	// ITU-R M.1371 addressed/broadcast text using 6-bit ASCII (DAC=1, FID=0).
	void JSONAIS::asm_imo_fid0_text(const AIS::Message &msg, int start)
	{
		B(msg, AIS::KEY_ACK_REQUIRED, start, 1);
		U(msg, AIS::KEY_TEXT_SEQUENCE, start + 1, 11);
		T(msg, AIS::KEY_TEXT, start + 12, MIN(924, msg.getLength() - (start + 12)), text);
	}

	// ITU-R M.1371 interrogation for a specified FMS (DAC=1, FID=2).
	void JSONAIS::asm_imo_fid2_interrogation(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_REQUESTED_DAC, start, 10);
		U(msg, AIS::KEY_REQUESTED_FID, start + 10, 6);
	}

	// ITU-R M.1371 interrogation (ext.) for a specified FMS (DAC=1, FID=3).
	void JSONAIS::asm_imo_fid3_interrogation_ext(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_REQUESTED_DAC, start, 10);
		X(msg, AIS::KEY_SPARE, start + 10, 6);
	}

	// ITU-R M.1371 capability reply — 128-bit AI-available bitstring (DAC=1, FID=4).
	void JSONAIS::asm_imo_fid4_capability_reply(const AIS::Message &msg, int start)
	{
		int bits_available = MIN(128, msg.getLength() - start);
		datastring.clear();
		for (int i = 0; i < bits_available; i++)
			datastring += msg.getUint(start + i, 1) ? '1' : '0';
		json.Add(AIS::KEY_AI_AVAILABLE, &datastring);
	}

	// ITU-R M.1371 / IMO Circ.236 — number of persons on board (DAC=1, FID=16 or 40, msg 6).
	void JSONAIS::asm_imo_fid16_persons(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_PERSONS, start, 13, 8191);
	}

	// IMO SN.1/Circ.289 Annex §14 Table 14.3 — text description, addressed (DAC=1, FID=30).
	void JSONAIS::asm_imo_fid30_text_addressed(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_LINKAGE_ID, start, 10, 0);
		int text_len = msg.getLength() - (start + 10);
		if (text_len < 6) text_len = 0;
		if (text_len > 930) text_len = 930;
		text_len -= text_len % 6;
		if (text_len > 0)
			T(msg, AIS::KEY_TEXT, start + 10, text_len, text);
	}

	// Inland ECE/TRANS/SC.3/176 — number of persons on board, detailed (DAC=200, FID=55).
	void JSONAIS::asm_inland_fid55_persons(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_CREW_COUNT, start, 8, 255);
		U(msg, AIS::KEY_PASSENGER_COUNT, start + 8, 13, 8191);
		U(msg, AIS::KEY_SHIPBOARD_PERSONNEL_COUNT, start + 21, 8, 255);
	}

	// IALA ASM — aid-to-navigation monitoring (DAC=235 UK&NI / DAC=250 ROI, FID=10).
	void JSONAIS::asm_uk_fid10_aton_monitor(const AIS::Message &msg, int start)
	{
		UL(msg, AIS::KEY_ANA_INT, start, 10, 0.05f, 0);
		UL(msg, AIS::KEY_ANA_EXT1, start + 10, 10, 0.05f, 0);
		UL(msg, AIS::KEY_ANA_EXT2, start + 20, 10, 0.05f, 0);
		U(msg, AIS::KEY_RACON, start + 30, 2);
		U(msg, AIS::KEY_HEALTH, start + 34, 1);
		U(msg, AIS::KEY_STAT_EXT, start + 35, 8);
		B(msg, AIS::KEY_OFF_POSITION, start + 43, 1);
	}

	// IALA ASM — Trinity House buoy position monitoring (DAC=235, FID=20).
	void JSONAIS::asm_uk_fid20_buoy_position(const AIS::Message &msg, int start)
	{
		T(msg, AIS::KEY_STATION_NAME, start, 204, text);
		U(msg, AIS::KEY_UTC_DAY, start + 204, 5, 0);
		U(msg, AIS::KEY_UTC_HOUR, start + 209, 5, 24);
		U(msg, AIS::KEY_UTC_MINUTE, start + 214, 6, 60);
		SL(msg, AIS::KEY_LON, start + 220, 28, 1 / 600000.0f, 0, 1810000);
		SL(msg, AIS::KEY_LAT, start + 248, 27, 1 / 600000.0f, 0, 910000);
		B(msg, AIS::KEY_OFF_POSITION, start + 275, 1);
		X(msg, AIS::KEY_SPARE, start + 276, 4);
	}

	// Saint Lawrence Seaway meteorological/hydrological messages (DAC=316 CA / 366 US, FID=1).
	// Sub-messages: 1=weather station, 2=wind, 3=water level, 6=water flow.
	void JSONAIS::asm_usa_fid1_sls_meteo(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_MESSAGE_ID, start + 2, 6);
		unsigned message_id = msg.getUint(start + 2, 6);
		int rpt = start + 8; // first report starts 8 bits into the payload

		// Common 111-bit header (timestamp + station ID + position) shared by msgs 1/2/3/6.
		auto emit_common_header = [&]() {
			U(msg, AIS::KEY_MONTH, rpt, 4, 0);
			U(msg, AIS::KEY_DAY, rpt + 4, 5, 0);
			U(msg, AIS::KEY_HOUR, rpt + 9, 5, 24);
			U(msg, AIS::KEY_MINUTE, rpt + 14, 6, 60);
			T(msg, AIS::KEY_STATION_ID, rpt + 20, 42, text);
			SL(msg, AIS::KEY_LON, rpt + 62, 25, 1 / 60000.0f, 0, 10800000);
			SL(msg, AIS::KEY_LAT, rpt + 87, 24, 1 / 60000.0f, 0, 5400000);
		};

		if (message_id == 1 && msg.getLength() >= rpt + 192)
		{
			emit_common_header();
			UL(msg, AIS::KEY_WSPEED, rpt + 111, 10, 0.1f, 0);
			UL(msg, AIS::KEY_WGUST, rpt + 121, 10, 0.1f, 0);
			U(msg, AIS::KEY_WDIR, rpt + 131, 9, 511);
			U(msg, AIS::KEY_BAROMETRIC_PRESSURE, rpt + 140, 14, 16383);
			SL(msg, AIS::KEY_AIR_TEMPERATURE, rpt + 154, 10, 0.1f, 0, -512);
			SL(msg, AIS::KEY_DEW_POINT, rpt + 164, 10, 0.1f, 0, -512);
			UL(msg, AIS::KEY_VISIBILITY_KM, rpt + 174, 8, 0.1f, 0);
			SL(msg, AIS::KEY_WATERTEMP, rpt + 182, 10, 0.1f, 0, -512);
		}
		else if (message_id == 3 && msg.getLength() >= rpt + 144)
		{
			emit_common_header();
			U(msg, AIS::KEY_WATER_LEVEL_TYPE, rpt + 111, 1);
			SL(msg, AIS::KEY_WATERLEVEL, rpt + 112, 16, 0.01f, 0, -32768);
			U(msg, AIS::KEY_REFERENCE_DATUM, rpt + 128, 2);
			U(msg, AIS::KEY_READING_TYPE, rpt + 130, 2);
			X(msg, AIS::KEY_SPARE, rpt + 132, 12);
		}
		else if (message_id == 2 && msg.getLength() >= rpt + 144)
		{
			emit_common_header();
			UL(msg, AIS::KEY_WIND_SPEED_AVG, rpt + 111, 10, 0.1f, 0);
			UL(msg, AIS::KEY_WIND_GUST_SPEED, rpt + 121, 10, 0.1f, 0);
			U(msg, AIS::KEY_WIND_DIRECTION_AVG, rpt + 131, 9, 511);
			X(msg, AIS::KEY_SPARE, rpt + 140, 4);
		}
		else if (message_id == 6 && msg.getLength() >= rpt + 144)
		{
			emit_common_header();
			U(msg, AIS::KEY_WATER_FLOW, rpt + 111, 14, 16383);
			X(msg, AIS::KEY_SPARE, rpt + 125, 19);
		}
	}

	// SLS vessel/lock scheduling (DAC=316/366, FID=2) — only message_id exposed.
	void JSONAIS::asm_usa_fid2_sls_lock(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_MESSAGE_ID, start + 2, 6);
	}

	// SLS specific messages (DAC=316/366, FID=32) — only message_id exposed.
	void JSONAIS::asm_usa_fid32_sls_specific(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_MESSAGE_ID, start + 2, 6);
	}

	// IALA ASM — VTS targets derived by non-AIS means (DAC=1, FID=16, msg 8).
	// Note: same (DAC,FID) is "persons on board" in msg 6; the spec re-uses the slot.
	void JSONAIS::asm_imo_fid16_vts_targets(const AIS::Message &msg, int start)
	{
		if (msg.getLength() < start + 120) return;
		U(msg, AIS::KEY_VTS_TARGET_ID_TYPE, start, 2);
		unsigned id_type = msg.getUint(start, 2);
		if (id_type == 2)
			T(msg, AIS::KEY_VTS_TARGET_ID, start + 2, 42, text);
		else
			U(msg, AIS::KEY_VTS_TARGET_ID, start + 2, 42);
		X(msg, AIS::KEY_SPARE, start + 44, 4);
		SL(msg, AIS::KEY_VTS_TARGET_LAT, start + 48, 24, 1 / 60000.0f, 0);
		SL(msg, AIS::KEY_VTS_TARGET_LON, start + 72, 25, 1 / 60000.0f, 0);
		U(msg, AIS::KEY_VTS_TARGET_COG, start + 97, 9, 360);
		U(msg, AIS::KEY_VTS_TARGET_TIMESTAMP, start + 106, 6, 60);
		U(msg, AIS::KEY_VTS_TARGET_SOG, start + 112, 8, 255);
	}

	// Inland ECE/TRANS/SC.3/176 — ERI ship static voyage data (DAC=200, FID=10).
	void JSONAIS::asm_inland_fid10_eri_static(const AIS::Message &msg, int start)
	{
		T(msg, AIS::KEY_VIN, start, 48, text);
		UL(msg, AIS::KEY_LENGTH, start + 48, 13, 0.1f, 0);
		UL(msg, AIS::KEY_BEAM, start + 61, 10, 0.1f, 0);
		E(msg, AIS::KEY_SHIPTYPE, start + 71, 14);
		E(msg, AIS::KEY_HAZARD, start + 85, 3);
		UL(msg, AIS::KEY_DRAUGHT, start + 88, 11, 0.01f, 0);
		E(msg, AIS::KEY_LOADED, start + 99, 2);
		B(msg, AIS::KEY_SPEED_Q, start + 101, 1);
		B(msg, AIS::KEY_COURSE_Q, start + 102, 1);
		B(msg, AIS::KEY_HEADING_Q, start + 103, 1);
	}

	// IMO SN.1/Circ.289 Annex §11 — meteorological/hydrological data (DAC=1, FID=31).
	void JSONAIS::asm_imo_fid31_meteo_hydro(const AIS::Message &msg, int start)
	{
		SL(msg, AIS::KEY_LON, start, 25, 1 / 60000.0f, 0);
		SL(msg, AIS::KEY_LAT, start + 25, 24, 1 / 60000.0f, 0);
		B(msg, AIS::KEY_ACCURACY, start + 49, 1);
		U(msg, AIS::KEY_DAY, start + 50, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 55, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 60, 6, 60);
		U(msg, AIS::KEY_WSPEED, start + 66, 7, 127);
		U(msg, AIS::KEY_WGUST, start + 73, 7, 127);
		U(msg, AIS::KEY_WDIR, start + 80, 9, 360);
		U(msg, AIS::KEY_WGUSTDIR, start + 89, 9, 360);
		SL(msg, AIS::KEY_AIRTEMP, start + 98, 11, 0.1f, 0, -1024);
		U(msg, AIS::KEY_HUMIDITY, start + 109, 7, 101);
		SL(msg, AIS::KEY_DEWPOINT, start + 116, 10, 0.1f, 0, 501);
		US(msg, AIS::KEY_PRESSURE, start + 126, 9, 799, 511);
		U(msg, AIS::KEY_PRESSURETEND, start + 135, 2, 3);
		B(msg, AIS::KEY_VISGREATER, start + 137, 1);
		UL(msg, AIS::KEY_VISIBILITY, start + 138, 7, 0.1f, 0, 127);
		UL(msg, AIS::KEY_WATERLEVEL, start + 145, 12, 0.01f, -10, 4001);
		U(msg, AIS::KEY_LEVELTREND, start + 157, 2, 3);
		UL(msg, AIS::KEY_CSPEED, start + 159, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_CDIR, start + 167, 9, 360);
		UL(msg, AIS::KEY_CSPEED2, start + 176, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_CDIR2, start + 184, 9, 360);
		U(msg, AIS::KEY_CDEPTH2, start + 193, 5, 31);
		UL(msg, AIS::KEY_CSPEED3, start + 198, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_CDIR3, start + 206, 9, 360);
		U(msg, AIS::KEY_CDEPTH3, start + 215, 5, 31);
		UL(msg, AIS::KEY_WAVEHEIGHT, start + 220, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_WAVEPERIOD, start + 228, 6, 63);
		U(msg, AIS::KEY_WAVEDIR, start + 234, 9, 360);
		UL(msg, AIS::KEY_SWELLHEIGHT, start + 243, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_SWELLPERIOD, start + 251, 6, 63);
		U(msg, AIS::KEY_SWELLDIR, start + 257, 9, 360);
		U(msg, AIS::KEY_SEASTATE, start + 266, 4);
		SL(msg, AIS::KEY_WATERTEMP, start + 270, 10, 0.1, 0, 501);
		U(msg, AIS::KEY_PRECIPTYPE, start + 280, 3, 7);
		U(msg, AIS::KEY_SALINITY, start + 283, 9, 510);
		U(msg, AIS::KEY_ICE, start + 292, 2, 3);
	}

	// Inland AIS (CCNR/CESNI) — present bridge clearance (DAC=200, FID=25). 168-bit, 1 slot.
	void JSONAIS::asm_inland_fid25_bridge_clearance(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_ASM_VERSION, start, 3);
		T(msg, AIS::KEY_UN_COUNTRY, start + 3, 12, text);
		U(msg, AIS::KEY_FAIRWAY_SECTION, start + 15, 17, 0);
		T(msg, AIS::KEY_OBJECT_CODE, start + 32, 30, name);
		U(msg, AIS::KEY_FAIRWAY_HECTOMETRE, start + 62, 17, 0);
		U(msg, AIS::KEY_BRIDGE_CLEARANCE, start + 79, 14, 0);
		U(msg, AIS::KEY_MEASUREMENT_AGE, start + 93, 10, 722);
		U(msg, AIS::KEY_CLEARANCE_ACCURACY, start + 103, 5, 0);
	}

	// IMO SN.1/Circ.289 Annex §10 Table 10.1 — weather observation from ship (DAC=1, FID=21).
	// Only variant 0 (non-WMO) is decoded; variant 1 (WMO BUFR) is not.
	void JSONAIS::asm_imo_fid21_weather_ship(const AIS::Message &msg, int start)
	{
		unsigned variant = msg.getUint(start, 1);
		U(msg, AIS::KEY_WEATHER_REPORT_TYPE, start, 1);
		if (variant != 0) return;
		T(msg, AIS::KEY_STATION_NAME, start + 1, 120, name);
		SL(msg, AIS::KEY_LON, start + 121, 25, 1 / 60000.0f, 0, 10860000);
		SL(msg, AIS::KEY_LAT, start + 146, 24, 1 / 60000.0f, 0, 5460000);
		U(msg, AIS::KEY_DAY, start + 170, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 175, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 180, 6, 60);
		U(msg, AIS::KEY_PRESENT_WEATHER, start + 186, 4, 8);
		B(msg, AIS::KEY_VISGREATER, start + 190, 1);
		UL(msg, AIS::KEY_VISIBILITY, start + 191, 7, 0.1f, 0, 127);
		U(msg, AIS::KEY_HUMIDITY, start + 198, 7, 101);
		U(msg, AIS::KEY_WSPEED, start + 205, 7, 127);
		U(msg, AIS::KEY_WDIR, start + 212, 9, 360);
		US(msg, AIS::KEY_PRESSURE, start + 221, 9, 799, 403);
		U(msg, AIS::KEY_PRESSURETEND_WMO, start + 230, 4, 15);
		SL(msg, AIS::KEY_AIRTEMP, start + 234, 11, 0.1f, 0, -1024);
		SL(msg, AIS::KEY_WATERTEMP, start + 245, 10, 0.1f, 0, 501);
		U(msg, AIS::KEY_WAVEPERIOD, start + 255, 6, 63);
		UL(msg, AIS::KEY_WAVEHEIGHT, start + 261, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_WAVEDIR, start + 269, 9, 360);
		UL(msg, AIS::KEY_SWELLHEIGHT, start + 278, 8, 0.1f, 0, 255);
		U(msg, AIS::KEY_SWELLDIR, start + 286, 9, 360);
		U(msg, AIS::KEY_SWELLPERIOD, start + 295, 6, 63);
	}

	// IMO SN.1/Circ.289 Annex §14 Table 14.1 — text description, broadcast (DAC=1, FID=29).
	void JSONAIS::asm_imo_fid29_text_description(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_LINKAGE_ID, start, 10, 0);
		int text_len = msg.getLength() - (start + 10);
		if (text_len < 6) text_len = 0;
		if (text_len > 966) text_len = 966;
		text_len -= text_len % 6;
		if (text_len > 0)
			T(msg, AIS::KEY_TEXT, start + 10, text_len, text);
	}

	// IMO SN.1/Circ.289 Annex §13 Table 13.1 — route information, broadcast (DAC=1, FID=27).
	void JSONAIS::asm_imo_fid27_route(const AIS::Message &msg, int start)
	{
		U(msg, AIS::KEY_LINKAGE_ID, start, 10, 0);
		U(msg, AIS::KEY_SENDER_CLASSIFICATION, start + 10, 3);
		U(msg, AIS::KEY_ROUTE_TYPE, start + 13, 5);
		U(msg, AIS::KEY_MONTH, start + 18, 4, 0);
		U(msg, AIS::KEY_DAY, start + 22, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 27, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 32, 6, 60);
		U(msg, AIS::KEY_DURATION_MINUTES, start + 38, 18, 262143);
		U(msg, AIS::KEY_WAYPOINT_COUNT, start + 56, 5, 0);
		unsigned n_wp = msg.getUint(start + 56, 5);
		if (n_wp > 16) n_wp = 16;
		int avail_bits = msg.getLength() - (start + 61);
		if ((int)(n_wp * 55) > avail_bits) n_wp = (avail_bits > 0 ? avail_bits / 55 : 0);
		datastring.clear();
		for (unsigned i = 0; i < n_wp; i++)
		{
			int base = start + 61 + i * 55;
			int lon_raw = msg.getInt(base, 28);
			int lat_raw = msg.getInt(base + 28, 27);
			if (i > 0) datastring += ';';
			char buf[48];
			snprintf(buf, sizeof(buf), "%.6f,%.6f",
				(double)lat_raw / 600000.0, (double)lon_raw / 600000.0);
			datastring += buf;
		}
		if (!datastring.empty())
			json.Add(AIS::KEY_WAYPOINTS, &datastring);
	}

	// IMO SN.1/Circ.289 Annex §12 Table 12.1 — environmental (DAC=1, FID=26).
	// Only the first sensor report's common header is exposed; per-sensor bodies not decoded.
	void JSONAIS::asm_imo_fid26_environmental(const AIS::Message &msg, int start)
	{
		if (msg.getLength() < start + 27) return;
		U(msg, AIS::KEY_SENSOR_REPORT_TYPE, start, 4);
		U(msg, AIS::KEY_DAY, start + 4, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 9, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 14, 6, 60);
		U(msg, AIS::KEY_SITE_ID, start + 20, 7);
	}

	// Legacy IMO SN/Circ.236 meteo/hydro (DAC=1, FID=11) — superseded by FID=31.
	void JSONAIS::asm_imo_fid11_meteo_hydro_legacy(const AIS::Message &msg, int start)
	{
		SL(msg, AIS::KEY_LAT, start, 24, 1 / 60000.0f, 0, 8388607);
		SL(msg, AIS::KEY_LON, start + 24, 25, 1 / 60000.0f, 0, 16777215);
		U(msg, AIS::KEY_DAY, start + 49, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 54, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 59, 6, 60);
		U(msg, AIS::KEY_WSPEED, start + 65, 7, 127);
		U(msg, AIS::KEY_WGUST, start + 72, 7, 127);
		U(msg, AIS::KEY_WDIR, start + 79, 9, 511);
		U(msg, AIS::KEY_WGUSTDIR, start + 88, 9, 511);
		UL(msg, AIS::KEY_AIRTEMP, start + 97, 11, 0.1f, -60.0f, 2047);
		U(msg, AIS::KEY_HUMIDITY, start + 108, 7, 127);
		UL(msg, AIS::KEY_DEWPOINT, start + 115, 10, 0.1f, -20.0f, 1023);
		US(msg, AIS::KEY_PRESSURE, start + 125, 9, 800, 511);
		U(msg, AIS::KEY_PRESSURETEND, start + 134, 2, 3);
		UL(msg, AIS::KEY_VISIBILITY, start + 136, 8, 0.1f, 0.0f, 255);
		UL(msg, AIS::KEY_WATERLEVEL, start + 144, 9, 0.1f, -10.0f, 511);
		U(msg, AIS::KEY_LEVELTREND, start + 153, 2, 3);
		UL(msg, AIS::KEY_CSPEED, start + 155, 8, 0.1f, 0.0f, 255);
		U(msg, AIS::KEY_CDIR, start + 163, 9, 511);
		UL(msg, AIS::KEY_CSPEED2, start + 172, 8, 0.1f, 0.0f, 255);
		U(msg, AIS::KEY_CDIR2, start + 180, 9, 511);
		U(msg, AIS::KEY_CDEPTH2, start + 189, 5, 31);
		UL(msg, AIS::KEY_CSPEED3, start + 194, 8, 0.1f, 0.0f, 255);
		U(msg, AIS::KEY_CDIR3, start + 202, 9, 511);
		U(msg, AIS::KEY_CDEPTH3, start + 211, 5, 31);
		UL(msg, AIS::KEY_WAVEHEIGHT, start + 216, 8, 0.1f, 0.0f, 255);
		U(msg, AIS::KEY_WAVEPERIOD, start + 224, 6, 63);
		U(msg, AIS::KEY_WAVEDIR, start + 230, 9, 511);
		UL(msg, AIS::KEY_SWELLHEIGHT, start + 239, 8, 0.1f, 0.0f, 255);
		U(msg, AIS::KEY_SWELLPERIOD, start + 247, 6, 63);
		U(msg, AIS::KEY_SWELLDIR, start + 253, 9, 511);
		U(msg, AIS::KEY_SEASTATE, start + 262, 4, 13);
		UL(msg, AIS::KEY_WATERTEMP, start + 266, 10, 0.1f, -10.0f, 1023);
		U(msg, AIS::KEY_PRECIPTYPE, start + 276, 3, 7);
		UL(msg, AIS::KEY_SALINITY, start + 279, 9, 0.1f, 0.0f, 511);
		U(msg, AIS::KEY_ICE, start + 288, 2, 3);
	}

	// U.S. Environmental Sensor Report (DAC=367, FID=33) — first sensor report only.
	void JSONAIS::asm_usa_fid33_environmental(const AIS::Message &msg, int start)
	{
		if (msg.getLength() < start + 27) return;
		int report_type = msg.getUint(start, 4);
		U(msg, AIS::KEY_REPORT_TYPE, start, 4);
		U(msg, AIS::KEY_DAY, start + 4, 5, 0);
		U(msg, AIS::KEY_HOUR, start + 9, 5, 24);
		U(msg, AIS::KEY_MINUTE, start + 14, 6, 60);
		U(msg, AIS::KEY_SITE_ID, start + 20, 7);
		if (msg.getLength() < start + 112) return;
		int body = start + 27;
		if (report_type == 0)
		{
			U(msg, AIS::KEY_VERSION, body, 6);
			SL(msg, AIS::KEY_LON, body + 6, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, body + 34, 27, 1 / 600000.0f, 0);
			U(msg, AIS::KEY_PRECISION, body + 61, 3);
			S(msg, AIS::KEY_ALT, body + 64, 12, -4096);
		}
		else if (report_type == 1)
		{
			T(msg, AIS::KEY_NAME, body, 84, text);
		}
		else if (report_type == 2)
		{
			U(msg, AIS::KEY_WSPEED, body, 7, 127);
			U(msg, AIS::KEY_WGUST, body + 7, 7, 127);
			U(msg, AIS::KEY_WDIR, body + 14, 9, 360);
			U(msg, AIS::KEY_WGUSTDIR, body + 23, 9, 360);
			U(msg, AIS::KEY_SENSOR_DESCRIPTION, body + 33, 2);
			U(msg, AIS::KEY_FORECAST_WSPEED, body + 35, 7, 127);
			U(msg, AIS::KEY_FORECAST_WGUST, body + 42, 7, 127);
			U(msg, AIS::KEY_FORECAST_WDIR, body + 49, 9, 360);
			U(msg, AIS::KEY_FORECAST_DAY, body + 58, 5, 0);
			U(msg, AIS::KEY_FORECAST_HOUR, body + 63, 5, 24);
			U(msg, AIS::KEY_FORECAST_MINUTE, body + 68, 6, 60);
			U(msg, AIS::KEY_FORECAST_DURATION, body + 74, 8, 255);
		}
		else if (report_type == 3)
		{
			U(msg, AIS::KEY_WATER_LEVEL_TYPE, body, 1);
			SL(msg, AIS::KEY_WATERLEVEL, body + 1, 16, 0.01f, 0, -32768);
			U(msg, AIS::KEY_LEVELTREND, body + 17, 2);
			U(msg, AIS::KEY_REFERENCE_DATUM, body + 19, 5);
		}
	}

	// ---------- Dispatchers ----------

	void JSONAIS::ProcessMsg6Data(const AIS::Message &msg)
	{
		const int start = 88;
		int dac = msg.getUint(72, 10);
		int fid = msg.getUint(82, 6);

		if (dac == 0 && fid == 0)                              asm_iala_fid0_buoy_monitor(msg, start);
		else if (dac == 1 && fid == 0)                         asm_imo_fid0_text(msg, start);
		else if (dac == 1 && fid == 2)                         asm_imo_fid2_interrogation(msg, start);
		else if (dac == 1 && fid == 3)                         asm_imo_fid3_interrogation_ext(msg, start);
		else if (dac == 1 && fid == 4)                         asm_imo_fid4_capability_reply(msg, start);
		else if (dac == 1 && (fid == 16 || fid == 40))         asm_imo_fid16_persons(msg, start);
		else if (dac == 1 && fid == 30)                        asm_imo_fid30_text_addressed(msg, start);
		else if (dac == 200 && fid == 55)                      asm_inland_fid55_persons(msg, start);
		else if ((dac == 235 || dac == 250) && fid == 10)      asm_uk_fid10_aton_monitor(msg, start);
		else if (dac == 235 && fid == 20)                      asm_uk_fid20_buoy_position(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 1)       asm_usa_fid1_sls_meteo(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 2)       asm_usa_fid2_sls_lock(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 32)      asm_usa_fid32_sls_specific(msg, start);
		else                                                   D(msg, AIS::KEY_DATA, start, MIN(920, msg.getLength() - start), datastring);
	}

	void JSONAIS::ProcessMsg8Data(const AIS::Message &msg)
	{
		const int start = 56;
		int dac = msg.getUint(40, 10);
		int fid = msg.getUint(50, 6);

		if (dac == 1 && fid == 0)                              asm_imo_fid0_text(msg, start);
		else if (dac == 1 && fid == 16)                        asm_imo_fid16_vts_targets(msg, start);
		else if (dac == 200 && fid == 10)                      asm_inland_fid10_eri_static(msg, start);
		else if (dac == 1 && fid == 31)                        asm_imo_fid31_meteo_hydro(msg, start);
		else if (dac == 200 && fid == 25)                      asm_inland_fid25_bridge_clearance(msg, start);
		else if (dac == 1 && fid == 21)                        asm_imo_fid21_weather_ship(msg, start);
		else if (dac == 1 && fid == 29)                        asm_imo_fid29_text_description(msg, start);
		else if (dac == 1 && fid == 27)                        asm_imo_fid27_route(msg, start);
		else if (dac == 1 && fid == 26)                        asm_imo_fid26_environmental(msg, start);
		else if (dac == 1 && fid == 11)                        asm_imo_fid11_meteo_hydro_legacy(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 1)       asm_usa_fid1_sls_meteo(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 2)       asm_usa_fid2_sls_lock(msg, start);
		else if ((dac == 316 || dac == 366) && fid == 32)      asm_usa_fid32_sls_specific(msg, start);
		else if (dac == 367 && fid == 33)                      asm_usa_fid33_environmental(msg, start);
		else                                                   D(msg, AIS::KEY_DATA, start, MIN(952, msg.getLength() - start), datastring);
	}


	void JSONAIS::ProcessRadio(const AIS::Message &msg, int start, int len)
	{
		unsigned radio_value = msg.getUint(start, len);

		if (radio_value != 0 && len == 19)
		{
			json.Add(AIS::KEY_RADIO, (int)radio_value);

			unsigned sync_state = (radio_value >> 17) & 0x03;
			json.Add(AIS::KEY_SYNC_STATE, (int)sync_state);

			unsigned slot_timeout = (radio_value >> 14) & 0x07;
			json.Add(AIS::KEY_SLOT_TIMEOUT, (int)slot_timeout);

			unsigned sub_msg = radio_value & 0x3FFF;

			if (slot_timeout == 0)
			{
				json.Add(AIS::KEY_SLOT_OFFSET, (int)sub_msg);
			}
			else if (slot_timeout == 1)
			{
				const int utc_hour = (sub_msg >> 9) & 0x1F;	  // 5 bits (13-9)
				const int utc_minute = (sub_msg >> 2) & 0x7F; // 7 bits (8-2)

				if (utc_hour < 24 && utc_minute < 60)
				{
					json.Add(AIS::KEY_UTC_HOUR, utc_hour);
					json.Add(AIS::KEY_UTC_MINUTE, utc_minute);
				}
			}
			else if (slot_timeout == 2 || slot_timeout == 4 || slot_timeout == 6)
			{
				json.Add(AIS::KEY_SLOT_NUMBER, (int)sub_msg);
			}
			else if (slot_timeout == 3 || slot_timeout == 5 || slot_timeout == 7)
			{
				json.Add(AIS::KEY_RECEIVED_STATIONS, (int)sub_msg);
			}
		}
		else
		{
			json.Add(AIS::KEY_RADIO, 0);
		}
	}
	void JSONAIS::ProcessMsg(const AIS::Message &msg, TAG &tag)
	{

		rxtime = msg.getRxTime();
		channel = std::string(1, msg.getChannel());

		json.Add(AIS::KEY_CLASS, &class_str);
		json.Add(AIS::KEY_DEVICE, &device);
		json.Add(AIS::KEY_VERSION, tag.version);
		json.Add(AIS::KEY_DRIVER, (int)tag.driver);
		json.Add(AIS::KEY_HARDWARE, &tag.hardware);

		if (tag.mode & 2)
		{
			json.Add(AIS::KEY_RXTIME, &rxtime);
			json.Add(AIS::KEY_RXUXTIME, (double)msg.getRxTimeMicros() / 1000000.0);
		}

		if (msg.getTOA())
			json.Add(AIS::KEY_TOA, (double)msg.getTOA() / 1000000.0);

		json.Add(AIS::KEY_SCALED, true);

		if (tag.error != MESSAGE_ERROR_NONE)
			json.Add(AIS::KEY_ERROR, (int)tag.error);

		json.Add(AIS::KEY_CHANNEL, &channel);

		nmea_values.clear();
		for (const auto &s : msg.sentences())
		{
			JSON::Value v;
			v.setString(const_cast<std::string *>(&s));
			nmea_values.push_back(v);
		}
		JSON::Value arr;
		arr.setArray(&nmea_values);
		json.Add(AIS::KEY_NMEA, arr);

		if (tag.mode & 1)
		{
			json.Add(AIS::KEY_SIGNAL_POWER, tag.level);
			json.Add(AIS::KEY_PPM, tag.ppm);
		}

		if (msg.getStation())
		{
			json.Add(AIS::KEY_STATION_ID, msg.getStation());
		}

		if (msg.getLength() > 0)
		{
			U(msg, AIS::KEY_TYPE, 0, 6);
			U(msg, AIS::KEY_REPEAT, 6, 2);
			U(msg, AIS::KEY_MMSI, 8, 30);

			if (tag.mode & 4)
			{
				COUNTRY(msg);
			}
		}

		switch (msg.type())
		{
		case 1:
		case 2:
		case 3:
		{
			E(msg, AIS::KEY_STATUS, 38, 4, AIS::KEY_STATUS_TEXT);
			TURN(msg, AIS::KEY_TURN, 42, 8);
			UL(msg, AIS::KEY_SPEED, 50, 10, 0.1f, 0, 1023);
			B(msg, AIS::KEY_ACCURACY, 60, 1);
			SL(msg, AIS::KEY_LON, 61, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 89, 27, 1 / 600000.0f, 0);
			UL(msg, AIS::KEY_COURSE, 116, 12, 0.1f, 0);
			U(msg, AIS::KEY_HEADING, 128, 9 /*, 511*/);
			U(msg, AIS::KEY_SECOND, 137, 6);
			E(msg, AIS::KEY_MANEUVER, 143, 2);
			X(msg, AIS::KEY_SPARE, 145, 2);
			// M.1371-6 Table 46: bit 147 is Transmit power (0=high, 1=low). Pre-M.1371-6 was part of spare.
			B(msg, AIS::KEY_POWER, 147, 1);
			B(msg, AIS::KEY_RAIM, 148, 1);

			ProcessRadio(msg, 149, MAX(MIN(19, msg.getLength() - 149), 0));
			break;
		}
		case 4:
		case 11:
			TIMESTAMP(msg, AIS::KEY_TIMESTAMP, 38, 40, timestamp);
			U(msg, AIS::KEY_YEAR, 38, 14, 0);
			U(msg, AIS::KEY_MONTH, 52, 4, 0);
			U(msg, AIS::KEY_DAY, 56, 5, 0);
			U(msg, AIS::KEY_HOUR, 61, 5, 24);
			U(msg, AIS::KEY_MINUTE, 66, 6, 60);
			U(msg, AIS::KEY_SECOND, 72, 6, 60);
			B(msg, AIS::KEY_ACCURACY, 78, 1);
			SL(msg, AIS::KEY_LON, 79, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 107, 27, 1 / 600000.0f, 0);
			E(msg, AIS::KEY_EPFD, 134, 4, AIS::KEY_EPFD_TEXT);
			// M.1371-6 Table 49: bit 138 is Transmission control flag for satellite broadcast. Pre-M.1371-6 was part of spare.
			B(msg, AIS::KEY_TRANSMISSION_CONTROL, 138, 1);
			X(msg, AIS::KEY_SPARE, 139, 9);
			B(msg, AIS::KEY_RAIM, 148, 1);

			ProcessRadio(msg, 149, MAX(MIN(19, msg.getLength() - 149), 0));
			break;
		case 5:
			U(msg, AIS::KEY_AIS_VERSION, 38, 2);
			U(msg, AIS::KEY_IMO, 40, 30);
			T(msg, AIS::KEY_CALLSIGN, 70, 42, callsign);
			T(msg, AIS::KEY_SHIPNAME, 112, 120, shipname);
			E(msg, AIS::KEY_SHIPTYPE, 232, 8, AIS::KEY_SHIPTYPE_TEXT);
			U(msg, AIS::KEY_TO_BOW, 240, 9);
			U(msg, AIS::KEY_TO_STERN, 249, 9);
			U(msg, AIS::KEY_TO_PORT, 258, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 264, 6);
			E(msg, AIS::KEY_EPFD, 270, 4, AIS::KEY_EPFD_TEXT);
			ETA(msg, AIS::KEY_ETA, 274, 20, eta);
			U(msg, AIS::KEY_MONTH, 274, 4, 0);
			U(msg, AIS::KEY_DAY, 278, 5, 0);
			U(msg, AIS::KEY_HOUR, 283, 5, 24);
			U(msg, AIS::KEY_MINUTE, 288, 6, 60);
			UL(msg, AIS::KEY_DRAUGHT, 294, 8, 0.1f, 0);
			T(msg, AIS::KEY_DESTINATION, 302, 120, destination);
			B(msg, AIS::KEY_DTE, 422, 1);
			X(msg, AIS::KEY_SPARE, 423, 1);
			break;
		case 6:
			U(msg, AIS::KEY_SEQNO, 38, 2);
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			B(msg, AIS::KEY_RETRANSMIT, 70, 1);
			X(msg, AIS::KEY_SPARE, 71, 1);
			U(msg, AIS::KEY_DAC, 72, 10);
			U(msg, AIS::KEY_FID, 82, 6);
			ProcessMsg6Data(msg);
			break;
		case 7:
		case 13:
			X(msg, AIS::KEY_SPARE, 38, 2);
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_MMSISEQ1, 70, 2);
			if (msg.getLength() <= 72)
				break;
			U(msg, AIS::KEY_MMSI2, 72, 30);
			U(msg, AIS::KEY_MMSISEQ2, 102, 2);
			if (msg.getLength() <= 104)
				break;
			U(msg, AIS::KEY_MMSI3, 104, 30);
			U(msg, AIS::KEY_MMSISEQ3, 134, 2);
			if (msg.getLength() <= 136)
				break;
			U(msg, AIS::KEY_MMSI4, 136, 30);
			U(msg, AIS::KEY_MMSISEQ4, 166, 2);
			break;
		case 8:
			U(msg, AIS::KEY_DAC, 40, 10);
			U(msg, AIS::KEY_FID, 50, 6);
			ProcessMsg8Data(msg);
			break;
		case 9:
			U(msg, AIS::KEY_ALT, 38, 12);
			U(msg, AIS::KEY_SPEED, 50, 10);
			B(msg, AIS::KEY_ACCURACY, 60, 1);
			SL(msg, AIS::KEY_LON, 61, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 89, 27, 1 / 600000.0f, 0);
			UL(msg, AIS::KEY_COURSE, 116, 12, 0.1f, 0);
			U(msg, AIS::KEY_SECOND, 128, 6);
			// M.1371-6 Table 57: bit 134 is Altitude sensor (0=GNSS, 1=barometric); 135-141 is spare. Pre-M.1371-6 was 8-bit regional reserved.
			B(msg, AIS::KEY_ALT_SENSOR, 134, 1);
			X(msg, AIS::KEY_SPARE, 135, 7);
			B(msg, AIS::KEY_DTE, 142, 1);
			B(msg, AIS::KEY_ASSIGNED, 146, 1);
			B(msg, AIS::KEY_RAIM, 147, 1);
			U(msg, AIS::KEY_RADIO, 148, 20);
			break;
		case 10:
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			break;
		case 12:
			U(msg, AIS::KEY_SEQNO, 38, 2);
			U(msg, AIS::KEY_DEST_MMSI, 40, 30);
			B(msg, AIS::KEY_RETRANSMIT, 70, 1);
			T(msg, AIS::KEY_TEXT, 72, MIN(936, msg.getLength() - 72), text);
			break;
		case 14:
			T(msg, AIS::KEY_TEXT, 40, MIN(968, msg.getLength() - 40), text);
			break;
		case 15:
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_TYPE1_1, 70, 6);
			U(msg, AIS::KEY_OFFSET1_1, 76, 12);
			if (msg.getLength() <= 90)
				break;
			U(msg, AIS::KEY_TYPE1_2, 90, 6);
			U(msg, AIS::KEY_OFFSET1_2, 96, 12);
			if (msg.getLength() <= 110)
				break;
			U(msg, AIS::KEY_MMSI2, 110, 30);
			U(msg, AIS::KEY_TYPE2_1, 140, 6);
			U(msg, AIS::KEY_OFFSET2_1, 146, 12);
			break;
		case 16:
			U(msg, AIS::KEY_MMSI1, 40, 30);
			U(msg, AIS::KEY_OFFSET1, 70, 12);
			U(msg, AIS::KEY_INCREMENT1, 82, 10);
			if (msg.getLength() == 92)
				break;
			U(msg, AIS::KEY_MMSI2, 92, 30);
			U(msg, AIS::KEY_OFFSET2, 122, 12);
			U(msg, AIS::KEY_INCREMENT2, 134, 10);
			break;
		case 17:
			SL(msg, AIS::KEY_LON, 40, 18, 1 / 600.0f, 0);
			SL(msg, AIS::KEY_LAT, 58, 17, 1 / 600.0f, 0);
			D(msg, AIS::KEY_DATA, 80, MIN(736, msg.getLength() - 80), datastring);
			break;
		case 18:
			UL(msg, AIS::KEY_SPEED, 46, 10, 0.1f, 0);
			B(msg, AIS::KEY_ACCURACY, 56, 1);
			SL(msg, AIS::KEY_LON, 57, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 85, 27, 1 / 600000.0f, 0);
			UL(msg, AIS::KEY_COURSE, 112, 12, 0.1f, 0);
			U(msg, AIS::KEY_HEADING, 124, 9);
			U(msg, AIS::KEY_RESERVED, 38, 8);
			U(msg, AIS::KEY_SECOND, 133, 6);
			// M.1371-6 Table 68: bit 139 is Transmit power (0=high, 1=low); 140 is spare. Pre-M.1371-6 was 2-bit regional reserved.
			B(msg, AIS::KEY_POWER, 139, 1);
			X(msg, AIS::KEY_SPARE, 140, 1);
			B(msg, AIS::KEY_CS, 141, 1);
			B(msg, AIS::KEY_DISPLAY, 142, 1);
			B(msg, AIS::KEY_DSC, 143, 1);
			B(msg, AIS::KEY_BAND, 144, 1);
			B(msg, AIS::KEY_MSG22, 145, 1);
			B(msg, AIS::KEY_ASSIGNED, 146, 1);
			B(msg, AIS::KEY_RAIM, 147, 1);
			U(msg, AIS::KEY_RADIO, 148, 20);
			break;
		case 19:
			UL(msg, AIS::KEY_SPEED, 46, 10, 0.1, 0);
			SL(msg, AIS::KEY_LON, 57, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 85, 27, 1 / 600000.0f, 0);
			UL(msg, AIS::KEY_COURSE, 112, 12, 0.1f, 0);
			U(msg, AIS::KEY_HEADING, 124, 9);
			T(msg, AIS::KEY_SHIPNAME, 143, 120, shipname);
			E(msg, AIS::KEY_SHIPTYPE, 263, 8, AIS::KEY_SHIPTYPE_TEXT);
			U(msg, AIS::KEY_TO_BOW, 271, 9);
			U(msg, AIS::KEY_TO_STERN, 280, 9);
			U(msg, AIS::KEY_TO_PORT, 289, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 295, 6);
			E(msg, AIS::KEY_EPFD, 301, 4, AIS::KEY_EPFD_TEXT);
			B(msg, AIS::KEY_ACCURACY, 56, 1);
			X(msg, AIS::KEY_SPARE, 38, 8);
			U(msg, AIS::KEY_SECOND, 133, 6);
			// M.1371-6 Table 69: bits 139-142 are spare (pre-M.1371-6 was 4-bit regional reserved).
			X(msg, AIS::KEY_SPARE, 139, 4);
			B(msg, AIS::KEY_RAIM, 305, 1);
			B(msg, AIS::KEY_DTE, 306, 1);
			B(msg, AIS::KEY_ASSIGNED, 307, 1);
			X(msg, AIS::KEY_SPARE, 308, 4);
			break;
		case 20:
			U(msg, AIS::KEY_OFFSET1, 40, 12);
			U(msg, AIS::KEY_NUMBER1, 52, 4);
			U(msg, AIS::KEY_TIMEOUT1, 56, 3);
			U(msg, AIS::KEY_INCREMENT1, 59, 11);
			if (msg.getLength() <= 99)
				break;
			U(msg, AIS::KEY_OFFSET2, 70, 12);
			U(msg, AIS::KEY_NUMBER2, 82, 4);
			U(msg, AIS::KEY_TIMEOUT2, 86, 3);
			U(msg, AIS::KEY_INCREMENT2, 89, 11);
			if (msg.getLength() <= 129)
				break;
			U(msg, AIS::KEY_OFFSET3, 100, 12);
			U(msg, AIS::KEY_NUMBER3, 112, 4);
			U(msg, AIS::KEY_TIMEOUT3, 116, 3);
			U(msg, AIS::KEY_INCREMENT3, 119, 11);
			if (msg.getLength() <= 159)
				break;
			U(msg, AIS::KEY_OFFSET4, 130, 12);
			U(msg, AIS::KEY_NUMBER4, 142, 4);
			U(msg, AIS::KEY_TIMEOUT4, 146, 3);
			U(msg, AIS::KEY_INCREMENT4, 149, 11);
			break;
		case 21:
			E(msg, AIS::KEY_AID_TYPE, 38, 5, AIS::KEY_AID_TYPE_TEXT);
			T(msg, AIS::KEY_NAME, 43, 120, name);
			B(msg, AIS::KEY_ACCURACY, 163, 1);
			SL(msg, AIS::KEY_LON, 164, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 192, 27, 1 / 600000.0f, 0);
			U(msg, AIS::KEY_TO_BOW, 219, 9);
			U(msg, AIS::KEY_TO_STERN, 228, 9);
			U(msg, AIS::KEY_TO_PORT, 237, 6);
			U(msg, AIS::KEY_TO_STARBOARD, 243, 6);
			E(msg, AIS::KEY_EPFD, 249, 4, AIS::KEY_EPFD_TEXT);
			U(msg, AIS::KEY_SECOND, 253, 6);
			B(msg, AIS::KEY_OFF_POSITION, 259, 1);
			// M.1371-6 Table 71: bits 260-267 are AtoN status (per IALA R0126). Pre-M.1371-6 referenced as regional.
			U(msg, AIS::KEY_ATON_STATUS, 260, 8);
			B(msg, AIS::KEY_RAIM, 268, 1);
			B(msg, AIS::KEY_VIRTUAL_AID, 269, 1);
			B(msg, AIS::KEY_ASSIGNED, 270, 1);
			break;
		case 22:
			U(msg, AIS::KEY_CHANNEL_A, 40, 12);
			U(msg, AIS::KEY_CHANNEL_B, 52, 12);
			U(msg, AIS::KEY_TXRX, 64, 4);
			B(msg, AIS::KEY_POWER, 68, 1);
			if (msg.getUint(139, 1))
			{
				U(msg, AIS::KEY_DEST1, 69, 30);	 // check // if addressed is 1
				U(msg, AIS::KEY_DEST2, 104, 30); // check
			}
			else
			{
				SL(msg, AIS::KEY_NE_LON, 69, 18, 1.0 / 600.0f, 0);
				SL(msg, AIS::KEY_NE_LAT, 87, 17, 1.0 / 600.0f, 0);
				SL(msg, AIS::KEY_SW_LON, 104, 18, 1.0 / 600.0f, 0);
				SL(msg, AIS::KEY_SW_LAT, 122, 17, 1.0 / 600.0f, 0);
			}
			B(msg, AIS::KEY_ADDRESSED, 139, 1);
			B(msg, AIS::KEY_BAND_A, 140, 1);
			B(msg, AIS::KEY_BAND_B, 141, 1);
			U(msg, AIS::KEY_ZONESIZE, 142, 3);
			break;
		case 23:
			SL(msg, AIS::KEY_NE_LON, 40, 18, 1.0 / 600.0f, 0);
			SL(msg, AIS::KEY_NE_LAT, 58, 17, 1.0 / 600.0f, 0);
			SL(msg, AIS::KEY_SW_LON, 75, 18, 1.0 / 600.0f, 0);
			SL(msg, AIS::KEY_SW_LAT, 93, 17, 1.0 / 600.0f, 0);
			E(msg, AIS::KEY_STATION_TYPE, 110, 4);
			E(msg, AIS::KEY_SHIPTYPE, 114, 8, AIS::KEY_SHIPTYPE_TEXT);
			U(msg, AIS::KEY_TXRX, 144, 2);
			E(msg, AIS::KEY_INTERVAL, 146, 4);
			U(msg, AIS::KEY_QUIET, 150, 4);
			break;
		case 24:
			U(msg, AIS::KEY_PARTNO, 38, 2);

			if (msg.getUint(38, 2) == 0)
			{
				T(msg, AIS::KEY_SHIPNAME, 40, 120, shipname);
			}
			else
			{
				E(msg, AIS::KEY_SHIPTYPE, 40, 8, AIS::KEY_SHIPTYPE_TEXT);
				T(msg, AIS::KEY_VENDORID, 48, 18, vendorid);
				U(msg, AIS::KEY_MODEL, 66, 4);
				U(msg, AIS::KEY_SERIAL, 70, 20);
				T(msg, AIS::KEY_CALLSIGN, 90, 42, callsign);
				if (msg.mmsi() / 10000000 == 98)
				{
					U(msg, AIS::KEY_MOTHERSHIP_MMSI, 132, 30);
				}
				else
				{
					U(msg, AIS::KEY_TO_BOW, 132, 9);
					U(msg, AIS::KEY_TO_STERN, 141, 9);
					U(msg, AIS::KEY_TO_PORT, 150, 6);
					U(msg, AIS::KEY_TO_STARBOARD, 156, 6);
				}
				// M.1371-6 extends Part B from 162 to 168 bits with EPFD + VDES capabilities.
				if (msg.getLength() >= 168)
				{
					E(msg, AIS::KEY_EPFD, 162, 4, AIS::KEY_EPFD_TEXT);
					U(msg, AIS::KEY_VDES_CAPABILITIES, 166, 2);
				}
			}
			break;
		case 25:
		{
			// Table 79: Single-slot binary message
			B(msg, AIS::KEY_ADDRESSED, 38, 1);
			B(msg, AIS::KEY_AI_AVAILABLE, 39, 1);
			int p = 40;
			if (msg.getUint(38, 1))
			{
				U(msg, AIS::KEY_DEST_MMSI, 40, 30);
				X(msg, AIS::KEY_SPARE, 70, 2);
				p = 72;
			}
			if (msg.getUint(39, 1))
			{
				U(msg, AIS::KEY_DAC, p, 10);
				U(msg, AIS::KEY_FID, p + 10, 6);
			}
			break;
		}
		case 26:
		{
			// Table 81: Multi-slot binary message with comm state
			B(msg, AIS::KEY_ADDRESSED, 38, 1);
			B(msg, AIS::KEY_AI_AVAILABLE, 39, 1);
			int p = 40;
			if (msg.getUint(38, 1))
			{
				U(msg, AIS::KEY_DEST_MMSI, 40, 30);
				X(msg, AIS::KEY_SPARE, 70, 2);
				p = 72;
			}
			if (msg.getUint(39, 1))
			{
				U(msg, AIS::KEY_DAC, p, 10);
				U(msg, AIS::KEY_FID, p + 10, 6);
			}
			// Trailing 19-bit communication state preceded by 1-bit selector flag and 4-bit spare.
			int comm_start = msg.getLength() - 20;
			if (comm_start >= 40)
				ProcessRadio(msg, comm_start, 20);
			break;
		}
		case 27:
			U(msg, AIS::KEY_ACCURACY, 38, 1);
			U(msg, AIS::KEY_RAIM, 39, 1);
			E(msg, AIS::KEY_STATUS, 40, 4, AIS::KEY_STATUS_TEXT);
			SL(msg, AIS::KEY_LON, 44, 18, 1 / 600.0f, 0);
			SL(msg, AIS::KEY_LAT, 62, 17, 1 / 600.0f, 0);
			U(msg, AIS::KEY_SPEED, 79, 6);
			U(msg, AIS::KEY_COURSE, 85, 9);
			U(msg, AIS::KEY_GNSS, 94, 1);
			break;
		case 28:
			// AtoN Report (single-slot) per ITU-R M.1371-6 §A7-3.26, Table 84
			U(msg, AIS::KEY_SECOND, 38, 6);
			SL(msg, AIS::KEY_LON, 44, 28, 1 / 600000.0f, 0);
			SL(msg, AIS::KEY_LAT, 72, 27, 1 / 600000.0f, 0);
			U(msg, AIS::KEY_RESTRICTED_USE, 99, 2);
			U(msg, AIS::KEY_ATON_STATION_TYPE, 101, 3);
			// station_type==4 means Virtual AtoN — mirror msg 21's virtual_aid flag.
			json.Add(AIS::KEY_VIRTUAL_AID, (bool)(msg.getUint(101, 3) == 4));
			E(msg, AIS::KEY_AID_TYPE, 104, 7, AIS::KEY_AID_TYPE_TEXT);
			U(msg, AIS::KEY_IALA_MRN, 111, 17);
			U(msg, AIS::KEY_DIM_TYPE, 128, 4);
			U(msg, AIS::KEY_TO_BOW, 132, 9);
			U(msg, AIS::KEY_TO_STERN, 141, 11);
			B(msg, AIS::KEY_ADDITIONAL_FLAG, 152, 1);
			B(msg, AIS::KEY_CHARTED_STATUS, 153, 1);
			U(msg, AIS::KEY_ON_STATION_STATUS, 154, 4);
			U(msg, AIS::KEY_ATON_STATUS, 158, 8);
			X(msg, AIS::KEY_SPARE, 166, 1);
			B(msg, AIS::KEY_AUTH_FLAG, 167, 1);
			break;
		default:
			break;
		}
	}

	// Country MID table sourced from:
	// https://help.marinetraffic.com/hc/en-us/articles/360018392858-How-does-MarineTraffic-identify-a-vessel-s-country-and-flag-
	// spellchecker:off
	const std::vector<COUNTRY> JSON_MAP_MID = {
		{201, "Albania", "AL"},
		{202, "Andorra", "AD"},
		{203, "Austria", "AT"},
		{204, "Portugal", "PT"},
		{205, "Belgium", "BE"},
		{206, "Belarus", "BY"},
		{207, "Bulgaria", "BG"},
		{208, "Vatican", "VA"},
		{209, "Cyprus", "CY"},
		{210, "Cyprus", "CY"},
		{211, "Germany", "DE"},
		{212, "Cyprus", "CY"},
		{213, "Georgia", "GE"},
		{214, "Moldova", "MD"},
		{215, "Malta", "MT"},
		{216, "Armenia", "AM"},
		{218, "Germany", "DE"},
		{219, "Denmark", "DK"},
		{220, "Denmark", "DK"},
		{224, "Spain", "ES"},
		{225, "Spain", "ES"},
		{226, "France", "FR"},
		{227, "France", "FR"},
		{228, "France", "FR"},
		{229, "Malta", "MT"},
		{230, "Finland", "FI"},
		{231, "Faroe Is", "FO"},
		{232, "United Kingdom", "GB"},
		{233, "United Kingdom", "GB"},
		{234, "United Kingdom", "GB"},
		{235, "United Kingdom", "GB"},
		{236, "Gibraltar", "GI"},
		{237, "Greece", "GR"},
		{238, "Croatia", "HR"},
		{239, "Greece", "GR"},
		{240, "Greece", "GR"},
		{241, "Greece", "GR"},
		{242, "Morocco", "MA"},
		{243, "Hungary", "HU"},
		{244, "Netherlands", "NL"},
		{245, "Netherlands", "NL"},
		{246, "Netherlands", "NL"},
		{247, "Italy", "IT"},
		{248, "Malta", "MT"},
		{249, "Malta", "MT"},
		{250, "Ireland", "IE"},
		{251, "Iceland", "IS"},
		{252, "Liechtenstein", "LI"},
		{253, "Luxembourg", "LU"},
		{254, "Monaco", "MC"},
		{255, "Portugal", "PT"},
		{256, "Malta", "MT"},
		{257, "Norway", "NO"},
		{258, "Norway", "NO"},
		{259, "Norway", "NO"},
		{261, "Poland", "PL"},
		{262, "Montenegro", "ME"},
		{263, "Portugal", "PT"},
		{264, "Romania", "RO"},
		{265, "Sweden", "SE"},
		{266, "Sweden", "SE"},
		{267, "Slovakia", "SK"},
		{268, "San Marino", "SM"},
		{269, "Switzerland", "CH"},
		{270, "Czech Republic", "CZ"},
		{271, "Turkey", "TR"},
		{272, "Ukraine", "UA"},
		{273, "Russia", "RU"},
		{274, "FYR Macedonia", "MK"},
		{275, "Latvia", "LV"},
		{276, "Estonia", "EE"},
		{277, "Lithuania", "LT"},
		{278, "Slovenia", "SI"},
		{279, "Serbia", "RS"},
		{301, "Anguilla", "AI"},
		{303, "USA", "US"},
		{304, "Antigua Barbuda", "AG"},
		{305, "Antigua Barbuda", "AG"},
		{306, "Curacao", "CW"},
		{307, "Aruba", "AW"},
		{308, "Bahamas", "BS"},
		{309, "Bahamas", "BS"},
		{310, "Bermuda", "BM"},
		{311, "Bahamas", "BS"},
		{312, "Belize", "BZ"},
		{314, "Barbados", "BB"},
		{316, "Canada", "CA"},
		{319, "Cayman Is", "KY"},
		{321, "Costa Rica", "CR"},
		{323, "Cuba", "CU"},
		{325, "Dominica", "DM"},
		{327, "Dominican Rep", "DO"},
		{329, "Guadeloupe", "GP"},
		{330, "Grenada", "GD"},
		{331, "Greenland", "GL"},
		{332, "Guatemala", "GT"},
		{334, "Honduras", "HN"},
		{336, "Haiti", "HT"},
		{338, "USA", "US"},
		{339, "Jamaica", "JM"},
		{341, "St Kitts Nevis", "KN"},
		{343, "St Lucia", "LC"},
		{345, "Mexico", "MX"},
		{347, "Martinique", "MQ"},
		{348, "Montserrat", "MS"},
		{350, "Nicaragua", "NI"},
		{351, "Panama", "PA"},
		{352, "Panama", "PA"},
		{353, "Panama", "PA"},
		{354, "Panama", "PA"},
		{355, "Panama", "PA"},
		{356, "Panama", "PA"},
		{357, "Panama", "PA"},
		{358, "Puerto Rico", "PR"},
		{359, "El Salvador", "SV"},
		{361, "St Pierre Miquelon", "PM"},
		{362, "Trinidad Tobago", "TT"},
		{364, "Turks Caicos Is", "TC"},
		{366, "USA", "US"},
		{367, "USA", "US"},
		{368, "USA", "US"},
		{369, "USA", "US"},
		{370, "Panama", "PA"},
		{371, "Panama", "PA"},
		{372, "Panama", "PA"},
		{373, "Panama", "PA"},
		{374, "Panama", "PA"},
		{375, "St Vincent Grenadines", "VC"},
		{376, "St Vincent Grenadines", "VC"},
		{377, "St Vincent Grenadines", "VC"},
		{378, "British Virgin Is", "VG"},
		{379, "US Virgin Is", "VI"},
		{401, "Afghanistan", "AF"},
		{403, "Saudi Arabia", "SA"},
		{405, "Bangladesh", "BD"},
		{408, "Bahrain", "BH"},
		{410, "Bhutan", "BT"},
		{412, "China", "CN"},
		{413, "China", "CN"},
		{414, "China", "CN"},
		{416, "Taiwan", "TW"},
		{417, "Sri Lanka", "LK"},
		{419, "India", "IN"},
		{422, "Iran", "IR"},
		{423, "Azerbaijan", "AZ"},
		{425, "Iraq", "IQ"},
		{428, "Israel", "IL"},
		{431, "Japan", "JP"},
		{432, "Japan", "JP"},
		{434, "Turkmenistan", "TM"},
		{436, "Kazakhstan", "KZ"},
		{437, "Uzbekistan", "UZ"},
		{438, "Jordan", "JO"},
		{440, "Korea", "KR"},
		{441, "Korea", "KR"},
		{443, "Palestine", "PS"},
		{445, "DPR Korea", "KP"},
		{447, "Kuwait", "KW"},
		{450, "Lebanon", "LB"},
		{451, "Kyrgyz Republic", "KG"},
		{453, "Macao", "MO"},
		{455, "Maldives", "MV"},
		{457, "Mongolia", "MN"},
		{459, "Nepal", "NP"},
		{461, "Oman", "OM"},
		{463, "Pakistan", "PK"},
		{466, "Qatar", "QA"},
		{468, "Syria", "SY"},
		{470, "UAE", "AE"},
		{471, "UAE", "AE"},
		{472, "Tajikistan", "TJ"},
		{473, "Yemen", "YE"},
		{475, "Yemen", "YE"},
		{477, "Hong Kong", "HK"},
		{478, "Bosnia and Herzegovina", "BA"},
		{501, "Antarctica", "AQ"},
		{503, "Australia", "AU"},
		{506, "Myanmar", "MM"},
		{508, "Brunei", "BN"},
		{510, "Micronesia", "FM"},
		{511, "Palau", "PW"},
		{512, "New Zealand", "NZ"},
		{514, "Cambodia", "KH"},
		{515, "Cambodia", "KH"},
		{516, "Christmas Is", "CX"},
		{518, "Cook Is", "CK"},
		{520, "Fiji", "FJ"},
		{523, "Cocos Is", "CC"},
		{525, "Indonesia", "ID"},
		{529, "Kiribati", "KI"},
		{531, "Laos", "LA"},
		{533, "Malaysia", "MY"},
		{536, "N Mariana Is", "MP"},
		{538, "Marshall Is", "MH"},
		{540, "New Caledonia", "NC"},
		{542, "Niue", "NU"},
		{544, "Nauru", "NR"},
		{546, "French Polynesia", "PF"},
		{548, "Philippines", "PH"},
		{553, "Papua New Guinea", "PG"},
		{555, "Pitcairn Is", "PN"},
		{557, "Solomon Is", "SB"},
		{559, "American Samoa", "AS"},
		{561, "Samoa", "WS"},
		{563, "Singapore", "SG"},
		{564, "Singapore", "SG"},
		{565, "Singapore", "SG"},
		{566, "Singapore", "SG"},
		{567, "Thailand", "TH"},
		{570, "Tonga", "TO"},
		{572, "Tuvalu", "TV"},
		{574, "Vietnam", "VN"},
		{576, "Vanuatu", "VU"},
		{577, "Vanuatu", "VU"},
		{578, "Wallis Futuna Is", "WF"},
		{601, "South Africa", "ZA"},
		{603, "Angola", "AO"},
		{605, "Algeria", "DZ"},
		{607, "St Paul Amsterdam Is", "TF"},
		{608, "Ascension Is", "IO"},
		{609, "Burundi", "BI"},
		{610, "Benin", "BJ"},
		{611, "Botswana", "BW"},
		{612, "Cen Afr Rep", "CF"},
		{613, "Cameroon", "CM"},
		{615, "Congo", "CG"},
		{616, "Comoros", "KM"},
		{617, "Cape Verde", "CV"},
		{618, "Antarctica", "AQ"},
		{619, "Ivory Coast", "CI"},
		{620, "Comoros", "KM"},
		{621, "Djibouti", "DJ"},
		{622, "Egypt", "EG"},
		{624, "Ethiopia", "ET"},
		{625, "Eritrea", "ER"},
		{626, "Gabon", "GA"},
		{627, "Ghana", "GH"},
		{629, "Gambia", "GM"},
		{630, "Guinea-Bissau", "GW"},
		{631, "Equ. Guinea", "GQ"},
		{632, "Guinea", "GN"},
		{633, "Burkina Faso", "BF"},
		{634, "Kenya", "KE"},
		{635, "Antarctica", "AQ"},
		{636, "Liberia", "LR"},
		{637, "Liberia", "LR"},
		{642, "Libya", "LY"},
		{644, "Lesotho", "LS"},
		{645, "Mauritius", "MU"},
		{647, "Madagascar", "MG"},
		{649, "Mali", "ML"},
		{650, "Mozambique", "MZ"},
		{654, "Mauritania", "MR"},
		{655, "Malawi", "MW"},
		{656, "Niger", "NE"},
		{657, "Nigeria", "NG"},
		{659, "Namibia", "NA"},
		{660, "Reunion", "RE"},
		{661, "Rwanda", "RW"},
		{662, "Sudan", "SD"},
		{663, "Senegal", "SN"},
		{664, "Seychelles", "SC"},
		{665, "St Helena", "SH"},
		{666, "Somalia", "SO"},
		{667, "Sierra Leone", "SL"},
		{668, "Sao Tome Principe", "ST"},
		{669, "Swaziland", "SZ"},
		{670, "Chad", "TD"},
		{671, "Togo", "TG"},
		{672, "Tunisia", "TN"},
		{674, "Tanzania", "TZ"},
		{675, "Uganda", "UG"},
		{676, "DR Congo", "CD"},
		{677, "Tanzania", "TZ"},
		{678, "Zambia", "ZM"},
		{679, "Zimbabwe", "ZW"},
		{701, "Argentina", "AR"},
		{710, "Brazil", "BR"},
		{720, "Bolivia", "BO"},
		{725, "Chile", "CL"},
		{730, "Colombia", "CO"},
		{735, "Ecuador", "EC"},
		{740, "UK", "UK"},
		{745, "Guiana", "GF"},
		{750, "Guyana", "GY"},
		{755, "Paraguay", "PY"},
		{760, "Peru", "PE"},
		{765, "Suriname", "SR"},
		{770, "Uruguay", "UY"},
		{775, "Venezuela", "VE"}};
	// spellchecker:on
}
