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
#include <iostream>
#include <string.h>
#include <memory>
#include <ctime>

#include "Common.h"
#include "PackedInt.h"

namespace JSON { class Writer; }

const int CLASS_A_MASK = (1 << 1) | (1 << 2) | (1 << 3);
const int CLASS_B_MASK = (1 << 18) | (1 << 19);

const int CLASS_A_STATIC_MASK = 1 << 5;
const int CLASS_B_STATIC_MASK = 1 << 24;

const int BASESTATION_MASK = (1 << 4) | (1 << 16) | (1 << 17) | (1 << 20) | (1 << 22) | (1 << 23);
const int SAR_MASK = 1 << 9;
const int ATON_MASK = (1 << 21) | (1 << 28);

const int MAX_MSG_TYPE = 28;

// Fields cleared once none of their producer types has been heard recently. Only
// mmsi, count, the last_signal times and group survive; the sweep trims msg_type.
const uint32_t F_LATLON = 1 << 0;
const uint32_t F_SPEED_COG = 1 << 1;
const uint32_t F_HEADING = 1 << 2;
const uint32_t F_STATUS = 1 << 3;
const uint32_t F_MANEUVER = 1 << 4;
const uint32_t F_ALTITUDE = 1 << 5;
const uint32_t F_RECV_STATIONS = 1 << 6;
const uint32_t F_OFF_POSITION = 1 << 7;
const uint32_t F_VOYAGE = 1 << 8;
const uint32_t F_STATIC = 1 << 9;
const uint32_t F_COMM_CAP = 1 << 10;
const uint32_t F_SIGNAL = 1 << 11;

struct ShipLL
{
    int prev = -1, next = -1;
};

struct Ship
{
    ShipLL incoming; // time-ordered linked list
    ShipLL hash;     // hash bucket chain
    uint32_t mmsi;
    int count, msg_type, shipclass, mmsi_type, shiptype, heading, status;
    int to_port, to_bow, to_starboard, to_stern, IMO, angle, altitude, received_stations;
    int unit_model, unit_serial;
    char month, day, hour, minute;
    float lat, lon, ppm, level, speed, cog, draught, distance;
    std::time_t last_signal, last_direct_signal, last_static_signal;
    char shipname[21], destination[21], callsign[8], country_code[3], vin[9], vendorid[4];
    std::string msg;
    uint64_t last_group, group_mask;
    // types heard since the last sweep; not persisted
    int type_ttl;
    Util::PackedInt flags;

    void reset();
    void markType(int type) { msg_type |= 1 << type; type_ttl |= 1 << type; }
    void decayAndExpire();
    void clearFields(uint32_t doomed);
    int getMMSItype();
    int getShipTypeClassEri();
    int getShipTypeClass();
    void setType();
    bool getKML(std::string &) const;
    bool getGeoJSON(JSON::Writer &) const;

    // File persistence functions
    bool Save(std::ofstream &file) const;
    bool Load(std::ifstream &file);

private:
    static const int _SHIP_MAGIC = 0x53484950; // "SHIP" in hex
    static const int _SHIP_VERSION = 3;

public:
    // Setters for PackedInt fields
    void setValidated(int val) { flags.set(0, 2, val); }
    void setRepeat(int val) { flags.set(2, 2, val); }
    void setVirtualAid(int val) { flags.set(4, 1, val); }
    void setApproximate(int val) { flags.set(5, 1, val); }
    void orOpChannels(int val) { flags.orOp(6, 4, val); }
    void clearOpChannels() { flags.set(6, 4, 0); }
    void setCSUnit(int val) { flags.set(10, 2, val); } // 0=unknown, 1=SOTDMA, 2=Carrier Sense
    void setRAIM(int val) { flags.set(12, 2, val); } // 0=unknown, 1=false, 2=true
    void setDTE(int val) { flags.set(14, 2, val); } // 0=unknown, 1=ready, 2=not ready
    void setAssigned(int val) { flags.set(16, 2, val); } // 0=unknown, 1=autonomous, 2=assigned
    void setDisplay(int val) { flags.set(18, 2, val); } // 0=unknown, 1=false, 2=true
    void setDSC(int val) { flags.set(20, 2, val); } // 0=unknown, 1=false, 2=true
    void setBand(int val) { flags.set(22, 2, val); } // 0=unknown, 1=false, 2=true
    void setMsg22(int val) { flags.set(24, 2, val); } // 0=unknown, 1=false, 2=true
    void setOffPosition(int val) { flags.set(26, 2, val); } // 0=unknown, 1=on position, 2=off position
    void setManeuver(int val) { flags.set(28, 2, val); } // 0=not available, 1=no special, 2=special

    // Getters for PackedInt fields
    int getValidated() const { return flags.get(0, 2); }
    int getRepeat() const { return flags.get(2, 2); }
    int getVirtualAid() const { return flags.get(4, 1); }
    int getApproximate() const { return flags.get(5, 1); }
    int getChannels() const { return flags.get(6, 4); }
    int getCSUnit() const { return flags.get(10, 2); } // 0=unknown, 1=SOTDMA, 2=Carrier Sense
    int getRAIM() const { return flags.get(12, 2); } // 0=unknown, 1=false, 2=true
    int getDTE() const { return flags.get(14, 2); } // 0=unknown, 1=ready, 2=not ready
    int getAssigned() const { return flags.get(16, 2); } // 0=unknown, 1=autonomous, 2=assigned
    int getDisplay() const { return flags.get(18, 2); } // 0=unknown, 1=false, 2=true
    int getDSC() const { return flags.get(20, 2); } // 0=unknown, 1=false, 2=true
    int getBand() const { return flags.get(22, 2); } // 0=unknown, 1=false, 2=true
    int getMsg22() const { return flags.get(24, 2); } // 0=unknown, 1=false, 2=true
    int getOffPosition() const { return flags.get(26, 2); } // 0=unknown, 1=on position, 2=off position
    int getManeuver() const { return flags.get(28, 2); } // 0=not available, 1=no special, 2=special
};