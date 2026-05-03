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

#include <vector>
#include <string>

#define JSON_DICT_FULL 0
#define JSON_DICT_MINIMAL 1
#define JSON_DICT_SPARSE 2
#define JSON_DICT_APRS 3
#define JSON_DICT_SETTING 4
#define JSON_DICT_INPUT 5
#define JSON_DICT_COLUMNS 6

namespace AIS
{

	struct KeyInfo
	{
		const char* unit;
		const char* description;
		const std::vector<std::string>* lookup_table;

		KeyInfo(const char* u, const char* d, const std::vector<std::string>* lt = nullptr)
			: unit(u), description(d), lookup_table(lt) {}
	};

	extern const std::string KeyMap[][JSON_DICT_COLUMNS];
	extern const KeyInfo KeyInfoMap[];
	extern const std::vector<std::string> LookupTable_aid_types;
	extern const std::vector<std::string> LookupTable_aton_station_types;
	extern const std::vector<std::string> LookupTable_aton_on_station_status;
	extern const std::vector<std::string> LookupTable_aton_restricted_use;
	extern const std::vector<std::string> LookupTable_aton_dim_type;
	extern const std::vector<std::string> LookupTable_vdes_capabilities;
	extern const std::vector<std::string> LookupTable_dte_types;
	extern const std::vector<std::string> LookupTable_epfd_types;
	extern const std::vector<std::string> LookupTable_interval_types;
	extern const std::vector<std::string> LookupTable_maneuver_types;
	extern const std::vector<std::string> LookupTable_nav_status;
	extern const std::vector<std::string> LookupTable_precipation_types;
	extern const std::vector<std::string> LookupTable_seastate_types;
	extern const std::vector<std::string> LookupTable_ship_types;
	extern const std::vector<std::string> LookupTable_station_types;
	extern const std::vector<std::string> LookupTable_txrx_types;
	extern const std::vector<std::string> LookupTable_sync_state;
	extern const std::vector<std::string> LookupTable_racon_status;
	extern const std::vector<std::string> LookupTable_hazard_types;
	extern const std::vector<std::string> LookupTable_loaded_types;
	extern const std::vector<std::string> LookupTable_route_types;
	extern const std::vector<std::string> LookupTable_sender_classification;
	extern const std::vector<std::string> LookupTable_pressuretend;
	extern const std::vector<std::string> LookupTable_pressuretend_wmo;
	extern const std::vector<std::string> LookupTable_ais_version;
	extern const std::vector<std::string> LookupTable_vts_target_id_type;
	extern const std::vector<std::string> LookupTable_health;
	extern const std::vector<std::string> LookupTable_message_types;

	// JSON keys - generated from KeyDefs.h
	enum Keys
	{
#define X(name, full, min, sparse, aprs, setting, input, unit, desc, lookup) name,
#include "KeyDefs.h"
#undef X
		KEY_COUNT
	};

	Keys lookupSettingKey(const std::string &option);
}
