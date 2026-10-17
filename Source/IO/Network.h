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
#include <ctime>
#include <list>
#include <thread>
#include <mutex>

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

		std::string url, url_json;
		bool gzip = false, show_response = true;
		int INTERVAL = 60;
		int TIMEOUT = 10;

		std::string stationid, lat, lon;
		std::string model, model_setting;
		std::string product, vendor, serial, device_setting;

		enum class UploadProtocol
		{
			AISCATCHER,
			APRS,
			LIST,
			AIRFRAMES,
			NMEA
		} protocol = UploadProtocol::AISCATCHER;

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
		int msg_count = 0, pos_count = 0, dropped_count = 0;

		static const size_t MSG_LIST_MAX = 100000;
		bool dropped_warned = false;

		// caller holds msg_list_mutex
		void enqueue(std::string s)
		{
			if (msg_list.size() >= MSG_LIST_MAX)
			{
				msg_list.pop_front();
				stats.dropped++;
				dropped_count++;
			}
			msg_list.push_back(std::move(s));
		}

		// caller holds msg_list_mutex
		void count(const AIS::Message &msg)
		{
			msg_count++;
			if (msg.isPositionReport())
				pos_count++;
		}

	public:
		~HTTPStreamer() { Stop(); }
		HTTPStreamer() : OutputMessage("HTTP"), url("http://127.0.0.1") { fmt = MessageFormat::JSON_FULL; }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
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

		void ResetIfNeeded();
		bool applySocketOptions();

		bool readyToSend() override
		{
			ResetIfNeeded();
			return true;
		}

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			if (sock != -1 && sendto(sock, data, len, 0, address->ai_addr, (int)address->ai_addrlen) > 0)
				stats.bytes_out += len;
			else
				stats.dropped++;
		}

	public:
		~UDPStreamer();
		UDPStreamer() : OutputMessage("UDP")
		{
			fmt = MessageFormat::NMEA;
		}

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
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
		bool stop_requested = false;

		// sendFormatted() on the decoding thread only sends; poll() on the engine's main
		// loop makes and keeps the connection, and only tries the lock
		std::mutex mtx;
		static const int CONNECT_TIMEOUT = 10;

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::lock_guard<std::mutex> lock(mtx);
			SendTo(data, len);
		}

	public:
		TCPClientStreamer() : OutputMessage("TCP Client") { fmt = MessageFormat::NMEA; }
		~TCPClientStreamer() { Stop(); }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
		void poll() override;

		int SendTo(const char *data, int len)
		{
			if (!connection)
				return -1;

			int r = connection->send(data, len);
			// 0: not sent (socket full or connection down), -1: the connection failed
			// under it; there is no queue, so it is dropped either way
			if (r <= 0 && len > 0)
				stats.dropped++;
			return r;
		}
	};

	class HubStreamer : public OutputMessage
	{
		Protocol::TCP tcp;
		Protocol::Hub hub;
		Protocol::ProtocolBase *connection = nullptr;

		static const int KEEPALIVE_S = 300;

		// as the TCP output: sends on the decoding thread, the connection and the
		// keep-alive from poll()
		std::mutex mtx;
		Protocol::Clock::time_point last_send, last_tick;

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::lock_guard<std::mutex> lock(mtx);

			last_send = Protocol::Clock::now();
			if (connection && connection->send(data, len) <= 0 && len > 0)
				stats.dropped++;
		}

	public:
		HubStreamer();
		~HubStreamer() { Stop(); }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
		void poll() override;
	};

	class TCPlistenerStreamer : public OutputMessage, public IO::TCPServer
	{
		int port = 5010;

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			SendAll(data, len);
		}

	public:
		TCPlistenerStreamer() : OutputMessage("TCP Listener") { fmt = MessageFormat::NMEA; }

		virtual ~TCPlistenerStreamer() {};

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override {}
	};

	class MQTTStreamer : public OutputMessage
	{

	private:
		PROTOCOL protocol = PROTOCOL::MQTT;
		Protocol::TCP tcp;
		Protocol::MQTT mqtt;
		Protocol::TLS tls;
		Protocol::WebSocket ws;
		Protocol::ProtocolBase *session = &tcp;

		Util::TemplateString topic_template;
		std::string topic_buf;

		// as the TCP output: sends on the decoding thread, the connection from poll()
		std::mutex mtx;

		void sendFormatted(const char *data, int len, const AIS::Message *msg, TAG &tag) override
		{
			std::lock_guard<std::mutex> lock(mtx);

			int sent;
			if (session == &mqtt && msg)
			{
				topic_template.write(tag, *msg, topic_buf);
				sent = mqtt.send(data, len, topic_buf);
			}
			else
				sent = session->send(data, len);

			if (sent <= 0 && len > 0)
				stats.dropped++;
		}

	public:
		MQTTStreamer() : OutputMessage("MQTT"), topic_template("ais/data")
		{
			fmt = MessageFormat::JSON_FULL;
			forward_gps = false;
		}
		~MQTTStreamer() { Stop(); }

		void Start() override;
		void Stop() override;
		void poll() override;

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;
	};
}
