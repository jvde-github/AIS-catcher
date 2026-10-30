
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

#include <cstdint>
#include <mutex>

#include "JSON/Writer.h"

namespace IO
{
    // A count kept on an output's thread and read on a server's. One lock for all of them, used for
    // moments: a 64-bit atomic needs libatomic on armv6.
    class OutputCount
    {
        uint64_t value = 0;
        static std::mutex &lock()
        {
            static std::mutex m;
            return m;
        }

    public:
        OutputCount &operator=(uint64_t n)
        {
            std::lock_guard<std::mutex> l(lock());
            value = n;
            return *this;
        }
        OutputCount &operator+=(uint64_t n)
        {
            std::lock_guard<std::mutex> l(lock());
            value += n;
            return *this;
        }
        void operator++(int) { *this += 1; }
        uint64_t get() const
        {
            std::lock_guard<std::mutex> l(lock());
            return value;
        }
    };

    struct OutputStats
    {
        OutputCount bytes_out;
        OutputCount bytes_in;
        OutputCount connect_ok;
        OutputCount connect_fail;
        OutputCount reconnects;
        OutputCount connected;
        OutputCount dropped;

        void writeJSON(JSON::Writer &w) const
        {
            w.beginObject()
                .kv("bytes_out", bytes_out.get())
                .kv("bytes_in", bytes_in.get())
                .kv("connect_ok", connect_ok.get())
                .kv("connect_fail", connect_fail.get())
                .kv("reconnects", reconnects.get())
                .kv("connected", connected.get())
                .kv("dropped", dropped.get())
                .endObject();
        }
    };
}