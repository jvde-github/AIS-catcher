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
#include <list>
#include <thread>
#include <mutex>
#include <sstream>

#include "TemplateString.h"

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
#include "Common.h"
#include "TCPServer.h"
#include "Library/ZIP.h"
#include "HTTPServer.h"
#include "HTTPClient.h"
#include "Protocol.h"

#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"
#include "JSON/JSONBuilder.h"
#include "MsgOut.h"

namespace IO
{

	class HTTPStreamer : public OutputJSON
	{

		JSON::StringBuilder builder;
		HTTPClient http;

		AIS::Filter filter;
		std::string json;

		std::thread run_thread;
		bool terminate = false, running = false;

		ZIP zip;
		std::ostringstream oss;

		std::string url, url_json, userpwd;
		bool gzip = false, show_response = true;
		int INTERVAL = 60;
		int TIMEOUT = 10;

		std::string stationid = "null", lat = "null", lon = "null";
		std::string model = "null", model_setting = "null";
		std::string product = "null", vendor = "null", serial = "null", device_setting = "null";

		enum class PROTOCOL
		{
			AISCATCHER,
			APRS,
			LIST,
			AIRFRAMES,
			NMEA
		} protocol = PROTOCOL::AISCATCHER;

		std::string protocol_string = "jsonaiscatcher";

		void post();
		void process();

		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		std::list<std::string> msg_list;
		std::mutex msg_list_mutex;

	public:
		~HTTPStreamer() { Stop(); }
		HTTPStreamer() : builder(&AIS::KeyMap, JSON_DICT_FULL), url("http://127.0.0.1"), userpwd("") {}

		Setting &Set(std::string option, std::string arg);

		void Start();
		void Stop();
	};

	class UDPEndPoint
	{
		std::string address;
		std::string port;

		int sourceID = -1;

	public:
		friend class UDPStreamer;
		friend class TCPClientStreamer;

		UDPEndPoint(std::string a, std::string p, int id = -1)
		{
			address = a, port = p;
			sourceID = id;
		}
		int ID() { return sourceID; }
	};

	class UDPStreamer : public OutputMessage
	{
		SOCKET sock = -1;
		struct addrinfo *address = NULL;
		std::string host = "127.0.0.1";
		std::string port = "10110";
		int reset = -1;
		long last_reconnect = 0;
		bool broadcast = false;
		std::string uuid;
		bool include_sample_start = false;

		void ResetIfNeeded();

	public:
		~UDPStreamer();
		UDPStreamer();

		Setting &Set(std::string option, std::string arg);

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		void Start();
		void Start(UDPEndPoint &u)
		{
			host = u.address;
			port = u.port;
			Start();
		}
		void Stop();
		void SendTo(std::string str)
		{
			sendto(sock, str.c_str(), (int)str.length(), 0, address->ai_addr, (int)address->ai_addrlen);
		}
	};

	class TCPClientStreamer : public OutputMessage
	{
		Protocol::TCP tcp;
		Protocol::ProtocolBase *connection = nullptr;
		std::string host = "127.0.0.1";
		std::string port = "10110";
		bool keep_alive = false;
		bool persistent = true;
		std::string uuid;
		bool include_sample_start = false;
		unsigned long lines_sent = 0;

	public:
		TCPClientStreamer() : OutputMessage() { fmt = MessageFormat::NMEA; }

		Setting &Set(std::string option, std::string arg);

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		void Start();
		void Stop();

		int SendTo(std::string str)
		{
			if (connection)
			{
				lines_sent++;
				return connection->send(str.c_str(), (int)str.length());
			}

			return -1;
		}

		int SendTo(const char *str)
		{
			if (connection)
			{
				lines_sent++;
				return connection->send(str, strlen(str));
			}
			return -1;
		}

		bool isFirstDataSend()
		{
			return tcp.getBytesSent() == 0;
		}
	};

	class TCPlistenerStreamer : public OutputMessage, public IO::TCPServer
	{
		int port = 5010;
		// AIS::Filter filter;
		bool include_sample_start = false;

	public:
		TCPlistenerStreamer() : OutputMessage() { fmt = MessageFormat::NMEA; }

		virtual ~TCPlistenerStreamer() {};

		Setting &Set(std::string option, std::string arg);

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		void Start();
		void Stop() {}
	};

	class MQTTStreamer : public OutputMessage
	{

	private:
		PROTOCOL Protocol = PROTOCOL::MQTT;
		Protocol::TCP tcp;
		Protocol::MQTT mqtt;
		Protocol::TLS tls;
		Protocol::WebSocket ws;
		Protocol::ProtocolBase *session = &tcp;

		std::string json;
		Util::TemplateString topic_template;

	public:
		MQTTStreamer() : OutputMessage(), topic_template("ais/data")
		{
			fmt = MessageFormat::JSON_FULL;
		}

		void Start();
		void Stop();

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const JSON::JSON *data, int len, TAG &tag);

		Setting &Set(std::string option, std::string arg);
	};
}
