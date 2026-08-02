#include <array>

#include "ADSB.h"
#include "SlotTable.h"
#include "Geodesy.h"
#include "Stream.h"
#include "JSON/Writer.h"

class PlaneDB : public StreamIn<Plane::ADSB>
{
private:
    std::mutex mtx;

    const static int CPR_CACHE_SIZE = 3;

    Plane::CPR CPR_cache_even[CPR_CACHE_SIZE];
    Plane::CPR CPR_cache_odd[CPR_CACHE_SIZE];

    int CPR_cache_even_idx = 0;
    int CPR_cache_odd_idx = 0;

    static const int N = 512;
    static const int NBUCKETS = 1031;

    SlotTable<Plane::ADSB, uint32_t> table;

    FLOAT32 station_lat = LAT_UNDEFINED, station_lon = LON_UNDEFINED;

public:
    PlaneDB()
    {
        table.setup(N, NBUCKETS);

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
        if (table[ptr].lat != LAT_UNDEFINED && table[ptr].lon != LON_UNDEFINED)
        {
            lat = table[ptr].lat;
            lon = table[ptr].lon;
        }
    }

    bool checkInCPRCache(const Plane::CPR &cpr, bool even)
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

    // Process a single decoded message; caller must hold mtx.
    void update(const Plane::ADSB *msg, TAG &tag)
    {
        bool position_updated = false;

        // Skip invalid messages
        if (msg->hexident == HEXIDENT_UNDEFINED || msg->status == STATUS_ERROR)
            return;

        // Find or create plane entry
        int ptr = table.find(msg->hexident);

        if (ptr == SlotTable<Plane::ADSB, uint32_t>::NIL)
        {
            // if ICAO is implied from CRC, ignore the message if not known
            if (msg->hexident_status == HEXINDENT_IMPLIED_FROM_CRC)
                return;

            ptr = table.create(msg->hexident);
            table[ptr].clear();
            table[ptr].hexident = msg->hexident;
            table[ptr].hexident_status = msg->hexident_status;

            if (msg->hexident_status == HEXINDENT_DIRECT)
                table[ptr].setCountryCode();
        }
        else
            table.touch(ptr);

        Plane::ADSB &plane = table[ptr];

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
            if (!checkInCPRCache(msg->even, true))
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
            if (!checkInCPRCache(msg->odd, false))
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
                int independent = (plane.CPR_history_idx + 1) % 3;

                if (plane.CPR_history[prev].cpr.Valid() && plane.CPR_history[plane.CPR_history_idx].cpr.Valid() && plane.CPR_history[plane.CPR_history_idx].even != plane.CPR_history[prev].even)
                {
                    if (plane.CPR_history[independent].cpr.Valid())
                    {
                        // check against last independent position, i.e. with different CPR pair for both legs
                        double deltat = 1 - plane.CPR_history[independent].cpr.timestamp + plane.CPR_history[plane.CPR_history_idx].cpr.timestamp;
                        if (deltat < 15 * 60)
                        {
                            FLOAT32 distance = DISTANCE_UNDEFINED;
                            int angle = ANGLE_UNDEFINED;
                            Util::Geodesy::distanceBearing(plane.CPR_history[independent].lat, plane.CPR_history[independent].lon, lat_new, lon_new, distance, angle);

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
            Util::Geodesy::distanceBearing(tag.station_lat, tag.station_lon, plane.lat, plane.lon, plane.distance, plane.angle);
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

        if (msg->signalLevel != LEVEL_UNDEFINED)
        {
            plane.signalLevel = msg->signalLevel;
        }
    }

    void Receive(const Plane::ADSB *msg, int len, TAG &tag)
    {
        std::lock_guard<std::mutex> lock(mtx);

        for (int i = 0; i < len; i++)
            update(&msg[i], tag);
    }

    std::string getCompactArray(bool include_inactive = false, std::time_t since = 0)
    {
        std::lock_guard<std::mutex> lock(mtx);

        std::string content;
        JSON::Writer w(content, 4096);
        std::time_t now = std::time(nullptr);
        w.beginObject().kv("count", table.size()).kv("time", (long long)now).key("values").beginArray();

        int ptr = table.front();

        while (ptr != -1)
        {
            const Plane::ADSB &plane = table[ptr];

            if (plane.hexident != HEXIDENT_UNDEFINED)
            {
                long int time_since_update = now - plane.getRxTimeUnix();

                // Skip inactive planes unless requested
                if (!include_inactive && time_since_update > 300)
                {
                    break;
                }

                // Incremental: stop once we hit planes older than `since`
                if (since > 0 && (std::time_t)plane.getRxTimeUnix() < since)
                {
                    break;
                }

                if (time_since_update <= 60 || (time_since_update <= 300 && plane.airborne == 0))
                {
                    w.beginArray().val(plane.hexident)
                        .val_unless(plane.lat, LAT_UNDEFINED).val_unless(plane.lon, LON_UNDEFINED)
                        .val_unless(plane.altitude, ALTITUDE_UNDEFINED).val_unless(plane.speed, SPEED_UNDEFINED)
                        .val_unless(plane.heading, HEADING_UNDEFINED).val_unless(plane.vertrate, VERT_RATE_UNDEFINED)
                        .val_unless(plane.squawk, SQUAWK_UNDEFINED)
                        .val(plane.callsign).val(plane.airborne).val(plane.nMessages).val((long long)plane.getRxTimeUnix())
                        .val_unless(plane.category, CATEGORY_UNDEFINED).val_unless(plane.signalLevel, LEVEL_UNDEFINED);
                    if (plane.country_code[0] != ' ')
                        w.val({plane.country_code, 2});
                    else
                        w.val_null();
                    w.val_unless(plane.distance, DISTANCE_UNDEFINED)
                        .val(plane.message_types).val(plane.message_subtypes)
                        .val(plane.group_mask).val(plane.last_group)
                        .val_unless(plane.angle, ANGLE_UNDEFINED).endArray();
                }
            }
            ptr = table.next(ptr);
        }

        w.endArray().kv("error", false).endObject().raw("\n\n");
        w.finish();
        return content;
    }

    int getCount() const { return table.size(); }

    void setLat(FLOAT32 lat) { this->station_lat = lat; }
    void setLon(FLOAT32 lon) { this->station_lon = lon; }
};
