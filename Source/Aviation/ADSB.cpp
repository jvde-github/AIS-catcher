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

#include <algorithm>
#include <iostream>

#include "ADSB.h"
#include "Logger.h"
#include "JSON/Writer.h"

// References:
// https://github.com/antirez/dump1090/blob/master/dump1090.c
// https://github.com/wiedehopf/readsb/blob/dev/mode_s.c
// https://github.com/cjkreklow/go-adsb
// https://pure.tudelft.nl/ws/portalfiles/portal/104877928/11_Book_Manuscript_69_1_10_20210510.pdf

// below is a simplified version of the code from dump1090.c by Antirez
namespace Plane
{

    static const uint32_t crc_table2[256] = {
        0x000000U, 0xfff409U, 0x001c1bU, 0xffe812U, 0x003836U, 0xffcc3fU, 0x00242dU, 0xffd024U,
        0x00706cU, 0xff8465U, 0x006c77U, 0xff987eU, 0x00485aU, 0xffbc53U, 0x005441U, 0xffa048U,
        0x00e0d8U, 0xff14d1U, 0x00fcc3U, 0xff08caU, 0x00d8eeU, 0xff2ce7U, 0x00c4f5U, 0xff30fcU,
        0x0090b4U, 0xff64bdU, 0x008cafU, 0xff78a6U, 0x00a882U, 0xff5c8bU, 0x00b499U, 0xff4090U,
        0x01c1b0U, 0xfe35b9U, 0x01ddabU, 0xfe29a2U, 0x01f986U, 0xfe0d8fU, 0x01e59dU, 0xfe1194U,
        0x01b1dcU, 0xfe45d5U, 0x01adc7U, 0xfe59ceU, 0x0189eaU, 0xfe7de3U, 0x0195f1U, 0xfe61f8U,
        0x012168U, 0xfed561U, 0x013d73U, 0xfec97aU, 0x01195eU, 0xfeed57U, 0x010545U, 0xfef14cU,
        0x015104U, 0xfea50dU, 0x014d1fU, 0xfeb916U, 0x016932U, 0xfe9d3bU, 0x017529U, 0xfe8120U,
        0x038360U, 0xfc7769U, 0x039f7bU, 0xfc6b72U, 0x03bb56U, 0xfc4f5fU, 0x03a74dU, 0xfc5344U,
        0x03f30cU, 0xfc0705U, 0x03ef17U, 0xfc1b1eU, 0x03cb3aU, 0xfc3f33U, 0x03d721U, 0xfc2328U,
        0x0363b8U, 0xfc97b1U, 0x037fa3U, 0xfc8baaU, 0x035b8eU, 0xfcaf87U, 0x034795U, 0xfcb39cU,
        0x0313d4U, 0xfce7ddU, 0x030fcfU, 0xfcfbc6U, 0x032be2U, 0xfcdfebU, 0x0337f9U, 0xfcc3f0U,
        0x0242d0U, 0xfdb6d9U, 0x025ecbU, 0xfdaac2U, 0x027ae6U, 0xfd8eefU, 0x0266fdU, 0xfd92f4U,
        0x0232bcU, 0xfdc6b5U, 0x022ea7U, 0xfddaaeU, 0x020a8aU, 0xfdfe83U, 0x021691U, 0xfde298U,
        0x02a208U, 0xfd5601U, 0x02be13U, 0xfd4a1aU, 0x029a3eU, 0xfd6e37U, 0x028625U, 0xfd722cU,
        0x02d264U, 0xfd266dU, 0x02ce7fU, 0xfd3a76U, 0x02ea52U, 0xfd1e5bU, 0x02f649U, 0xfd0240U,
        0x0706c0U, 0xf8f2c9U, 0x071adbU, 0xf8eed2U, 0x073ef6U, 0xf8caffU, 0x0722edU, 0xf8d6e4U,
        0x0776acU, 0xf882a5U, 0x076ab7U, 0xf89ebeU, 0x074e9aU, 0xf8ba93U, 0x075281U, 0xf8a688U,
        0x07e618U, 0xf81211U, 0x07fa03U, 0xf80e0aU, 0x07de2eU, 0xf82a27U, 0x07c235U, 0xf8363cU,
        0x079674U, 0xf8627dU, 0x078a6fU, 0xf87e66U, 0x07ae42U, 0xf85a4bU, 0x07b259U, 0xf84650U,
        0x06c770U, 0xf93379U, 0x06db6bU, 0xf92f62U, 0x06ff46U, 0xf90b4fU, 0x06e35dU, 0xf91754U,
        0x06b71cU, 0xf94315U, 0x06ab07U, 0xf95f0eU, 0x068f2aU, 0xf97b23U, 0x069331U, 0xf96738U,
        0x0627a8U, 0xf9d3a1U, 0x063bb3U, 0xf9cfbaU, 0x061f9eU, 0xf9eb97U, 0x060385U, 0xf9f78cU,
        0x0657c4U, 0xf9a3cdU, 0x064bdfU, 0xf9bfd6U, 0x066ff2U, 0xf99bfbU, 0x0673e9U, 0xf987e0U,
        0x0485a0U, 0xfb71a9U, 0x0499bbU, 0xfb6db2U, 0x04bd96U, 0xfb499fU, 0x04a18dU, 0xfb5584U,
        0x04f5ccU, 0xfb01c5U, 0x04e9d7U, 0xfb1ddeU, 0x04cdfaU, 0xfb39f3U, 0x04d1e1U, 0xfb25e8U,
        0x046578U, 0xfb9171U, 0x047963U, 0xfb8d6aU, 0x045d4eU, 0xfba947U, 0x044155U, 0xfbb55cU,
        0x041514U, 0xfbe11dU, 0x04090fU, 0xfbfd06U, 0x042d22U, 0xfbd92bU, 0x043139U, 0xfbc530U,
        0x054410U, 0xfab019U, 0x05580bU, 0xfaac02U, 0x057c26U, 0xfa882fU, 0x05603dU, 0xfa9434U,
        0x05347cU, 0xfac075U, 0x052867U, 0xfadc6eU, 0x050c4aU, 0xfaf843U, 0x051051U, 0xfae458U,
        0x05a4c8U, 0xfa50c1U, 0x05b8d3U, 0xfa4cdaU, 0x059cfeU, 0xfa68f7U, 0x0580e5U, 0xfa74ecU,
        0x05d4a4U, 0xfa20adU, 0x05c8bfU, 0xfa3cb6U, 0x05ec92U, 0xfa189bU, 0x05f089U, 0xfa0480U};

    // software style CRC calculation as used in READSB
    // https://github.com/wiedehopf/readsb/blob/dev/crc.c

    uint32_t ADSB::calcCRC()
    {
        uint32_t rem = 0;

        for (int i = 0; i < len - 3; ++i)
        {
            rem = (rem << 8) ^ crc_table2[msg[i] ^ ((rem & 0xff0000) >> 16)];
            rem = rem & 0xffffff;
        }

        return rem;
    }

    void ADSB::Callsign()
    {
        static const char *cs_table = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        int n = 0;

        for (int i = 0; i < 8; i++)
        {
            uint32_t c = getUint(40 + (i * 6), 6);
            if (cs_table[c] != '#')
            {
                callsign[n++] = cs_table[c];
            }
        }
        callsign[n] = 0;

        while (n > 0 && callsign[n - 1] == ' ')
            callsign[--n] = 0;
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
                int n = ((msg[2] & 31) << 6) | ((msg[3] & 0x80) >> 2) | ((msg[3] & 0x20) >> 1) | (msg[3] & 15);
                return n * 25 - 1000;
            }
        }
        /*
        altitude in meters not implemented
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
            return n * 25 - 1000;
        }
        return ALTITUDE_UNDEFINED;
    }

    double ADSB::decodeMovement()
    {
        int val = getUint(37, 7);

        if (val == 1)
            return 0.0;
        if (val >= 2 && val <= 8)
            return 0.125 * (val - 1);
        if (val >= 9 && val <= 12)
            return 1.0 + 0.25 * (val - 8);
        if (val >= 13 && val <= 38)
            return 2.0 + 0.5 * (val - 12);
        if (val >= 39 && val <= 93)
            return 15.0 + 1.0 * (val - 38);
        if (val >= 94 && val <= 108)
            return 70.0 + 2.0 * (val - 93);
        if (val >= 109 && val <= 123)
            return 100.0 + 5.0 * (val - 108);
        if (val == 124)
            return 175.0;

        return SPEED_UNDEFINED;
    }

    // Gillham-coded identity, interleaved across msg[2] and msg[3]
    void ADSB::decodeSquawk()
    {
        int a = ((msg[3] & 0x80) >> 5) | ((msg[2] & 0x02) >> 0) | ((msg[2] & 0x08) >> 3);
        int b = ((msg[3] & 0x02) << 1) | ((msg[3] & 0x08) >> 2) | ((msg[3] & 0x20) >> 5);
        int c = ((msg[2] & 0x01) << 2) | ((msg[2] & 0x04) >> 1) | ((msg[2] & 0x10) >> 4);
        int d = ((msg[3] & 0x01) << 2) | ((msg[3] & 0x04) >> 1) | ((msg[3] & 0x10) >> 4);

        squawk = a * 1000 + b * 100 + c * 10 + d;
    }

    // Subtypes 1 and 2 only: 3 and 4 report airspeed, not ground velocity
    void ADSB::decodeVelocity(int ST)
    {
        int ewVelocity = getUint(46, 10);
        int nsVelocity = getUint(57, 10);

        if (ewVelocity && nsVelocity)
        {
            bool Dew = getUint(45, 1);
            bool Dns = getUint(56, 1);

            ewVelocity = Dew ? -(ewVelocity - 1) : (ewVelocity - 1);
            nsVelocity = Dns ? -(nsVelocity - 1) : (nsVelocity - 1);

            speed = sqrt(nsVelocity * nsVelocity + ewVelocity * ewVelocity);
            heading = atan2(ewVelocity, nsVelocity) * 360.0 / (2 * PI);
            if (heading < 0)
                heading += 360;

            if (ST == 2)
                speed *= 4;
        }

        int VR = getUint(69, 9);
        if (VR)
        {
            bool Svr = getUint(68, 1);
            vertrate = (VR - 1) * 64 * (Svr ? -1 : 1);
        }
        airborne = 1;
    }

    void ADSB::storeCPR(bool is_airborne)
    {
        CPR &cpr = getUint(53, 1) ? odd : even;

        cpr.lat = getUint(54, 17);
        cpr.lon = getUint(71, 17);
        cpr.timestamp = rxtime;
        cpr.airborne = is_airborne;
    }

    void ADSB::Decode()
    {
        if (msgtype == '1')
            return;

        df = getUint(0, 5);

        if (!validateLength())
        {
            status = STATUS_ERROR;
            return;
        }

        setCRC();

        message_types |= 1 << df;

        switch (df)
        {
        case 0:  // Short Air-Air Surveillance
        case 4:  // Surveillance, Altitude Reply
        case 20: // Comm-B, Altitude Reply
            setCRCandICAO();
            altitude = decodeAC13Field();
            break;

        case 5:  // Surveillance, Identity Reply
        case 21: // Comm-B, Identity Reply, ground or air
            setCRCandICAO();
            decodeSquawk();
            break;

        case 11: // All-Call Reply, ground or air
        {
            hexident = getUint(8, 24);
            hexident_status = HEXINDENT_DIRECT;

            int capability = getUint(5, 3);

            if (capability == 4)
                airborne = 0;
            else if (capability == 5)
                airborne = 1;

            if (!verifyCRC())
            {
                status = STATUS_ERROR;
                return;
            }

            break;
        }
        case 18: // Extended Squitter/Supplementary (TIS-B / ADS-R)
        case 17: // Extended Squitter
        {
            if (df == 18)
            {
                // Only CF 0 (ADS-B, ICAO address), 1 (ADS-B, non-ICAO),
                // 2 (TIS-B fine) and 6 (ADS-R) carry a DF17-style ME field.
                int cf = getUint(5, 3);
                if (cf != 0 && cf != 1 && cf != 2 && cf != 6)
                    break;
                hexident_status = (cf == 1) ? HEXINDENT_NON_ICAO : HEXINDENT_DIRECT;
            }
            else
                hexident_status = HEXINDENT_DIRECT;

            hexident = getUint(8, 24);

            if (!verifyCRC())
            {
                status = STATUS_ERROR;
                return;
            }

            int TC = getUint(32, 5);
            int ST = getUint(37, 3);

            message_subtypes |= 1 << TC;

            switch (TC)
            {
            case 1: // Aircraft Identification
            case 2:
            case 3:
            case 4:

                category = TC * 10 + ST;
                Callsign();
                break;

            case 19: // Airborne Velocity
                if (ST == 1 || ST == 2)
                    decodeVelocity(ST);
                break;

            case 5: // Surface Position
            case 6:
            case 7:
            case 8:
                airborne = 0;

                if (getUint(44, 1))
                    heading = getUint(45, 7) * 360 / 128.0;

                speed = decodeMovement();
                storeCPR(false);
                break;

            case 9: // Airborne Position (barometric altitude)
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 20: // Airborne Position (GNSS height instead of barometric)
            case 21:
            case 22:
                altitude = decodeAC12Field();
                airborne = 1;
                storeCPR(true);
                break;
            }
            break;
        }
        }
    }

    int ADSB::MOD(int a, int b)
    {
        int res = a % b;
        if (res < 0)
            res += b;
        return res;
    }

    int ADSB::NL(double lat)
    {
        lat = std::abs(lat);

        if (lat == 0)
            return 59;
        else if (lat == 87)
            return 2;
        else if (lat > 87)
            return 1;

        double tmp = 1 - (1 - cos(PI / (2.0 * 15.0))) / pow(cos(PI / 180.0 * lat), 2);
        // Near zone boundaries float rounding can push tmp just below -1,
        // making acos return NaN and the int conversion undefined.
        tmp = MAX(tmp, -1.0);
        return std::floor(2 * PI / acos(tmp));
    }

    bool ADSB::decodeCPR(FLOAT32 ref_lat, FLOAT32 ref_lon, bool use_even, bool &updated, FLOAT32 &lt, FLOAT32 &ln)
    {
        if (!even.Valid() || !odd.Valid() || (even.airborne != odd.airborne))
            return false;

        double max_dt = even.airborne ? CPR_MAX_TIMEDIFF : CPR_MAX_TIMEDIFF_SURFACE;
        if (std::abs(std::difftime(even.timestamp, odd.timestamp)) > max_dt)
            return false;

        if (even.airborne)
            return decodeCPR_airborne(use_even, updated, lt, ln);

        return decodeCPR_surface(ref_lat, ref_lon, use_even, updated, lt, ln);
    }

    bool ADSB::decodeCPR_airborne(bool use_even, bool &updated, FLOAT32 &lt, FLOAT32 &ln)
    {
        constexpr double CPR_SCALE = (double)(1 << 17);

        int j = (int)std::floor(((59.0 * even.lat) - (60.0 * odd.lat)) / CPR_SCALE + 0.5);

        double lat_even = 360.0 / 60 * (MOD(j, 60) + even.lat / CPR_SCALE);
        double lat_odd = 360.0 / 59 * (MOD(j, 59) + odd.lat / CPR_SCALE);

        if (lat_even >= 270.0)
            lat_even -= 360.0;
        if (lat_odd >= 270.0)
            lat_odd -= 360.0;

        if (std::abs(lat_even) > 90.0 || std::abs(lat_odd) > 90.0)
            return false;

        int nl = NL(lat_even);

        if (nl != NL(lat_odd))
            return false;

        lt = use_even ? lat_even : lat_odd;

        int ni = MAX(nl - (use_even ? 0 : 1), 1);
        int m = std::floor((even.lon * (nl - 1) - odd.lon * nl) / CPR_SCALE + 0.5);

        double lon_final = use_even ? even.lon : odd.lon;

        ln = (360.0 / ni) * (MOD(m, ni) + lon_final / CPR_SCALE);

        if (ln > 180.0)
            ln -= 360.0;

        position_timestamp = use_even ? even.timestamp : odd.timestamp;
        updated = true;

        return true;
    }

    // based on the example from the 1090 riddle
    bool ADSB::decodeCPR_surface(FLOAT32 ref_lat, FLOAT32 ref_lon, bool use_even, bool &updated, FLOAT32 &lt, FLOAT32 &ln)
    {
        static bool warning_given = false;

        if (ref_lat == LAT_UNDEFINED || ref_lon == LON_UNDEFINED)
        {
            if (!warning_given)
            {
                Error() << "ADSB: Reference position is not available. Cannot determine location of onground planes. Provide receiver location with -Z lat lon.";
                warning_given = true;
            }
            return false;
        }

        constexpr double CPR_SCALE = (double)(1 << 17);

        int j = (int)std::floor((59 * even.lat - 60 * odd.lat) / CPR_SCALE + 0.5);

        double lat_even = 90.0 / 60 * (MOD(j, 60) + even.lat / CPR_SCALE);
        double lat_odd = 90.0 / 59 * (MOD(j, 59) + odd.lat / CPR_SCALE);

        int nl = NL(lat_even);

        if (nl != NL(lat_odd))
            return false;

        lt = use_even ? lat_even : lat_odd;
        lt -= 90.0 * std::floor((lt - ref_lat + 45.0) / 90.0);

        int ni = MAX(nl - (use_even ? 0 : 1), 1);

        int m = std::floor((even.lon * (nl - 1) - odd.lon * nl) / CPR_SCALE + 0.5);
        double lon_final = use_even ? even.lon : odd.lon;

        ln = (90.0 / ni) * (MOD(m, ni) + lon_final / CPR_SCALE);
        ln -= 90.0 * std::floor((ln - ref_lon + 45.0) / 90.0);

        position_timestamp = use_even ? even.timestamp : odd.timestamp;
        updated = true;
        return true;
    }

    // ICAO country range table.
    // Sources: https://github.com/rikgale/ICAOList
    //          https://github.com/flightaware/dump1090/blob/master/public_html/flags.js
    struct RangeInfo
    {
        uint32_t startHex;
        uint32_t endHex;
        char isoCountry[2];
    };

    // spellchecker:off
    static const RangeInfo rangeInfo[] = {
        {0x004000, 0x0043FF, {'Z','W'}},
        {0x006000, 0x006FFF, {'M','Z'}},
        {0x008000, 0x00FFFF, {'Z','A'}},
        {0x010000, 0x017FFF, {'E','G'}},
        {0x018000, 0x01FFFF, {'L','Y'}},
        {0x020000, 0x027FFF, {'M','A'}},
        {0x028000, 0x02FFFF, {'T','N'}},
        {0x030000, 0x0303FF, {'B','W'}},
        {0x032000, 0x032FFF, {'B','I'}},
        {0x034000, 0x034FFF, {'C','M'}},
        {0x035000, 0x0353FF, {'K','M'}},
        {0x036000, 0x036FFF, {'C','G'}},
        {0x038000, 0x038FFF, {'C','I'}},
        {0x03E000, 0x03EFFF, {'G','A'}},
        {0x040000, 0x040FFF, {'E','T'}},
        {0x042000, 0x042FFF, {'G','Q'}},
        {0x044000, 0x044FFF, {'G','H'}},
        {0x046000, 0x046FFF, {'G','N'}},
        {0x048000, 0x0483FF, {'G','W'}},
        {0x04A000, 0x04A3FF, {'L','S'}},
        {0x04C000, 0x04CFFF, {'K','E'}},
        {0x050000, 0x050FFF, {'L','R'}},
        {0x054000, 0x054FFF, {'M','G'}},
        {0x058000, 0x058FFF, {'M','W'}},
        {0x05A000, 0x05A3FF, {'M','V'}},
        {0x05C000, 0x05CFFF, {'M','L'}},
        {0x05E000, 0x05E3FF, {'M','R'}},
        {0x060000, 0x0603FF, {'M','U'}},
        {0x062000, 0x062FFF, {'N','E'}},
        {0x064000, 0x064FFF, {'N','G'}},
        {0x068000, 0x068FFF, {'U','G'}},
        {0x06A000, 0x06A3FF, {'Q','A'}},
        {0x06C000, 0x06CFFF, {'C','F'}},
        {0x06E000, 0x06EFFF, {'R','W'}},
        {0x070000, 0x070FFF, {'S','N'}},
        {0x074000, 0x0743FF, {'S','C'}},
        {0x076000, 0x0763FF, {'S','L'}},
        {0x078000, 0x078FFF, {'S','O'}},
        {0x07A000, 0x07A3FF, {'S','Z'}},
        {0x07C000, 0x07CFFF, {'S','D'}},
        {0x080000, 0x080FFF, {'T','Z'}},
        {0x084000, 0x084FFF, {'T','D'}},
        {0x088000, 0x088FFF, {'T','G'}},
        {0x08A000, 0x08AFFF, {'Z','M'}},
        {0x08C000, 0x08CFFF, {'C','D'}},
        {0x090000, 0x090FFF, {'A','O'}},
        {0x094000, 0x0943FF, {'B','J'}},
        {0x096000, 0x0963FF, {'C','V'}},
        {0x098000, 0x0983FF, {'D','J'}},
        {0x09A000, 0x09AFFF, {'G','M'}},
        {0x09C000, 0x09CFFF, {'B','F'}},
        {0x09E000, 0x09E3FF, {'S','T'}},
        {0x0A0000, 0x0A7FFF, {'D','Z'}},
        {0x0A8000, 0x0A8FFF, {'B','S'}},
        {0x0AA000, 0x0AA3FF, {'B','B'}},
        {0x0AB000, 0x0AB3FF, {'B','Z'}},
        {0x0AC000, 0x0ACFFF, {'C','O'}},
        {0x0AE000, 0x0AEFFF, {'C','R'}},
        {0x0B0000, 0x0B0FFF, {'C','U'}},
        {0x0B2000, 0x0B2FFF, {'S','V'}},
        {0x0B4000, 0x0B4FFF, {'G','T'}},
        {0x0B6000, 0x0B6FFF, {'G','Y'}},
        {0x0B8000, 0x0B8FFF, {'H','T'}},
        {0x0BA000, 0x0BAFFF, {'H','N'}},
        {0x0BC000, 0x0BC3FF, {'V','C'}},
        {0x0BE000, 0x0BEFFF, {'J','M'}},
        {0x0C0000, 0x0C0FFF, {'N','I'}},
        {0x0C2000, 0x0C2FFF, {'P','A'}},
        {0x0C4000, 0x0C4FFF, {'D','O'}},
        {0x0C6000, 0x0C6FFF, {'T','T'}},
        {0x0C8000, 0x0C8FFF, {'S','R'}},
        {0x0CA000, 0x0CA3FF, {'A','G'}},
        {0x0CC000, 0x0CC3FF, {'G','D'}},
        {0x0D0000, 0x0D7FFF, {'M','X'}},
        {0x0D8000, 0x0DFFFF, {'V','E'}},
        {0x100000, 0x1FFFFF, {'R','U'}},
        {0x201000, 0x2013FF, {'N','A'}},
        {0x202000, 0x2023FF, {'E','R'}},
        {0x300000, 0x33FFFF, {'I','T'}},
        {0x340000, 0x37FFFF, {'E','S'}},
        {0x380000, 0x3BFFFF, {'F','R'}},
        {0x3C0000, 0x3FFFFF, {'D','E'}},
        {0x400000, 0x43FFFF, {'G','B'}},
        {0x440000, 0x447FFF, {'A','T'}},
        {0x448000, 0x44FFFF, {'B','E'}},
        {0x450000, 0x457FFF, {'B','G'}},
        {0x458000, 0x45FFFF, {'D','K'}},
        {0x460000, 0x467FFF, {'F','I'}},
        {0x468000, 0x46FFFF, {'G','R'}},
        {0x470000, 0x477FFF, {'H','U'}},
        {0x478000, 0x47FFFF, {'N','O'}},
        {0x480000, 0x487FFF, {'N','L'}},
        {0x488000, 0x48FFFF, {'P','L'}},
        {0x490000, 0x497FFF, {'P','T'}},
        {0x498000, 0x49FFFF, {'C','Z'}},
        {0x4A0000, 0x4A7FFF, {'R','O'}},
        {0x4A8000, 0x4AFFFF, {'S','E'}},
        {0x4B0000, 0x4B7FFF, {'C','H'}},
        {0x4B8000, 0x4BFFFF, {'T','R'}},
        {0x4C0000, 0x4C7FFF, {'R','S'}},
        {0x4C8000, 0x4C83FF, {'C','Y'}},
        {0x4CA000, 0x4CAFFF, {'I','E'}},
        {0x4CC000, 0x4CCFFF, {'I','S'}},
        {0x4D0000, 0x4D03FF, {'L','U'}},
        {0x4D2000, 0x4D23FF, {'M','T'}},
        {0x4D4000, 0x4D43FF, {'M','C'}},
        {0x500000, 0x5003FF, {'S','M'}},
        {0x501000, 0x5013FF, {'A','L'}},
        {0x501C00, 0x501FFF, {'H','R'}},
        {0x502C00, 0x502FFF, {'L','V'}},
        {0x503C00, 0x503FFF, {'L','T'}},
        {0x504C00, 0x504FFF, {'M','D'}},
        {0x505C00, 0x505FFF, {'S','K'}},
        {0x506C00, 0x506FFF, {'S','I'}},
        {0x507C00, 0x507FFF, {'U','Z'}},
        {0x508000, 0x50FFFF, {'U','A'}},
        {0x510000, 0x5103FF, {'B','Y'}},
        {0x511000, 0x5113FF, {'E','E'}},
        {0x512000, 0x5123FF, {'M','K'}},
        {0x513000, 0x5133FF, {'B','A'}},
        {0x514000, 0x5143FF, {'G','E'}},
        {0x515000, 0x5153FF, {'T','J'}},
        {0x600000, 0x6003FF, {'A','M'}},
        {0x600800, 0x600BFF, {'A','Z'}},
        {0x601000, 0x6013FF, {'K','G'}},
        {0x601800, 0x601BFF, {'T','M'}},
        {0x680000, 0x6803FF, {'B','T'}},
        {0x681000, 0x6813FF, {'F','M'}},
        {0x682000, 0x6823FF, {'M','N'}},
        {0x683000, 0x6833FF, {'K','Z'}},
        {0x684000, 0x6843FF, {'P','W'}},
        {0x700000, 0x700FFF, {'A','F'}},
        {0x702000, 0x702FFF, {'B','D'}},
        {0x704000, 0x704FFF, {'M','M'}},
        {0x706000, 0x706FFF, {'K','W'}},
        {0x708000, 0x708FFF, {'L','A'}},
        {0x70A000, 0x70AFFF, {'N','P'}},
        {0x70C000, 0x70C3FF, {'O','M'}},
        {0x70E000, 0x70EFFF, {'K','H'}},
        {0x710000, 0x717FFF, {'S','A'}},
        {0x718000, 0x71FFFF, {'K','R'}},
        {0x720000, 0x727FFF, {'K','P'}},
        {0x728000, 0x72FFFF, {'I','Q'}},
        {0x730000, 0x737FFF, {'I','R'}},
        {0x738000, 0x73FFFF, {'I','L'}},
        {0x740000, 0x747FFF, {'J','O'}},
        {0x748000, 0x74FFFF, {'L','B'}},
        {0x750000, 0x757FFF, {'M','Y'}},
        {0x758000, 0x75FFFF, {'P','H'}},
        {0x760000, 0x767FFF, {'P','K'}},
        {0x768000, 0x76FFFF, {'S','G'}},
        {0x770000, 0x777FFF, {'L','K'}},
        {0x778000, 0x77FFFF, {'S','Y'}},
        {0x780000, 0x7BFFFF, {'C','N'}},
        {0x7C0000, 0x7FFFFF, {'A','U'}},
        {0x800000, 0x83FFFF, {'I','N'}},
        {0x840000, 0x87FFFF, {'J','P'}},
        {0x880000, 0x887FFF, {'T','H'}},
        {0x888000, 0x88FFFF, {'V','N'}},
        {0x890000, 0x890FFF, {'Y','E'}},
        {0x894000, 0x894FFF, {'B','H'}},
        {0x895000, 0x8953FF, {'B','N'}},
        {0x896000, 0x896FFF, {'A','E'}},
        {0x897000, 0x8973FF, {'S','B'}},
        {0x898000, 0x898FFF, {'P','G'}},
        {0x899000, 0x8993FF, {'T','W'}},
        {0x8A0000, 0x8A7FFF, {'I','D'}},
        {0x900000, 0x9003FF, {'M','H'}},
        {0x901000, 0x9013FF, {'C','K'}},
        {0x902000, 0x9023FF, {'W','S'}},
        {0xA00000, 0xAFFFFF, {'U','S'}},
        {0xC00000, 0xC3FFFF, {'C','A'}},
        {0xC80000, 0xC87FFF, {'N','Z'}},
        {0xC88000, 0xC88FFF, {'F','J'}},
        {0xC8A000, 0xC8A3FF, {'N','R'}},
        {0xC8C000, 0xC8C3FF, {'L','C'}},
        {0xC8D000, 0xC8D3FF, {'T','O'}},
        {0xC8E000, 0xC8E3FF, {'K','I'}},
        {0xC90000, 0xC903FF, {'V','U'}},
        {0xE00000, 0xE3FFFF, {'A','R'}},
        {0xE40000, 0xE7FFFF, {'B','R'}},
        {0xE80000, 0xE80FFF, {'C','L'}},
        {0xE84000, 0xE84FFF, {'E','C'}},
        {0xE88000, 0xE88FFF, {'P','Y'}},
        {0xE8C000, 0xE8CFFF, {'P','E'}},
        {0xE90000, 0xE90FFF, {'U','Y'}},
        {0xE94000, 0xE94FFF, {'B','O'}},
    };
    // spellchecker:on

    static constexpr size_t rangeInfoCount = sizeof(rangeInfo) / sizeof(rangeInfo[0]);

    void ADSB::setCountryCode()
    {
        country_code[0] = country_code[1] = ' ';

        if (hexident < rangeInfo[0].startHex || hexident > rangeInfo[rangeInfoCount - 1].endHex)
            return;

        const RangeInfo *it = std::upper_bound(
            rangeInfo, rangeInfo + rangeInfoCount, hexident,
            [](uint32_t h, const RangeInfo &r) { return h < r.startHex; });

        if (it == rangeInfo)
            return;

        --it;
        if (hexident <= it->endHex)
        {
            country_code[0] = it->isoCountry[0];
            country_code[1] = it->isoCountry[1];
        }
    }
}
