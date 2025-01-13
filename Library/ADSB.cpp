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

#include "ADSB.h"

// References:
// https://github.com/antirez/dump1090/blob/master/dump1090.c
// https://github.com/wiedehopf/readsb/blob/dev/mode_s.c
// https://github.com/cjkreklow/go-adsb
// https://pure.tudelft.nl/ws/portalfiles/portal/104877928/11_Book_Manuscript_69_1_10_20210510.pdf

namespace Plane
{

    void ADSB::Callsign()
    {
        static const char *cs_table = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        char temp[9] = {0};
        int len = 0;

        for (int i = 0; i < 8; i++)
        {
            uint32_t c = getBits(40 + (i * 6), 6);
            if (cs_table[c] != '#')
            {
                temp[len++] = cs_table[c];
            }
        }

        if (len > 0)
        {
            std::strncpy(callsign, temp, sizeof(callsign) - 1);
            callsign[sizeof(callsign) - 1] = '\0';
        }
    }


    // Decode 13 bit AC altitude field (in DF 20 and others)
    int ADSB::decodeAC13Field()
    {
        int m_bit = msg[3] & (1 << 6);

        if (!m_bit)
        {
            int q_bit = msg[3] & (1 << 4);

            if (q_bit)
            {
                // N is the 11 bit integer resulting from removal of M and Q bits
                int n = ((msg[2] & 31) << 6) |
                        ((msg[3] & 0x80) >> 2) |
                        ((msg[3] & 0x20) >> 1) |
                        (msg[3] & 15);
                // Final altitude is the resulting number multiplied by 25, minus 1000.
                return n * 25 - 1000;
            }
        }
        /*
        else
        {
            *unit = UNIT_METERS;
        }
        */
        return ALTITUDE_UNDEFINED;
    }

    // Decode 12 bit AC altitude field (in DF 17 and others)
    int ADSB::decodeAC12Field()
    {
        int q_bit = msg[5] & 1;

        if (q_bit)
        {
            // N is the 11 bit integer after removing Q bit
            int n = ((msg[5] >> 1) << 4) | ((msg[6] & 0xF0) >> 4);
            // Final altitude is n * 25 - 1000
            return n * 25 - 1000;
        }
        return ALTITUDE_UNDEFINED;
    }

    void ADSB::Decode()
    {
        if (msgtype == '1')
            return;

        df = getBits(0, 5);

        switch (df)
        {
        case 0:  // Short Air-Air Surveillance
        case 4:  // Surveillance, Altitude Reply
        case 20: // Comm-B, Altitude Reply
        {
            altitude = decodeAC13Field();
            hexident = getBits(8, 24);
            // These message types are always from airborne aircraft
            airborne = 1;
        }
        break;

        case 5:  // Surveillance, Identity Reply
        case 21: // Comm-B, Identity Reply
            hexident = getBits(8, 24);
            squawk = ((msg[2] >> 2) & 0x1F) * 1000 +
                     ((msg[2] & 0x03) * 100) +
                     ((msg[3] >> 4) & 0x0F) * 10 +
                     (msg[3] & 0x0F);
            // These can be from ground or air, so leave as unknown
            airborne = 2;
            break;

        case 11: // All-Call Reply
            hexident = getBits(8, 24);
            // All-call replies can be from ground or air
            airborne = 2;
            break;

        case 17: // Extended Squitter
        case 18: // Extended Squitter/Supplementary
            hexident = getBits(8, 24);
            msgtype = getBits(32, 5);
            int mesub = getBits(37, 3); // ME message subtype

            switch (msgtype)
            {
            case 1: // Aircraft Identification
            case 2:
            case 3:
            case 4:
                Callsign();
                break;

            case 19: // Airborne Velocity
                if (mesub >= 1 && mesub <= 4)
                {
                    // Velocity decoding
                    if (mesub == 1 || mesub == 2)
                    {
                        int ew_dir = (msg[5] & 4) >> 2;
                        int ew_vel = ((msg[5] & 3) << 8) | msg[6];
                        int ns_dir = (msg[7] & 0x80) >> 7;
                        int ns_vel = ((msg[7] & 0x7f) << 3) | ((msg[8] & 0xe0) >> 5);

                        if (ew_vel && ns_vel)
                        {
                            if (ew_dir)
                                ew_vel *= -1;
                            if (ns_dir)
                                ns_vel *= -1;

                            speed = sqrt(ns_vel * ns_vel + ew_vel * ew_vel);
                            heading = atan2(ew_vel, ns_vel) * 360.0 / (2 * M_PI);
                            if (heading < 0)
                                heading += 360;
                        }

                        int vr_sign = (msg[8] & 0x8) >> 3;
                        vertrate = ((msg[8] & 7) << 6) | ((msg[9] & 0xfc) >> 2);
                        if (vr_sign)
                            vertrate *= -1;
                        vertrate = (vertrate - 1) * 64;

                        // Airborne velocity messages are always from airborne aircraft
                        airborne = 1;
                    }
                }
                break;

            case 5: // Surface Position
            case 6:
            case 7:
            case 8:
                airborne = 0; // Aircraft is on the ground
                {
                    // Store CPR position
                    int is_odd = getBits(54, 1);
                    if (is_odd)
                    {
                        odd.lat = getBits(55, 17);
                        odd.lon = getBits(72, 17);
                        odd.timestamp = timestamp;
                        odd.airborne = false;
                    }
                    else
                    {
                        even.lat = getBits(55, 17);
                        even.lon = getBits(72, 17);
                        even.timestamp = timestamp;
                        even.airborne = false;
                    }
                }
                break;

            case 9: // Airborne Position
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            {
                altitude = decodeAC12Field();
                airborne = 1; // Aircraft is airborne

                // Store CPR position
                int is_odd = getBits(54, 1);
                if (is_odd)
                {
                    odd.lat = getBits(55, 17);
                    odd.lon = getBits(72, 17);
                    odd.timestamp = timestamp;
                    odd.airborne = true;
                }
                else
                {
                    even.lat = getBits(55, 17);
                    even.lon = getBits(72, 17);
                    even.timestamp = timestamp;
                    even.airborne = true;
                }
            }
            break;
            }
            break;
        }
    }

    void ADSB::Print() const
    {
        std::cout << "RX: " << std::put_time(std::localtime(&rxtime), "%F %T") << std::endl;

        std::cout << "MSG: ";
        for (int i = 0; i < len; i++)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)msg[i];
        }
        std::cout << std::dec << std::endl;

        if (msgtype != MSG_TYPE_UNDEFINED)
            std::cout << "MSGTYPE: " << msgtype << std::endl;

        if (df != DF_UNDEFINED)
            std::cout << "DF: " << df << std::endl;

        if (hexident != HEXIDENT_UNDEFINED)
            std::cout << "HEX: " << std::hex << hexident << std::dec << std::endl;

        if (altitude != ALTITUDE_UNDEFINED)
            std::cout << "ALT: " << altitude << std::endl;

        if (lat != LAT_UNDEFINED)
            std::cout << "LAT: " << lat << std::endl;

        if (lon != LON_UNDEFINED)
            std::cout << "LON: " << lon << std::endl;

        if (speed != SPEED_UNDEFINED)
            std::cout << "SPD: " << speed << std::endl;

        if (heading != HEADING_UNDEFINED)
            std::cout << "HDG: " << heading << std::endl;

        if (vertrate != VERT_RATE_UNDEFINED)
            std::cout << "V/S: " << vertrate << std::endl;

        if (squawk != SQUAWK_UNDEFINED)
            std::cout << "SQK: " << squawk << std::endl;

        if (airborne != AIRBORNE_UNDEFINED)
            std::cout << "AIR: " << airborne << std::endl;

        if (callsign[0] != '\0')
            std::cout << "CSN: " << callsign << std::endl;

        if (odd.lat != LAT_UNDEFINED)
            std::cout << "ODD: " << odd.lat << ", " << odd.lon << std::endl;

        if (even.lat != LAT_UNDEFINED)
            std::cout << "EVEN: " << even.lat << ", " << even.lon << std::endl;
    }
}