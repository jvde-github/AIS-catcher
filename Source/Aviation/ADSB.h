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

#include <cstring>
#include <string>
#include <time.h>
#include <vector>

#include "Common.h"

namespace Plane
{
    static constexpr double BEAST_CLOCK_MHZ = 12.0;
    // CPR position update related constants
    static constexpr double CPR_MAX_TIMEDIFF = 10.0;         // Maximum time difference between even/odd messages (airborne)
    static constexpr double CPR_MAX_TIMEDIFF_SURFACE = 50.0; // Surface positions are transmitted far less often
    static constexpr int NZ = 15;                    // Number of geographic latitude zones
    static constexpr double AirDlat0 = 360.0 / 60;   // Even message latitude zone size
    static constexpr double AirDlat1 = 360.0 / 59;   // Odd message latitude zone size

    enum class ValueStatus
    {
        VALID,
        INVALID,
        UNKNOWN
    };

    struct CPR
    {
        int lat, lon;
        std::time_t timestamp;
        bool airborne;

        void clear()
        {
            lat = CPR_POSITION_UNDEFINED;
            lon = CPR_POSITION_UNDEFINED;
            timestamp = TIME_UNDEFINED;
            airborne = false;
        }

        bool Valid() const
        {
            return lat != CPR_POSITION_UNDEFINED && lon != CPR_POSITION_UNDEFINED;
        }
    };

    struct ADSB
    {
        struct LL
        {
            int prev, next;
        };

        LL time_ll;
        LL hash_ll;

        struct
        {
            FLOAT32 lat, lon;
            std::time_t timestamp;
            struct Plane::CPR cpr;
            bool even;
        } CPR_history[3];

        int CPR_history_idx = 0;

        static constexpr int MAX_BYTES = 14; // Mode-S long frame; covers DF 16/17/18/20/21.
        uint8_t msg[MAX_BYTES + 4];          // +4 padding so the 5-byte read in getUint is always in-bounds.
        int df;          // Downlink format
        int msgtype;     // Message type
        int len;         // Length of message
        std::time_t rxtime;
        uint32_t hexident; // Aircraft Mode S hex code
        int hexident_status;
        int altitude;     // Mode C altitude
        FLOAT32 lat, lon; // Position
        ValueStatus position_status;
        std::time_t position_timestamp;
        FLOAT32 speed;         // Speed over ground
        FLOAT32 heading;       // Track angle
        FLOAT32 distance;      // Distance from station
        int angle;             // Angle from station
        int vertrate;          // Vertical rate
        char callsign[9];      // Aircraft callsign, nul terminated
        int squawk;            // Mode A squawk code
        int category;          // ADS-B category
        int airborne;          // 0 = on ground, 1 = airborne, 2 = unknown
        long messages;         // Number of Mode S messages received
        struct CPR even, odd;  // CPR coordinates
        FLOAT32 signalLevel;   // Signal level
        std::time_t timestamp; // Timestamp
        int crc;               // CRC checksum
        int status;            // Status
        char country_code[2];  // Country code
        long nMessages;
        uint32_t message_types;
        uint32_t message_subtypes;
        uint64_t last_group, group_mask;

        void Stamp(std::time_t t = (std::time_t)0L)
        {
            std::time(&rxtime);
            if ((long int)t != 0 && t < rxtime)
                setRxTimeUnix(t);
        }

        void setRxTimeUnix(std::time_t t) { rxtime = t; }
        std::time_t getRxTimeUnix() const { return rxtime; }

        void Decode();

        void clear()
        {
            msgtype = MSG_TYPE_UNDEFINED;
            df = DF_UNDEFINED;
            len = 0;
            rxtime = TIME_UNDEFINED;
            hexident = HEXIDENT_UNDEFINED;
            hexident_status = HEXIDENT_UNDEFINED;
            altitude = ALTITUDE_UNDEFINED;
            lat = LAT_UNDEFINED;
            lon = LON_UNDEFINED;
            position_timestamp = TIME_UNDEFINED;
            speed = SPEED_UNDEFINED;
            heading = HEADING_UNDEFINED;
            vertrate = VERT_RATE_UNDEFINED;
            squawk = SQUAWK_UNDEFINED;
            airborne = AIRBORNE_UNDEFINED;
            signalLevel = LEVEL_UNDEFINED;
            timestamp = TIME_UNDEFINED;
            category = CATEGORY_UNDEFINED;
            distance = DISTANCE_UNDEFINED;
            angle = ANGLE_UNDEFINED;
            message_types = 0;
            message_subtypes = 0;
            position_status = ValueStatus::UNKNOWN;

            for (int i = 0; i < 3; i++)
            {
                CPR_history[i].cpr.clear();
                CPR_history[i].lat = LAT_UNDEFINED;
                CPR_history[i].lon = LON_UNDEFINED;
            }

            crc = CRC_UNDEFINED;
            status = STATUS_OK;
            country_code[0] = country_code[1] = ' ';
            nMessages = 0;

            even.clear();
            odd.clear();

            callsign[0] = '\0';
            last_group = GROUP_OUT_UNDEFINED;
            group_mask = 0;
        }

        inline void reset(std::time_t rx)
        {
            clear();
            rxtime = rx;
        }

        // SWAR getUint: 5-byte big-endian load + shift + mask. msg[]'s +4 padding
        // keeps the read in-bounds for any valid (startBit, bitLen).
        uint32_t getUint(int startBit, int bitLen) const
        {
            if (startBit < 0 || startBit + bitLen > this->len * 8)
                return 0;

            const int x = startBit >> 3;
            const int y = startBit & 7;

            uint32_t hi;
            std::memcpy(&hi, msg + x, 4);
            hi = ((hi & 0xFFu) << 24) | ((hi & 0xFF00u) << 8) | ((hi & 0xFF0000u) >> 8) | ((hi & 0xFF000000u) >> 24);
            const uint64_t w = ((uint64_t)hi << 8) | (uint64_t)msg[x + 4];

            const unsigned shift = 40u - (unsigned)y - (unsigned)bitLen;
            const uint32_t mask = (bitLen >= 32) ? 0xFFFFFFFFu : ((1u << bitLen) - 1u);
            return (uint32_t)((w >> shift) & mask);
        }

        int getMessageLength(int df)
        {
            if (df == 17 || df == 18 || df == 16 || df == 20 || df == 21)
                return 14;

            return 7;
        }

        bool validateLength()
        {
            int expected = getMessageLength(df);            
            return len == expected;
        }

        void setCRC()
        {
            if (len >= 3)
            {
                crc = ((uint32_t)msg[len - 3] << 16) |
                      ((uint32_t)msg[len - 2] << 8) |
                      ((uint32_t)msg[len - 1]);
            }
            else
            {
                crc = CRC_UNDEFINED;
            }
        }

        uint32_t calcCRC();
        void setCRCandICAO()
        {
            setCRC();

            uint32_t calc_crc = calcCRC();
            hexident = crc ^ calc_crc;

            hexident_status = HEXINDENT_IMPLIED_FROM_CRC;
        }

        bool verifyCRC()
        {
            uint32_t computed = calcCRC();

            return (computed == crc);
        }

        void Callsign();
        int decodeAC12Field();
        int decodeAC13Field();
        double decodeMovement();

        static int MOD(int a, int b);
        static int NL(double lat);

        bool decodeCPR(FLOAT32 ref_lat, FLOAT32 ref_lon, bool is_even, bool &, FLOAT32 &lt, FLOAT32 &ln);
        bool decodeCPR_airborne(bool is_even, bool &, FLOAT32 &lt, FLOAT32 &ln);
        bool decodeCPR_surface(FLOAT32, FLOAT32, bool, bool &, FLOAT32 &lt, FLOAT32 &ln);

        void setCountryCode();
    };
}