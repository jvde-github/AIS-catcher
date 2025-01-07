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

#include <iomanip>

#include "Message.h"
#include "Stream.h"

#include "Utilities.h"
#include "Keys.h"
#include "ADSB.h"


class Beast : public SimpleStreamInOut<RAW, Plane::ADSB> {
enum {
    BEAST_ESCAPE = 0x1A,
    BEAST_ESCAPE_START = 0x1B
};

    enum class State {
        WAIT_ESCAPE,
        WAIT_TYPE,
        READ_TIMESTAMP,
        READ_PAYLOAD,
        READ_ESCAPE
    };

    uint8_t type;
    uint64_t timestamp;
    std::vector<uint8_t> payload;

    State state = State::WAIT_ESCAPE;
    int bytes_read = 0;
    std::vector<uint8_t> buffer;

public:
    virtual ~Beast() {}

    void Receive(const RAW* data, int len, TAG& tag) {
        for (int j = 0; j < len; j++) {
            for (int i = 0; i < data[j].size; i++) {
                uint8_t c = ((uint8_t*)(data[j].data))[i];
                ProcessByte(c, tag);
            }
        }
    }

private:
       void ProcessByte(uint8_t byte, TAG& tag) {
        switch (state) {
            case State::WAIT_ESCAPE:
                if (byte == BEAST_ESCAPE) {
                    state = State::WAIT_TYPE;
                    buffer.clear();
                }
                break;

            case State::WAIT_TYPE:
                if (byte >= 0x31 && byte <= 0x33) {  // Valid message types: '1', '2', '3'
                    type = byte;
                    state = State::READ_TIMESTAMP;
                    bytes_read = 0;
                    buffer.clear();  // Clear buffer when starting new message
                } else {
                    state = State::WAIT_ESCAPE;
                    // Add error logging here if needed
                }
                break;

            case State::READ_TIMESTAMP:
                if (byte == BEAST_ESCAPE) {
                    state = State::READ_ESCAPE;
                    break;
                }
                
                buffer.push_back(byte);
                bytes_read++;
                
                if (bytes_read == 6) {  // 6-byte timestamp
                    timestamp = 0;
                    for (int i = 0; i < 6; i++) {
                        timestamp = (timestamp << 8) | buffer[i];
                    }
                    buffer.clear();
                    state = State::READ_PAYLOAD;
                    bytes_read = 0;
                }
                break;

            case State::READ_PAYLOAD: {
                if (byte == BEAST_ESCAPE) {
                    state = State::READ_ESCAPE;
                    break;
                }
                
                buffer.push_back(byte);
                bytes_read++;
                
                int expected_len = 0;
                switch (type) {
                    case '1': expected_len = 7; break;  // Mode AC
                    case '2': expected_len = 14; break; // Mode S short
                    case '3': expected_len = 21; break; // Mode S long
                    default:
                        state = State::WAIT_ESCAPE;  // Reset on invalid type
                        return;
                }
                
                if (bytes_read == expected_len) {
                    switch (type) {
                        case '1': ProcessModeAC(); break;  // Mode AC
                        case '2': ProcessModeSshort(); break; // Mode S short
                        case '3': ProcessModeSlong(); break; // Mode S long

                    }
                    state = State::WAIT_ESCAPE;
                    buffer.clear();
                } else if (bytes_read > expected_len) {  
                    state = State::WAIT_ESCAPE;
                    buffer.clear();
                }
            }
            break;

            case State::READ_ESCAPE:
                if (byte == BEAST_ESCAPE) {
                    buffer.push_back(BEAST_ESCAPE);
                    bytes_read++;
                    state = State::READ_PAYLOAD;
                } else if (byte == BEAST_ESCAPE_START) {
                    state = State::WAIT_TYPE;
                    buffer.clear();
                    bytes_read = 0;
                } else {
                    state = State::WAIT_ESCAPE;  
                    buffer.clear();
                }
                break;
        }
    }

  void ProcessModeAC() {
        // Mode A/C messages are 7 bytes
        if (buffer.size() != 7) return;

        // First byte is signal level
        uint8_t signalLevel = buffer[0];

        // Mode A/C data is in the next 2 bytes
        uint16_t modeAC = (buffer[1] << 8) | buffer[2];
        
        std::cout << "\nMode A/C Message:" << std::endl;
        std::cout << "Timestamp: " << formatTimestamp(timestamp) << std::endl;
        std::cout << "Signal Level: " << (int)signalLevel << " dB" << std::endl;
        std::cout << "Mode A/C Code: " << std::hex << modeAC << std::dec << std::endl;
    }

    void ProcessModeSshort() {
        // Mode S short messages are 14 bytes
        if (buffer.size() != 14) return;

        // First byte is signal level
        uint8_t signalLevel = buffer[0];

        // ICAO address is in bytes 1-3
        uint32_t icao = (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];

        // Payload starts at byte 4
        std::cout << "\nMode S Short Message:" << std::endl;
        std::cout << "Timestamp: " << formatTimestamp(timestamp) << std::endl;
        std::cout << "Signal Level: " << (int)signalLevel << " dB" << std::endl;
        std::cout << "ICAO: " << formatICAO(icao) << std::endl;
        
        // Downlink Format (DF) is in the first 5 bits
        uint8_t df = getBits(buffer, 32, 5);
        std::cout << "Downlink Format: " << (int)df << std::endl;

        // Print raw payload in hex
        std::cout << "Payload: ";
        for (size_t i = 4; i < buffer.size(); i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) 
                     << (int)buffer[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    void ProcessModeSlong() {
        // Mode S long messages are 21 bytes
        if (buffer.size() != 21) return;

        // First byte is signal level
        uint8_t signalLevel = buffer[0];

        // ICAO address is in bytes 1-3
        uint32_t icao = (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];

        std::cout << "\nMode S Long Message:" << std::endl;
        std::cout << "Timestamp: " << formatTimestamp(timestamp) << std::endl;
        std::cout << "Signal Level: " << (int)signalLevel << " dB" << std::endl;
        std::cout << "ICAO: " << formatICAO(icao) << std::endl;

        // Downlink Format (DF) is in the first 5 bits
        uint8_t df = getBits(buffer, 32, 5);
        std::cout << "Downlink Format: " << (int)df << std::endl;

        // If DF17 (ADS-B), decode more fields
        if (df == 17) {
            uint8_t capability = getBits(buffer, 37, 3);
            uint8_t typeCode = getBits(buffer, 40, 5);
            
            std::cout << "ADS-B Message:" << std::endl;
            std::cout << "Type Code: " << (int)typeCode << std::endl;
            
            // Decode position for airborne position messages
            if (typeCode >= 9 && typeCode <= 18) {
                uint8_t altitude_code = getBits(buffer, 45, 12);
                std::cout << "Altitude Code: " << (int)altitude_code << std::endl;
            }
            // Decode identification messages
            else if (typeCode >= 1 && typeCode <= 4) {
                char callsign[9] = {0};
                for (int i = 0; i < 8; i++) {
                    uint8_t c = getBits(buffer, 45 + i*6, 6);
                    callsign[i] = (c > 0 && c <= 26) ? ('A' + c - 1) : 
                                 (c >= 48 && c <= 57) ? (c - 48 + '0') : ' ';
                }
                std::cout << "Callsign: " << callsign << std::endl;
            }
        }

        // Print raw payload in hex
        std::cout << "Payload: ";
        for (size_t i = 4; i < buffer.size(); i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) 
                     << (int)buffer[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
      uint32_t getBits(const std::vector<uint8_t>& data, int start, int len) {
        uint32_t result = 0;
        for (int i = 0; i < len; i++) {
            int byteIndex = (start + i) / 8;
            int bitIndex = 7 - ((start + i) % 8);
            if (data[byteIndex] & (1 << bitIndex)) {
                result |= (1 << (len - 1 - i));
            }
        }
        return result;
    }

    std::string formatTimestamp(uint64_t ts) {
        time_t seconds = ts / 1000000;
        uint32_t microseconds = ts % 1000000;
        struct tm* tm = gmtime(&seconds);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
        std::stringstream ss;
        ss << buffer << "." << std::setfill('0') << std::setw(6) << microseconds;
        return ss.str();
    }

    // Helper to format ICAO address
    std::string formatICAO(uint32_t icao) {
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(6) << icao;
        return ss.str();
    }
};