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

#include "ADSB.h"

namespace Plane
{
  
    void ADSB::Callsign()
    {
        static const char *cs_table = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ###############0123456789######";
        char temp[9] = {0};
        int len = 0;

        for (int i = 0; i < 8; i++)
        {
            uint32_t c = getBits( 40 + (i * 6), 6);
            if (cs_table[c] != '#')
            {
                temp[len++] = cs_table[c];
            }
        }

        if (len > 0)
        {
            std::strncpy(callsign, temp, sizeof(callsign) - 1);
            callsign[sizeof(callsign) - 1] = '\0';
        }
    }

    void ADSB::Decode()
    {
        if(msgtype == '1') return;

         df = getBits(0, 5);

        switch (df)
        {
        case 0:  // Short Air-Air Surveillance
        case 4:  // Surveillance, Altitude Reply
        case 20: // Comm-B, Altitude Reply
        {
            //unsigned ac = getBits( 19, 13);
            //if (ac)
            //    altitude = decodeESAlt(ac);
        }
        break;

        case 11: // All-Call Reply
            hexident = getBits(8, 24);
            break;

        case 17: // Extended Squitter
        case 18: // Extended Squitter/Supplementary
        {
            hexident = getBits( 8, 24);
            msgtype = getBits( 32, 5);

            switch (msgtype)
            {
            case 1:
            case 2:
            case 3:
            case 4: // Aircraft Identification
                Callsign();
                break;

            case 19: // Airborne Velocity
                //decodeVelocity();
                break;

            case 5:
            case 6:
            case 7:
            case 8: // Surface Position
            case 9:
            case 10:
            case 11:
            case 12: // Airborne Position
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
                if (msgtype >= 9)
                {
                    //unsigned ac = getBits( 37, 12);
                    //if (ac)
                    //    altitude = decodeESAlt(ac);
                }
                //Position( getBits(msgData, 54, 1));
                break;
            }
        }
        break;
        }

    }
}