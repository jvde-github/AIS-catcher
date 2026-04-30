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
	const std::string KeyMap[][JSON_DICT_COLUMNS] = {
#define X(name, full, minimal, sparse, aprs, setting, input, unit, desc, lookup) {full, minimal, sparse, aprs, setting, input},
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
		"Default, Type of Aid to Navigation not specified",
		"Reference point",
		"RACON (radar transponder marking a navigation hazard)",
		"Fixed offshore structure",
		"Spare, Reserved for future use.",
		"Light, without sectors",
		"Light, with sectors",
		"Leading Light Front",
		"Leading Light Rear",
		"Beacon, Cardinal N",
		"Beacon, Cardinal E",
		"Beacon, Cardinal S",
		"Beacon, Cardinal W",
		"Beacon, Port hand",
		"Beacon, Starboard hand",
		"Beacon, Preferred Channel port hand",
		"Beacon, Preferred Channel starboard hand",
		"Beacon, Isolated danger",
		"Beacon, Safe water",
		"Beacon, Special mark",
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
		// Codes 32-50 from ITU-R M.1371-6 Table 85 (msg 28 7-bit aid_type extension)
		"Ocean Data Acquisition System (ODAS)",
		"Water Sampling and/or Monitoring equipment",
		"Research equipment",
		"Towed Cable, Pipe or Semi-submerged Object Marker",
		"Towed Vessel or Object",
		"Flotsam Marker, Large (greater than 5 metres)",
		"Flotsam Marker, Small (less than 5 metres)",
		"Navigation hazard",
		"Synthetic Target Marker",
		"Protected Species Marker",
		"Military Operation Target Marker",
		"Dangerous Object",
		"Pollution Spill Marker",
		"Search & Rescue Datum Mark",
		"Datum Mark",
		"Operating Underwater (at times)",
		"Underwater Operations Marker",
		"Military Operation or Restricted Area",
		"Dynamic Area",
	};

	// ITU-R M.1371-6 Table 84 (msg 28 single-slot AtoN report), 3-bit station type.
	const std::vector<std::string> LookupTable_aton_station_types = {
		"Physical AIS AtoN (floating)",
		"Physical AIS AtoN (fixed)",
		"Synthetic predicted AIS AtoN",
		"Synthetic monitored AIS AtoN",
		"Virtual AIS AtoN",
		"Mobile AIS AtoN",
		"Reserved",
		"Reserved",
	};

	// ITU-R M.1371-6 Table 84 (msg 28), 2-bit Restricted Use Indicator.
	const std::vector<std::string> LookupTable_aton_restricted_use = {
		"Unrestricted use",
		"Restricted to territorial waters of flag state",
		"Restricted to Exclusive Economic Zone of flag state",
		"Restricted as defined by flag state",
	};

	// ITU-R M.1371-6 Table 84 (msg 28), 4-bit AtoN Dimensions Type.
	const std::vector<std::string> LookupTable_aton_dim_type = {
		"Default (msg 21 dimensions)",
		"AtoN Height and Structural Area",
		"AtoN Swing Circle",
		"Mobile AtoN Vector",
		"AtoN Area-Polygon",
		"AtoN Area-Circle",
		"AtoN Boundary Line 1",
		"AtoN Area-Sector",
		"AtoN Boundary Line 2",
		"AtoN Area-Quadrilateral",
		"AtoN Large Boundary Line 1",
		"AtoN Large Area-Sector",
		"AtoN Large Boundary Line 2",
		"AtoN Large Area-Quadrilateral",
		"Reserved",
		"Reserved",
	};

	// ITU-R M.1371-6 Table 77 (msg 24B), 2-bit VDES capabilities.
	const std::vector<std::string> LookupTable_vdes_capabilities = {
		"AIS only",
		"Supports VDES ASM",
		"Supports VDES ASM/VDE-TER",
		"Supports VDES ASM/VDE-TER/VDE-SAT",
	};

	// ITU-R M.1371-6 Table 84 (msg 28 single-slot AtoN report), 4-bit on-station status.
	const std::vector<std::string> LookupTable_aton_on_station_status = {
		"On-station",
		"On-station or on course (Mobile AtoN)",
		"On-station, but damaged, occulted, submerged or otherwise not properly visible",
		"Off-station, virtual AtoN reporting intended position",
		"Off-station, location unknown",
		"Off-station, but reporting current position",
		"Off-station, adrift",
		"Off-station, removed or relocated",
		"On-station, as a new or temporary AtoN",
		"Unmarked navigation hazard",
		"Unmarked obstruction",
		"Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
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
		// M.1371-6 Table 49 / 77 additions:
		"BDS",
		"Reserved",
		"Reserved",
		"Integrated PNT system",
		"Inertial navigation system",
		"Terrestrial radio navigation system",
		"Internal GNSS",
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

	// ITU-R M.1371-6 Table 46 (msg 1/2/3 navigational status).
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
		"Reserved for future use",
		"Reserved for future use",
		"Power-driven vessel towing astern (regional use)",
		"Power-driven vessel pushing ahead or towing alongside (regional use)",
		"Reserved for future use",
		"Active AIS-SART, MOB-AIS or EPIRB-AIS",
		"Undefined (default)",
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

	// ITU-R M.1371-6 Table 51 (8-bit). Codes 100-199 = regional; 200-255 = reserved.
	const std::vector<std::string> LookupTable_ship_types = {
		"Not available",
		"Science / Research vessel",
		"Training vessel",
		"Ship owned or operated by a government",
		"Ice breaker",
		"Buoy (Aids to Navigation) tender",
		"Cable layer",
		"Pipe layer",
		"Reserved",
		"Special purpose ship, no additional information",
		"Reserved",
		"FPSO (Floating, Production, Storage, Offloading) vessel",
		"Fish factory ship",
		"Fish farm support vessel",
		"Offshore support vessel",
		"Reserved",
		"Reserved",
		"Construction vessel",
		"Crew boat",
		"Support vessel, no additional information",
		"Wing in ground (WIG) - all ships of this type",
		"Wing in ground (WIG) - Hazardous category A",
		"Wing in ground (WIG) - Hazardous category B",
		"Wing in ground (WIG) - Hazardous category C",
		"Wing in ground (WIG) - Hazardous category D",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - No additional information",
		"Fishing",
		"Towing",
		"Towing: length exceeds 200m or breadth exceeds 25m",
		"Dredging or underwater ops",
		"Diving ops",
		"Military ops",
		"Sailing",
		"Pleasure Craft",
		"Trawler",
		"Patrol vessel",
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
		"Ships of States not parties to an armed conflict",
		"Passenger ships - all ships of this type",
		"Passenger ships - Hazardous category A",
		"Passenger ships - Hazardous category B",
		"Passenger ships - Hazardous category C",
		"Passenger ships - Hazardous category D",
		"Passenger (cruise) ship",
		"Passenger (ferry) ship",
		"Passenger (excursion) ship",
		"Reserved",
		"Passenger ships - No additional information",
		"Cargo ships - all ships of this type",
		"Cargo ships - Hazardous category A",
		"Cargo ships - Hazardous category B",
		"Cargo ships - Hazardous category C",
		"Cargo ships - Hazardous category D",
		"Cargo ship, bulk carrier",
		"Cargo ship, container ship",
		"Cargo ship, roll-on-roll-off carrier",
		"Cargo ship, landing craft",
		"Cargo ships - No additional information",
		"Tanker(s) - all ships of this type",
		"Tanker(s) - Hazardous category A",
		"Tanker(s) - Hazardous category B",
		"Tanker(s) - Hazardous category C",
		"Tanker(s) - Hazardous category D",
		"Tanker(s) - non-hazardous or non-pollutant carrier",
		"Tanker(s) - Reserved for future use",
		"Tanker(s) - Reserved for future use",
		"Tanker(s) - Reserved for future use",
		"Tanker(s) - No additional information",
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

	// ITU-R M.1371-6 Table 74 (msg 23 station_type, 4-bit).
	const std::vector<std::string> LookupTable_station_types = {
		"All types of mobiles (default)",
		"Class A AIS stations only",
		"All types of Class B mobile stations",
		"SAR airborne mobile station",
		"Class B \"SO\" mobile stations only",
		"Class B \"CS\" AIS stations only",
		"Inland waterways",
		"Regional use",
		"Regional use",
		"Regional use",
		"Base station coverage area",
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

	const std::vector<std::string> LookupTable_sync_state = {
		"UTC direct",
		"UTC indirect",
		"Synchronized to a base station",
		"Synchronized to another station",
	};

	const std::vector<std::string> LookupTable_racon_status = {
		"No RACON installed",
		"RACON installed, not monitored",
		"RACON operational",
		"RACON error",
	};

	const std::vector<std::string> LookupTable_hazard_types = {
		"Not available (default)",
		"Blue cones / lights: 1",
		"Blue cones / lights: 2",
		"Blue cones / lights: 3",
		"B-Flag",
		"Unknown",
	};

	const std::vector<std::string> LookupTable_loaded_types = {
		"Not available (default)",
		"Loaded",
		"Unloaded",
		"Reserved",
	};

	const std::vector<std::string> LookupTable_route_types = {
		"Undefined (default)",
		"Mandatory route",
		"Recommended route",
		"Alternative route",
		"Recommended route through ice",
		"Ship route plan",
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
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Cancel route identified by Message Linkage ID",
	};

	const std::vector<std::string> LookupTable_sender_classification = {
		"Ship",
		"Authority",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
	};

	const std::vector<std::string> LookupTable_pressuretend = {
		"Steady",
		"Decreasing",
		"Increasing",
		"N/A (default)",
	};

	// ITU-R M.1371-6 Table 50 (msg 5 AIS version indicator).
	const std::vector<std::string> LookupTable_ais_version = {
		"ITU-R M.1371-1",
		"ITU-R M.1371-3",
		"ITU-R M.1371-5",
		"ITU-R M.1371-6 (or later)",
	};

	const std::vector<std::string> LookupTable_vts_target_id_type = {
		"MMSI",
		"IMO number",
		"Call sign",
		"Other",
	};

	const std::vector<std::string> LookupTable_health = {
		"Good",
		"Alarm",
	};

	const std::vector<std::string> LookupTable_pressuretend_wmo = {
		"Increasing, then decreasing; now same or higher than 3 h ago",
		"Increasing, then steady; or rising, then rising more slowly",
		"Increasing steadily or unsteadily",
		"Decreasing or steady, then increasing; or increasing, then increasing more rapidly",
		"Steady; same as 3 h ago",
		"Decreasing, then increasing; now same or lower than 3 h ago",
		"Decreasing, then steady; or decreasing, then decreasing more slowly",
		"Decreasing steadily or unsteadily",
		"Steady or increasing, then decreasing; or decreasing, then decreasing more rapidly",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"N/A (default)",
	};

	// Generated from KeyDefs.h
	const KeyInfo KeyInfoMap[] = {
#define X(name, full, minimal, sparse, aprs, setting, input, unit, desc, lookup) {unit, desc, lookup},
#include "KeyDefs.h"
#undef X
	};
}
