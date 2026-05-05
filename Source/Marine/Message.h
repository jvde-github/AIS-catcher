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

#include <string>
#include <time.h>
#include <chrono>
#include <cstdint>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <iostream>

#include "Convert.h"
#include "Keys.h"
#include "MessageHistory.h"

namespace AIS
{

#define MAX_AIS_BYTES 128
#define MAX_AIS_LENGTH (MAX_AIS_BYTES * 8)

	class GPS
	{
		float lat = 0, lon = 0;
		const std::string &source;
		bool isJSON;

		const std::string formatLatLon(float, bool) const;

	public:
		GPS(float lt, float ln, const std::string &src, bool json) : lat(lt), lon(ln), source(src), isJSON(json) {}

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

		uint8_t data[MAX_AIS_BYTES + 4]; // +4 padding so 5-byte reads in getText/getUint never go out of bounds
		int64_t rxtime; // microseconds since epoch
		int64_t toa; // time of arrival in seconds since epoch
		int length;
		char channel;
		long start_idx, end_idx;
		int station;
		int own_mmsi = -1;

		std::vector<std::string> NMEA;
		size_t nmea_count = 0;

	public:
		struct NMEAView
		{
			const std::string *p;
			size_t n;
			const std::string *begin() const { return p; }
			const std::string *end() const { return p + n; }
			size_t size() const { return n; }
			bool empty() const { return n == 0; }
			const std::string &operator[](size_t i) const { return p[i]; }
		};
		NMEAView sentences() const { return {NMEA.data(), nmea_count}; }

		void beginNMEA() { nmea_count = 0; }

		void pushNMEA(const char *data, size_t len)
		{
			if (nmea_count < NMEA.size())
			{
				NMEA[nmea_count].assign(data, len);
			}
			else
			{
				NMEA.emplace_back();
				NMEA.back().reserve(128);
				NMEA.back().assign(data, len);
			}
			++nmea_count;
		}
		void pushNMEA(const std::string &s) { pushNMEA(s.data(), s.size()); }

		void Stamp()
		{
			auto now = std::chrono::system_clock::now();
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
			rxtime = us.count();
		}

		int getNMEAJSON(std::string &out, const TAG &tag, bool include_ss = false, const std::string &uid = "", const char *suffix = nullptr) const;
		int getNMEATagBlock(std::string &out, const char *suffix = nullptr) const;
		int getBinaryNMEA(std::string &out, const TAG &tag, bool crc = false) const;

		std::string getRxTime() const
		{
			std::time_t t = (std::time_t)(rxtime / 1000000);
			return Util::Convert::toTimeStr(t);
		}

		std::time_t getRxTimeUnix() const
		{
			return (std::time_t)(rxtime / 1000000);
		}

		int64_t getRxTimeMicros() const
  		{
      		return rxtime;
  		}

		void setTOA(int64_t t)
		{
			toa = t;
		}

		int64_t getTOA() const
		{
			return toa;
		}

		void setRxTimeUnix(std::time_t t)
		{
			rxtime = (int64_t)t * 1000000;
		}

		void setRxTimeMicros(int64_t t)
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
			toa = 0;
			nmea_count = 0;
			start_idx = 0;
			end_idx = 0;
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
			if (i >= MAX_AIS_LENGTH || i < 0)
				return;

			if (b)
				data[i >> 3] |= (1 << (i & 7));
			else
				data[i >> 3] &= ~(1 << (i & 7));
		}

		bool getBit(int i) const
		{
			if (i >= MAX_AIS_LENGTH || i < 0)
				return false;

			return data[i >> 3] & (1 << (i & 7));
		}

		char getLetter(int pos) const;
		void setLetter(int pos, char c);
		void appendLetter(char c) { setLetter(length / 6, c); }
		void appendPayload(const char *src, int count);
		void getPayload(char *dst, int pos, int count) const;
		void reduceLength(int l) { length = MAX(length - l, 0); }

		void setLength(int l)
		{
			if (l >= 0 && l <= MAX_AIS_LENGTH)
				length = l;
		}
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

		uint64_t getHash() const
		{
			uint64_t hash = 0;

			// Bits 0-29: MMSI (30 bits)
			hash |= (uint64_t)(mmsi() & 0x3FFFFFFF);

			// Bit 30: Channel (1 bit)
			hash |= ((uint64_t)(channel == 'B' ? 1 : 0)) << 30;

			// Bits 31-35: Type (5 bits)
			hash |= ((uint64_t)(type() & 0x1F)) << 31;

			// Bits 36-63: Data content hash (28 bits)
			// Simple FNV-1a-like hash of message data

			uint32_t data_hash = 2166136261u; // FNV offset basis
			int bytes = (length + 7) / 8;	  // Convert bits to bytes

			for (int i = 0; i < bytes && i < MAX_AIS_BYTES; i++)
			{
				data_hash ^= data[i];
				data_hash *= 16777619u; // FNV prime
			}
			hash |= ((uint64_t)(data_hash & 0x0FFFFFFF)) << 36;

			return hash;
		}
	};

	class Filter
	{
		static const uint32_t all = 0xFFFFFFFF;
		uint32_t allow = all;
		uint32_t allow_repeat = all;
		bool on = false;
		bool GPS = true, AIS = true;
		std::vector<int> ID_allowed;
		std::vector<int> MMSI_allowed;
		std::vector<int> MMSI_blocked;
		std::string allowed_channels;

		long int last_VDO = 0;

		unsigned position_interval = 0;
		unsigned unique_interval = 0;
		unsigned own_interval = 0;

		// Position downsampling per MMSI
		PositionHistory position_history;
		DuplicateHistory duplicate_history;

		bool remove_empty = false;

	public:
		virtual ~Filter() {}
		bool SetOptionKey(AIS::Keys key, const std::string &arg);
		std::string Get();
		bool isOn() { return on; }
		bool hasIDFilter() const { return !ID_allowed.empty() || !MMSI_allowed.empty(); }
		std::string getAllowed();
		bool includeGPS() { return on ? GPS : true; }
		bool include(const Message &msg);
	};
}
