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
        int val = getBits(37, 7);

        if (val == 1)
            return 0.0; // Stopped (v < 0.125 kt)
        if (val >= 2 && val <= 8)
            return 0.125 * (val - 1); // 0.125 kt steps
        if (val >= 9 && val <= 12)
            return 1.0 + 0.25 * (val - 8); // 0.25 kt steps
        if (val >= 13 && val <= 38)
            return 2.0 + 0.5 * (val - 12); // 0.5 kt steps
        if (val >= 39 && val <= 93)
            return 15.0 + 1.0 * (val - 38); // 1 kt steps
        if (val >= 94 && val <= 108)
            return 70.0 + 2.0 * (val - 93); // 2 kt steps
        if (val >= 109 && val <= 123)
            return 100.0 + 5.0 * (val - 108); // 5 kt steps
        if (val == 124)
            return 175.0; // v ≥ 175 kt

        return SPEED_UNDEFINED;
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
        {
            hexident = getBits(8, 24);
            hexident_status = HEXINDENT_DIRECT;

            int capability = getBits(5, 3);

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
        case 17: // Extended Squitter
        case 18: // Extended Squitter/Supplementary
            hexident = getBits(8, 24);
            hexident_status = HEXINDENT_DIRECT;

            if (!verifyCRC())
            {
                status = STATUS_ERROR;
                return;
            }

            int TC = getBits(32, 5);
            int ST = getBits(37, 3); // ME message subtype

            switch (TC)
            {
            case 1: // Aircraft Identification
            case 2:
            case 3:
            case 4:
                Callsign();
                break;

            case 19: // Airborne Velocity
                if (ST >= 1 && ST <= 4)
                {
                    if (ST == 1 || ST == 2)
                    {
                        int Vew = getBits(46, 10);
                        int Vns = getBits(57, 10);

                        if (Vew && Vns)
                        {
                            bool Dew = getBits(45, 1);
                            bool Dns = getBits(56, 1);

                            Vew = Dew ? -(Vew - 1) : (Vew - 1);
                            Vns = Dns ? -(Vns - 1) : (Vns - 1);

                            speed = sqrt(Vns * Vns + Vew * Vew);
                            heading = atan2(Vew, Vns) * 360.0 / (2 * PI);
                            if (heading < 0)
                                heading += 360;

                            if (ST == 2)
                                speed *= 4;
                        }

                        int VR = getBits(69, 9);
                        if (VR)
                        {
                            bool Svr = getBits(68, 1);
                            vertrate = (VR - 1) * 64 * (Svr ? -1 : 1);
                        }
                        airborne = 1;
                    }
                    // ignore ST 3/4, unknown aircraft speed
                }

                break;

            case 5: // Surface Position
            case 6:
            case 7:
            case 8:
            {
                airborne = 0;

                if (getBits(44, 1))
                    heading = getBits(45, 7) * 360 / 128.0;

                decodeMovement();

                CPR &cpr = getBits(53, 1) ? odd : even;

                cpr.lat = getBits(54, 17);
                cpr.lon = getBits(71, 17);
                cpr.timestamp = rxtime;
                cpr.airborne = false;
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
                airborne = 1;

                CPR &cpr = getBits(53, 1) ? odd : even;

                cpr.lat = getBits(54, 17);
                cpr.lon = getBits(71, 17);
                cpr.timestamp = rxtime;
                cpr.airborne = true;
            }
            break;
            }
            break;
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

        double tmp = 1 - (1 - cos(PI / (2.0 * 15.0))) / pow(cos(PI / 180.0 * abs(lat)), 2);
        return std::floor(2 * PI / acos(tmp));
    }

    bool ADSB::decodeCPR(FLOAT32 lat, FLOAT32 lon, bool use_even)
    {
        if (!even.Valid() || !odd.Valid() || (even.airborne != odd.airborne))
            return false;

        if (even.airborne)
        {
            return decodeCPR_airborne(use_even);
        }
        return decodeCPR_surface(lat, lon, use_even);
    }

    bool ADSB::decodeCPR_airborne(bool use_even)
    {
        constexpr double CPR_SCALE = (double)(1 << 17);

        int j = (int)std::floor(((59.0 * even.lat) - (60.0 * odd.lat)) / CPR_SCALE + 0.5);

        double lat_even = 360.0 / 60 * (MOD(j, 60) + even.lat / CPR_SCALE);
        double lat_odd = 360.0 / 59 * (MOD(j, 59) + odd.lat / CPR_SCALE);

        if (lat_even >= 270.0)
            lat_even -= 360.0;
        if (lat_odd >= 270.0)
            lat_odd -= 360.0;

        int nl = NL(lat_even);

        if (nl != NL(lat_odd))
            return false;

        lat = use_even ? lat_even : lat_odd;

        int ni = MAX(nl - (use_even ? 0 : 1), 1);
        int m = std::floor((even.lon * (nl - 1) - odd.lon * nl) / CPR_SCALE + 0.5);

        double lon_final = use_even ? even.lon : odd.lon;

        lon = (360.0 / ni) * (MOD(m, ni) + lon_final / CPR_SCALE);

        if (lon > 180.0)
            lon -= 360.0;

        latlon_timestamp = use_even ? even.timestamp : odd.timestamp;

        return true;
    }

    bool ADSB::decodeCPR_airborne_reference(bool use_even, FLOAT32 ref_lat, FLOAT32 ref_lon)
    {
        constexpr double CPR_SCALE = (double)(1 << 17);

        CPR &cpr = use_even ? even : odd;
        double d_lat = use_even ? 360.0 / 60 : 360.0 / 59;
        int j = std::floor(ref_lat / d_lat) + std::floor(0.5 + MOD(ref_lat, d_lat) / d_lat - cpr.lat / CPR_SCALE + 0.5);

        lat = d_lat * (j + cpr.lat / CPR_SCALE);

        int ni = NL(lat) - (use_even ? 0 : 1);
        double d_lon = ni > 0 ? 360 / ni : 360;

        int m = std::floor(ref_lon / d_lon) + std::floor(0.5 + (MOD(ref_lon, d_lon) / d_lon) - cpr.lon / CPR_SCALE);

        lon = d_lon * (m + cpr.lon / CPR_SCALE);
        latlon_timestamp = cpr.timestamp;

        return true;
    }

    // based on the example from the 1090 riddle
    bool ADSB::decodeCPR_surface(FLOAT32 ref_lat, FLOAT32 ref_lon, bool use_even)
    {
        static bool warning_given = false;

        if (ref_lat == LAT_UNDEFINED || ref_lon == LON_UNDEFINED)
        {
            if (!warning_given)
            {
                Error() << "ADSB: Reference position is not available. Cannot determine location of onground planes. Provide receiver location with -Z lat lon." << std::endl;
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

        lat = use_even ? lat_even : lat_odd;
        lat -= 90.0 * std::floor((lat - ref_lat + 45.0) / 90.0);

        int ni = MAX(nl - (use_even ? 0 : 1), 1);

        int m = std::floor((even.lon * (nl - 1) - odd.lon * nl) / CPR_SCALE + 0.5);
        double lon_final = use_even ? even.lon : odd.lon;

        lon = (90.0 / ni) * (MOD(m, ni) + lon_final / CPR_SCALE);
        lon -= 90.0 * std::floor((lon - ref_lon + 45.0) / 90.0);

        latlon_timestamp = use_even ? even.timestamp : odd.timestamp;

        return true;
    }

    bool ADSB::decodeCPR_surface_reference(bool use_even, FLOAT32 ref_lat, FLOAT32 ref_lon)
    {
        constexpr double CPR_SCALE = (double)(1 << 17);

        CPR &cpr = use_even ? even : odd;
        double d_lat = use_even ? 90.0 / 60 : 90.0 / 59;
        int j = std::floor(ref_lat / d_lat) + std::floor(0.5 + MOD(ref_lat, d_lat) / d_lat - cpr.lat / CPR_SCALE + 0.5);

        lat = d_lat * (j + cpr.lat / CPR_SCALE);

        int ni = NL(lat) - (use_even ? 0 : 1);
        double d_lon = ni > 0 ? 90 / ni : 90;

        int m = std::floor(ref_lon / d_lon) + std::floor(0.5 + (MOD(ref_lon, d_lon) / d_lon) - cpr.lon / CPR_SCALE);

        lon = d_lon * (m + cpr.lon / CPR_SCALE);
        latlon_timestamp = cpr.timestamp;

        return true;
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

        if (timestamp != TIME_UNDEFINED)
            std::cout << "TS: " << (long)(timestamp) << std::endl;

        if (signalLevel != LEVEL_UNDEFINED)
            std::cout << "LVL: " << signalLevel << std::endl;
    }
}