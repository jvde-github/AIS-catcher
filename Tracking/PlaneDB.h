#include <array>

#include "ADSB.h"
#include "Stream.h"

class PlaneDB : public StreamIn<Plane::ADSB>
{
private:
    std::mutex mtx;

    struct LL
    {
        int prev, next;
    };
    const int END = -1;
    const int FREE = -2;

    const static int CPR_CACHE_SIZE = 3;

    Plane::CPR CPR_cache_even[CPR_CACHE_SIZE];
    Plane::CPR CPR_cache_odd[CPR_CACHE_SIZE];

    int CPR_cache_even_idx = 0;
    int CPR_cache_odd_idx = 0;

    static float deg2rad(float deg) { return deg * PI / 180.0f; }
    static int rad2deg(float rad) { return (int)(360 + rad * 180 / PI) % 360; }

    // https://www.movable-type.co.uk/scripts/latlong.html
    static void getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float &distance, int &bearing)
    {
        const float EarthRadius = 6371.0f;          // Earth radius in kilometers
        const float NauticalMilePerKm = 0.5399568f; // Conversion factor

        // Convert the latitudes and longitudes from degrees to radians
        lat1 = deg2rad(lat1);
        lon1 = deg2rad(lon1);
        lat2 = deg2rad(lat2);
        lon2 = deg2rad(lon2);

        // Compute the distance using the haversine formula
        float dlat = lat2 - lat1, dlon = lon2 - lon1;
        float a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
        distance = 2 * EarthRadius * NauticalMilePerKm * asin(sqrt(a));

        float y = sin(dlon) * cos(lat2);
        float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
        bearing = rad2deg(atan2(y, x));
    }

    int first = -1;
    int last = -1;
    int count = 0;
    std::vector<Plane::ADSB> items;
    const int N = 512;
    std::array<LL, 512> hash_ll;

    FLOAT32 station_lat = LAT_UNDEFINED, station_lon = LON_UNDEFINED;

    // for debugging
    void checkDepth()
    {

        int m = 0, gc = 0;
        for (int i = 0; i < N; i++)
        {
            int c = 0;
            int ptr = hash_ll[i].next;
            while (ptr != END)
            {
                c++;
                ptr = items[ptr].hash_ll.next;
            }
            if (c > m)
                m = c;

            gc += c;
        }
        std::cerr << "Max: " << m << " out of total " << gc << std::endl;
    }

    // FNV-1 hash
    int hash(uint32_t hexident) const
    {
        const uint32_t PRIME = 16777619;
        uint32_t hash = 2166136261;
        hash = (hash ^ hexident) * PRIME;
        return hash % N;
    }

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
            items[i].time_ll.next = i - 1;
            items[i].time_ll.prev = i + 1;

            items[i].hash_ll.prev = FREE;
            items[i].hash_ll.next = FREE;
        }
        items[N - 1].time_ll.prev = -1;

        for (int i = 0; i < N; i++)
        {
            hash_ll[i].prev = END;
            hash_ll[i].next = END;
        }

        for (int i = 0; i < CPR_CACHE_SIZE; i++)
        {
            CPR_cache_even[i].clear();
            CPR_cache_odd[i].clear();
        }
    }

    void calcReferencePosition(TAG &tag, int ptr, FLOAT32 &lat, FLOAT32 &lon)
    {
        lat = station_lat;
        lon = station_lon;

        if (tag.station_lat != LAT_UNDEFINED && tag.station_lon != LON_UNDEFINED)
        {
            lat = tag.station_lat;
            lon = tag.station_lon;
        }
        if (items[ptr].lat != LAT_UNDEFINED && items[ptr].lon != LON_UNDEFINED)
        {
            lat = items[ptr].lat;
            lon = items[ptr].lon;
        }
    }

    void moveToFront(int ptr)
    {
        if (ptr == first)
            return;

        // remove ptr out of the linked list
        if (items[ptr].time_ll.next != -1)
            items[items[ptr].time_ll.next].time_ll.prev = items[ptr].time_ll.prev;
        else
            last = items[ptr].time_ll.prev;

        items[items[ptr].time_ll.prev].time_ll.next = items[ptr].time_ll.next;

        // new ship is first in list
        items[ptr].time_ll.next = first;
        items[ptr].time_ll.prev = -1;

        items[first].time_ll.prev = ptr;
        first = ptr;
    }

    int create(int hexident)
    {
        int ptr = last;

        int oldhash = hash(items[ptr].hexident);
        int newhash = hash(hexident);

        // Remove from hash list if already present
        if (items[ptr].hash_ll.next != FREE || items[ptr].hash_ll.prev != FREE)
        {
            if (items[ptr].hash_ll.next != END)
                items[items[ptr].hash_ll.next].hash_ll.prev = items[ptr].hash_ll.prev;
            else
                hash_ll[oldhash].prev = items[ptr].hash_ll.prev;

            if (items[ptr].hash_ll.prev != END)
                items[items[ptr].hash_ll.prev].hash_ll.next = items[ptr].hash_ll.next;
            else
                hash_ll[oldhash].next = items[ptr].hash_ll.next;
        }

        // Insert into hash list (node is guaranteed to be removed already)
        items[ptr].hash_ll.prev = END;
        items[ptr].hash_ll.next = hash_ll[newhash].next;

        if (hash_ll[newhash].next != END)
            items[hash_ll[newhash].next].hash_ll.prev = ptr;

        hash_ll[newhash].next = ptr;

        count = MIN(count + 1, N);
        items[ptr].clear();
        items[ptr].hexident = hexident;

        items[ptr].setCountryCode();

        return ptr;
    }

    int find(int hexid) const
    {

        int h = hash(hexid);
        int ptr = hash_ll[h].next;
        while (ptr != END)
        {
            if (items[ptr].hexident == hexid)
                return ptr;
            ptr = items[ptr].hash_ll.next;
        }

        return -1;
    }

    bool checkInCPRCache(const Plane::CPR &cpr, bool even, FLOAT32 &lat)
    {
        bool duplicate = false;
        int &idx = even ? CPR_cache_even_idx : CPR_cache_odd_idx;

        for (int i = 0; i < CPR_CACHE_SIZE && !duplicate; i++)
        {
            Plane::CPR &cache = even ? CPR_cache_even[i] : CPR_cache_odd[i];

            if (!cache.Valid() || cpr.timestamp - cache.timestamp > 2)
                continue;

            duplicate = cache.lat == cpr.lat && cache.lon == cpr.lon && cache.airborne == cpr.airborne;
        }

        if (!duplicate)
        {
            (even ? CPR_cache_even[idx] : CPR_cache_odd[idx]) = cpr;
            idx = (idx + 1) % CPR_CACHE_SIZE;
        }

        return duplicate;
    }

    void Receive(const Plane::ADSB *msg, int len, TAG &tag)
    {
        bool position_updated = false;

        std::lock_guard<std::mutex> lock(mtx);

        // Skip invalid messages
        if (msg->hexident == HEXIDENT_UNDEFINED || msg->status == STATUS_ERROR)
            return;

        // Find or create plane entry
        int ptr = find(msg->hexident);

        if (ptr == -1)
        {
            // if ICAO is implied from CRC, ignore the message if not known
            if (msg->hexident_status == HEXINDENT_IMPLIED_FROM_CRC)
                return;

            ptr = create(msg->hexident);
        }

        // Move to front and update data
        moveToFront(ptr);
        Plane::ADSB &plane = items[ptr];

        // Update timestamp and core identifiers
        plane.rxtime = msg->rxtime;
        plane.nMessages++;
        plane.group_mask |= tag.group;
        plane.last_group = tag.group;

        plane.message_types |= msg->message_types;
        plane.message_subtypes |= msg->message_subtypes;

        // update category if valid
        if (msg->category != CATEGORY_UNDEFINED)
            plane.category = msg->category;

        // Update position if valid
        if (msg->lat != LAT_UNDEFINED && msg->lon != LON_UNDEFINED)
        {
            plane.lat = msg->lat;
            plane.lon = msg->lon;
            plane.position_timestamp = msg->rxtime;
        }

        FLOAT32 lat_new = LAT_UNDEFINED, lon_new = LON_UNDEFINED;

        if(msg->lat != LAT_UNDEFINED && msg->lon != LON_UNDEFINED)
        {
            lat_new = msg->lat;
            lon_new = msg->lon;
            position_updated = true;
        }
        
        if (msg->even.Valid())
        {
            if (!checkInCPRCache(msg->even, true, lat_new))
            {
                plane.even.lat = msg->even.lat;
                plane.even.lon = msg->even.lon;
                plane.even.timestamp = msg->even.timestamp;
                plane.even.airborne = msg->even.airborne;

                FLOAT32 ref_lat = LAT_UNDEFINED, ref_lon = LON_UNDEFINED;
                if (!msg->even.airborne)
                    calcReferencePosition(tag, ptr, ref_lat, ref_lon);

                plane.decodeCPR(ref_lat, ref_lon, true, position_updated, lat_new, lon_new);
            }
        }

        if (msg->odd.Valid())
        {
            if (!checkInCPRCache(msg->odd, false, lat_new))
            {
                plane.odd.lat = msg->odd.lat;
                plane.odd.lon = msg->odd.lon;
                plane.odd.timestamp = msg->odd.timestamp;
                plane.odd.airborne = msg->odd.airborne;

                FLOAT32 ref_lat = LAT_UNDEFINED, ref_lon = LON_UNDEFINED;

                if (!msg->odd.airborne)
                    calcReferencePosition(tag, ptr, ref_lat, ref_lon);

                plane.decodeCPR(ref_lat, ref_lon, false, position_updated, lat_new, lon_new);
            }
        }

        if (position_updated)
        {
            if (plane.position_status == Plane::ValueStatus::VALID)
            {
                // this can be improved by checking the distance to the last known position
                if (std::fabs(plane.lat - lat_new) > 0.1 || std::fabs(plane.lon - lon_new) > 0.1)
                {
                    plane.position_status = Plane::ValueStatus::UNKNOWN;
                }
            }

            // store the history of the last 3 CPR positions
            plane.CPR_history[plane.CPR_history_idx].lat = lat_new;
            plane.CPR_history[plane.CPR_history_idx].lon = lon_new;
            plane.CPR_history[plane.CPR_history_idx].even = msg->even.Valid();
            plane.CPR_history[plane.CPR_history_idx].cpr = msg->even.Valid() ? plane.even : plane.odd;

            if (plane.position_status == Plane::ValueStatus::UNKNOWN)
            {
                // check for consistency with independent position
                int prev = (plane.CPR_history_idx + 2) % 3;
                int indepedent = (plane.CPR_history_idx + 1) % 3;

                if (plane.CPR_history[prev].cpr.Valid() && plane.CPR_history[plane.CPR_history_idx].cpr.Valid() && plane.CPR_history[plane.CPR_history_idx].even != plane.CPR_history[prev].even)
                {
                    if (plane.CPR_history[indepedent].cpr.Valid())
                    {
                        // check against last independent position, i.e. with different CPR pair for both legs
                        double deltat = 1 - plane.CPR_history[indepedent].cpr.timestamp + plane.CPR_history[plane.CPR_history_idx].cpr.timestamp;
                        if (deltat < 15 * 60)
                        {
                            FLOAT32 distance = DISTANCE_UNDEFINED;
                            int angle = ANGLE_UNDEFINED;
                            getDistanceAndBearing(plane.CPR_history[indepedent].lat, plane.CPR_history[indepedent].lon, lat_new, lon_new, distance, angle);

                            double speed = plane.speed == SPEED_UNDEFINED ? 1000 : plane.speed * 1.5;
                            double max_distance = deltat * speed / 3600.0;

                            if (distance < max_distance)
                            {
                                plane.position_status = Plane::ValueStatus::VALID;
                            }
                        }
                    }
                }
            }

            plane.CPR_history_idx = (plane.CPR_history_idx + 1) % 3;

            if (plane.position_status == Plane::ValueStatus::VALID)
            {
                plane.lat = lat_new;
                plane.lon = lon_new;
                plane.position_timestamp = msg->rxtime;
            }
        }

        if (position_updated && tag.station_lat != LAT_UNDEFINED && tag.station_lon != LON_UNDEFINED)
        {
            getDistanceAndBearing(tag.station_lat, tag.station_lon, plane.lat, plane.lon, plane.distance, plane.angle);
            tag.distance = plane.distance;
            tag.angle = plane.angle;
        }
        else
        {
            tag.distance = DISTANCE_UNDEFINED;
            tag.angle = ANGLE_UNDEFINED;
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
                if (!include_inactive && time_since_update > 300)
                {
                    break;
                }

                if (time_since_update <= 60 || (time_since_update <= 300 && plane.airborne == 0))
                {
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
                               std::to_string(plane.airborne) + comma + std::to_string(plane.nMessages) + comma + std::to_string(time_since_update) + comma +
                               (plane.category != CATEGORY_UNDEFINED ? std::to_string(plane.category) : null_str) + comma +
                               (plane.signalLevel != LEVEL_UNDEFINED ? std::to_string(plane.signalLevel) : null_str) + comma +
                               (plane.country_code[0] != ' ' ? "\"" + std::string(plane.country_code, 2) + "\"" : null_str) + comma +
                               (plane.distance != DISTANCE_UNDEFINED ? std::to_string(plane.distance) : null_str) + comma +
                               std::to_string(plane.message_types) + comma + std::to_string(plane.message_subtypes) + comma +
                               std::to_string(plane.group_mask) + comma + std::to_string(plane.last_group) + comma +
                               (plane.angle != ANGLE_UNDEFINED ? std::to_string(plane.angle) : null_str) +
                               "]";

                    delim = comma;
                }
            }
            ptr = items[ptr].time_ll.next;
        }

        content += "],\"error\":false}\n\n";
        return content;
    }

    int getFirst() const { return first; }
    int getLast() const { return last; }
    int getCount() const { return count; }

    void setLat(FLOAT32 lat) { this->station_lat = lat; }
    void setLon(FLOAT32 lon) { this->station_lon = lon; }
};
