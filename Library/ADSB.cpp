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

// below is a simplified version of the code from dump1090.c by Antirez
namespace Plane
{

    uint32_t crc_table[112] = {
        0x3935ea, 0x1c9af5, 0xf1b77e, 0x78dbbf, 0xc397db, 0x9e31e9, 0xb0e2f0, 0x587178,
        0x2c38bc, 0x161c5e, 0x0b0e2f, 0xfa7d13, 0x82c48d, 0xbe9842, 0x5f4c21, 0xd05c14,
        0x682e0a, 0x341705, 0xe5f186, 0x72f8c3, 0xc68665, 0x9cb936, 0x4e5c9b, 0xd8d449,
        0x939020, 0x49c810, 0x24e408, 0x127204, 0x093902, 0x049c81, 0xfdb444, 0x7eda22,
        0x3f6d11, 0xe04c8c, 0x702646, 0x381323, 0xe3f395, 0x8e03ce, 0x4701e7, 0xdc7af7,
        0x91c77f, 0xb719bb, 0xa476d9, 0xadc168, 0x56e0b4, 0x2b705a, 0x15b82d, 0xf52612,
        0x7a9309, 0xc2b380, 0x6159c0, 0x30ace0, 0x185670, 0x0c2b38, 0x06159c, 0x030ace,
        0x018567, 0xff38b7, 0x80665f, 0xbfc92b, 0xa01e91, 0xaff54c, 0x57faa6, 0x2bfd53,
        0xea04ad, 0x8af852, 0x457c29, 0xdd4410, 0x6ea208, 0x375104, 0x1ba882, 0x0dd441,
        0xf91024, 0x7c8812, 0x3e4409, 0xe0d800, 0x706c00, 0x383600, 0x1c1b00, 0x0e0d80,
        0x0706c0, 0x038360, 0x01c1b0, 0x00e0d8, 0x00706c, 0x003836, 0x001c1b, 0xfff409,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000};

    void ADSB::Callsign()
    {
        static const char *cs_table = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        int len = 0;

        for (int i = 0; i < 8; i++)
        {
            uint32_t c = getBits(40 + (i * 6), 6);
            if (cs_table[c] != '#')
            {
                callsign[len++] = cs_table[c];
            }
        }
        callsign[len] = 0;
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

        if (!validateLength())
        {
            status = STATUS_ERROR;
            return;
        }

        setCRC();

        switch (df)
        {
        case 0:  // Short Air-Air Surveillance
        case 4:  // Surveillance, Altitude Reply
        case 20: // Comm-B, Altitude Reply
        {
            setCRCandICAO();
            altitude = decodeAC13Field();

            // These message types are always from airborne aircraft
            airborne = 1;
        }
        break;

        case 5:  // Surveillance, Identity Reply
        case 21: // Comm-B, Identity Reply, ground or air
        {
            setCRCandICAO();

            int a = ((msg[3] & 0x80) >> 5) | ((msg[2] & 0x02) >> 0) | ((msg[2] & 0x08) >> 3);
            int b = ((msg[3] & 0x02) << 1) | ((msg[3] & 0x08) >> 2) | ((msg[3] & 0x20) >> 5);
            int c = ((msg[2] & 0x01) << 2) | ((msg[2] & 0x04) >> 1) | ((msg[2] & 0x10) >> 4);
            int d = ((msg[3] & 0x01) << 2) | ((msg[3] & 0x04) >> 1) | ((msg[3] & 0x10) >> 4);

            squawk = a * 1000 + b * 100 + c * 10 + d;
            break;
        }
        case 11: // All-Call Reply, ground or air
            hexident = getBits(8, 24);
            hexident_status = HEXINDENT_DIRECT;

            if (!verifyCRC())
            {
                status = STATUS_ERROR;
                return;
            }

            break;

        case 17: // Extended Squitter
        case 18: // Extended Squitter/Supplementary
            hexident = getBits(8, 24);
            hexident_status = HEXINDENT_DIRECT;

            if (!verifyCRC())
            {
                status = STATUS_ERROR;
                return;
            }

            int EStype = getBits(32, 5);
            int ESsubtype = getBits(37, 3); // ME message subtype

            switch (EStype)
            {
            case 1: // Aircraft Identification
            case 2:
            case 3:
            case 4:
                Callsign();
                break;

            case 19: // Airborne Velocity
                if (ESsubtype >= 1 && ESsubtype <= 4)
                {
                    // Velocity decoding
                    if (ESsubtype == 1 || ESsubtype == 2)
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
                            heading = atan2(ew_vel, ns_vel) * 360.0 / (2 * PI);
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
                        odd.timestamp = rxtime;
                        odd.airborne = false;
                    }
                    else
                    {
                        even.lat = getBits(55, 17);
                        even.lon = getBits(72, 17);
                        even.timestamp = rxtime;
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
                int is_odd = msg[6] & (1 << 2);

                int lat = ((msg[6] & 3) << 15) | (msg[7] << 7) | (msg[8] >> 1);
                int lon = ((msg[8] & 1) << 16) | (msg[9] << 8) | msg[10];

                CPR &cpr = is_odd ? odd : even;

                cpr.lat = lat;
                cpr.lon = lon;
                cpr.timestamp = rxtime;
                cpr.airborne = true;
            }
            break;
            }
            break;
        }
    }

    int ADSB::cprModFunction(int a, int b)
    {
        int res = a % b;
        if (res < 0)
            res += b;
        return res;
    }

    static constexpr double NL_LATS[] = {
        10.47047130, 14.82817437, 18.18626357, 21.02939493, 23.54504487,
        25.82924707, 27.93898710, 29.91135686, 31.77209708, 33.53993436,
        35.22899598, 36.85025108, 38.41241892, 39.92256684, 41.38651832,
        42.80914012, 44.19454951, 45.54626723, 46.86733252, 48.16039128,
        49.42776439, 50.67150166, 51.89342469, 53.09516153, 54.27817472,
        55.44378444, 56.59318756, 57.72747354, 58.84763776, 59.95459277,
        61.04917774, 62.13216659, 63.20427479, 64.26616523, 65.31845310,
        66.36171008, 67.39646774, 68.42322022, 69.44242631, 70.45451075,
        71.45986473, 72.45884545, 73.45177442, 74.43893416, 75.42056257,
        76.39684391, 77.36789461, 78.33374083, 79.29428225, 80.24923213,
        81.19801349, 82.13956981, 83.07199445, 83.99173563, 84.89166191,
        85.75541621, 86.53536998, 87.00000000};

    int ADSB::cprNLFunction(double lat)
    {
        lat = (lat < 0) ? -lat : lat;

        if (lat >= 87.0)
            return 1;
        if (lat < NL_LATS[0])
            return 59;

        int low = 0;
        int high = sizeof(NL_LATS) / sizeof(NL_LATS[0]) - 1;

        while (low <= high)
        {
            int mid = (low + high) >> 1;
            double val = NL_LATS[mid];

            if (lat < val)
            {
                high = mid - 1;
            }
            else if (lat > val)
            {
                low = mid + 1;
            }
            else
            {
                return 59 - mid;
            }
        }
        return 59 - low;
    }

    int ADSB::cprNFunction(double lat, int isodd)
    {
        return MAX(cprNLFunction(lat) - isodd, 1);
    }

    double ADSB::cprDlonFunction(double lat, int isodd)
    {
        return 360.0 / cprNFunction(lat, isodd);
    }

    void ADSB::decodeCPR()
    {
        // Validate input data
        if (even.lat == CPR_POSITION_UNDEFINED ||
            odd.lat == CPR_POSITION_UNDEFINED ||
            std::abs(even.timestamp - odd.timestamp) > 10)
        {
            return;
        }

        if (even.airborne != odd.airborne)
            return;

        // Constants
        constexpr double AirDlat0 = 360.0 / 60;
        constexpr double AirDlat1 = 360.0 / 59;
        constexpr double CPR_SCALE = 131072.0;

        // Calculate global latitude
        int j = floor(((59 * even.lat - 60 * odd.lat) / CPR_SCALE) + 0.5);
        double rlat0 = AirDlat0 * (cprModFunction(j, 60) + even.lat / CPR_SCALE);
        double rlat1 = AirDlat1 * (cprModFunction(j, 59) + odd.lat / CPR_SCALE);

        // Normalize latitudes
        if (rlat0 >= 270)
            rlat0 -= 360;
        if (rlat1 >= 270)
            rlat1 -= 360;

        // Validate latitude zones
        if (cprNLFunction(rlat0) != cprNLFunction(rlat1))
            return;

        // Select most recent position
        bool use_even = even.timestamp > odd.timestamp;
        double rlat = use_even ? rlat0 : rlat1;
        double raw_lon = use_even ? even.lon : odd.lon;
        int nl = cprNLFunction(rlat);
        int ni = MAX(nl - (use_even ? 0 : 1), 1);

        // Calculate longitude
        double m = floor((((even.lon * (nl - 1)) - (odd.lon * nl)) / CPR_SCALE) + 0.5);

        // Set final position
        lon = (360.0 / ni) * (cprModFunction(m, ni) + raw_lon / CPR_SCALE);
        lat = rlat;
        latlon_timestamp = use_even ? even.timestamp : odd.timestamp;

        // Normalize longitude
        if (lon > 180)
            lon -= 360;
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

        if (crc != CRC_UNDEFINED)
            std::cout << "CRC: " << std::hex << crc << std::dec << std::endl;

        if (msgtype != MSG_TYPE_UNDEFINED)
            std::cout << "MSGTYPE: " << msgtype << std::endl;

        if (df != DF_UNDEFINED)
            std::cout << "DF: " << df << std::endl;

        if (hexident != HEXIDENT_UNDEFINED)
            std::cout << "HEX: " << std::hex << hexident << std::dec << (hexident_status == HEXINDENT_DIRECT ? " (direct)" : " (crc)") << std::endl;

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

        if (odd.lat != CPR_POSITION_UNDEFINED)
            std::cout << "ODD: " << odd.lat << ", " << odd.lon << std::endl;

        if (even.lat != CPR_POSITION_UNDEFINED)
            std::cout << "EVEN: " << even.lat << ", " << even.lon << std::endl;
    }
}