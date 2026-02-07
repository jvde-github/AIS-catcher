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

#include "SSEStreamer.h"
#include "JSON/JSONBuilder.h"

void SSEStreamer::obfuscateNMEA(std::string &nmea)
{
    int end = nmea.rfind(',');
    if (end == std::string::npos)
        return;

    int start = nmea.rfind(',', end - 1);
    if (start == std::string::npos)
        return;

    int len = end - start - 1;
    if (len == 0)
        return;

    for (int i = 0; i < 3; i++)
    {
        index = (index + 1) % len;
        nmea[MIN(start + 1 + index, nmea.length() - 1)] = '*';
    }
}

void SSEStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
{
    if (server)
    {
        AIS::Message *m = (AIS::Message *)data[0].binary;
        std::time_t now = std::time(nullptr);
        JSON::JSONBuilder json;

        if (!m->NMEA.empty())
        {
            json.start()
                .add("mmsi", m->mmsi())
                .add("timestamp", (long)now)
                .add("channel", std::string(1, m->getChannel()))
                .add("type", m->type())
                .add("shipname", tag.shipname)
                .key("nmea")
                .startArray();

            for (const auto &s : m->NMEA)
            {
                std::string nmea = s;

                if (obfuscate)
                    obfuscateNMEA(nmea);
                json.value(nmea);
            }

            json.endArray()
                .end();

            server->sendSSE(1, "nmea", json.str());
        }
        if (tag.lat != 0 && tag.lon != 0)
        {
            json.clear();
            json.start()
                .add("mmsi", m->mmsi())
                .add("channel", std::string(1, m->getChannel()))
                .add("lat", tag.lat)
                .add("lon", tag.lon)
                .end();

            server->sendSSE(2, "nmea", json.str());
        }
    }
}
