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

#include "SocketUtil.h"

#include "Stream.h"
#include "Common.h"
#include "TCPServer.h"
#include "Library/ZIP.h"
#include "HTTPServer.h"
#include "HTTPClient.h"
#include "Protocol.h"

#include "JSON/JSON.h"
#include "JSON/Writer.h"
#include "MsgOut.h"

namespace IO
{

	class HTTPStreamer : public OutputMessage
	{

		HTTPClient http;

		std::string post_body;
		std::thread run_thread;
		std::atomic<bool> terminate{false}, running{false};

		ZIP zip;

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

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
		void Receive(const AIS::GPS *data, int len, TAG &tag) override;

		std::list<std::string> msg_list;
		std::mutex msg_list_mutex;

		static const size_t MSG_LIST_MAX = 100000;

		// caller holds msg_list_mutex
		void enqueue(std::string s)
		{
			if (msg_list.size() >= MSG_LIST_MAX)
			{
				msg_list.pop_front();
				stats.dropped++;
			}
			msg_list.push_back(std::move(s));
		}

	public:
		~HTTPStreamer() { Stop(); }
		HTTPStreamer() : OutputMessage("HTTP"), url("http://127.0.0.1"), userpwd("") { fmt = MessageFormat::JSON_FULL; }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
	};

	class UDPEndPoint
	{
		std::string address;
		std::string port;

		int sourceID = -1;

	public:
		friend class UDPStreamer;
		friend class TCPClientStreamer;

		UDPEndPoint(const std::string &a, const std::string &p, int id = -1)
			: address(a), port(p), sourceID(id)
		{
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
		bool applySocketOptions();

	public:
		~UDPStreamer();
		UDPStreamer() : OutputMessage("UDP")
		{
			fmt = MessageFormat::NMEA;
		}

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
		void Receive(const AIS::GPS *data, int len, TAG &tag) override;

		void Start() override;
		void Start(UDPEndPoint &u)
		{
			host = u.address;
			port = u.port;
			Start();
		}
		void Stop() override;
		void SendTo(const std::string &str)
		{
			if (sendto(sock, str.c_str(), (int)str.length(), 0, address->ai_addr, (int)address->ai_addrlen) > 0)
				stats.bytes_out += str.length();
			else
				stats.dropped++;
		}
		void SendTo(const char *data, int len)
		{
			if (sendto(sock, data, len, 0, address->ai_addr, (int)address->ai_addrlen) > 0)
				stats.bytes_out += len;
			else
				stats.dropped++;
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
		int reset = -1;
		std::string uuid;
		bool include_sample_start = false;
		bool stop_requested = false;

	public:
		TCPClientStreamer() : OutputMessage("TCP Client") { fmt = MessageFormat::NMEA; }

		bool hasUUID() const override { return !uuid.empty(); }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
		void Receive(const AIS::GPS *data, int len, TAG &tag) override;

		void Start() override;
		void Stop() override;

		int SendTo(const std::string &str) { return SendTo(str.c_str(), (int)str.length()); }

		int SendTo(const char *str) { return SendTo(str, (int)strlen(str)); }

		int SendTo(const char *data, int len)
		{
			if (!connection)
				return -1;

			int r = connection->send(data, len);
			// send() returns 0 when the message could not be sent (socket would block
			// or persistent reconnect in progress); there is no queue, so it is dropped.
			if (r == 0 && len > 0)
				stats.dropped++;
			return r;
		}
	};

	class TCPlistenerStreamer : public OutputMessage, public IO::TCPServer
	{
		int port = 5010;
		bool include_sample_start = false;

	public:
		TCPlistenerStreamer() : OutputMessage("TCP Listener") { fmt = MessageFormat::NMEA; }

		virtual ~TCPlistenerStreamer() {};

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;
		void Receive(const AIS::GPS *data, int len, TAG &tag) override;

		void Start() override;
		void Stop() override {}
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

		Util::TemplateString topic_template;
		std::string topic_buf;

	public:
		MQTTStreamer() : OutputMessage("MQTT"), topic_template("ais/data")
		{
			fmt = MessageFormat::JSON_FULL;
		}

		void Start() override;
		void Stop() override;

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Receive(const AIS::Message *data, int len, TAG &tag) override;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;
	};
}
