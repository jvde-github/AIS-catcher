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

#include "Keys.h"

#include <unordered_map>
#include "Convert.h"

namespace AIS
{
	// Generated from KeyDefs.h
	const char* const KeyMap[][JSON_DICT_COLUMNS] = {
#define X(name, full, minimal, sparse, aprs, setting, unit, desc, lookup) {full, minimal, sparse, aprs, setting, ""},
#include "KeyDefs.h"
#undef X
	};

	Keys lookupSettingKey(const std::string &option)
	{
		static std::unordered_map<std::string, Keys> map;
		if (map.empty())
		{
			for (int i = KEY_SETTING_ABOUT; i <= KEY_SETTING_ZONE; i++)
			{
				std::string s = KeyMap[i][JSON_DICT_SETTING];
				Util::Convert::toUpper(s);
				map[s] = (Keys)i;
			}
		}

		auto it = map.find(option);
		return it != map.end() ? it->second : (Keys)-1;
	}

	const std::vector<std::string> LookupTable_aid_types = {
		"Default Type of Aid to Navigation not specified",
		"Reference point",
		"RACON (radar transponder marking a navigation hazard)",
		"Fixed offshore structure",
		"Spare Reserved for future use.",
		"Light without sectors",
		"Light with sectors",
		"Leading Light Front",
		"Leading Light Rear",
		"Beacon Cardinal N",
		"Beacon Cardinal E",
		"Beacon Cardinal S",
		"Beacon Cardinal W",
		"Beacon Port hand",
		"Beacon Starboard hand",
		"Beacon Preferred Channel port hand",
		"Beacon Preferred Channel starboard hand",
		"Beacon Isolated danger",
		"Beacon Safe water",
		"Beacon Special mark",
		"Cardinal Mark N",
		"Cardinal Mark E",
		"Cardinal Mark S",
		"Cardinal Mark W",
		"Port hand Mark",
		"Starboard hand Mark",
		"Preferred Channel Port hand",
		"Preferred Channel Starboard hand",
		"Isolated danger",
		"Safe Water",
		"Special Mark",
		"Light Vessel / LANBY / Rigs",
	};

	const std::vector<std::string> LookupTable_dte_types = {
		"Data Terminal Ready",
		"Data Terminal Not Ready",
	};

	const std::vector<std::string> LookupTable_message_error_types = {
		"None",
		"Undefined Error",
		"NMEA Checksum Error"};

	const std::vector<std::string> LookupTable_epfd_types = {
		"Undefined",
		"GPS",
		"GLONASS",
		"Combined GPS/GLONASS",
		"Loran-C",
		"Chayka",
		"Integrated navigation system",
		"Surveyed",
		"Galileo",
	};

	const std::vector<std::string> LookupTable_interval_types = {
		"As given by the autonomous mode",
		"10 Minutes",
		"6 Minutes",
		"3 Minutes",
		"1 Minute",
		"30 Seconds",
		"15 Seconds",
		"10 Seconds",
		"5 Seconds",
		"Next Shorter Reporting Interval",
		"Next Longer Reporting Interval",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
	};

	const std::vector<std::string> LookupTable_maneuver_types = {
		"Not available (default)",
		"No special maneuver",
		"Special maneuver (such as regional passing arrangement)",
	};

	const std::vector<std::string> LookupTable_nav_status = {
		"Under way using engine",
		"At anchor",
		"Not under command",
		"Restricted maneuverability",
		"Constrained by her draught",
		"Moored",
		"Aground",
		"Engaged in fishing",
		"Under way sailing",
		"Reserved for HSC",
		"Reserved for WIG",
		"Towing astern (regional)",
		"Pushing ahead or towing alongside (regional)",
		"Reserved",
		"AIS-SART is active",
		"Not defined",
	};

	const std::vector<std::string> LookupTable_precipation_types = {
		"Reserved",
		"Rain",
		"Thunderstorm",
		"Freezing rain",
		"Mixed/ice",
		"Snow",
		"Reserved",
		"N/A (default)",
	};

	const std::vector<std::string> LookupTable_seastate_types = {
		"Calm",
		"Light air",
		"Light breeze",
		"Gentle breeze",
		"Moderate breeze",
		"Fresh breeze",
		"Strong breeze",
		"High wind",
		"Gale",
		"Strong gale",
		"Storm",
		"Violent storm",
		"Hurricane force",
		"N/A (default)",
		"Reserved",
		"Reserved",
	};

	const std::vector<std::string> LookupTable_ship_types = {
		"Not available",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Wing in ground (WIG) - all ships of this type",
		"Wing in ground (WIG) - Hazardous category A",
		"Wing in ground (WIG) - Hazardous category B",
		"Wing in ground (WIG) - Hazardous category C",
		"Wing in ground (WIG) - Hazardous category D",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Fishing",
		"Towing",
		"Towing: length exceeds 200m or breadth exceeds 25m",
		"Dredging or underwater ops",
		"Diving ops",
		"Military ops",
		"Sailing",
		"Pleasure Craft",
		"Reserved",
		"Reserved",
		"High speed craft (HSC) - all ships of this type",
		"High speed craft (HSC) - Hazardous category A",
		"High speed craft (HSC) - Hazardous category B",
		"High speed craft (HSC) - Hazardous category C",
		"High speed craft (HSC) - Hazardous category D",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - No additional information",
		"Pilot Vessel",
		"Search and Rescue vessel",
		"Tug",
		"Port Tender",
		"Anti-pollution equipment",
		"Law Enforcement",
		"Spare - Local Vessel",
		"Spare - Local Vessel",
		"Medical Transport",
		"Noncombatant ship according to RR Resolution No. 18",
		"Passenger - all ships of this type",
		"Passenger - Hazardous category A",
		"Passenger - Hazardous category B",
		"Passenger - Hazardous category C",
		"Passenger - Hazardous category D",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - No additional information",
		"Cargo - all ships of this type",
		"Cargo - Hazardous category A",
		"Cargo - Hazardous category B",
		"Cargo - Hazardous category C",
		"Cargo - Hazardous category D",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - No additional information",
		"Tanker - all ships of this type",
		"Tanker - Hazardous category A",
		"Tanker - Hazardous category B",
		"Tanker - Hazardous category C",
		"Tanker - Hazardous category D",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - No additional information",
		"Other Type - all ships of this type",
		"Other Type - Hazardous category A",
		"Other Type - Hazardous category B",
		"Other Type - Hazardous category C",
		"Other Type - Hazardous category D",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - no additional information",
	};

	const std::vector<std::string> LookupTable_station_types = {
		"All types of mobiles (default)",
		"Reserved for future use",
		"All types of Class B mobile stations",
		"SAR airborne mobile station",
		"Aid to Navigation station",
		"Class B shipborne mobile station (IEC62287 only)",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
	};

	const std::vector<std::string> LookupTable_txrx_types = {
		"TxA/TxB, RxA/RxB (default)",
		"TxA, RxA/RxB",
		"TxB, RxA/RxB",
		"Reserved for Future Use",
	};

	// Generated from KeyDefs.h
	const KeyInfo KeyInfoMap[] = {
#define X(name, full, minimal, sparse, aprs, setting, unit, desc, lookup) {unit, desc, lookup},
#include "KeyDefs.h"
#undef X
	};
}
