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

#include "JSONBuilder.h"

namespace JSON
{

    void JSONBuilder::ensure(size_t len)
    {
        if (cursor + len >= buf.size())
        {
            buf.resize(MAX(buf.size() * 2, cursor + len + 64));
        }
    }

    void JSONBuilder::writeRaw(const char *data, size_t len)
    {
        std::memcpy(&buf[cursor], data, len);
        cursor += len;
    }

    void JSONBuilder::comma()
    {
        if (need_comma)
        {
            ensure(1);
            buf[cursor++] = ',';
        }
        need_comma = true;
    }

    void JSONBuilder::escapeString(const char *s, size_t len)
    {
        // Reserve enough space for worst case (all chars need escaping)
        // But cap the pre-allocation to avoid excessive memory for long strings
        size_t max_growth = len < 512 ? len * 2 + 2 : len + 1024;
        ensure(max_growth);

        buf[cursor++] = '"';
        const char *end = s + len;

        for (const char *p = s; p < end; ++p)
        {
            unsigned char c = *p;

            // Fast path for common printable ASCII
            if (c >= 0x20 && c != '"' && c != '\\')
            {
                buf[cursor++] = c;
                continue;
            }

            // Handle special cases
            const char *repl = nullptr;

            if (c == '"')
                repl = "\\\"";
            else if (c == '\\')
                repl = "\\\\";
            else if (c == '\b')
                repl = "\\b";
            else if (c == '\f')
                repl = "\\f";
            else if (c == '\n')
                repl = "\\n";
            else if (c == '\r')
                repl = "\\r";
            else if (c == '\t')
                repl = "\\t";
            else if (c < 0x20)
                continue; // Skip control chars

            if (repl)
            {
                // May need more space if we're near the end
                if (cursor + 2 >= buf.size())
                    ensure(64);
                buf[cursor++] = repl[0];
                buf[cursor++] = repl[1];
            }
        }
        buf[cursor++] = '"';
    }

    void JSONBuilder::writeValDouble(double v)
    {
        ensure(32);
        cursor += std::snprintf(&buf[cursor], 32, "%.8g", v);
    }

    void JSONBuilder::writeValBool(bool v)
    {
        ensure(5);
        writeRaw(v ? "true" : "false", v ? 4 : 5);
    }

    void JSONBuilder::writeValString(const std::string &v)
    {
        escapeString(v.data(), v.size());
    }

    void JSONBuilder::writeValCString(const char *v)
    {
        v ? escapeString(v, std::strlen(v)) : writeRaw("null", 4);
    }

    JSONBuilder &JSONBuilder::start()
    {
        comma();
        ensure(1);
        buf[cursor++] = '{';
        need_comma = false;
        return *this;
    }

    JSONBuilder &JSONBuilder::end()
    {
        ensure(1);
        buf[cursor++] = '}';
        need_comma = true;
        return *this;
    }

    JSONBuilder &JSONBuilder::startArray()
    {
        comma();
        ensure(1);
        buf[cursor++] = '[';
        need_comma = false;
        return *this;
    }

    JSONBuilder &JSONBuilder::endArray()
    {
        ensure(1);
        buf[cursor++] = ']';
        need_comma = true;
        return *this;
    }

    JSONBuilder &JSONBuilder::key(const char *k, size_t len)
    {
        ensure(len + 4);
        if (need_comma)
            buf[cursor++] = ',';
        buf[cursor++] = '"';
        writeRaw(k, len);
        writeRaw("\":", 2);
        need_comma = false;
        return *this;
    }

    JSONBuilder &JSONBuilder::key(const char *k)
    {
        return key(k, std::strlen(k));
    }

    JSONBuilder &JSONBuilder::key(const std::string &k)
    {
        return key(k.data(), k.size());
    }

    JSONBuilder &JSONBuilder::addRaw(const std::string &k, const std::string &v)
    {
        key(k);
        ensure(v.size());
        writeRaw(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    JSONBuilder &JSONBuilder::valueRaw(const std::string &v)
    {
        comma();
        ensure(v.size());
        writeRaw(v.data(), v.size());
        return *this;
    }

    JSONBuilder &JSONBuilder::valueNull()
    {
        comma();
        ensure(4);
        writeRaw("null", 4);
        return *this;
    }

} // namespace JSON
