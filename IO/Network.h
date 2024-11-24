/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
#include "TCP.h"
#include "Library/ZIP.h"
#include "HTTPServer.h"
#include "HTTPClient.h"
#include "Protocol.h"

#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"
#include "MsgOut.h"

namespace IO {

	class HTTPStreamer : public OutputJSON {

		JSON::StringBuilder builder;
		HTTPClient http;

		AIS::Filter filter;
		std::string json;

		std::thread run_thread;
		bool terminate = false, running = false;
		std::mutex queue_mutex;

		ZIP zip;
		std::string msg, url, userpwd, stationid;
		bool gzip = false, show_response = true;

		int INTERVAL = 60;
		int TIMEOUT = 10;
		float lat = 0.0, lon = 0.0;

		std::string model = "N/A", model_setting = "N/A";
		std::string product = "N/A", vendor = "N/A", serial = "N/A", device_setting = "N/A";

		enum class PROTOCOL {
			AISCATCHER,
			APRS,
			LIST,
			AIRFRAMES
		} protocol = PROTOCOL::AISCATCHER;
		std::string protocol_string = "jsonaiscatcher";

		void post();
		void process();

		void Receive(const JSON::JSON* data, int len, TAG& tag) {

			for (int i = 0; i < len; i++) {
				if (filter.include(*(AIS::Message*)data[i].binary)) {
					json.clear();
					builder.stringify(data[i], json);
					{
						const std::lock_guard<std::mutex> lock(queue_mutex);
						queue.push_back(json);
					}
				}
			}
		}

		void Receive(const AIS::GPS* data, int len, TAG& tag) {
			lat = data->getLat();
			lon = data->getLon();
		}

		std::list<std::string> queue;

	public:
		~HTTPStreamer() { Stop(); }
		HTTPStreamer() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}

		Setting& Set(std::string option, std::string arg);

		void Start();
		void Stop();
	};

	class UDPEndPoint {
		std::string address;
		std::string port;

		int sourceID = -1;

	public:
		friend class UDPStreamer;
		friend class TCPClientStreamer;

		UDPEndPoint(std::string a, std::string p, int id = -1) {
			address = a, port = p;
			sourceID = id;
		}
		int ID() { return sourceID; }
	};

	class UDPStreamer : public OutputMessage {
		SOCKET sock = -1;
		struct addrinfo* address = NULL;
		std::string host, port;
		int reset = -1;
		long last_reconnect = 0;
		bool broadcast = false;
		bool JSON = false;
		std::string uuid;

		void ResetIfNeeded();

	public:
		~UDPStreamer();
		UDPStreamer();

		Setting& Set(std::string option, std::string arg);

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void Receive(const JSON::JSON* data, int len, TAG& tag);
		void Receive(const AIS::GPS* data, int len, TAG& tag);

		void Start();
		void Start(UDPEndPoint& u) {
			host = u.address;
			port = u.port;
			Start();
		}
		void Stop();
		void SendTo(std::string str) {
			sendto(sock, str.c_str(), (int)str.length(), 0, address->ai_addr, (int)address->ai_addrlen);
		}
		void setJSON(bool b) { JSON = b; }
	};

	class TCPClientStreamer : public OutputMessage {
		::TCP::Client tcp;
		AIS::Filter filter;
		bool JSON = false;
		std::string host, port;
		bool keep_alive = false;
		bool persistent = true;
		std::string uuid;

	public:
		Setting& Set(std::string option, std::string arg);

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void Receive(const JSON::JSON* data, int len, TAG& tag);
		void Receive(const AIS::GPS* data, int len, TAG& tag);

		void Start();
		void Stop();

		int SendTo(std::string str) {

			return tcp.send(str.c_str(), (int)str.length());
		}
		void setJSON(bool b) { JSON = b; }
	};

	class TCPlistenerStreamer : public OutputMessage, public TCP::Server {
		int port = 5010;
		AIS::Filter filter;
		bool JSON = false;

	public:
		virtual ~TCPlistenerStreamer() {};

		Setting& Set(std::string option, std::string arg);

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void Receive(const JSON::JSON* data, int len, TAG& tag);
		void Receive(const AIS::GPS* data, int len, TAG& tag);

		void Start();
		void Stop() {}
	};

	class MQTTStreamer : public OutputMessage {

	private:
		Protocol::TCP tcp;
		Protocol::MQTT mqtt;
		std::string json;

	public:
		MQTTStreamer() : OutputMessage() {
			JSON_input = true;
			tcp.add(&mqtt);
		}

		void Start();
		void Stop();

		void Receive(const AIS::Message* data, int len, TAG& tag);
		void Receive(const JSON::JSON* data, int len, TAG& tag);

		Setting& Set(std::string option, std::string arg);
	};
}
