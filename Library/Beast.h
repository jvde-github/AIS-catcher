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

#include <vector>
#include <ctime>

#include "JSON/JSON.h"
#include "Keys.h"
#include "JSON/StringBuilder.h"

// References:
// https://github.com/wiedehopf/readsb/blob/dev/mode_s.c
// https://github.com/cjkreklow/go-adsb
// https://pure.tudelft.nl/ws/portalfiles/portal/104877928/11_Book_Manuscript_69_1_10_20210510.pdf

class Beast : public SimpleStreamInOut<RAW, JSON::JSON>
{
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;
    static constexpr double BEAST_CLOCK_MHZ = 12.0;

    JSON::JSON json;
    std::string callsign;
    const std::string undefined = "Undefined";

    enum
    {
        BEAST_ESCAPE = 0x1A,
        BEAST_ESCAPE_START = 0x1B
    };

    enum class State
    {
        WAIT_ESCAPE,
        WAIT_TYPE,
        READ_TIMESTAMP,
        READ_SIGNAL,
        READ_PAYLOAD,
        READ_ESCAPE
    };

    uint8_t type;
    uint64_t timestamp;
    double signalLevel;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> buffer;

    State state = State::WAIT_ESCAPE;
    int bytes_read = 0;

    uint32_t getBits(const std::vector<uint8_t> &data, int startBit, int len)
    {
        uint32_t result = 0;
        for (int i = 0; i < len; i++)
        {
            int byteIdx = (startBit + i) / 8;
            int bitIdx = 7 - ((startBit + i) % 8);
            if (byteIdx < data.size() && (data[byteIdx] & (1 << bitIdx)))
            {
                result |= (1 << (len - 1 - i));
            }
        }
        return result;
    }

    void U(const std::vector<uint8_t> &msg, int key, int start, int len, unsigned undefined = 0)
    {
        unsigned u = getBits(msg, start, len);
        if (u != undefined)
        {
            json.Add(key, (int)u);
        }
    }

    void B(const std::vector<uint8_t> &msg, int key, int start, int len)
    {
        json.Add(key, (bool)getBits(msg, start, len));
    }

    void E(const std::vector<uint8_t> &msg, int p, int start, int len, int pmap, const std::vector<std::string> *map)
    {
        unsigned u = getBits(msg, start, len);
        json.Add(p, (int)u);
        if (map)
        {
            if (u < map->size())
                json.Add(pmap, &(*map)[u]);
            else
                json.Add(pmap, &undefined);
        }
    }

    int decodeAC(unsigned ac)
    {
        if (ac == 0)
            return 0;

        bool q_bit = (ac & 0x10);

        if (q_bit)
        {
            int n = ((ac & 0x0FE0) >> 1) | (ac & 0x000F);
            return (n * 25) - 1000;
        }
        return 0; // Return 0 for non-Q-bit format
    }

    int decodeESAlt(unsigned alt)
    {
        if (alt == 0)
            return 0;

        bool q_bit = (alt & 0x10);

        if (q_bit)
        {

            int n = ((alt & 0xFE0) >> 1) | (alt & 0xF);
            return ((n * 25) - 1000);
        }
        return 0;
    }

    void decodeAltitude(const std::vector<uint8_t> &msg, int startBit, int len)
    {
        unsigned ac = getBits(msg, startBit, len);
        if (ac == 0)
            return;

        uint8_t df = getBits(msg, 0, 5);
        if (df == 17 || df == 18)
        {
            int altitude = decodeESAlt(ac);
            if (altitude != 0)
            {
                json.Add(AIS::KEY_ALT, altitude);
            }
        }
        else
        {
            int altitude = decodeAC(ac);
            if (altitude != 0)
            {
                json.Add(AIS::KEY_ALT, altitude);
            }
        }
    }

    std::string bytesToHexString(const std::vector<uint8_t> &bytes)
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (const auto &byte : bytes)
        {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        return ss.str();
    }

    void decodeVelocity(const std::vector<uint8_t> &msg)
    {
        uint8_t subtype = getBits(msg, 37, 3);
        json.Add(AIS::KEY_VELOCITY_TYPE, (int)subtype);

        if (subtype == 1 || subtype == 2)
        { // Ground speed
            // East-West velocity
            bool ew_sign = getBits(msg, 45, 1);
            int ew_vel = (getBits(msg, 46, 10) - 1) * (ew_sign ? -1 : 1);

            // North-South velocity
            bool ns_sign = getBits(msg, 56, 1);
            int ns_vel = (getBits(msg, 57, 10) - 1) * (ns_sign ? -1 : 1);

            // Calculate ground speed and heading
            double speed = sqrt(ew_vel * ew_vel + ns_vel * ns_vel);
            double heading = atan2(ew_vel, ns_vel) * 180 / PI;
            if (heading < 0)
                heading += 360;

            json.Add(AIS::KEY_GROUND_SPEED, speed);
            json.Add(AIS::KEY_HEADING, heading);

            // Vertical rate
            bool vr_sign = getBits(msg, 68, 1);
            int vr = (getBits(msg, 69, 9) - 1) * 64 * (vr_sign ? -1 : 1);
            json.Add(AIS::KEY_VERTICAL_RATE, vr);
        }
        else if (subtype == 3 || subtype == 4)
        { // Airspeed
            bool airspeed_type = getBits(msg, 56, 1);
            int airspeed = getBits(msg, 57, 10);
            json.Add(airspeed_type ? AIS::KEY_TRUE_AIRSPEED : AIS::KEY_INDICATED_AIRSPEED, airspeed);

            // Heading
            int heading = getBits(msg, 46, 10) * 360.0 / 1024.0;
            json.Add(AIS::KEY_HEADING, heading);
        }
    }

    void decodeCallsign(const std::vector<uint8_t> &msg)
    {
        static const char *ais_charset = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        callsign.clear();
        for (int i = 0; i < 8; i++)
        {
            uint32_t char_code = getBits(msg, 40 + (i * 6), 6);

            char c = ais_charset[char_code];
            if (c != '#')
                callsign += c;
        }

        if (!callsign.empty())
        {
            json.Add(AIS::KEY_CALLSIGN, &callsign);
        }
    }

    void decodeExtendedSquitter(const std::vector<uint8_t> &msg)
    {
        uint8_t type = getBits(msg, 32, 5);
        E(msg, AIS::KEY_CA, 5, 3, AIS::KEY_CA_TEXT, &ADSB_MAP_CA);
        U(msg, AIS::KEY_ICAO, 8, 24);
        E(msg, AIS::KEY_TYPE, 32, 5, AIS::KEY_TYPE_TEXT, &ADSB_MAP_TYPE);

        switch (type)
        {
        case 1:
        case 2:
        case 3:
        case 4: // Aircraft Identification
            E(msg, AIS::KEY_CA, 37, 3, AIS::KEY_WAKE_VORTEX, &ADSB_MAP_WAKE_VORTEX[type]);
            decodeCallsign(msg);
            break;

        case 19: // Airborne Velocity
            decodeVelocity(msg);
            break;

        case 5:
        case 6:
        case 7:
        case 8: // Surface Position
            B(msg, AIS::KEY_CPR_ODD, 54, 1);
            U(msg, AIS::KEY_CPR_LAT, 55, 17);
            U(msg, AIS::KEY_CPR_LON, 72, 17);
            break;

        case 9:
        case 10:
        case 11:
        case 12: // Airborne Position
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
            decodeAltitude(msg, 37, 12);
            B(msg, AIS::KEY_CPR_ODD, 54, 1);
            U(msg, AIS::KEY_CPR_LAT, 55, 17);
            U(msg, AIS::KEY_CPR_LON, 72, 17);
            break;
        }
    }

    void ProcessModeS(const std::vector<uint8_t> &msg, double signalLevel, uint64_t timestamp, TAG &tag)
    {
        json.clear();

        // Add basic message info
        uint8_t df = getBits(msg, 0, 5);
        json.Add(AIS::KEY_CLASS, &class_str);
        json.Add(AIS::KEY_DEVICE, &device);

        // json.Add(AIS::KEY_DF, (int)df);
        E(msg, AIS::KEY_DF, 0, 5, AIS::KEY_DF_TEXT, &ADSB_MAP_DF);
        json.Add(AIS::KEY_SIGNAL, signalLevel);
        json.Add(AIS::KEY_TIMESTAMP, (double)timestamp / BEAST_CLOCK_MHZ);
        json.Add(AIS::KEY_RAW_MESSAGE, bytesToHexString(msg));

        switch (df)
        {
        case 0: // Short Air-Air Surveillance
            U(msg, AIS::KEY_VS, 5, 1);
            U(msg, AIS::KEY_CC, 6, 1);
            U(msg, AIS::KEY_SL, 8, 3);
            U(msg, AIS::KEY_RI, 13, 4);
            decodeAltitude(msg, 19, 13);
            break;

        case 4:  // Surveillance, Altitude Reply
        case 20: // Comm-B, Altitude Reply
            U(msg, AIS::KEY_FS, 5, 3);
            U(msg, AIS::KEY_DR, 8, 5);
            decodeAltitude(msg, 19, 13);
            break;

        case 11: // All-Call Reply
            U(msg, AIS::KEY_CA, 5, 3);
            U(msg, AIS::KEY_AA, 8, 24);
            U(msg, AIS::KEY_IID, 17, 4);
            break;

        case 17: // Extended Squitter
            U(msg, AIS::KEY_CA, 5, 3);
            // U(msg, AIS::KEY_AA, 8, 24);
            decodeExtendedSquitter(msg);
            Send(&json, 1, tag);

            break;

        case 18: // Extended Squitter/Supplementary
            // U(msg, AIS::KEY_CF, 5, 3);  // Control Field instead of CA
            // U(msg, AIS::KEY_AA, 8, 24);
            decodeExtendedSquitter(msg);
            break;
        }
    }

    const std::string class_str = "ADSB";
    const std::string device = "AIS-catcher";

public:
    virtual ~Beast() {}

    void Receive(const RAW *data, int len, TAG &tag)
    {
        for (int j = 0; j < len; j++)
        {
            for (int i = 0; i < data[j].size; i++)
            {
                uint8_t c = ((uint8_t *)(data[j].data))[i];
                ProcessByte(c, tag);
            }
        }
    }

private:
    void Clear()
    {
        state = State::WAIT_ESCAPE;
        buffer.clear();
    }

    void ProcessByte(uint8_t byte, TAG &tag)
    {
        if (buffer.size() > MAX_MESSAGE_SIZE)
        {
            Clear();
            return;
        }

        switch (state)
        {
        case State::WAIT_ESCAPE:
            if (byte == BEAST_ESCAPE)
            {
                state = State::WAIT_TYPE;
                buffer.clear();
            }
            break;

        case State::WAIT_TYPE:
            if (byte >= 0x31 && byte <= 0x33)
            { // Valid types: '1', '2', '3'
                type = byte;
                state = State::READ_TIMESTAMP;
                bytes_read = 0;
                buffer.clear();
            }
            else
            {
                state = State::WAIT_ESCAPE;
            }
            break;

        case State::READ_TIMESTAMP:
            if (byte == BEAST_ESCAPE)
            {
                state = State::READ_ESCAPE;
                break;
            }

            buffer.push_back(byte);
            bytes_read++;

            if (bytes_read == 6)
            { // 6-byte timestamp
                timestamp = ParseTimestamp();
                buffer.clear();
                state = State::READ_SIGNAL;
                bytes_read = 0;
            }
            break;

        case State::READ_SIGNAL:
            if (byte == BEAST_ESCAPE)
            {
                state = State::READ_ESCAPE;
                break;
            }

            // Process signal level as in readsb
            signalLevel = static_cast<double>(byte) / 255.0;
            signalLevel = signalLevel * signalLevel; // Square for power
            state = State::READ_PAYLOAD;
            bytes_read = 0;
            break;

        case State::READ_PAYLOAD:
        {
            if (byte == BEAST_ESCAPE)
            {
                state = State::READ_ESCAPE;
                break;
            }

            buffer.push_back(byte);
            bytes_read++;

            int expected_len = GetExpectedLength();
            if (bytes_read == expected_len)
            {
                ProcessMessage(tag);
                state = State::WAIT_ESCAPE;
                buffer.clear();
            }
        }
        break;

        case State::READ_ESCAPE:
            if (byte == BEAST_ESCAPE)
            {
                buffer.push_back(BEAST_ESCAPE);
                bytes_read++;
                state = State::READ_PAYLOAD;
            }
            else if (byte == BEAST_ESCAPE_START)
            {
                state = State::WAIT_TYPE;
                buffer.clear();
                bytes_read = 0;
            }
            else
            {
                state = State::WAIT_ESCAPE;
                buffer.clear();
            }
            break;
        }
    }

    uint64_t ParseTimestamp() const
    {
        if (buffer.size() < 6)
        {
            Error() << "Insufficient data for timestamp";
        }

        uint64_t ts = 0;
        for (int i = 0; i < 6; i++)
        {
            ts = (ts << 8) | buffer[i];
        }
        return ts;
    }

    int GetExpectedLength() const
    {
        switch (type)
        {
        case '1':
            return 2; // Mode AC
        case '2':
            return 7; // Mode S short
        case '3':
            return 14; // Mode S long
        default:
            return 0;
        }
    }

    void ProcessMessage(TAG &tag)
    {
        switch (type)
        {
        case '1':
            // ProcessModeAC(buffer, signalLevel, timestamp);
            break;

        case '2':
        case '3':
            ProcessModeS(buffer, signalLevel, timestamp, tag);
            break;
        }
    }

    const std::vector<std::string> ADSB_MAP_CA = {
        "Level 1 transponder",               // 0
        "Reserved",                          // 1
        "Reserved",                          // 2
        "Reserved",                          // 3
        "Level 2+ transponder (on-ground)",  // 4
        "Level 2+ transponder (airborne)",   // 5
        "Level 2+ transponder (ground/air)", // 6
        "DR=0 or FS=2,3,4,5 (ground/air)"    // 7
    };

    const std::vector<std::string> ADSB_MAP_DF = {
        "Short air-air surveillance (ACAS)", // 0
        "Reserved",                          // 1
        "Reserved",                          // 2
        "Reserved",                          // 3
        "Surveillance, altitude reply",      // 4
        "Surveillance, identity reply",      // 5
        "Reserved",                          // 6
        "Reserved",                          // 7
        "Reserved",                          // 8
        "Reserved",                          // 9
        "Reserved",                          // 10
        "All-Call reply",                    // 11
        "Reserved",                          // 12
        "Reserved",                          // 13
        "Reserved",                          // 14
        "Reserved",                          // 15
        "Long air-air surveillance (ACAS)",  // 16
        "Extended squitter",                 // 17
        "Extended squitter/non transponder", // 18
        "Military extended squitter",        // 19
        "Comm-B, altitude reply",            // 20
        "Comm-B, identity reply",            // 21
        "Reserved",                          // 22
        "Reserved",                          // 23
        "Comm-D (ELM)"                       // 24
    };

    const std::vector<std::string> ADSB_MAP_TYPE = {
        "Reserved",                            // 0
        "Aircraft identification",             // 1
        "Aircraft identification",             // 2
        "Aircraft identification",             // 3
        "Aircraft identification",             // 4
        "Surface position",                    // 5
        "Surface position",                    // 6
        "Surface position",                    // 7
        "Surface position",                    // 8
        "Airborne position (w/Baro Altitude)", // 9
        "Airborne position (w/Baro Altitude)", // 10
        "Airborne position (w/Baro Altitude)", // 11
        "Airborne position (w/Baro Altitude)", // 12
        "Airborne position (w/Baro Altitude)", // 13
        "Airborne position (w/Baro Altitude)", // 14
        "Airborne position (w/Baro Altitude)", // 15
        "Airborne position (w/Baro Altitude)", // 16
        "Airborne position (w/Baro Altitude)", // 17
        "Airborne position (w/Baro Altitude)", // 18
        "Airborne velocities",                 // 19
        "Airborne position (w/GNSS Height)",   // 20
        "Airborne position (w/GNSS Height)",   // 21
        "Airborne position (w/GNSS Height)",   // 22
        "Reserved",                            // 23
        "Reserved",                            // 24
        "Reserved",                            // 25
        "Reserved",                            // 26
        "Reserved",                            // 27
        "Aircraft status",                     // 28
        "Target state and status information", // 29
        "Reserved",                            // 30
        "Aircraft operation status"            // 31
    };

    const std::vector<std::vector<std::string>> ADSB_MAP_WAKE_VORTEX = {
        {

        },
        // Type 1 (index 0)
        {
          "No category information"
        },
        // Type 2 (index 1)
        {
            "No category information",
            "Surface emergency vehicle",
            "Reserved",
            "Surface service vehicle",
            "Ground obstruction",
            "Ground obstruction",
            "Ground obstruction",
            "Ground obstruction"},
        // Type 3 (index 2)
        {
            "No category information",
            "Glider, sailplane",
            "Lighter-than-air",
            "Parachutist, skydiver",
            "Ultralight, hang-glider, paraglider",
            "Reserved",
            "Unmanned aerial vehicle",
            "Space or transatmospheric vehicle"},
        // Type 4 (index 3)
        {
            "No category information",
            "Light (< 7000 kg)",
            "Medium 1 (7000-34000 kg)",
            "Medium 2 (34000-136000 kg)",
            "High vortex aircraft",
            "Heavy (> 136000 kg)",
            "High performance (> 400 kt, > 5g acceleration)",
            "Rotorcraft"
        }
    };
};
