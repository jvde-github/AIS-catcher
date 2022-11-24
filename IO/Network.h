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
#include <list>
#include <thread>
#include <mutex>

#ifdef HASCURL
#include <curl/curl.h>
#endif

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
#include "Common.h"
#include "AIS.h"
#include "JSON/JSON.h"
#include "TCP.h"
#include "Utilities.h"
#include "ZIP.h"

namespace IO {

	class HTTP : public StreamIn<JSON::JSON>, public Setting {

		int source = -1;

#ifdef HASCURL
		AIS::Filter filter;
		JSON::StringBuilder builder;
		std::string json;

		std::thread run_thread;
		bool terminate = false, running = false;
		std::mutex queue_mutex;

		ZIP zip;
		std::string msg, url, userpwd, stationid;
		bool gzip = false, show_response = true;

		int INTERVAL = 60;
		int TIMEOUT = 10;

		std::string model, model_setting;
		std::string product, vendor, serial, device_setting;

		char response[1024];

		enum class PROTOCOL{ AISCATCHER, APRS, LIST } protocol = PROTOCOL::AISCATCHER;

		static size_t curl_cb(char* contents, size_t size, size_t nmemb, char* s);

		void send(const std::string&, const std::string&);
		void post();
		void process();

		void Receive(const JSON::JSON* data, int len, TAG& tag) {
			for (int i = 0; i < len; i++) {
				if (filter.include(*(AIS::Message*)data[i].binary)) {
					json.clear();
					builder.build(data[i], json);
					{
						const std::lock_guard<std::mutex> lock(queue_mutex);
						queue.push_back(json);
					}
				}
			}
		}

		std::list<std::string> queue;

	public:
		~HTTP() {
			Stop();
		}
#endif
	public:
		virtual void Set(std::string option, std::string arg);

		void Start();
		void Stop();

		void setSource(int s) { source = s; }
		int getSource() { return source; }
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

	class UDP : public StreamIn<AIS::Message>, public Setting {
		SOCKET sock = -1;
		struct addrinfo* address = NULL;
		int source = -1;
		std::string host, port;

		AIS::Filter filter;

	public:
		~UDP();
		UDP();

		virtual void Set(std::string option, std::string arg);

		void Receive(const AIS::Message* data, int len, TAG& tag);

		void Start();
		void Start(UDPEndPoint& u) {
			host = u.address;
			port = u.port;
			Start();
		}
		void Stop();

		void setSource(int s) { source = s; }
		int getSource() { return source; }
	};
}
