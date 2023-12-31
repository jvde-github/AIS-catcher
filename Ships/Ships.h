/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
#include <iostream>
#include <string.h>
#include <memory>
#include <ctime>

#include "Common.h"


const float DISTANCE_UNDEFINED = -1;
const float LAT_UNDEFINED = 91;
const float LON_UNDEFINED = 181;
const float COG_UNDEFINED = 360;
const float SPEED_UNDEFINED = -1;

const int HEADING_UNDEFINED = 511;
const int STATUS_UNDEFINED = 15;
const int DIMENSION_UNDEFINED = -1;
const int ETA_DAY_UNDEFINED = 0;
const int ETA_MONTH_UNDEFINED = 0;
const int ETA_HOUR_UNDEFINED = 24;
const int ETA_MINUTE_UNDEFINED = 60;
const int IMO_UNDEFINED = 0;
const int ANGLE_UNDEFINED = -1;

enum ShippingClass {
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

enum MMSI_Class {
	MMSI_OTHER = 0,
	MMSI_CLASS_A = 1,
	MMSI_CLASS_B = 2,
	MMSI_BASESTATION = 3,
	MMSI_SAR = 4,
	MMSI_SARTEPIRB = 5,
	MMSI_ATON = 6
};

const int CLASS_A_MASK = (1 << 1) | (1 << 2) | (1 << 3);
const int CLASS_B_MASK = (1 << 18) | (1 << 19);

const int CLASS_A_STATIC_MASK = 1 << 5;
const int CLASS_B_STATIC_MASK = 1 << 24;

const int BASESTATION_MASK = (1 << 4) | (1 << 16) | (1 << 17) | (1 << 20) | (1 << 22) | (1 << 23);
const int SAR_MASK = 1 << 9;
const int ATON_MASK = 1 << 21;

struct Ship {
	int prev, next;
	uint32_t mmsi;
	int count, msg_type, shipclass, mmsi_type, channels, shiptype, heading, status, virtual_aid, path_ptr;
	int to_port, to_bow, to_starboard, to_stern, IMO, angle, validated;
	char month, day, hour, minute;
	float lat, lon, ppm, level, speed, cog, draught, distance;
	std::time_t last_signal;
	bool approximate;
	char shipname[21], destination[21], callsign[8], country_code[3];
	std::string msg;
	uint64_t last_group, group_mask;

	void reset() {
		mmsi = count = msg_type = channels = shiptype = validated = virtual_aid = group_mask = 0;
		path_ptr = -1;

		heading = HEADING_UNDEFINED;
		status = STATUS_UNDEFINED;
		to_port = to_bow = to_starboard = to_stern = DIMENSION_UNDEFINED;
		IMO = IMO_UNDEFINED;
		angle = ANGLE_UNDEFINED;
		month = ETA_MONTH_UNDEFINED;
		day = ETA_DAY_UNDEFINED;
		hour = ETA_HOUR_UNDEFINED;
		minute = ETA_MINUTE_UNDEFINED;
		lat = LAT_UNDEFINED;
		lon = LON_UNDEFINED;
		ppm = PPM_UNDEFINED;
		level = LEVEL_UNDEFINED;
		speed = draught = distance = 0;
		cog = COG_UNDEFINED;
		last_signal = {};
		approximate = false;
		shipclass = CLASS_UNKNOWN;
		mmsi_type = MMSI_OTHER;

		memset(shipname, 0, sizeof(shipname));
		memset(destination, 0, sizeof(destination));
		memset(callsign, 0, sizeof(callsign));
		memset(country_code, 0, sizeof(country_code));
		last_group = GROUP_OUT_UNDEFINED;

		msg.clear();
	}

	int getMMSItype() const {
		if ((mmsi > 111000000 && mmsi < 111999999) || (mmsi > 11100000 && mmsi < 11199999)) {
			return MMSI_SAR;
		}

		if (msg_type & CLASS_A_MASK) {
			return MMSI_CLASS_A;
		}
		if (msg_type & CLASS_B_MASK) {
			return MMSI_CLASS_B;
		}

		if (msg_type & BASESTATION_MASK) {
			return MMSI_BASESTATION;
		}
		if (msg_type & SAR_MASK) {
			return MMSI_SAR;
		}
		if (msg_type & ATON_MASK) {
			return MMSI_ATON;
		}

		if (mmsi >= 97000000 && mmsi <= 98000000) {
			return MMSI_SARTEPIRB;
		}

		if (msg_type & CLASS_A_STATIC_MASK) {
			return MMSI_CLASS_A;
		}
		if (msg_type & CLASS_B_STATIC_MASK) {
			return MMSI_CLASS_B;
		}

		return MMSI_OTHER;
	}

	int getShipTypeClassEri() const {
		switch (shiptype) {
		// Cargo cases
		case 8030:
		case 8010:
		case 8070:
		case 8210:
		case 8220:
		case 8230:
		case 8240:
		case 8250:
		case 8260:
		case 8270:
		case 8280:
		case 8290:
		case 8310:
		case 8320:
		case 8330:
		case 8340:
		case 8350:
		case 8360:
		case 8370:
		case 8380:
		case 8390:
		case 8130:
		case 8140:
		case 8150:
		case 8170:
		case 8410:
			return CLASS_CARGO;
		// Tanker cases
		case 8020:
		case 8021:
		case 8022:
		case 8023:
		case 8040:
		case 8060:
		case 8160:
		case 8161:
		case 8162:
		case 8163:
		case 8180:
		case 8490:
		case 8500:
		case 1530:
		case 1540:
			return CLASS_TANKER;
		// Special cases
		case 8050:
		case 8080:
		case 8090:
		case 8100:
		case 8110:
		case 8120:
		case 8400:
		case 8420:
		case 8430:
		case 8450:
		case 8460:
		case 8470:
		case 8510:
			return CLASS_SPECIAL;
		// Passenger cases
		case 8440:
		case 8441:
		case 8442:
		case 8443:
		case 8444:
			return CLASS_PASSENGER;
		// Other cases
		case 8480:
			return CLASS_FISHING;
		case 1850:
			return CLASS_B;
		case 1900:
		case 1910:
		case 1920:
			return CLASS_HIGHSPEED;
		default:
			return CLASS_OTHER;
		}
	}

	int getShipTypeClass() {
		int c = CLASS_UNKNOWN;

		switch (mmsi_type) {
		case MMSI_CLASS_A:
		case MMSI_CLASS_B:
			c = (mmsi_type == MMSI_CLASS_B) ? CLASS_B : CLASS_UNKNOWN;

			// Overwrite default if there's a better mapping
			if (shiptype >= 80 && shiptype < 90)
				c = CLASS_TANKER;
			else if (shiptype >= 70 && shiptype < 80)
				c = CLASS_CARGO;
			else if (shiptype >= 60 && shiptype < 70)
				c = CLASS_PASSENGER;
			else if (shiptype >= 40 && shiptype < 50)
				c = CLASS_HIGHSPEED;
			else if (shiptype >= 50 && shiptype < 60)
				c = CLASS_SPECIAL;
			else if (shiptype == 30)
				c = CLASS_FISHING;
			else if ((shiptype >= 1500 && shiptype <= 1920) || (shiptype >= 8000 && shiptype <= 8510)) {
				c = getShipTypeClassEri();
			}
			break;
		case MMSI_BASESTATION:
			c = CLASS_STATION;
			break;
		case MMSI_SAR:
			c = CLASS_HELICOPTER;
			if ((mmsi > 111000000 && mmsi < 111999999) || (mmsi > 11100000 && mmsi < 11199999)) {
				if ((mmsi / 100) % 10 == 1 || (mmsi / 10) % 10 == 1) {
					c = CLASS_PLANE;
				}
			}
			break;
		case MMSI_SARTEPIRB:
			c = CLASS_SARTEPIRB;
			break;
		case MMSI_ATON:
			c = CLASS_ATON;
			break;
		default:
			break;
		}

		return c;
	}

	void setType() {
		mmsi_type = getMMSItype();
		shipclass = getShipTypeClass();
	}
};
