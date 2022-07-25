/*
    Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include "AIS.h"

// Sources:
//	https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-0-199811-S!!PDF-E.pdf
//	https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
// 	https://gpsd.gitlab.io/gpsd/AIVDM.html

#include "Property.h"
#include "AISMessageDecoder.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

namespace AIS
{

    void AISMessageDecoder::Receive(const AIS::Message* data, int len, TAG& tag)
    {
        Submit(PROPERTY_FIRST, std::string(""));

        const AIS::Message& msg = data[0];

        Submit(PROPERTY_CLASS, std::string("AIS"));
        Submit(PROPERTY_DEVICE, std::string("AIS-catcher"));
        Submit(PROPERTY_SCALED, true);
        Submit(PROPERTY_CHANNEL, std::string(1, msg.channel));
        Submit(PROPERTY_NMEA, msg.sentence);

	//if(msg.type() != 24) return;

        if(tag.mode & 1)
	{
		Submit(PROPERTY_SIGNAL_POWER, tag.level);
        	Submit(PROPERTY_PPM, tag.ppm);
	}

        switch (msg.type())
        {
        case 1:
        case 2:
        case 3:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            E(msg, PROPERTY_STATUS, 38, 4);
            TURN(msg, PROPERTY_TURN, 42, 8);
            U1(msg, PROPERTY_SPEED, 50, 10, 1023);
            B(msg, PROPERTY_ACCURACY, 60, 1);
            POS(msg, PROPERTY_LON, 61, 28);
            POS(msg, PROPERTY_LAT, 89, 27);
            U1(msg, PROPERTY_COURSE, 116, 12);
            U(msg, PROPERTY_HEADING, 128, 9);
            U(msg, PROPERTY_SECOND, 137, 6);
            E(msg, PROPERTY_MANEUVER, 143, 2);
            X(msg, PROPERTY_SPARE, 145, 3);
            B(msg, PROPERTY_RAIM, 148, 1);
            U(msg, PROPERTY_RADIO, 149, 19);
            break;
        case 4:
	    case 11:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            U(msg, PROPERTY_YEAR, 38, 14, 0);
            U(msg, PROPERTY_MONTH, 52, 4, 0);
            U(msg, PROPERTY_DAY, 56, 5, 0);
            U(msg, PROPERTY_HOUR, 61, 5, 24);
            U(msg, PROPERTY_MINUTE, 66, 6, 60);
            U(msg, PROPERTY_SECOND, 72, 6, 60);
            B(msg, PROPERTY_ACCURACY, 78, 1);
            POS(msg, PROPERTY_LON, 79, 28);
            POS(msg, PROPERTY_LAT, 107, 27);
            E(msg, PROPERTY_EPFD, 134, 4);
            X(msg, PROPERTY_SPARE, 138, 10);
            B(msg, PROPERTY_RAIM, 148, 1);
            U(msg, PROPERTY_RADIO, 149, 19);
            break;
        case 5:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            U(msg, PROPERTY_AIS_VERSION, 38, 2);
            U(msg, PROPERTY_IMO, 40, 30);
            T(msg, PROPERTY_CALLSIGN, 70, 42);
            T(msg, PROPERTY_SHIPNAME, 112, 120);
            E(msg, PROPERTY_SHIPTYPE, 232, 8);
            U(msg, PROPERTY_TO_BOW, 240, 9);
            U(msg, PROPERTY_TO_STERN, 249, 9);
            U(msg, PROPERTY_TO_PORT, 258, 6);
            U(msg, PROPERTY_TO_STARBOARD, 264, 6);
            E(msg, PROPERTY_EPFD, 270, 4);
            U(msg, PROPERTY_MONTH, 274, 4, 0);
            U(msg, PROPERTY_DAY, 278, 5, 0);
            U(msg, PROPERTY_HOUR, 283, 5, 0);
            U(msg, PROPERTY_MINUTE, 288, 6, 0);
            U1(msg, PROPERTY_DRAUGHT, 294, 8);
            T(msg, PROPERTY_DESTINATION, 302, 120);
            B(msg, PROPERTY_DTE, 422, 1);
            X(msg, PROPERTY_SPARE, 423, 1);
            break;
        case 6:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            U(msg, PROPERTY_SEQNO, 38, 2);
            U(msg, PROPERTY_DEST_MMSI, 40, 30);
            B(msg, PROPERTY_RETRANSMIT, 70, 1);
            X(msg, PROPERTY_SPARE, 71, 1);
            U(msg, PROPERTY_DAC, 72, 10);
            U(msg, PROPERTY_FID, 82, 6);
            break;
        case 7:
    	case 13:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            X(msg, PROPERTY_SPARE, 38, 2);
            U(msg, PROPERTY_MMSI1, 40, 30);
            U(msg, PROPERTY_MMSISEQ1, 70, 2);
            U(msg, PROPERTY_MMSI2, 72, 30);
            U(msg, PROPERTY_MMSISEQ2, 102, 2);
            U(msg, PROPERTY_MMSI3, 104, 30);
            U(msg, PROPERTY_MMSISEQ3, 134, 2);
            U(msg, PROPERTY_MMSI4, 136, 30);
            U(msg, PROPERTY_MMSISEQ4, 166, 2);
            break;
        case 9:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_ALT,38,12);
            U(msg,PROPERTY_SPEED,50,10);
            B(msg,PROPERTY_ACCURACY,60,1);
            POS(msg,PROPERTY_LON,61,28);
            POS(msg,PROPERTY_LAT,89,27);
            U1(msg,PROPERTY_COURSE,116,12);
            U(msg,PROPERTY_SECOND,128,6);
            X(msg,PROPERTY_REGIONAL,134,8);
            B(msg,PROPERTY_DTE,142,1);
            B(msg,PROPERTY_ASSIGNED,146,1);
            B(msg,PROPERTY_RAIM,147,1);
            U(msg,PROPERTY_RADIO,148,20);
            break;
        case 10:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_DEST_MMSI,40,30);
            break;
        case 12:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_SEQNO,38,2);
            U(msg,PROPERTY_DEST_MMSI,40,30);
            B(msg,PROPERTY_RETRANSMIT,70,1);
            T(msg,PROPERTY_TEXT,72,936);
            break;
        case 14:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            T(msg,PROPERTY_TEXT,40,968);
            break;
        case 15:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_MMSI1,40,30);
            U(msg,PROPERTY_TYPE1_1,70,6);
            U(msg,PROPERTY_OFFSET1_1,76,12);
            U(msg,PROPERTY_TYPE1_2,90,6);
            U(msg,PROPERTY_OFFSET1_2,96,12);
            U(msg,PROPERTY_MMSI2,110,30);
            U(msg,PROPERTY_TYPE2_1,140,6);
            U(msg,PROPERTY_OFFSET2_1,146,12);
            break;
        case 16:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_MMSI1,40,30);
            U(msg,PROPERTY_OFFSET1,70,12);
            U(msg,PROPERTY_INCREMENT1,82,10);
            break;
        case 17:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            POS1(msg,PROPERTY_LON,40,18);
            POS1(msg,PROPERTY_LAT,58,17);
            D(msg,PROPERTY_DATA,80,736);
            break;
        case 18:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            X(msg, PROPERTY_RESERVED, 38, 8);
            U1(msg, PROPERTY_SPEED, 46, 10);
            B(msg, PROPERTY_ACCURACY, 56, 1);
            POS(msg, PROPERTY_LON, 57, 28);
            POS(msg, PROPERTY_LAT, 85, 27);
            U1(msg, PROPERTY_COURSE, 112, 12);
            U(msg, PROPERTY_HEADING, 124, 9);
            U(msg, PROPERTY_SECOND, 133, 6);
            U(msg, PROPERTY_REGIONAL, 139, 2);
            B(msg, PROPERTY_CS, 141, 1);
            B(msg, PROPERTY_DISPLAY, 142, 1);
            B(msg, PROPERTY_DSC, 143, 1);
            B(msg, PROPERTY_BAND, 144, 1);
            B(msg, PROPERTY_MSG22, 145, 1);
            B(msg, PROPERTY_ASSIGNED, 146, 1);
            B(msg, PROPERTY_RAIM, 147, 1);
            U(msg, PROPERTY_RADIO, 148, 20);
            break;
        case 19:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            U(msg, PROPERTY_RESERVED, 38, 8);
            U1(msg, PROPERTY_SPEED, 46, 10);
            B(msg, PROPERTY_ACCURACY, 56, 1);
            POS(msg, PROPERTY_LON, 57, 28);
            POS(msg, PROPERTY_LAT, 85, 27);
            U1(msg, PROPERTY_COURSE, 112, 12);
            U(msg, PROPERTY_HEADING, 124, 9);
            U(msg, PROPERTY_SECOND, 133, 6);
            U(msg, PROPERTY_REGIONAL, 139, 4);
            T(msg, PROPERTY_SHIPNAME, 143, 120);
            U(msg, PROPERTY_SHIPTYPE, 263, 8);
            U(msg, PROPERTY_TO_BOW, 271, 9);
            U(msg, PROPERTY_TO_STERN, 280, 9);
            U(msg, PROPERTY_TO_PORT, 289, 6);
            U(msg, PROPERTY_TO_STARBOARD, 295, 6);
            E(msg, PROPERTY_EPFD, 301, 4);
            B(msg, PROPERTY_RAIM, 305, 1);
            B(msg, PROPERTY_DTE, 306, 1);
            U(msg, PROPERTY_ASSIGNED, 307, 1);
            X(msg, PROPERTY_SPARE, 308, 4);
            break;
        case 20:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_OFFSET1,40,12);
            U(msg,PROPERTY_NUMBER1,52,4);
            U(msg,PROPERTY_TIMEOUT1,56,3);
            U(msg,PROPERTY_INCREMENT1,59,11);
            break;
        case 21:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            E(msg, PROPERTY_AID_TYPE, 38, 5);
            T(msg, PROPERTY_NAME, 43, 120);
            B(msg, PROPERTY_ACCURACY, 163, 1);
            POS(msg, PROPERTY_LON, 164, 28);
            POS(msg, PROPERTY_LAT, 192, 27);
            U(msg, PROPERTY_TO_BOW, 219, 9);
            U(msg, PROPERTY_TO_STERN, 228, 9);
            U(msg, PROPERTY_TO_PORT, 237, 6);
            U(msg, PROPERTY_TO_STARBOARD, 243, 6);
            E(msg, PROPERTY_EPFD, 249, 4);
            U(msg, PROPERTY_SECOND, 253, 6);
            B(msg, PROPERTY_OFF_POSITION, 259, 1);
            U(msg, PROPERTY_REGIONAL, 260, 8);
            B(msg, PROPERTY_RAIM, 268, 1);
            B(msg, PROPERTY_VIRTUAL_AID, 269, 1);
            B(msg, PROPERTY_ASSIGNED, 270, 1);
            break;
        case 22:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_CHANNEL_A,40,12);
            U(msg,PROPERTY_CHANNEL_B,52,12);
            U(msg,PROPERTY_TXRX,64,4);
            B(msg,PROPERTY_POWER,68,1);
            I1(msg,PROPERTY_NE_LON,69,18);
            I1(msg,PROPERTY_NE_LAT,87,17);
            I1(msg,PROPERTY_SW_LON,104,18);
            I1(msg,PROPERTY_SW_LAT,122,17);
            U(msg,PROPERTY_DEST1,69,30);
            U(msg,PROPERTY_DEST2,104,30);
            B(msg,PROPERTY_ADDRESSED,139,1);
            B(msg,PROPERTY_BAND_A,140,1);
            B(msg,PROPERTY_BAND_B,141,1);
            U(msg,PROPERTY_ZONESIZE,142,3);
            break;
        case 23:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_NE_LON,40,18);
            U(msg,PROPERTY_NE_LAT,58,17);
            U(msg,PROPERTY_SW_LON,75,18);
            U(msg,PROPERTY_SW_LAT,93,17);
            E(msg,PROPERTY_STATION_TYPE,110,4);
            E(msg,PROPERTY_SHIP_TYPE,114,8);
            U(msg,PROPERTY_TXRX,144,2);
            E(msg,PROPERTY_INTERVAL,146,4);
            U(msg,PROPERTY_QUIET,150,4);
            break;
        case 24:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            U(msg, PROPERTY_PARTNO, 38, 2);

    		if(msg.getUint(38,2) == 0)
	    	{
                T(msg, PROPERTY_SHIPNAME, 40, 120);
		    }
		    else
	    	{
                E(msg, PROPERTY_SHIPTYPE, 40, 8);
                T(msg, PROPERTY_VENDORID, 48, 18);
                U(msg, PROPERTY_MODEL, 66, 4);
                U(msg, PROPERTY_SERIAL, 70, 20);
                T(msg, PROPERTY_CALLSIGN, 90, 42);
                U(msg, PROPERTY_TO_BOW, 132, 9);
                U(msg, PROPERTY_TO_STERN, 141, 9);
                U(msg, PROPERTY_TO_PORT, 150, 6);
                U(msg, PROPERTY_TO_STARBOARD, 156, 6);
                U(msg, PROPERTY_MOTHERSHIP_MMSI, 132, 30);
            }
            break;
        case 27:
            U(msg,PROPERTY_TYPE,0,6);
            U(msg,PROPERTY_REPEAT,6,2);
            U(msg,PROPERTY_MMSI,8,30);
            U(msg,PROPERTY_ACCURACY,38,1);
            U(msg,PROPERTY_RAIM,39,1);
            U(msg,PROPERTY_STATUS,40,4);
            POS(msg,PROPERTY_LON,44,18,181000);
            POS(msg,PROPERTY_LAT,62,17,91000);
            U(msg,PROPERTY_SPEED,79,6);
            U(msg,PROPERTY_COURSE,85,9);
            U(msg,PROPERTY_GNSS,94,1);
            break;
        default:
            U(msg, PROPERTY_TYPE, 0, 6);
            U(msg, PROPERTY_REPEAT, 6, 2);
            U(msg, PROPERTY_MMSI, 8, 30);
            break;
        }
        Submit(PROPERTY_LAST, std::string(""));
    }
}
