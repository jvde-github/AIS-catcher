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

    uint64_t ParseTimestamp()
    {
        if (buffer.size() != 6)
        {
            Error() << "Invalid timestamp buffer size";
        }

        uint64_t ts = 0;
        // Process in big-endian order
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

        uint16_t modeAC = (buffer[0] << 8) | buffer[1];
        // Process Mode A/C message...
        // Implementation details depend on your message handling system
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

    uint32_t getICAO(const std::vector<uint8_t>& buffer, uint8_t df) {
    switch(df) {
        case 17:  // ADS-B
        case 18:  // TIS-B
            return (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
            
        case 11:  // All Call Reply
            // ICAO is in bits 9-32
            return getBits(buffer, 9, 24);
            
        case 0:   // Short Air-Air Surveillance
        case 4:   // Altitude Reply
        case 5:   // Identity Reply
        case 16:  // Long Air-Air Surveillance
            return 0;  // These formats don't contain ICAO
            
        default:
            return 0;  // Unknown format
    }
}
    void ProcessModeS(TAG &tag, bool isLong)
    {
        uint8_t df = getBits(buffer, 0, 5);    
        uint32_t icao = getICAO(buffer, df);

        std::cerr << "-----" << std::endl;
        if(icao)
        std::cerr << "ICAO: " << std::hex << std::setw(6) << std::setfill('0') << icao << std::dec << std::endl;
        std::cerr << "df = " << static_cast<int>(df) << std::endl;
        std::cerr << "Timestamp: " << timestamp << std::endl;
        std::cerr << "Signal level: " << signalLevel << std::endl;
        std::cerr << "DF meaning: " << getDFMeaning(df) << std::endl;

        // Create and populate your message structure based on DF type
        // Implementation depends on your message handling system
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