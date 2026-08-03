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

    struct CPRCache
    {
        Plane::CPR entries[CPR_CACHE_SIZE];
        int idx = 0;

        // true when an identical frame was seen in the last two seconds;
        // otherwise the frame is remembered
        bool isDuplicate(const Plane::CPR &cpr)
        {
            for (const auto &e : entries)
                if (e.Valid() && cpr.timestamp - e.timestamp <= 2 &&
                    e.lat == cpr.lat && e.lon == cpr.lon && e.airborne == cpr.airborne)
                    return true;

            entries[idx] = cpr;
            idx = (idx + 1) % CPR_CACHE_SIZE;
            return false;
        }
    };

    CPRCache cpr_cache[2]; // indexed by parity

    static const int N = 512;
    static const int NBUCKETS = 1031;

    static const int TIMEOUT_AIRBORNE = 60; // ADS-B goes quiet only when the plane is gone
    static const int TIMEOUT_GROUND = 300;  // ground traffic reports lazily
    static const int CONFIRM_WINDOW = 15 * 60;

    typedef SlotTable<Plane::ADSB, uint32_t> Table;
    Table table;

    void calcReferencePosition(TAG &tag, int ptr, FLOAT32 &lat, FLOAT32 &lon)
    {
        lat = LAT_UNDEFINED;
        lon = LON_UNDEFINED;

        if (isValidCoord(table[ptr].lat, table[ptr].lon))
        {
            lat = table[ptr].lat;
            lon = table[ptr].lon;
        }
        else if (isValidCoord(tag.station_lat, tag.station_lon))
        {
            lat = tag.station_lat;
            lon = tag.station_lon;
        }
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

    void updateCPRLeg(Plane::ADSB &plane, int ptr, const Plane::CPR &leg, bool even, TAG &tag, bool &positionUpdated, FLOAT32 &lat_new, FLOAT32 &lon_new)
    {
        if (!leg.Valid() || cpr_cache[even].isDuplicate(leg))
            return;

        (even ? plane.even : plane.odd) = leg;

        FLOAT32 ref_lat = LAT_UNDEFINED, ref_lon = LON_UNDEFINED;
        if (!leg.airborne)
            calcReferencePosition(tag, ptr, ref_lat, ref_lon);

        plane.decodeCPR(ref_lat, ref_lon, even, positionUpdated, lat_new, lon_new);
    }

    // An unvalidated fix becomes trusted when it lies within plausible travel
    // range of the last position decoded from an independent CPR pair.
    bool confirmedByHistory(const Plane::ADSB &plane, FLOAT32 lat, FLOAT32 lon)
    {
        const auto &cur = plane.CPR_history[plane.CPR_history_idx];
        const auto &prev = plane.CPR_history[(plane.CPR_history_idx + 2) % 3];
        const auto &independent = plane.CPR_history[(plane.CPR_history_idx + 1) % 3];

        if (!cur.cpr.Valid() || !prev.cpr.Valid() || !independent.cpr.Valid() || cur.even == prev.even)
            return false;

        double deltat = 1 + cur.cpr.timestamp - independent.cpr.timestamp;
        if (deltat >= CONFIRM_WINDOW)
            return false;

        FLOAT32 distance = DISTANCE_UNDEFINED;
        int angle = ANGLE_UNDEFINED;
        Util::Geodesy::distanceBearing(independent.lat, independent.lon, lat, lon, distance, angle);

        // unknown speed assumes fast; a known speed gets a 50% margin
        double max_speed = plane.speed == SPEED_UNDEFINED ? 1000 : plane.speed * 1.5;
        return distance < deltat * max_speed / 3600.0;
    }

    // Process a single decoded message; caller must hold mtx.
    void updatePlane(const Plane::ADSB *msg, TAG &tag)
    {
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
        bool positionUpdated = false;

        if (isValidCoord(msg->lat, msg->lon))
        {
            plane.lat = msg->lat;
            plane.lon = msg->lon;
            plane.position_timestamp = msg->rxtime;

            lat_new = msg->lat;
            lon_new = msg->lon;
            positionUpdated = true;
        }

        updateCPRLeg(plane, ptr, msg->even, true, tag, positionUpdated, lat_new, lon_new);
        updateCPRLeg(plane, ptr, msg->odd, false, tag, positionUpdated, lat_new, lon_new);

        if (positionUpdated)
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
            bool even = msg->even.Valid();
            plane.CPR_history[plane.CPR_history_idx] = {lat_new, lon_new, even ? plane.even : plane.odd, even};

            if (plane.position_status == Plane::ValueStatus::UNKNOWN && confirmedByHistory(plane, lat_new, lon_new))
                plane.position_status = Plane::ValueStatus::VALID;

            plane.CPR_history_idx = (plane.CPR_history_idx + 1) % 3;

            if (plane.position_status == Plane::ValueStatus::VALID)
            {
                plane.lat = lat_new;
                plane.lon = lon_new;
                plane.position_timestamp = msg->rxtime;
            }
        }

        if (positionUpdated && isValidCoord(tag.station_lat, tag.station_lon))
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

        for (auto &c : cpr_cache)
            for (auto &e : c.entries)
                e.clear();
    }

    void Receive(const Plane::ADSB *msg, int len, TAG &tag)
    {
        std::lock_guard<std::mutex> lock(mtx);

        for (int i = 0; i < len; i++)
            updatePlane(&msg[i], tag);
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

            if (time_since_update > TIMEOUT_GROUND)
                return false;
            if (since > 0 && rx < since)
                return false;

            if (time_since_update <= TIMEOUT_AIRBORNE || (time_since_update <= TIMEOUT_GROUND && plane.airborne == 0))
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
