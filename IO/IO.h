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
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET int
#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Stream.h"

namespace IO
{
	template<typename T>
	class StreamCounter : public StreamIn<T>
	{
		uint64_t count = 0;
		uint64_t lastcount = 0;
		float rate = 0.0f;

		high_resolution_clock::time_point time_lastupdate;

	public:

		StreamCounter() : StreamIn<T>()
		{
			resetStatistic();
		}

		void Receive(const T* data, int len, TAG& tag)
		{
			count += len;
		}

		uint64_t getCount() { return count; }

		float getRate()
		{
			auto timeNow = high_resolution_clock::now();
			float seconds = 1e-6f * duration_cast<microseconds>(timeNow - time_lastupdate).count();

			rate += 0.5f * ((count - lastcount) / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;

			return ((int)(10 * rate + 0.5f)) / 10.0f;
		}

		void resetStatistic()
		{
			count = 0;
			time_lastupdate = high_resolution_clock::now();
		}
	};

	template <typename T>
	class SinkFile : public StreamIn<T>
	{
		std::ofstream file;
		std::string filename;

	public:

		~SinkFile()
		{
			if (file.is_open())
				file.close();
		}
		void openFile(std::string fn)
		{
			filename = fn;
			file.open(filename, std::ios::out | std::ios::binary);
		}

		void Receive(const T* data, int len, TAG& tag)
		{
			file.write((char*)data, len * sizeof(T));
		}
	};


	class SinkScreen : public StreamIn<NMEA>
	{
	public:
		enum class Level { NONE, SPARSE, FULL, JSON_NMEA };
	private:
		Level level;
	public:
		void setDetail(Level l) { level = l; }

		void Receive(const NMEA* data, int len, TAG& tag)
		{
			if(level == Level::NONE) return;

			for (int i = 0; i < len; i++)
			{
				if(level == Level::FULL || level == Level::SPARSE)
					for (auto s : data[i].sentence)
					{
						std::cout << s;

						if (level == Level::FULL)
							std::cout << " ( MSG: " << data[i].msg << ", REPEAT: " << data[i].repeat << ", MMSI: " << data[i].mmsi
							          << ", lvl: " << tag.level << ", ppm: " << tag.ppm
							          << ")";
						std::cout << std::endl;
					}
				else if(level == Level::JSON_NMEA)
				{
					std::cout << "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"channel\":\"" << data[i].channel << "\""
					          << ",\"power\":" << tag.level << ",\"ppm\":" << tag.ppm
					          << ",\"mmsi\":" << data[i].mmsi << ",\"type\":" << data[i].msg  << ",\"repeat\":"<<data[i].repeat
					          << ",\"NMEA\":[\"" << data[i].sentence[0] << "\"";

					for(int j = 1; j < data[i].sentence.size(); j++)
						std::cout << ",\"" << data[i].sentence[j] << "\"";
					std::cout << "]}" << std::endl;
				}
			}
		}
	};

	class UDPEndPoint
	{
		std::string address;
		std::string port;

		int sourceID = -1;
	public:

		friend class UDP;

		UDPEndPoint(std::string a, std::string p, int id = -1) { address = a, port = p; sourceID = id; }
		int ID() { return sourceID; }
	};

	class UDP : public StreamIn<NMEA>
	{
		SOCKET sock = -1;
		struct addrinfo* address = NULL;

	public:
		~UDP();
		UDP();

		void Receive(const NMEA* data, int len, TAG& tag);
		void openConnection(const std::string& host, const std::string& port);
		void openConnection(UDPEndPoint& u) { openConnection(u.address, u.port); }
        void closeConnection();
	};
}
