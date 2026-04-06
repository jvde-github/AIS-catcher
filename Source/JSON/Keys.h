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

	extern const char* const KeyMap[][JSON_DICT_COLUMNS];
	extern const KeyInfo KeyInfoMap[];
	extern const std::vector<std::string> LookupTable_aid_types;
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

	// JSON keys - generated from KeyDefs.h
	enum Keys
	{
#define X(name, full, min, sparse, aprs, setting, unit, desc, lookup) name,
#include "KeyDefs.h"
#undef X
		KEY_COUNT
	};

	Keys lookupSettingKey(const std::string &option);
}
