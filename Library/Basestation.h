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
        if (fields[0] == "MSG")
        {
            msg.setType(Plane::MessageType::MSG);

            if (!fields[1].empty())
            {
                int trans = std::stoi(fields[1]);
                msg.setTransmission(static_cast<Plane::TransmissionType>(trans));
            }
        }
        else if (fields[0] == "SEL")
            msg.setType(Plane::MessageType::SEL);
        else if (fields[0] == "ID")
            msg.setType(Plane::MessageType::ID);
        else if (fields[0] == "AIR")
            msg.setType(Plane::MessageType::AIR);
        else if (fields[0] == "STA")
            msg.setType(Plane::MessageType::STA);
        else if (fields[0] == "CLK")
            msg.setType(Plane::MessageType::CLK);

        // HexIdent (Field 4)
        if (!fields[4].empty())
        {
            msg.setHexIdent(std::stoul(fields[4], nullptr, 16));
        }

        // Timestamps (Fields 7-10)
        if (!fields[6].empty() && !fields[7].empty())
        {
            std::string datetime = fields[6] + " " + fields[7];
            msg.setRxTimeUnix(Util::Parse::DateTime(datetime));
        }

        // Callsign (Field 10)
        if (fields.size() > 10 && !fields[10].empty())
        {
            msg.setCallsign(fields[10]);
        }

        // Altitude (Field 11)
        if (fields.size() > 11 && !fields[11].empty())
        {
            msg.setAltitude(std::stoi(fields[11]));
        }

        // Groundspeed (Field 12)
        if (fields.size() > 12 && !fields[12].empty())
        {
            msg.setGroundSpeed(std::stof(fields[12]));
        }

        // Track (Field 13)
        if (fields.size() > 13 && !fields[13].empty())
        {
            msg.setTrack(std::stof(fields[13]));
        }

        // Position (Fields 14,15)
        if (fields.size() > 15 && !fields[14].empty() && !fields[15].empty())
        {
            msg.setPosition(std::stof(fields[14]), std::stof(fields[15]));
        }

        // Vertical Rate (Field 16)
        if (fields.size() > 16 && !fields[16].empty())
        {
            msg.setVertRate(std::stoi(fields[16]));
        }

        // Squawk (Field 17)
        if (fields.size() > 17 && !fields[17].empty())
        {
            msg.setSquawk(std::stoi(fields[17]));
        }

        // Alert, Emergency, SPI (Fields 18-20)
        if (fields.size() > 18 && !fields[18].empty())
        {
            msg.setAlert((Plane::BoolType)(fields[18] == "-1"));
        }
        if (fields.size() > 19 && !fields[19].empty())
        {
            msg.setEmergency((Plane::BoolType)(fields[19] == "-1"));
        }
        if (fields.size() > 20 && !fields[20].empty())
        {
            msg.setSPI((Plane::BoolType)(fields[20] == "-1"));
        }

        // Ground (Field 21)
        if (fields.size() > 21 && !fields[21].empty())
        {
            msg.setOnGround((Plane::BoolType)(fields[21] == "-1"));
        }

        msg.Print();
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
                        processLine();
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
