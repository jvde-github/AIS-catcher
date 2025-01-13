/*
    Copyright(c) 2021-2025 jvde.github@gmail.com

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
#include <string>
#include <time.h>
#include <vector>
#include <iomanip>
#include <sstream>

#include "Common.h"
#include "Utilities.h"

namespace Plane
{
    static constexpr double BEAST_CLOCK_MHZ = 12.0;

    struct CPR
    {
        int lat, lon;
        std::time_t timestamp;
        bool airborne;

        void clear()
        {
            lat = LAT_UNDEFINED;
            lon = LON_UNDEFINED;
            timestamp = TIME_UNDEFINED;
            airborne = false;
        }
    };

    struct ADSB
    {
        uint8_t msg[14]; // Raw message
        int df;          // Downlink format
        int msgtype;     // Message type
        int len;         // Length of message
        int prev, next;
        std::time_t rxtime;
        uint32_t hexident;     // Aircraft Mode S hex code
        int altitude;          // Mode C altitude
        FLOAT32 lat, lon;      // Position
        FLOAT32 speed;         // Speed over ground
        FLOAT32 heading;       // Track angle
        int vertrate;          // Vertical rate
        char callsign[10];     // Aircraft callsign, nul terminated
        int squawk;            // Mode A squawk code
        int airborne;          // 0 = on ground, 1 = airborne, 2 = unknown
        long messages;         // Number of Mode S messages received
        struct CPR even, odd;  // CPR coordinates
        FLOAT32 signalLevel;   // Signal level
        std::time_t timestamp; // Timestamp

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
            altitude = ALTITUDE_UNDEFINED;
            lat = LAT_UNDEFINED;
            lon = LON_UNDEFINED;
            speed = SPEED_UNDEFINED;
            heading = HEADING_UNDEFINED;
            vertrate = VERT_RATE_UNDEFINED;
            squawk = SQUAWK_UNDEFINED;
            airborne = AIRBORNE_UNDEFINED;
            signalLevel = LEVEL_UNDEFINED;
            timestamp = TIME_UNDEFINED;

            even.clear();
            odd.clear();

            callsign[0] = '\0';
        }

        std::string getRaw() const
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (const auto &byte : msg)
            {
                ss << std::setw(2) << static_cast<int>(byte);
            }
            return ss.str();
        }

        uint32_t getBits(int startBit, int len) const
        {
            uint32_t result = 0;
            for (int i = 0; i < len; i++)
            {
                int byteIdx = (startBit + i) / 8;
                int bitIdx = 7 - ((startBit + i) % 8);
                if (byteIdx < sizeof(msg) && (msg[byteIdx] & (1 << bitIdx)))
                {
                    result |= (1 << (len - 1 - i));
                }
            }
            return result;
        }

        void Print() const;
        void Callsign();
        int decodeAC12Field();
        int decodeAC13Field();
    };
}