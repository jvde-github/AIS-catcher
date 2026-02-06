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

#include <string>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <type_traits>

#include "Common.h"

namespace JSON {

class JSONBuilder {
    std::string buf;
    size_t cursor;
    bool need_comma;

    void ensure(size_t len) {
        if (cursor + len >= buf.size()) {
            buf.resize(MAX(buf.size() * 2, cursor + len + 64));
        }
    }

    void writeRaw(const char* data, size_t len) {
        std::memcpy(&buf[cursor], data, len);
        cursor += len;
    }

    void comma() {
        if (need_comma) { ensure(1); buf[cursor++] = ','; }
        need_comma = true;
    }

    void escapeString(const char* s, size_t len) {
        ensure(len * 2 + 2);
        buf[cursor++] = '"';
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = s[i];
            const char* repl = nullptr;
            if (c == '"') repl = "\\\""; else if (c == '\\') repl = "\\\\";
            else if (c == '\b') repl = "\\b"; else if (c == '\f') repl = "\\f";
            else if (c == '\n') repl = "\\n"; else if (c == '\r') repl = "\\r";
            else if (c == '\t') repl = "\\t"; else if (c < 0x20) continue; 
            
            if (repl) { writeRaw(repl, 2); } 
            else { buf[cursor++] = c; }
        }
        buf[cursor++] = '"';
    }

    // Numeric Writers
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type writeVal(T v) {
        ensure(24);
        if (std::is_signed<T>::value && v < 0) {
            buf[cursor++] = '-';
            return writeVal(static_cast<typename std::make_unsigned<T>::type>(-v));
        }
        char tmp[24], *p = tmp + 23;
        if (v == 0) *--p = '0';
        while (v > 0) { *--p = '0' + (v % 10); v /= 10; }
        writeRaw(p, (tmp + 23) - p);
    }

    void writeVal(double v) { ensure(32); cursor += std::snprintf(&buf[cursor], 32, "%.6g", v); }
    void writeVal(bool v) { ensure(5); writeRaw(v ? "true" : "false", v ? 4 : 5); }
    void writeVal(const std::string& v) { escapeString(v.data(), v.size()); }
    void writeVal(const char* v) { v ? escapeString(v, std::strlen(v)) : writeRaw("null", 4); }

public:
    JSONBuilder() : cursor(0), need_comma(false) { buf.resize(1024); }

    std::string str() const { return std::string(buf.data(), cursor); }
    size_t size() const { return cursor; }
    void clear() { cursor = 0; need_comma = false; }
    
    JSONBuilder& nl() { ensure(1); buf[cursor++] = '\n'; return *this; }
    JSONBuilder& crlf() { ensure(2); buf[cursor++] = '\r'; buf[cursor++] = '\n'; return *this; }

    // Structure
    JSONBuilder& start() { comma(); ensure(1); buf[cursor++] = '{'; need_comma = false; return *this; }
    JSONBuilder& end() { ensure(1); buf[cursor++] = '}'; need_comma = true; return *this; }
    JSONBuilder& startArray() { comma(); ensure(1); buf[cursor++] = '['; need_comma = false; return *this; }
    JSONBuilder& endArray() { ensure(1); buf[cursor++] = ']'; need_comma = true; return *this; }
    JSONBuilder& startArr() { return startArray(); }
    JSONBuilder& endArr() { return endArray(); }

    // Key Management
    JSONBuilder& key(const char* k, size_t len) {
        ensure(len + 4);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"'; writeRaw(k, len); writeRaw("\":", 2);
        need_comma = false;
        return *this;
    }
    JSONBuilder& key(const char* k) { return key(k, std::strlen(k)); }
    JSONBuilder& key(const std::string& k) { return key(k.data(), k.size()); }

    // Unified Add (Literal Keys - compile-time length)
    template<size_t N, typename T>
    JSONBuilder& add(const char (&k)[N], T v) { key(k, N-1); writeVal(v); need_comma = true; return *this; }

    // Unified Add (Runtime key with strlen)
    template<typename T>
    JSONBuilder& add(const std::string& k, T v) { key(k); writeVal(v); need_comma = true; return *this; }

    // Specialized Adders
    template<size_t N>
    JSONBuilder& addRaw(const char (&k)[N], const std::string& v) { key(k, N-1); ensure(v.size()); writeRaw(v.data(), v.size()); need_comma = true; return *this; }

    JSONBuilder& addRaw(const std::string& k, const std::string& v) { key(k); ensure(v.size()); writeRaw(v.data(), v.size()); need_comma = true; return *this; }

    template<size_t N>
    JSONBuilder& addSafe(const char (&k)[N], const std::string& v) { 
        key(k, N-1); ensure(v.size() + 2); buf[cursor++] = '"'; writeRaw(v.data(), v.size()); buf[cursor++] = '"';
        need_comma = true; return *this; 
    }

    template<typename T>
    JSONBuilder& addIf(bool cond, const char* k, T v) { return cond ? add(k, v) : *this; }

    // Array Values
    template<typename T>
    JSONBuilder& value(T v) { comma(); writeVal(v); return *this; }
    
    JSONBuilder& valueRaw(const std::string& v) { comma(); ensure(v.size()); writeRaw(v.data(), v.size()); return *this; }
    
    JSONBuilder& valueNull() { comma(); ensure(4); writeRaw("null", 4); return *this; }
    
    template<typename T>
    JSONBuilder& valueOrNull(T v, T undef) { return (v == undef) ? valueNull() : value(v); }

    // Raw JSON fragment appending (no key, for manual control)
    JSONBuilder& write(const char* str) { size_t len = std::strlen(str); ensure(len); writeRaw(str, len); return *this; }
    JSONBuilder& write(const std::string& str) { ensure(str.size()); writeRaw(str.data(), str.size()); return *this; }

    // Nullable logic
    template<size_t N, typename T>
    JSONBuilder& addOrNull(const char (&k)[N], T v, T undef) { return (v == undef) ? add(k, nullptr) : add(k, v); }

    template<size_t N>
    JSONBuilder& addStringOrNull(const char (&k)[N], const std::string& v) { return v.empty() ? add(k, nullptr) : add(k, v); }
    
    template<size_t N>
    JSONBuilder& addNull(const char (&k)[N]) { key(k, N-1); ensure(4); writeRaw("null", 4); need_comma = true; return *this; }
    
    template<typename T>
    JSONBuilder& addIf(bool cond, const std::string& k, T v) { return cond ? add(k, v) : *this; }
};

} // namespace JSON
