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
#include "SWAR.h"
#include "Convert.h"

namespace
{
	struct SignalLevelTable
	{
		float v[256];
		SignalLevelTable()
		{
			v[0] = LEVEL_UNDEFINED;
			for (int i = 1; i < 256; i++)
				v[i] = 20.0f * std::log10((float)i / 255.0f);
		}
	};
	static const SignalLevelTable signal_db;
}

// RAW1090 — AVR text format: *<hex chars>;
// During ACCUMULATE, msg.len counts nibbles; halved at dispatch.
void RAW1090::Receive(const RAW *data, int len, TAG &tag)
{
    std::time(&rxtime_cache);

    for (int j = 0; j < len; j++)
    {
        const uint8_t *src = (const uint8_t *)data[j].data;
        const uint8_t *const end = src + data[j].size;

        while (src < end)
        {
            if (state == State::WAIT_START)
            {
                const uint8_t *p = (const uint8_t *)std::memchr(src, '*', end - src);
                if (!p) break;
                src = p + 1;
                msg.reset(rxtime_cache);
                msg.len = 0;
                state = State::ACCUMULATE;
            }

            while (src + 1 < end && (msg.len & 1) == 0)
            {
                int v1 = Util::Convert::hexValue(src[0]);
                int v2 = Util::Convert::hexValue(src[1]);
                if ((v1 | v2) < 0 || msg.len >= MAX_MSG_NIBBLES) break;
                msg.msg[msg.len >> 1] = (uint8_t)((v1 << 4) | v2);
                msg.len += 2;
                src += 2;
            }

            if (src >= end) continue;
            uint8_t b = *src++;
            switch (b)
            {
            case ';':
                msg.len >>= 1; // nibbles → bytes
                if (msg.len > 0)
                {
                    msg.Decode();
                    Send(&msg, 1, tag);
                }
                state = State::WAIT_START;
                continue;
            case '*':
                msg.reset(rxtime_cache);
                msg.len = 0;
                continue;
            case '\r':
            case '\n':
                state = State::WAIT_START;
                continue;
            default:
                break;
            }

            int v = Util::Convert::hexValue(b);
            if (v < 0 || msg.len >= MAX_MSG_NIBBLES)
            {
                state = State::WAIT_START;
                continue;
            }
            if ((msg.len & 1) == 0)
                msg.msg[msg.len >> 1] = (uint8_t)(v << 4);
            else
                msg.msg[msg.len >> 1] |= (uint8_t)v;
            msg.len++;
        }
    }
}

static inline int frame_size_for(uint8_t type)
{
    return 8 + ((type == '3') ? 14 : (type == '2') ? 7 : 2);
}

void Beast::process_frame(TAG &tag)
{
    msg.reset(rxtime_cache);
    msg.msgtype = frame_buf[0];
    msg.timestamp =
        ((uint64_t)frame_buf[1] << 40) |
        ((uint64_t)frame_buf[2] << 32) |
        ((uint64_t)frame_buf[3] << 24) |
        ((uint64_t)frame_buf[4] << 16) |
        ((uint64_t)frame_buf[5] <<  8) |
        ((uint64_t)frame_buf[6]);
    msg.signalLevel = signal_db.v[frame_buf[7]];
    // Fixed 14-byte memcpy: bytes past msg.len are invisible to Decode
    // (getUint bounds reads by msg.len * 8).
    std::memcpy(msg.msg, &frame_buf[8], 14);
    msg.len = frame_total - 8;

    if (msg.msgtype == '2' || msg.msgtype == '3')
        msg.Decode();
    Send(&msg, 1, tag);
}

void Beast::Receive(const RAW *data, int len, TAG &tag)
{
    std::time(&rxtime_cache);

    for (int j = 0; j < len; j++)
    {
        const uint8_t *src = (const uint8_t *)data[j].data;
        const uint8_t *const end = src + data[j].size;

        while (src < end)
        {
            if (state == State::WAIT_START)
            {
                const uint8_t *p = (const uint8_t *)std::memchr(src, BEAST_ESCAPE, end - src);
                if (!p) break;
                src = p + 1;
                state = State::WAIT_TYPE;
            }

            if (state == State::WAIT_TYPE)
            {
                if (src >= end) break;
                uint8_t type = *src++;
                if (type >= 0x31 && type <= 0x33)
                {
                    frame_buf[0] = type;
                    frame_pos = 1;
                    frame_total = frame_size_for(type);
                    state = State::ACCUMULATE;
                }
                else
                {
                    // 0x1A 0x1A → stay in WAIT_TYPE; anything else resyncs.
                    if (type != BEAST_ESCAPE) state = State::WAIT_START;
                    continue;
                }
            }

            if (state == State::ACCUMULATE)
            {
                int need = frame_total - frame_pos;
                int have = (int)(end - src);
                int chunk = need < have ? need : have;

                constexpr size_t mask = SWAR::mask((char)BEAST_ESCAPE);
                uint8_t *dst = &frame_buf[frame_pos];
                const uint8_t *const s_stop = src + chunk;

                while (src + sizeof(size_t) <= s_stop)
                {
                    size_t word;
                    std::memcpy(&word, src, sizeof(word));
                    std::memcpy(dst, src, sizeof(word));
                    size_t hit = SWAR::has_byte(word, mask);
                    if (hit)
                    {
                        size_t pos = SWAR::first_byte_pos(hit);
                        src += pos;
                        dst += pos;
                        break;
                    }
                    src += sizeof(word);
                    dst += sizeof(word);
                }
                while (src < s_stop && *src != BEAST_ESCAPE)
                    *dst++ = *src++;

                frame_pos = (int)(dst - &frame_buf[0]);

                if (frame_pos == frame_total)
                {
                    process_frame(tag);
                    state = State::WAIT_START;
                    continue;
                }
                if (src >= end) break;
                // Hit a 0x1A; consume it and fall through to ACCUMULATE_ESC.
                src++;
                state = State::ACCUMULATE_ESC;
            }

            if (src >= end) break;
            uint8_t b = *src++;
            if (b == BEAST_ESCAPE)
            {
                frame_buf[frame_pos++] = BEAST_ESCAPE;
                state = State::ACCUMULATE;
                if (frame_pos == frame_total)
                {
                    process_frame(tag);
                    state = State::WAIT_START;
                }
            }
            else if (b >= 0x31 && b <= 0x33)
            {
                // 0x1A <type>: a new frame starts here.
                frame_buf[0] = b;
                frame_pos = 1;
                frame_total = frame_size_for(b);
                state = State::ACCUMULATE;
            }
            else
            {
                state = State::WAIT_START;
            }
        }
    }
}