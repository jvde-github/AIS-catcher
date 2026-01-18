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

#pragma once

#include <vector>
#include <ctime>

#include "Common.h"
#include "Stream.h"
#include "ADSB.h"

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
    uint8_t nibbles = 0;

public:
    virtual ~RAW1090() {}

    void ProcessByte(uint8_t byte, TAG &tag);
    void Receive(const RAW *data, int len, TAG &tag);
};

class Beast : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;

    enum BeastConstants
    {
        BEAST_ESCAPE = 0x1A
    };

    enum class State
    {
        WAIT_ESCAPE,
        WAIT_TYPE,
        READ_TIMESTAMP,
        READ_SIGNAL,
        READ_PAYLOAD,
    };

    Plane::ADSB msg;
    std::vector<uint8_t> buffer;
    State state = State::WAIT_ESCAPE;
    int bytes_read = 0;

public:
    virtual ~Beast() {}

    void Receive(const RAW *data, int len, TAG &tag);

private:
    bool inEscape = false;

    void Clear();
    void ProcessByte(uint8_t byte, TAG &tag);
    uint64_t ParseTimestamp() const;
    int GetExpectedLength() const;
};