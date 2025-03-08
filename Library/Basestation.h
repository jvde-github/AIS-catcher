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

class Basestation : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    std::string line;

    void processLine()
    {
        Plane::ADSB msg;
        msg.clear();
        TAG tag;
    
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ','))
        {
            if (field[0] == '"')
                field = field.substr(1, field.length() - 2);
            fields.push_back(field);
        }
    
        if (fields.size() < 10)
            return;
    
        // Message type
        /*
        if (fields[0] == "MSG")
        {
            msg.msgtype = MSG_TYPE_MSG; // Assuming this constant exists
            
            if (!fields[1].empty())
            {
                // Store transmission type if needed
                int trans = std::stoi(fields[1]);
                // You would need to store this in an appropriate field
            }
        }
        else if (fields[0] == "SEL")
            msg.msgtype = MSG_TYPE_SEL; // Assuming this constant exists
        else if (fields[0] == "ID")
            msg.msgtype = MSG_TYPE_ID; // Assuming this constant exists
        else if (fields[0] == "AIR")
            msg.msgtype = MSG_TYPE_AIR; // Assuming this constant exists
        else if (fields[0] == "STA")
            msg.msgtype = MSG_TYPE_STA; // Assuming this constant exists
        else if (fields[0] == "CLK")
            msg.msgtype = MSG_TYPE_CLK; // Assuming this constant exists
        */

        // HexIdent (Field 4)
        if (!fields[4].empty())
        {
            if(fields[4][0] == '~')
                msg.hexident = std::stoul(fields[4].substr(1), nullptr, 16);
            else
                msg.hexident = std::stoul(fields[4], nullptr, 16);
        }
    
        // Timestamps (Fields 7-10)
        if (!fields[6].empty() && !fields[7].empty())
        {
            std::string datetime = fields[6] + " " + fields[7];
            msg.rxtime = Util::Parse::DateTime(datetime);
        }
    
        // Callsign (Field 10)
        if (fields.size() > 10 && !fields[10].empty())
        {
            std::strncpy(msg.callsign, fields[10].c_str(), 8);
            msg.callsign[8] = '\0'; // Ensure null termination
        }
    
        // Altitude (Field 11)
        if (fields.size() > 11 && !fields[11].empty())
        {
            msg.altitude = std::stoi(fields[11]);
        }
    
        // Groundspeed (Field 12)
        if (fields.size() > 12 && !fields[12].empty())
        {
            msg.speed = std::stof(fields[12]);
        }
    
        // Track (Field 13)
        if (fields.size() > 13 && !fields[13].empty())
        {
            msg.heading = std::stof(fields[13]);
        }
    
        // Position (Fields 14,15)
        if (fields.size() > 15 && !fields[14].empty() && !fields[15].empty())
        {
            msg.lat = std::stof(fields[14]);
            msg.lon = std::stof(fields[15]);
            msg.position_status = Plane::ValueStatus::VALID;
            std::time(&msg.position_timestamp);
        }
    
        // Vertical Rate (Field 16)
        if (fields.size() > 16 && !fields[16].empty())
        {
            msg.vertrate = std::stoi(fields[16]);
        }
    
        // Squawk (Field 17)
        if (fields.size() > 17 && !fields[17].empty())
        {
            msg.squawk = std::stoi(fields[17]);
        }
    
        // Alert, Emergency, SPI, Ground (Fields 18-21)
        // These would need appropriate fields to store values, which aren't clear from the struct
        
        // Set country code based on hexident
        msg.setCountryCode();
        
        // Decode the message according to ADSB protocol
        // Send message to next processing stage
        Send(&msg, 1, tag);
    }

public:
    virtual ~Basestation() {}
    void Receive(const RAW *data, int len, TAG &tag)
    {
        for (int j = 0; j < len; j++)
        {
            for (int i = 0; i < data[j].size; i++)
            {
                char c = ((char *)(data[j].data))[i];
                if (c == '\n')
                {
                    if (line.length() > 0)
                    {
                        try {
                        processLine();
                        } catch (const std::exception &e) {
                            std::cerr << "Error processing line: " << line << " " << e.what() << std::endl;
                        
                        }
                        line.clear();
                    }
                }
                else
                {
                    if (c != '\r')
                        line = line + c;
                }
            }
        }
    }
};
