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
#include "ADSB.h"
#include "SlotTable.h"
#include "Geodesy.h"
#include "Stream.h"
#include "JSON/Writer.h"

class PlaneDB : public StreamIn<Plane::ADSB>
{
    std::mutex mtx;

    static const int CPR_CACHE_SIZE = 3;

    Plane::CPR CPR_cache_even[CPR_CACHE_SIZE];
    Plane::CPR CPR_cache_odd[CPR_CACHE_SIZE];

    int CPR_cache_even_idx = 0;
    int CPR_cache_odd_idx = 0;

    static const int N = 512;
    static const int NBUCKETS = 1031;

    typedef SlotTable<Plane::ADSB, uint32_t> Table;
    Table table;

    void calcReferencePosition(TAG &tag, int ptr, FLOAT32 &lat, FLOAT32 &lon)
    {
        lat = LAT_UNDEFINED;
        lon = LON_UNDEFINED;

        if (isValidCoord(tag.station_lat, tag.station_lon))
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
        Plane::CPR(&cache)[CPR_CACHE_SIZE] = even ? CPR_cache_even : CPR_cache_odd;
        int &idx = even ? CPR_cache_even_idx : CPR_cache_odd_idx;

        for (int i = 0; i < CPR_CACHE_SIZE; i++)
        {
            if (!cache[i].Valid() || cpr.timestamp - cache[i].timestamp > 2)
                continue;
            if (cache[i].lat == cpr.lat && cache[i].lon == cpr.lon && cache[i].airborne == cpr.airborne)
                return true;
        }

        cache[idx] = cpr;
        idx = (idx + 1) % CPR_CACHE_SIZE;
        return false;
    }

    // NIL when the ICAO is only implied from CRC and the plane is not already known
    int claimPlane(const Plane::ADSB *msg)
    {
        int ptr = table.find(msg->hexident);
        if (ptr != Table::NIL)
        {
            table.touch(ptr);
            return ptr;
        }

        if (msg->hexident_status == HEXINDENT_IMPLIED_FROM_CRC)
            return Table::NIL;

        ptr = table.create(msg->hexident);
        table[ptr].clear();
        table[ptr].hexident = msg->hexident;
        table[ptr].hexident_status = msg->hexident_status;

        if (msg->hexident_status == HEXINDENT_DIRECT)
            table[ptr].setCountryCode();

        return ptr;
    }

    void updateCPRLeg(Plane::ADSB &plane, int ptr, const Plane::CPR &leg, bool even, TAG &tag, bool &position_updated, FLOAT32 &lat_new, FLOAT32 &lon_new)
    {
        if (!leg.Valid() || checkInCPRCache(leg, even))
            return;

        (even ? plane.even : plane.odd) = leg;

        FLOAT32 ref_lat = LAT_UNDEFINED, ref_lon = LON_UNDEFINED;
        if (!leg.airborne)
            calcReferencePosition(tag, ptr, ref_lat, ref_lon);

        plane.decodeCPR(ref_lat, ref_lon, even, position_updated, lat_new, lon_new);
    }

    // Process a single decoded message; caller must hold mtx.
    void update(const Plane::ADSB *msg, TAG &tag)
    {
        bool position_updated = false;

        if (msg->hexident == HEXIDENT_UNDEFINED || msg->status == STATUS_ERROR)
            return;

        int ptr = claimPlane(msg);
        if (ptr == Table::NIL)
            return;

        Plane::ADSB &plane = table[ptr];

        plane.rxtime = msg->rxtime;

        plane.nMessages++;
        plane.group_mask |= tag.group;
        plane.last_group = tag.group;

        plane.message_types |= msg->message_types;
        plane.message_subtypes |= msg->message_subtypes;

        if (msg->category != CATEGORY_UNDEFINED)
            plane.category = msg->category;

        FLOAT32 lat_new = LAT_UNDEFINED, lon_new = LON_UNDEFINED;

        if (msg->lat != LAT_UNDEFINED && msg->lon != LON_UNDEFINED)
        {
            plane.lat = msg->lat;
            plane.lon = msg->lon;
            plane.position_timestamp = msg->rxtime;

            lat_new = msg->lat;
            lon_new = msg->lon;
            position_updated = true;
        }

        updateCPRLeg(plane, ptr, msg->even, true, tag, position_updated, lat_new, lon_new);
        updateCPRLeg(plane, ptr, msg->odd, false, tag, position_updated, lat_new, lon_new);

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
            auto &cur = plane.CPR_history[plane.CPR_history_idx];
            cur.lat = lat_new;
            cur.lon = lon_new;
            cur.even = msg->even.Valid();
            cur.cpr = msg->even.Valid() ? plane.even : plane.odd;

            if (plane.position_status == Plane::ValueStatus::UNKNOWN)
            {
                // check for consistency with the last independent position,
                // i.e. one with a different CPR pair for both legs
                const auto &prev = plane.CPR_history[(plane.CPR_history_idx + 2) % 3];
                const auto &independent = plane.CPR_history[(plane.CPR_history_idx + 1) % 3];

                if (prev.cpr.Valid() && cur.cpr.Valid() && cur.even != prev.even && independent.cpr.Valid())
                {
                    double deltat = 1 - independent.cpr.timestamp + cur.cpr.timestamp;
                    if (deltat < 15 * 60)
                    {
                        FLOAT32 distance = DISTANCE_UNDEFINED;
                        int angle = ANGLE_UNDEFINED;
                        Util::Geodesy::distanceBearing(independent.lat, independent.lon, lat_new, lon_new, distance, angle);

                        double speed = plane.speed == SPEED_UNDEFINED ? 1000 : plane.speed * 1.5;
                        double max_distance = deltat * speed / 3600.0;

                        if (distance < max_distance)
                        {
                            plane.position_status = Plane::ValueStatus::VALID;
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

        if (position_updated && isValidCoord(tag.station_lat, tag.station_lon))
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

        if (msg->altitude != ALTITUDE_UNDEFINED)
            plane.altitude = msg->altitude;

        if (msg->speed != SPEED_UNDEFINED)
            plane.speed = msg->speed;

        if (msg->heading != HEADING_UNDEFINED)
            plane.heading = msg->heading;

        if (msg->vertrate != VERT_RATE_UNDEFINED)
            plane.vertrate = msg->vertrate;

        if (msg->squawk != SQUAWK_UNDEFINED)
            plane.squawk = msg->squawk;

        if (msg->callsign[0] != '\0')
            std::memcpy(plane.callsign, msg->callsign, sizeof(plane.callsign));

        if (msg->airborne != AIRBORNE_UNDEFINED)
            plane.airborne = msg->airborne;

        if (msg->signalLevel != LEVEL_UNDEFINED)
            plane.signalLevel = msg->signalLevel;
    }

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

    void Receive(const Plane::ADSB *msg, int len, TAG &tag)
    {
        std::lock_guard<std::mutex> lock(mtx);

        for (int i = 0; i < len; i++)
            update(&msg[i], tag);
    }

    std::string getCompactArray(std::time_t since = 0)
    {
        std::lock_guard<std::mutex> lock(mtx);

        std::string content;
        JSON::Writer w(content, 32768);
        std::time_t now = std::time(nullptr);
        w.beginObject().kv("count", table.size()).kv("time", (long long)now).key("values").beginArray();

        table.forEach([&](int ptr) {
            const Plane::ADSB &plane = table[ptr];
            std::time_t rx = plane.getRxTimeUnix();
            long int time_since_update = now - rx;

            if (time_since_update > 300)
                return false;
            if (since > 0 && rx < since)
                return false;

            if (time_since_update <= 60 || (time_since_update <= 300 && plane.airborne == 0))
            {
                w.beginArray().val(plane.hexident)
                    .val_unless(plane.lat, LAT_UNDEFINED).val_unless(plane.lon, LON_UNDEFINED)
                    .val_unless(plane.altitude, ALTITUDE_UNDEFINED).val_unless(plane.speed, SPEED_UNDEFINED)
                    .val_unless(plane.heading, HEADING_UNDEFINED).val_unless(plane.vertrate, VERT_RATE_UNDEFINED)
                    .val_unless(plane.squawk, SQUAWK_UNDEFINED)
                    .val(plane.callsign).val(plane.airborne).val(plane.nMessages).val((long long)rx)
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
            return true;
        });

        w.endArray().kv("error", false).endObject().raw("\n\n");
        w.finish();
        return content;
    }
};
