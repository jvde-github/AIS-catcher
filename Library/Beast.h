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

class RAW1090 : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    Plane::ADSB msg;
    static constexpr size_t MAX_MSG_LEN = 14;
    enum class State
    {
        WAIT_START, // Waiting for '*'
        READ_MSG,   // Reading message content
    };
    State state = State::WAIT_START;
    uint8_t nibbles = 0; // Track number of nibbles processed

public:
    virtual ~RAW1090() {}
    void ProcessByte(uint8_t byte, TAG &tag)
    {
        if (byte == '\r' || byte == '\n')
        {
            state = State::WAIT_START;
            return;
        }
        switch (state)
        {
        case State::WAIT_START:
            if (byte == '*')
            {
                msg.clear();
                msg.Stamp();
                state = State::READ_MSG;
                nibbles = 0;
            }
            break;
        case State::READ_MSG:
            if (byte == ';')
            {
                if (msg.len > 0)
                {
                    msg.Decode();
                    //msg.Print();
                    Send(&msg, 1, tag);
                }
                state = State::WAIT_START;
                return;
            }

            if (msg.len >= MAX_MSG_LEN)
            {
                state = State::WAIT_START;
                return;
            }

            uint8_t val = (byte | 0x20) - '0';
            if (val > 9)
                val -= ('a' - '0' - 10);
            if (val > 15)
            {
                state = State::WAIT_START;
                return;
            }

            if (nibbles % 2 == 0)
                msg.msg[msg.len] = val << 4; // High nibble
            else
            {
                msg.msg[msg.len] |= val; // Low nibble
                msg.len++;
            }
            nibbles++;
            break;
        }
    }
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
};

class Beast : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;

    enum BeastConstants
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

    Plane::ADSB msg;
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
        msg.clear();
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
                msg.clear();
                msg.Stamp();
                msg.msgtype = byte;

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
                msg.timestamp = ParseTimestamp();
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
            msg.signalLevel = byte / 255.0f;
            msg.signalLevel = msg.signalLevel * msg.signalLevel; // Square for power
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
            msg.msg[bytes_read] = byte;
            bytes_read++;
            msg.len = bytes_read;

            int expected_len = GetExpectedLength();
            if (bytes_read == expected_len)
            {
                if (msg.msgtype == '2' || msg.msgtype == '3')
                {
                    msg.Decode();
                }
                else if (msg.msgtype != '1')
                {
                    std::cerr << "Unknown message type: " << msg.msgtype << std::endl;
                }
                //std::cout << "----- ADSB Message -----" << std::endl;
                //msg.Print();
                Send(&msg, 1, tag);

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
            return 0;

        uint64_t ts = 0;
        for (int i = 0; i < 6; i++)
        {
            ts = (ts << 8) | buffer[i];
        }
        return ts;
    }

    int GetExpectedLength() const
    {
        switch (msg.msgtype)
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
};