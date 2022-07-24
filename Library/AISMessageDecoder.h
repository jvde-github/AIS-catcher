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

#pragma once

#include "Stream.h"
#include "Property.h"
#include "AIS.h"
#include "Signals.h"

namespace AIS
{
    class AISMessageDecoder : public StreamIn<Message>, public PropertyStreamOut
    {
    protected:
        void U(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            unsigned u = msg.getUint(start, len);
            if (u != undefined)
                Submit(p, u);
        }
        void E(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            unsigned u = msg.getUint(start, len);
            if (u != undefined)
                Submit(p, u);
        }
        void TURN(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            int u = msg.getInt(start, len);

            if (u == -128) Submit(p, "n/a");
            if (u == -127) Submit(p, "fastleft");
            if (u == 127) Submit(p, "fastright");
                
            Submit(p,u * u / 4.733f / 4.733f);
        }

        void U1(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            unsigned u = msg.getUint(start, len);
            if (u != undefined)
                Submit(p, u / 10.0f);
        }
        void POS(const AIS::Message& msg, int p, int start, int len, int undefined = ~0)
        {
            int u = msg.getInt(start, len);
            if (u != undefined)
                Submit(p, u / 600000.0f);
        }
        void B(const AIS::Message& msg, int p, int start, int len)
        {
            unsigned u = msg.getUint(start, len);
            Submit(p, (bool)u);
        }
        void X(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
        }
        void T(const AIS::Message& msg, int p, int start, int len)
        {
            Submit(p, msg.getText(start, len));
        }
    public:
        void Receive(const AIS::Message* data, int len, TAG& tag);
    };

    class JSONscreen : public PropertyStreamIn
    {
    protected:
        std::string json;
        bool first = true;

        std::string delim()
        {
            bool f = first;
            first = false;

            if (f) return "";
            return ",";
        }
    public:

        virtual void Set(int p, int v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + "\"" + std::to_string(v) + "\""; }
        virtual void Set(int p, unsigned v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
        virtual void Set(int p, float v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
        virtual void Set(int p, bool v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + (v ? "true" : "false"); }
        virtual void Set(int p, const std::string& v)
        {
            if (p == PROPERTY_FIRST)
            {
                first = true;
                json = "{";
            }
            else if (p == PROPERTY_LAST)
                std::cout << json << "}" << std::endl;
            else
                json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + "\"" + v + "\"";
        }
    };
}
