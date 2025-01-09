#pragma once

#include <iomanip>
#include <vector>
#include <ctime>
#include <sstream>
#include <stdexcept>

class Beast : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    static constexpr size_t MAX_MESSAGE_SIZE = 1024; // Reasonable maximum size
    static constexpr double BEAST_CLOCK_MHZ = 12.0;  // 12 MHz clock rate

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
    void ProcessModeS(TAG &tag, bool isLong)
    {
        uint8_t df = getBits(buffer, 0, 5);
        uint32_t icao = getICAO(buffer, df);

        std::cerr << "-----" << std::endl;

        // Base message info - keep existing code
        std::cerr << "Raw message: ";
        for (const auto &b : buffer)
        {
            std::cerr << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cerr << std::dec << std::endl;

        std::cerr << "df = " << static_cast<int>(df) << std::endl;
        std::cerr << "Timestamp: " << timestamp << std::endl;
        std::cerr << "Signal level: " << signalLevel << std::endl;
        std::cerr << "DF meaning: " << getDFMeaning(df) << std::endl;

        if (icao)
        {
            std::cerr << "ICAO: " << std::hex << std::setw(6) << std::setfill('0') << icao << std::dec << std::endl;
        }

        // Enhanced decoding
        if (df == 17 || df == 18)
        {
            uint8_t tc = getBits(buffer, 32, 5);
            std::cerr << "Type Code: " << static_cast<int>(tc) << std::endl;

            // Aircraft Identification (Callsign)
            if (tc >= 1 && tc <= 4)
            {
                std::cerr << "Message Type: Aircraft Identification" << std::endl;
                std::string callsign = decodeCallsign();
                std::cerr << "Callsign: " << callsign << std::endl;
            }
            // Position messages
            else if (tc >= 9 && tc <= 18)
            {
                std::cerr << "Message Type: Airborne Position" << std::endl;
                //bool onGround = false;
                if (tc >= 9 && tc <= 18)
                {
                    std::cerr << "On Ground: No" << std::endl;
                }
                else if (tc >= 5 && tc <= 8)
                {
                    std::cerr << "On Ground: Yes" << std::endl;
                    //onGround = true;
                }
            }
            // Airborne velocity
            else if (tc == 19)
            {
                std::cerr << "Message Type: Airborne Velocity" << std::endl;
                decodeVelocity();
            }
        }

        // Flight Status (existing code)
        if (df == 4 || df == 5 || df == 20 || df == 21)
        {
            uint8_t fs = getBits(buffer, 5, 3);
            std::string fs_meaning;
            switch (fs)
            {
                // ... keep existing flight status code ...
            }
            std::cerr << "Flight Status: " << static_cast<int>(fs) << " (" << fs_meaning << ")" << std::endl;
        }

        // Flight Status for DF 4,5,20,21
        if (df == 4 || df == 5 || df == 20 || df == 21)
        {
            uint8_t fs = getBits(buffer, 5, 3);
            std::string fs_meaning;
            switch (fs)
            {
            case 0:
                fs_meaning = "no alert, no SPI, aircraft is airborne";
                break;
            case 1:
                fs_meaning = "no alert, no SPI, aircraft is on-ground";
                break;
            case 2:
                fs_meaning = "alert, no SPI, aircraft is airborne";
                break;
            case 3:
                fs_meaning = "alert, no SPI, aircraft is on-ground";
                break;
            case 4:
                fs_meaning = "alert, SPI, aircraft is airborne or on-ground";
                break;
            case 5:
                fs_meaning = "no alert, SPI, aircraft is airborne or on-ground";
                break;
            default:
                fs_meaning = "unknown";
                break;
            }
            std::cerr << "Flight Status: " << static_cast<int>(fs) << " (" << fs_meaning << ")" << std::endl;
        }

        // Altitude for DF 0,4,16,20
        if (df == 0 || df == 4 || df == 16 || df == 20)
        {
            std::string altitude_code = getBitsString(buffer, 19, 13);
            bool m_bit = altitude_code[6] == '1';
            bool q_bit = altitude_code[8] == '1';

            int altitude = -1;
            if (q_bit && !m_bit)
            { // 25ft interval
                std::string vbin = altitude_code.substr(0, 6) + altitude_code[7] + altitude_code.substr(9);
                altitude = binToInt(vbin) * 25 - 1000;
                std::cerr << "Altitude: " << altitude << " ft (25ft interval)" << std::endl;
            }
            else
            {
                std::cerr << "Altitude: encoding not implemented" << std::endl;
            }
        }

        // Identity/Squawk for DF 5,21
        if (df == 5 || df == 21)
        {
            std::string id_code = getBitsString(buffer, 19, 13);
            // Implement squawk code extraction similar to pyModeS squawk function
            std::cerr << "Identity code (raw): " << id_code << std::endl;
        }

        // Type code for DF 17,18 (ADS-B)
        if (df == 17 || df == 18)
        {
            uint8_t tc = getBits(buffer, 32, 5);
            std::cerr << "Type Code: " << static_cast<int>(tc) << std::endl;

            if (tc >= 1 && tc <= 4)
            {
                std::cerr << "Message Type: Aircraft Identification" << std::endl;
                // Implement aircraft identification decoding
            }
            else if (tc >= 5 && tc <= 8)
            {
                std::cerr << "Message Type: Surface Position" << std::endl;
            }
            else if (tc >= 9 && tc <= 18)
            {
                std::cerr << "Message Type: Airborne Position" << std::endl;
            }
            else if (tc == 19)
            {
                std::cerr << "Message Type: Airborne Velocity" << std::endl;
                // Implement velocity decoding
            }
        }

        // Print binary representation for debugging
        std::cerr << "Binary message: ";
        for (size_t i = 0; i < buffer.size() * 8; i++)
        {
            std::cerr << ((getBits(buffer, i, 1) == 1) ? "1" : "0");
        }
        std::cerr << std::endl;

        std::vector<uint8_t> full_message;
        full_message.push_back(BEAST_ESCAPE); // Start marker
        full_message.push_back(type);         // Message type

        // Add timestamp bytes
        for (int i = 0; i < 6; i++)
        {
            full_message.push_back((timestamp >> (i * 8)) & 0xFF);
        }

        // Add signal level
        full_message.push_back(static_cast<uint8_t>(signalLevel * 255));

        // Add payload
        full_message.insert(full_message.end(), buffer.begin(), buffer.end());

        std::cerr << "\nTest command:\n"
                  << generateEchoCommand(full_message) << std::endl;
    }

    std::string decodeCallsign()
    {
        static const char *callsignChars =
            "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

        std::string callsign;
        for (int i = 0; i < 8; i++)
        {
            uint32_t char_code = getBits(buffer, 40 + (i * 6), 6);
            if (char_code > 0 && char_code < 64)
            {
                char c = callsignChars[char_code];
                if (c != '?' && c != ' ')
                {
                    callsign += c;
                }
            }
        }
        return callsign;
    }

    void decodeVelocity()
    {
        // Get subtype (1-4)
        uint8_t subtype = getBits(buffer, 37, 3);

        if (subtype == 1 || subtype == 2)
        { // Ground speed
            // East-West velocity
            bool east_west_sign = getBits(buffer, 45, 1);
            int east_west_vel = getBits(buffer, 46, 10) - 1;
            if (east_west_sign)
                east_west_vel *= -1;

            // North-South velocity
            bool north_south_sign = getBits(buffer, 56, 1);
            int north_south_vel = getBits(buffer, 57, 10) - 1;
            if (north_south_sign)
                north_south_vel *= -1;

            // Vertical rate
            bool vert_rate_sign = getBits(buffer, 68, 1);
            int vert_rate = (getBits(buffer, 69, 9) - 1) * 64;
            if (vert_rate_sign)
                vert_rate *= -1;

            // Calculate ground speed and heading
            double speed = sqrt(east_west_vel * east_west_vel +
                                north_south_vel * north_south_vel);
            double heading = atan2(east_west_vel, north_south_vel) * 180 / M_PI;
            if (heading < 0)
                heading += 360;

            std::cerr << "Ground Speed: " << speed << " knots" << std::endl;
            std::cerr << "Heading: " << heading << " degrees" << std::endl;
            std::cerr << "Vertical Rate: " << vert_rate << " ft/min" << std::endl;
        }
        else if (subtype == 3 || subtype == 4)
        {                                                // Airspeed
            bool airspeed_type = getBits(buffer, 56, 1); // 0 = IAS, 1 = TAS
            int airspeed = getBits(buffer, 57, 10);

            std::cerr << (airspeed_type ? "True" : "Indicated")
                      << " Airspeed: " << airspeed << " knots" << std::endl;

            // Heading
            int heading = getBits(buffer, 46, 10) * 360.0 / 1024.0;
            std::cerr << "Heading: " << heading << " degrees" << std::endl;
        }
    }

    // Helper function to get bits as string
    std::string getBitsString(const std::vector<uint8_t> &data, int startBit, int length)
    {
        std::string result;
        for (int i = 0; i < length; i++)
        {
            int byteIdx = (startBit + i) / 8;
            int bitIdx = 7 - ((startBit + i) % 8);
            if (byteIdx < data.size())
            {
                result += (data[byteIdx] & (1 << bitIdx)) ? "1" : "0";
            }
        }
        return result;
    }

    // Helper function to convert binary string to integer
    int binToInt(const std::string &binstr)
    {
        return std::stoi(binstr, nullptr, 2);
    }

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

    static std::string generateEchoCommand(const std::vector<uint8_t> &data)
    {
        std::stringstream ss;
        ss << "echo -en \"";
        // Skip Beast format elements, just output the raw message
        for (const auto &byte : data) // Changed from buffer to data
        {
            ss << "\\x" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte);
        }
        ss << "\" > raw_message.bin"; // Changed filename to indicate raw format
        return ss.str();
    }

    uint64_t ParseTimestamp()
    {
        if (buffer.size() != 6)
        {
            Error() << "Invalid timestamp buffer size";
        }

        uint64_t ts = 0;

        for (int i = 5; i >= 0; i--)
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
            ProcessModeAC(tag);
            break;
        case '2':
            ProcessModeSShort(tag);
            break;
        case '3':
            ProcessModeSLong(tag);
            break;
        }
    }

    void ProcessModeAC(TAG &tag)
    {
        if (buffer.size() != 2)
            return;
    }

    void ProcessModeSShort(TAG &tag)
    {
        if (buffer.size() != 7)
            return;
        ProcessModeS(tag, false);
    }

    void ProcessModeSLong(TAG &tag)
    {
        if (buffer.size() != 14)
            return;
        ProcessModeS(tag, true);
    }

    std::string getDFMeaning(uint8_t df)
    {
        switch (df)
        {
        case 0:
            return "Short Air-Air Surveillance (ACAS)";
        case 4:
            return "Surveillance, Altitude Reply";
        case 5:
            return "Surveillance, Identity Reply";
        case 11:
            return "Mode S All-Call Reply";
        case 16:
            return "Long Air-Air Surveillance (ACAS)";
        case 17:
            return "Extended Squitter ADS-B";
        case 18:
            return "Extended Squitter/Supplementary, TIS-B";
        case 19:
            return "Extended Squitter Military Application";
        case 20:
            return "Comm-B Altitude Reply";
        case 21:
            return "Comm-B Identity Reply";
        case 24:
            return "Comm-D Extended Length Message";
        default:
            return "Unknown DF Type";
        }
    }

    uint32_t getICAO(const std::vector<uint8_t> &buffer, uint8_t df)
    {
        switch (df)
        {
        case 17:                                                     // ADS-B
        case 18:                                                     // TIS-B
            return (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]; // Changed from [0,1,2] to [1,2,3]

        case 11: // All Call Reply
            // ICAO is in bits 9-32
            return getBits(buffer, 9, 24);

        case 0:       // Short Air-Air Surveillance
        case 4:       // Altitude Reply
        case 5:       // Identity Reply
        case 16:      // Long Air-Air Surveillance
            return 0; // These formats don't contain ICAO

        default:
            return 0; // Unknown format
        }
    }

    uint32_t getBits(const std::vector<uint8_t> &data, int startBit, int length)
    {
        uint32_t result = 0;
        for (int i = 0; i < length; i++)
        {
            int byteIdx = (startBit + i) / 8;
            int bitIdx = 7 - ((startBit + i) % 8);
            if (byteIdx < data.size() && (data[byteIdx] & (1 << bitIdx)))
            {
                result |= (1 << (length - 1 - i));
            }
        }
        return result;
    }
};