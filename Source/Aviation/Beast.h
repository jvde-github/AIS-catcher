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

#include <ctime>
#include <cstdint>

#include "Common.h"
#include "Stream.h"
#include "ADSB.h"

class RAW1090 : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    static constexpr int MAX_MSG_NIBBLES = Plane::ADSB::MAX_BYTES * 2;

    enum class State { WAIT_START, ACCUMULATE };

    Plane::ADSB msg;
    State state = State::WAIT_START;
    std::time_t rxtime_cache = 0;
    // During ACCUMULATE, msg.len holds the nibble count (halved on dispatch).
    // msg.len & 1 == 0 → next char is the high nibble of a new byte.

public:
    virtual ~RAW1090() {}

    void Receive(const RAW *data, int len, TAG &tag);
};

class Beast : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    enum BeastConstants { BEAST_ESCAPE = 0x1A };

    enum class State { WAIT_START, WAIT_TYPE, ACCUMULATE, ACCUMULATE_ESC };

    Plane::ADSB msg;
    // frame_buf layout: [0] type, [1..6] BE timestamp, [7] sig, [8..21] payload.
    // Sized 24 so process_frame's fixed 14-byte payload memcpy is in-bounds.
    uint8_t frame_buf[24];
    int frame_pos = 0;
    int frame_total = 0;
    State state = State::WAIT_START;
    std::time_t rxtime_cache = 0;

public:
    virtual ~Beast() {}

    void Receive(const RAW *data, int len, TAG &tag);

private:
    void process_frame(TAG &tag);
};