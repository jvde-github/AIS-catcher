#include "ADSB.h"
#include "Stream.h"

class PlaneDB : public StreamIn<Plane::ADSB>
{
    std::mutex mtx;

private:
    int first = -1;
    int last = -1;
    int count = 0;
    std::vector<Plane::ADSB> items;
    int N = 512;

public:
    PlaneDB()
    {
        items.resize(N);

        first = N - 1;
        last = 0;
        count = 0;

        // set up linked list
        for (int i = 0; i < N; i++)
        {
            items[i].next = i - 1;
            items[i].prev = i + 1;
        }
        items[N - 1].prev = -1;
    }

    void moveToFront(int ptr)
    {
        if (ptr == first)
            return;

        // remove ptr out of the linked list
        if (items[ptr].next != -1)
            items[items[ptr].next].prev = items[ptr].prev;
        else
            last = items[ptr].prev;

        items[items[ptr].prev].next = items[ptr].next;

        // new ship is first in list
        items[ptr].next = first;
        items[ptr].prev = -1;

        items[first].prev = ptr;
        first = ptr;
    }

    int create()
    {
        int ptr = last;
        count = MIN(count + 1, N);
        items[ptr].clear();

        return ptr;
    }

    int find(int hexid) const
    {
        int ptr = first, cnt = count;
        while (ptr != -1 && --cnt >= 0)
        {
            if (items[ptr].hexident == hexid)
                return ptr;
            ptr = items[ptr].next;
        }
        return -1;
    }

    void Receive(const Plane::ADSB *msg, int len, TAG &tag)
    {
        std::lock_guard<std::mutex> lock(mtx);

        // Skip invalid messages
        if (msg->hexident == HEXIDENT_UNDEFINED)
            return;

        // Find or create plane entry
        int ptr = find(msg->hexident);

        if (ptr == -1)
        {
            // if ICAO is implied from CRC, ignore the message if not known
            if (msg->hexident_status == HEXINDENT_IMPLIED_FROM_CRC)
                return;

            ptr = create();
        }

        // Move to front and update data
        moveToFront(ptr);
        Plane::ADSB &plane = items[ptr];

        // Update timestamp and core identifiers
        plane.rxtime = msg->rxtime;
        plane.hexident = msg->hexident;
        plane.nMessages++;

        // Update position if valid
        if (msg->lat != LAT_UNDEFINED && msg->lon != LON_UNDEFINED)
        {
            plane.lat = msg->lat;
            plane.lon = msg->lon;
            plane.latlon_timestamp = msg->rxtime;
        }

        if (msg->even.lat != CPR_POSITION_UNDEFINED && msg->even.lon != CPR_POSITION_UNDEFINED)
        {
            plane.even.lat = msg->even.lat;
            plane.even.lon = msg->even.lon;
            plane.even.timestamp = msg->even.timestamp;

            plane.decodeCPR();
        }

        if (msg->odd.lat != CPR_POSITION_UNDEFINED && msg->odd.lon != CPR_POSITION_UNDEFINED)
        {
            plane.odd.lat = msg->odd.lat;
            plane.odd.lon = msg->odd.lon;
            plane.odd.timestamp = msg->odd.timestamp;

            plane.decodeCPR();
        }

        // Update altitude
        if (msg->altitude != ALTITUDE_UNDEFINED)
        {
            plane.altitude = msg->altitude;
        }

        // Update movement data
        if (msg->speed != SPEED_UNDEFINED)
        {
            plane.speed = msg->speed;
        }
        if (msg->heading != HEADING_UNDEFINED)
        {
            plane.heading = msg->heading;
        }
        if (msg->vertrate != VERT_RATE_UNDEFINED)
        {
            plane.vertrate = msg->vertrate;
        }

        // Update identification
        if (msg->squawk != SQUAWK_UNDEFINED)
        {
            plane.squawk = msg->squawk;
        }

        if (msg->callsign[0] != '\0')
        {
            std::memcpy(plane.callsign, msg->callsign, sizeof(plane.callsign));
        }

        if (msg->airborne != AIRBORNE_UNDEFINED)
        {
            plane.airborne = msg->airborne;
        }
    }

    std::string getCompactArray(bool include_inactive = false)
    {
        std::lock_guard<std::mutex> lock(mtx);

        const std::string null_str = "null";
        const std::string comma = ",";
        std::string content = "{\"count\":" + std::to_string(count) + ",\"values\":[";

        std::time_t now = std::time(nullptr);
        int ptr = first;
        std::string delim = "";

        while (ptr != -1)
        {
            const Plane::ADSB &plane = items[ptr];

            if (plane.hexident != HEXIDENT_UNDEFINED)
            {
                long int time_since_update = now - plane.getRxTimeUnix();

                // Skip inactive planes unless requested
                if (!include_inactive && time_since_update > 60)
                {
                    break;
                }

                content += delim + "[" +
                           std::to_string(plane.hexident) + comma +
                           (plane.lat != LAT_UNDEFINED ? std::to_string(plane.lat) : null_str) + comma +
                           (plane.lon != LON_UNDEFINED ? std::to_string(plane.lon) : null_str) + comma +
                           (plane.altitude != ALTITUDE_UNDEFINED ? std::to_string(plane.altitude) : null_str) + comma +
                           (plane.speed != SPEED_UNDEFINED ? std::to_string(plane.speed) : null_str) + comma +
                           (plane.heading != HEADING_UNDEFINED ? std::to_string(plane.heading) : null_str) + comma +
                           (plane.vertrate != VERT_RATE_UNDEFINED ? std::to_string(plane.vertrate) : null_str) + comma +
                           (plane.squawk != SQUAWK_UNDEFINED ? std::to_string(plane.squawk) : null_str) + comma +
                           std::string("\"") + plane.callsign + "\"" + comma +
                           std::to_string(plane.airborne) + comma + std::to_string(plane.nMessages) + comma + std::to_string(time_since_update) + "]";

                delim = comma;
            }
            ptr = items[ptr].next;
        }

        content += "],\"error\":false}\n\n";
        return content;
    }

    int getFirst() const { return first; }
    int getLast() const { return last; }
    int getCount() const { return count; }
};