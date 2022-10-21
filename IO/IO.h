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
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET		int
#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Stream.h"
#include "AIS.h"
#include "Property.h"
#include "TCP.h"

namespace IO {
	template <typename T>
	class StreamCounter : public StreamIn<T> {
		uint64_t count = 0;
		uint64_t lastcount = 0;
		float rate = 0.0f;

		high_resolution_clock::time_point time_lastupdate;
		float seconds = 0;
		int msg_count = 0;

	public:
		StreamCounter() : StreamIn<T>() {
			resetStatistic();
		}

		void Receive(const T* data, int len, TAG& tag) {
			count += len;
		}

		uint64_t getCount() { return count; }

		void Stamp() {
			auto timeNow = high_resolution_clock::now();
			float seconds = 1e-6f * duration_cast<microseconds>(timeNow - time_lastupdate).count();

			msg_count = count - lastcount;
			rate += 1.0f * (msg_count / seconds - rate);

			time_lastupdate = timeNow;
			lastcount = count;
		}

		float getRate() { return rate; }

		int getDeltaCount() { return msg_count; }

		void resetStatistic() {
			count = 0;
			time_lastupdate = high_resolution_clock::now();
		}
	};

	template <typename T>
	class SinkFile : public StreamIn<T> {
		std::ofstream file;
		std::string filename;

	public:
		~SinkFile() {
			if (file.is_open())
				file.close();
		}
		void openFile(std::string fn) {
			filename = fn;
			file.open(filename, std::ios::out | std::ios::binary);
		}

		void Receive(const T* data, int len, TAG& tag) {
			file.write((char*)data, len * sizeof(T));
		}
	};

	class SinkScreenString : public StreamIn<std::string> {
	public:
		void Receive(const std::string* data, int len, TAG& tag) {
			for (int i = 0; i < len; i++)
				std::cout << data[i] << std::endl;
		}
	};

	class SinkScreenMessage : public StreamIn<AIS::Message> {
	private:
		OutputLevel level;

	public:
		void setDetail(OutputLevel l) { level = l; }

		void Receive(const AIS::Message* data, int len, TAG& tag) {
			if (level == OutputLevel::NONE) return;

			for (int i = 0; i < len; i++) {
				if (level == OutputLevel::FULL || level == OutputLevel::SPARSE)
					for (auto s : data[i].sentence) {
						std::cout << s;

						if (level == OutputLevel::FULL) {
							std::cout << " ( MSG: " << data[i].type() << ", REPEAT: " << data[i].repeat() << ", MMSI: " << data[i].mmsi();
							if (tag.mode & 1) std::cout << ", signalpower: " << tag.level << ", ppm: " << tag.ppm;
							if (tag.mode & 2) {
								std::cout << ", timestamp: " << data[i].getRxTime();
							}
							std::cout << ")";
						}
						std::cout << std::endl;
					}
				else if (level == OutputLevel::JSON_NMEA) {
					std::cout << "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"channel\":\"" << data[i].channel << "\"";
					if (tag.mode & 2) {
						std::cout << ",\"rxtime\":\"" << data[i].getRxTime() << "\"";
					}
					if (tag.mode & 1) std::cout << ",\"signalpower\":" << tag.level << ",\"ppm\":" << tag.ppm;
					std::cout << ",\"mmsi\":" << data[i].mmsi() << ",\"type\":" << data[i].type()
							  << ",\"nmea\":[\"" << data[i].sentence[0] << "\"";

					for (int j = 1; j < data[i].sentence.size(); j++)
						std::cout << ",\"" << data[i].sentence[j] << "\"";
					std::cout << "]}" << std::endl;
				}
			}
		}
	};

	class UDPEndPoint {
		std::string address;
		std::string port;

		int sourceID = -1;

	public:
		friend class UDP;
		friend class TCP;

		UDPEndPoint(std::string a, std::string p, int id = -1) {
			address = a, port = p;
			sourceID = id;
		}
		int ID() { return sourceID; }
	};

	class UDP : public StreamIn<AIS::Message> {
		SOCKET sock = -1;
		struct addrinfo* address = NULL;

	public:
		~UDP();
		UDP();

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void openConnection(const std::string& host, const std::string& port);
		void openConnection(UDPEndPoint& u) { openConnection(u.address, u.port); }
		void closeConnection();
	};

	class TCP : public StreamIn<AIS::Message> {
		::TCP::Client con;

	public:
		~TCP();
		TCP();

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void openConnection(const std::string& host, const std::string& port);
		void openConnection(UDPEndPoint& u) { openConnection(u.address, u.port); }
		void closeConnection();
	};

	class PropertyToJSON : public PropertyStreamIn, public StreamOut<std::string> {
	protected:
		std::string json;
		bool first = true;

		std::string delim() {
			bool f = first;
			first = false;

			if (f) return "";
			return ",";
		}

		std::string jsonify(const std::string& str) {
			std::string out;
			for (int i = 0; i < str.length(); i++) {
				char c = str[i];
				if (c == '\"') out += "\\";
				out += c;
			}
			return out;
		}

	public:
		virtual void Set(int p, int v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, unsigned v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, float v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + std::to_string(v); }
		virtual void Set(int p, bool v) { json = json + delim() + "\"" + PropertyDict[p] + "\"" + ":" + (v ? "true" : "false"); }

		virtual void Set(int p, const std::string& v) {
			if (p == PROPERTY_OBJECT_START) {
				first = true;
				json = "{";
				json.reserve(2048);
			}
			else if (p == PROPERTY_OBJECT_END) {
				if (json != "{") {
					TAG tag;
					json += "}";
					Send(&json, 1, tag);
				}
			}
			else
				json = json + delim() + "\"" + PropertyDict[p] + "\":\"" + jsonify(v) + "\"";
		}
		virtual void Set(int p, const std::vector<std::string>& v) {
			json += delim() + "\"" + PropertyDict[p] + "\":[\"" + jsonify(v[0]) + "\"";
			for (int i = 1; i < v.size(); i++)
				json += ",\"" + jsonify(v[i]) + "\"";
			json += "]";
		}
	};
}
