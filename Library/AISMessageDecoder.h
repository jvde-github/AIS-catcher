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

            if (u == -128) Submit(p, std::string("nan") );
            else if (u == -127) Submit(p, std::string("fastleft") );
            else if (u == 127) Submit(p, std::string("fastright") );
            else
            {
		double rot = u / 4.733;
		rot = (u<0) ? -rot * rot : rot * rot;
            	Submit(p,(int)(rot+0.5));
            }
        }

        void U1(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            unsigned u = msg.getUint(start, len);
            if (u != undefined)
                Submit(p, (float) (u / 10.0) );
        }
        void I1(const AIS::Message& msg, int p, int start, int len, unsigned undefined = ~0)
        {
            int u = msg.getInt(start, len);
            if (u != undefined)
                Submit(p, (float) (u / 10.0) );
        }
        void POS(const AIS::Message& msg, int p, int start, int len, int undefined = ~0)
        {
            int u = msg.getInt(start, len);
            if (u != undefined)
                Submit(p, (float)(u / 600000.0) );
        }
        void POS1(const AIS::Message& msg, int p, int start, int len, int undefined = ~0)
        {
            int u = msg.getInt(start, len);
            if (u != undefined)
                Submit(p, (float)(u / 600.0) );
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
            std::string text = msg.getText(start, len);
            while(text[text.length()-1]==' ') text.resize(text.length()-1);
            Submit(p, text);
        }
        void D(const AIS::Message& msg, int p, int start, int len)
        {
            std::string text = msg.getText(start, len);
            while(text[text.length()-1]==' ') text.resize(text.length()-1);
            Submit(p, text);
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

	std::string jsonify(const std::string &str)
	{
		std::string out;
		for(int i = 0; i < str.length(); i++)
		{
			char c = str[i];
			if(c == '\"') out += "\\";
			out += c;
		}
		return out;
	}

    public:

        virtual void Set(int p, int v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
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
                json = json + delim() + "\"" + jsonify(PropertyDict[p]) + "\"" + ":" + "\"" + v + "\"";
        }
    };
}
