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
    PlaneDB() {
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

    void moveToFront(int ptr) {
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

    int create() {
        int ptr = last;
        count = MIN(count + 1, N);
        items[ptr].clear();

        return ptr;
    }

    int find(int hexid) const {
	    int ptr = first, cnt = count;
        while (ptr != -1 && --cnt >= 0)
        {
            if (items[ptr].hexident == hexid)
                return ptr;
            ptr = items[ptr].next;
        }
        return -1;
    }

       void Receive(const Plane::ADSB* msg, int len, TAG& tag) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Skip invalid messages
        if (msg->getHexIdent() == 0) return;

        // Find or create plane entry
        int ptr = find(msg->getHexIdent());
        if (ptr == -1) {
            ptr = create();
        }

        // Move to front and update data
        moveToFront(ptr);
        Plane::ADSB& plane = items[ptr];

        // Update timestamp and core identifiers
        plane.setRxTimeUnix(msg->getRxTimeUnix());
        plane.setHexIdent(msg->getHexIdent());
        
        // Update position if valid
        if (msg->getLat() != LAT_UNDEFINED && msg->getLon() != LON_UNDEFINED) {
            plane.setPosition(msg->getLat(), msg->getLon());
        }

        // Update altitude
        if (msg->getAltitude() != 0) {
            plane.setAltitude(msg->getAltitude());
        }

        // Update movement data
        if (msg->getGroundSpeed() != GROUNDSPEED_UNDEFINED) {
            plane.setGroundSpeed(msg->getGroundSpeed());
        }
        if (msg->getTrack() != TRACK_UNDEFINED) {
            plane.setTrack(msg->getTrack());
        }
        if (msg->getVertRate() != VERTRATE_UNDEFINED) {
            plane.setVertRate(msg->getVertRate());
        }

        // Update identification
        if (msg->getSquawk() != SQUAWK_UNDEFINED) {
            plane.setSquawk(msg->getSquawk());
        }
        if (msg->getCallsign() != "@@@@@@@@") {
            plane.setCallsign(msg->getCallsign());
        }

        // Update status flags
        if (msg->getAlert() != Plane::BoolType::UNKNOWN) {
            plane.setAlert(msg->getAlert());
        }
        if (msg->getEmergency() != Plane::BoolType::UNKNOWN) {
            plane.setEmergency(msg->getEmergency());
        }
        if (msg->getSPI() != Plane::BoolType::UNKNOWN) {
            plane.setSPI(msg->getSPI());
        }
        if (msg->isOnGround() != Plane::BoolType::UNKNOWN) {
            plane.setOnGround(msg->isOnGround());
        }

        plane.setType(msg->getType());
        plane.setTransmission(msg->getTransmission());

        if (msg->getLat() != LAT_UNDEFINED && msg->getLon() != LON_UNDEFINED) {
            tag.lat = msg->getLat();
            tag.lon = msg->getLon();
        }
    }

  std::string getCompactArray(bool include_inactive = false) {
        std::lock_guard<std::mutex> lock(mtx);
        
        const std::string null_str = "null";
        const std::string comma = ",";
        std::string content = "{\"count\":" + std::to_string(count) + ",\"values\":[";
        
        std::time_t now = std::time(nullptr);
        int ptr = first;
        std::string delim = "";
        
        while (ptr != -1) {
            const Plane::ADSB& plane = items[ptr];

            if (plane.getHexIdent() != 0) {
                long int time_since_update = now - plane.getRxTimeUnix();
                
                // Skip inactive planes unless requested
                if (!include_inactive && time_since_update > 3600) {
                    //break;
                }
                
                content += delim + "[" +
                    std::to_string(plane.getHexIdent()) + comma +
                    (plane.getLat() != LAT_UNDEFINED ? std::to_string(plane.getLat()) : null_str) + comma +
                    (plane.getLon() != LON_UNDEFINED ? std::to_string(plane.getLon()) : null_str) + comma +
                    (plane.getAltitude() != 0 ? std::to_string(plane.getAltitude()) : null_str) + comma +
                    (plane.getGroundSpeed() != GROUNDSPEED_UNDEFINED ? std::to_string(plane.getGroundSpeed()) : null_str) + comma +
                    (plane.getTrack() != TRACK_UNDEFINED ? std::to_string(plane.getTrack()) : null_str) + comma +
                    (plane.getVertRate() != VERTRATE_UNDEFINED ? std::to_string(plane.getVertRate()) : null_str) + comma +
                    (plane.getSquawk() != SQUAWK_UNDEFINED ? std::to_string(plane.getSquawk()) : null_str) + comma +
                    plane.getCallsign()  + comma +
                    (plane.getAlert() != Plane::BoolType::UNKNOWN ? std::to_string(static_cast<int>(plane.getAlert())) : null_str) + comma +
                    (plane.getEmergency() != Plane::BoolType::UNKNOWN ? std::to_string(static_cast<int>(plane.getEmergency())) : null_str) + comma +
                    (plane.getSPI() != Plane::BoolType::UNKNOWN ? std::to_string(static_cast<int>(plane.getSPI())) : null_str) + comma +
                    (plane.isOnGround() != Plane::BoolType::UNKNOWN ? std::to_string(static_cast<int>(plane.isOnGround())) : null_str) + comma +
                    std::to_string(static_cast<int>(plane.getType())) + comma +
                    std::to_string(static_cast<int>(plane.getTransmission())) + comma +
                    std::to_string(time_since_update) + "]";
                
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