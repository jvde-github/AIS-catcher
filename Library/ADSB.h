#pragma once
#include <string>
#include <time.h>
#include <vector>
#include <iomanip>
#include <sstream>

#include "Common.h"
#include "Utilities.h"

namespace Plane
{
    enum class MessageType
    {
        MSG = 0,
        SEL, // Selection Change
        ID,  // New ID
        AIR, // New Aircraft
        STA, // Status Change
        CLK  // Click
    };

    enum class BoolType
    {
        FALSE = 0,
        TRUE = 1,
        UNKNOWN = 2
    };

    enum class TransmissionType
    {
        NONE = 0,
        ES_IDENTIFICATION = 1, // ES Identification and Category
        ES_SURFACE_POS = 2,    // ES Surface Position
        ES_AIRBORNE_POS = 3,   // ES Airborne Position
        ES_AIRBORNE_VEL = 4,   // ES Airborne Velocity
        SURVEILLANCE_ALT = 5,  // Surveillance Alt
        SURVEILLANCE_ID = 6,   // Surveillance ID
        AIR_TO_AIR = 7,        // Air To Air
        ALL_CALL_REPLY = 8     // All Call Reply
    };

    class ADSB
    {
    protected:
        std::time_t rxtime;
        MessageType type;
        TransmissionType transmission;
        uint32_t hexident;   // Aircraft Mode S hex code
        int altitude;        // Mode C altitude
        FLOAT32 lat, lon;    // Position
        FLOAT32 groundspeed; // Speed over ground
        FLOAT32 track;       // Track angle
        int vertrate;        // Vertical rate
        char callsign[8];    // Aircraft callsign
        int squawk;          // Mode A squawk code
        BoolType alert;      // Squawk change flag
        BoolType emergency;  // Emergency flag
        BoolType spi;        // Ident flag
        BoolType onground;   // Ground squat switch flag

    public:
        void Stamp(std::time_t t = (std::time_t)0L)
        {
            std::time(&rxtime);
            if ((long int)t != 0 && t < rxtime)
                setRxTimeUnix(t);
        }

        void setRxTimeUnix(std::time_t t) { rxtime = t; }
        std::time_t getRxTimeUnix() const { return rxtime; }

        void clear()
        {
            type = MessageType::MSG;
            transmission = TransmissionType::NONE;
            hexident = 0;
            altitude = 0;
            lat = LAT_UNDEFINED;
            lon = LON_UNDEFINED;
            groundspeed = GROUNDSPEED_UNDEFINED;
            track = TRACK_UNDEFINED;
            vertrate = VERTRATE_UNDEFINED;
            squawk = SQUAWK_UNDEFINED;
            alert = emergency = spi = onground = BoolType::UNKNOWN;
            std::memset(callsign, '@', 8);
        }

        // Getters
        MessageType getType() const { return type; }
        TransmissionType getTransmission() const { return transmission; }

        uint32_t getHexIdent() const { return hexident; }
        int getAltitude() const { return altitude; }
        FLOAT32 getLat() const { return lat; }
        FLOAT32 getLon() const { return lon; }
        FLOAT32 getGroundSpeed() const { return groundspeed; }
        FLOAT32 getTrack() const { return track; }
        int getVertRate() const { return vertrate; }
        std::string getCallsign() const { return std::string(callsign, 8); }
        int getSquawk() const { return squawk; }
        BoolType getAlert() const { return alert; }
        BoolType getEmergency() const { return emergency; }
        BoolType getSPI() const { return spi; }
        BoolType isOnGround() const { return onground; }

        // Setters
        void setType(MessageType t) { type = t; }
        void setTransmission(TransmissionType t) { transmission = t; }

        void setHexIdent(uint32_t h) { hexident = h; }
        void setAltitude(int a) { altitude = a; }
        void setPosition(FLOAT32 la, FLOAT32 lo)
        {
            lat = la;
            lon = lo;
        }
        void setGroundSpeed(FLOAT32 gs) { groundspeed = gs; }
        void setTrack(FLOAT32 t) { track = t; }
        void setVertRate(int vr) { vertrate = vr; }
        void setCallsign(const std::string &c)
        {
            std::memset(callsign, '@', 8);
            std::strncpy(callsign, c.c_str(), MIN(c.length(), sizeof(callsign)));
        }
        void setSquawk(int s) { squawk = s; }
        void setAlert(BoolType a) { alert = (BoolType)a; }
        void setEmergency(BoolType e) { emergency = (BoolType)e; }
        void setSPI(BoolType s) { spi = (BoolType)s; }
        void setOnGround(BoolType g) { onground = (BoolType)g; }

        void Print()
        {
            char timebuf[32];
            std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&rxtime));

            std::cout << "ADSB: " << std::endl;
            std::cout << "  Time: " << timebuf << std::endl;
            std::cout << "  Type: " << (int)type << std::endl;
            if (transmission != TransmissionType::NONE)
                std::cout << "  Transmission: " << (int)transmission << std::endl;
            std::cout << "  HexIdent: " << std::setfill('0') << std::setw(6) << std::hex << std::uppercase << hexident << std::dec << std::endl;
            if (altitude != 0)
                std::cout << "  Altitude: " << altitude << std::endl;
            if (lat != LAT_UNDEFINED)
                std::cout << "  Lat: " << lat << std::endl;
            if (lon != LON_UNDEFINED)
                std::cout << "  Lon: " << lon << std::endl;
            if (groundspeed != GROUNDSPEED_UNDEFINED)
                std::cout << "  GroundSpeed: " << groundspeed << std::endl;
            if (track != TRACK_UNDEFINED)
                std::cout << "  Track: " << track << std::endl;
            if (vertrate != VERTRATE_UNDEFINED)
                std::cout << "  VertRate: " << vertrate << std::endl;
            if (std::string(callsign, 8) != "@@@@@@@@")
                std::cout << "  Callsign: " << std::string(callsign, 8) << std::endl;
            if (squawk != SQUAWK_UNDEFINED)
                std::cout << "  Squawk: " << squawk << std::endl;
            if (alert != BoolType::UNKNOWN)
                std::cout << "  Alert: " << (int)alert << std::endl;
            if (emergency != BoolType::UNKNOWN)
                std::cout << "  Emergency: " << (int)emergency << std::endl;
            if (spi != BoolType::UNKNOWN)
                std::cout << "  SPI: " << (int)spi << std::endl;
            if (onground != BoolType::UNKNOWN)
                std::cout << "  OnGround: " << (int)onground << std::endl;
        }
    };
}