
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

#include "JSON/Writer.h"

namespace IO
{
    struct OutputStats
    {
        uint64_t bytes_out = 0;
        uint64_t bytes_in = 0;
        uint32_t connect_ok = 0;
        uint32_t connect_fail = 0;
        uint32_t reconnects = 0;
        uint32_t connected = 0;
        uint64_t dropped = 0;

        void writeJSON(JSON::Writer &w) const
        {
            w.beginObject()
                .kv("bytes_out", bytes_out)
                .kv("bytes_in", bytes_in)
                .kv("connect_ok", connect_ok)
                .kv("connect_fail", connect_fail)
                .kv("reconnects", reconnects)
                .kv("connected", connected)
                .kv("dropped", dropped)
                .endObject();
        }
    };
}