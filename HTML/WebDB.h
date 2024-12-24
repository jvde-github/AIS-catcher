#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace WebDB {
    struct FileData {
        const unsigned char* data;
        const size_t size;
        const char* mime_type;
        
        FileData(const unsigned char* d, const size_t s, const char* m) 
            : data(d), size(s), mime_type(m) {}
    };

    extern std::unordered_map<std::string, FileData> files;

}