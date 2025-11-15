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

#pragma once

#include <string>
#include <time.h>
#include <vector>
#include <iomanip>
#include <sstream>

#include "Utilities.h"

namespace AIS
{

#define MAX_AIS_LENGTH (128 * 8)

	class GPS
	{
		float lat = 0, lon = 0;
		const std::string &nmea;
		const std::string &json;

		const std::string formatLatLon(float, bool) const;

	public:
		GPS(float lt, float ln, const std::string &s, const std::string &j) : lat(lt), lon(ln), nmea(s), json(j) {}

		float getLat() const { return lat; }
		float getLon() const { return lon; }

		const std::string getNMEA() const;
		const std::string getJSON() const;
	};

	class Message
	{
	protected:
		const int MAX_NMEA_CHARS = 56;
		static int ID;
		std::string line = "!AIVDM,X,X,X,X," + std::string(MAX_NMEA_CHARS, '.') + ",X*XX\n\r"; // longest line

		uint8_t data[128];
		std::time_t rxtime;
		int length;
		char channel;
		long start_idx, end_idx;
		int station;
		int own_mmsi = -1;

	public:
		std::vector<std::string> NMEA;

		void Stamp(std::time_t t = (std::time_t)0L)
		{
			std::time(&rxtime);

			if ((long int)t != 0 && t < rxtime)
				setRxTimeUnix(t);
		}

		std::string getNMEAJSON(unsigned mode, float level, float ppm, int status, const std::string &hardware, int version, Type driver, bool include_ss = false,  uint32_t ipv4 = 0, const std::string &uid = "") const;
		std::string getCommunityHub() const;

		std::string getRxTime() const
		{
			return Util::Convert::toTimeStr(rxtime);
		}

		std::time_t getRxTimeUnix() const
		{
			return rxtime;
		}

		void setRxTimeUnix(std::time_t t)
		{
			rxtime = t;
		}

		void setStartIdx(long s)
		{
			start_idx = s;
		}

		void setEndIdx(long e)
		{
			end_idx = e;
		}

		void clear()
		{
			length = 0;
			NMEA.resize(0);
			std::memset(data, 0, 128);
		}

		bool validate();

		unsigned type() const
		{
			return data[0] >> 2;
		}

		unsigned repeat() const
		{
			return data[0] & 3;
		}

		unsigned mmsi() const
		{
			return (data[1] << 22) | (data[2] << 14) | (data[3] << 6) | (data[4] >> 2);
		}

		unsigned getUint(int start, int len) const;
		int getInt(int start, int len) const;
		void getText(int start, int len, std::string &str) const;
		void setText(int start, int len, const char *str);

		bool setUint(int start, int len, unsigned val);
		bool setInt(int start, int len, int val);

		void setBit(int i, bool b)
		{
			if (b)
				data[i >> 3] |= (1 << (i & 7));
			else
				data[i >> 3] &= ~(1 << (i & 7));
		}

		bool getBit(int i) const
		{
			return data[i >> 3] & (1 << (i & 7));
		}

		char getLetter(int pos) const;
		void setLetter(int pos, char c);
		void appendLetter(char c) { setLetter(length / 6, c); }
		void reduceLength(int l) { length -= l; }

		void setLength(int l) { length = l; }
		int getLength() const { return length; }

		void setChannel(char c) { channel = c; }
		char getChannel() const { return channel; }

		void setOrigin(char c, int s, int o)
		{
			channel = c;
			station = s;
			own_mmsi = o;
		}
		int getStation() const { return station; }
		void setOwnMMSI(int m) { own_mmsi = m; }
		void buildNMEA(TAG &tag, int id = -1);
		bool isOwn() const { return own_mmsi == mmsi(); }
	};

	class Filter : public Setting
	{
		const uint32_t all = 0xFFFFFFFF;
		uint32_t allow = all;
		uint32_t allow_repeat = all;
		bool on = false;
		bool downsample = false;
		bool GPS = true, AIS = true;
		std::vector<int> ID_allowed;
		std::vector<int> MMSI_allowed;
		std::vector<int> MMSI_blocked;
		std::string allowed_channels;
		int downsample_time = 10;
		long int last_VDO = 0;
		bool remove_empty = false;

	public:
		virtual ~Filter() {}
		bool SetOption(std::string option, std::string arg);
		bool isOn() { return on; }
		std::string getAllowed();
		bool includeGPS() { return on ? GPS : true; }
		bool include(const Message &msg);
	};
}
