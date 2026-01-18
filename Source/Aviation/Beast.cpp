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

#include <cmath>

#include "Beast.h"
#include "Logger.h"

// RAW1090 implementation
void RAW1090::ProcessByte(uint8_t byte, TAG &tag)
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

void RAW1090::Receive(const RAW *data, int len, TAG &tag)
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

// Beast implementation
void Beast::Receive(const RAW *data, int len, TAG &tag)
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

void Beast::Clear()
{
    state = State::WAIT_ESCAPE;
    buffer.clear();
    msg.clear();
    bytes_read = 0;
    inEscape = false;
}

void Beast::ProcessByte(uint8_t byte, TAG &tag)
{
    if (buffer.size() >= MAX_MESSAGE_SIZE)
    {
        Clear();
        return;
    }

    if (byte == BEAST_ESCAPE)
    {
        if (state == State::WAIT_ESCAPE)
        {
            state = State::WAIT_TYPE;
            buffer.clear();
            return;
        }
        else if (inEscape)
        {
            inEscape = false;
        }
        else
        {
            inEscape = true;
            return;
        }
    }

    switch (state)
    {
    case State::WAIT_ESCAPE:
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
        if (byte == 0)
        {
            msg.signalLevel = LEVEL_UNDEFINED;
        }
        else
        {
            msg.signalLevel = byte / 255.0f;
            msg.signalLevel = 20.0f * std::log10(msg.signalLevel);
        }
        state = State::READ_PAYLOAD;
        bytes_read = 0;
        break;

    case State::READ_PAYLOAD:
    {
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
                Error() << "Unknown message type: " << msg.msgtype;
            }
            Send(&msg, 1, tag);

            state = State::WAIT_ESCAPE;
            buffer.clear();
        }
    }
    break;
    }
}

uint64_t Beast::ParseTimestamp() const
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

int Beast::GetExpectedLength() const
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