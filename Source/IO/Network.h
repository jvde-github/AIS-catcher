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
		bool hasConnection() const override { return true; }
		HTTPStreamer() : OutputMessage("HTTP"), url("http://127.0.0.1") { fmt = MessageFormat::JSON_FULL; }

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
	};

	// sendFormatted() on the decoding thread only sends; poll() on the engine's loop looks
	// the host up, again while that fails and every `reset` minutes
	class UDPStreamer : public OutputMessage
	{
		SOCKET sock = -1;
		std::unique_ptr<Net::AddressInfo> address;
		Net::Resolver resolver;
		Net::Clock::time_point last_lookup;
		std::mutex mtx;
		std::string host = "127.0.0.1";
		std::string port = "10110";
		int reset = -1;
		bool broadcast = false;
		bool persistent = true;

		int failures = 0;

		std::string open(std::unique_ptr<Net::AddressInfo> found);

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (sock != -1 && sendto(sock, data, len, 0, address->get()->ai_addr, (int)address->get()->ai_addrlen) > 0)
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

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override;
		void poll() override;
	};

	// an output over one connection: sendFormatted() on the decoding thread only sends,
	// poll() on the engine's loop keeps the connection up and only tries the lock
	class SessionStreamer : public OutputMessage
	{
	protected:
		Protocol::TCP tcp;
		Protocol::ProtocolBase *session = nullptr;
		std::mutex mtx;

		// under the lock; what the connection does not take now is dropped, there is no queue
		void sendLocked(const char *data, int len)
		{
			if (session && session->send(data, len) <= 0 && len > 0)
				stats.dropped++;
		}

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::lock_guard<std::mutex> lock(mtx);
			sendLocked(data, len);
		}

		// after each step of the connection, under the lock
		virtual void polled() {}

	public:
		SessionStreamer(const std::string &type) : OutputMessage(type) {}
		bool hasConnection() const override { return true; }

		void Stop() override
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (session)
				session->disconnect();
		}

		void poll() override
		{
			std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
			if (!lock.owns_lock() || !session)
				return;

			session->poll();
			if (session->isConnected())
				session->drain();
			polled();
		}
	};

	class TCPClientStreamer : public SessionStreamer
	{
		std::string host = "127.0.0.1";
		std::string port = "10110";
		bool keep_alive = false;
		bool persistent = true;
		int reset = -1;
		bool stop_requested = false;

		static const int CONNECT_TIMEOUT = 10;

		void polled() override;

	public:
		TCPClientStreamer() : SessionStreamer("TCP Client") { fmt = MessageFormat::NMEA; }
		~TCPClientStreamer() { Stop(); }

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
	};

	class HubStreamer : public SessionStreamer
	{
		Protocol::Hub hub;

		static const int KEEPALIVE_S = 300;
		Net::Clock::time_point last_send;

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::lock_guard<std::mutex> lock(mtx);
			last_send = Net::Clock::now();
			sendLocked(data, len);
		}

		void polled() override;

	public:
		HubStreamer();
		~HubStreamer() { Stop(); }

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
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
		bool hasConnection() const override { return true; }

		// the server's thread ends while this object is whole: it calls back into it
		virtual ~TCPlistenerStreamer() { Stop(); }

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;

		void Start() override;
		void Stop() override { stopThread(); }
	};

	class MQTTStreamer : public SessionStreamer
	{
		PROTOCOL protocol = PROTOCOL::MQTT;
		Protocol::MQTT mqtt;
		Protocol::TLS tls;
		Protocol::WebSocket ws;
#if defined(__linux__) && !defined(__ANDROID__)
		Protocol::SocketCAN can;
#endif
		Protocol::N2KNode node;

		Util::TemplateString topic_template;
		std::string topic_buf;

		void sendFormatted(const char *data, int len, const AIS::Message *msg, TAG &tag) override
		{
			std::lock_guard<std::mutex> lock(mtx);

			if (session == &mqtt && msg)
			{
				topic_template.write(tag, *msg, topic_buf);
				if (mqtt.send(data, len, topic_buf) <= 0 && len > 0)
					stats.dropped++;
			}
			else
				sendLocked(data, len);
		}

	public:
		MQTTStreamer(const std::string &type = "MQTT") : SessionStreamer(type), topic_template("ais/data")
		{
			session = &tcp;
			fmt = MessageFormat::JSON_FULL;
			forward_gps = false;
		}
		~MQTTStreamer() { Stop(); }

		void Start() override;

		Setting &applyKey(AIS::Keys key, const std::string &arg) override;
	};

	// the AIS messages as NMEA 2000 on a CAN interface of this machine: -Q can://<interface>
	class N2KStreamer : public MQTTStreamer
	{
	public:
		N2KStreamer() : MQTTStreamer("NMEA2000")
		{
			applyKey(AIS::KEY_SETTING_HOST, "can0");
			applyKey(AIS::KEY_SETTING_PROTOCOL, "CAN");
		}

		Setting &applyKey(AIS::Keys key, const std::string &arg) override
		{
			return MQTTStreamer::applyKey(key == AIS::KEY_SETTING_DEVICE ? AIS::KEY_SETTING_HOST : key, arg);
		}
	};
}
