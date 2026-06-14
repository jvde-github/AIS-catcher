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

#pragma once

#include <iomanip>

#include "Stream.h"
#include "JSON/JSON.h"
#include "Keys.h"
#include "AIS.h"

namespace AIS {
	class JSONAIS : public SimpleStreamInOut<Message, JSON::JSON> {
		JSON::JSON json;
		std::vector<JSON::Value> nmea_values;

		void ProcessMsg(const AIS::Message& msg, TAG& tag);

		const std::string class_str = "AIS";
		const std::string device = "AIS-catcher";
		const std::string nan = "nan";
		const std::string fastleft = "fastleft";
		const std::string fastright = "fastright";
		const std::string undefined = "Undefined";

		std::string channel, timestamp, datastring, rxtime;
		std::string eta, text, callsign, shipname, destination, name, vendorid;
		std::string start_date, end_date, start_time, end_time;

	protected:
		void ProcessMsg8Data(const AIS::Message& msg);
		void ProcessMsg6Data(const AIS::Message& msg);
		void ProcessRadio(const AIS::Message &msg, int start, int len);

		// ASM decoders, payload-relative (start = bit index of first payload bit after header).
		// Shared between msg 6 (start=88) and msg 8 (start=56):
		void asm_imo_fid0_text(const AIS::Message& msg, int start);
		void asm_usa_fid1_sls_meteo(const AIS::Message& msg, int start);
		void asm_usa_fid2_sls_lock(const AIS::Message& msg, int start);
		void asm_usa_fid32_sls_specific(const AIS::Message& msg, int start);

		// Msg 6 only:
		void asm_iala_fid0_buoy_monitor(const AIS::Message& msg, int start);
		void asm_imo_fid2_interrogation(const AIS::Message& msg, int start);
		void asm_imo_fid3_interrogation_ext(const AIS::Message& msg, int start);
		void asm_imo_fid4_capability_reply(const AIS::Message& msg, int start);
		void asm_imo_fid16_persons(const AIS::Message& msg, int start);
		void asm_imo_fid20_berthing_data(const AIS::Message& msg, int start);
		void asm_imo_fid23_area_notice(const AIS::Message& msg, int start);
		void asm_imo_fid25_dangerous_cargo(const AIS::Message& msg, int start);
		void asm_imo_fid30_text_addressed(const AIS::Message& msg, int start);
		void asm_inland_fid21_eta(const AIS::Message& msg, int start);
		void asm_inland_fid22_rta(const AIS::Message& msg, int start);
		void asm_inland_fid23_emma_warning(const AIS::Message& msg, int start);
		void asm_inland_fid24_water_level(const AIS::Message& msg, int start);
		void asm_inland_fid55_persons(const AIS::Message& msg, int start);
		void asm_uk_fid10_aton_monitor(const AIS::Message& msg, int start);
		void asm_uk_fid20_buoy_position(const AIS::Message& msg, int start);

		// Msg 8 only:
		void asm_imo_fid16_vts_targets(const AIS::Message& msg, int start);
		void asm_inland_fid10_eri_static(const AIS::Message& msg, int start);
		void asm_imo_fid31_meteo_hydro(const AIS::Message& msg, int start);
		void asm_inland_fid25_bridge_clearance(const AIS::Message& msg, int start);
		void asm_inland_fid40_signal_status(const AIS::Message& msg, int start);
		void asm_swe_fid1_route(const AIS::Message& msg, int start);
		void asm_imo_fid21_weather_ship(const AIS::Message& msg, int start);
		void asm_imo_fid24_ext_static(const AIS::Message& msg, int start);
		void asm_imo_fid32_tidal_window(const AIS::Message& msg, int start);
		void asm_imo_fid19_traffic_signal(const AIS::Message& msg, int start);
		void asm_imo_fid17_vts_targets(const AIS::Message& msg, int start);
		void asm_imo_fid29_text_description(const AIS::Message& msg, int start);
		void asm_imo_fid27_route(const AIS::Message& msg, int start);
		void asm_imo_fid26_environmental(const AIS::Message& msg, int start);
		void asm_imo_fid11_meteo_hydro_legacy(const AIS::Message& msg, int start);
		void asm_usa_fid33_environmental(const AIS::Message& msg, int start);


		void U(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void UL(const AIS::Message& msg, int p, int start, int len, float a, float b, unsigned undefined = ~0);
		void US(const AIS::Message& msg, int p, int start, int len, int b, unsigned undefined = ~0);
		void S(const AIS::Message& msg, int p, int start, int len, int undefined = ~0);
		void SL(const AIS::Message& msg, int p, int start, int len, float a, float b, int undefined = ~0);
		void E(const AIS::Message& msg, int p, int start, int len, int pmap = 0);
		void TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0);
		void B(const AIS::Message& msg, int p, int start, int len);
		void X(const AIS::Message& msg, int p, int start, int len) {}

		void COUNTRY(const AIS::Message& msg);

		void D(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void T(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void TIMESTAMP(const AIS::Message& msg, int p, int start, int len, std::string& str);
		void ETA(const AIS::Message& msg, int p, int start, int len, std::string& str);

	public:
		virtual ~JSONAIS() {}

		void Receive(const AIS::Message* data, int len, TAG& tag);
	};
}
