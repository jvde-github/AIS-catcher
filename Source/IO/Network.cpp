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

#include <cstring>

#include "AIS-catcher.h"
#include "Network.h"
#include "Convert.h"
#include "Parse.h"
#include "Helper.h"

namespace IO
{

#ifdef _WIN32
	static class WSA
	{
	public:
		WSA()
		{
			WSADATA wsaData;
			WSAStartup(MAKEWORD(2, 2), &wsaData);
		}

		~WSA() { WSACleanup(); }
	} _wsa;
#endif

	void HTTPStreamer::Start()
	{
		http.setStats(&stats);

		if (!running)
		{

			running = true;
			terminate = false;

			run_thread = std::thread(&HTTPStreamer::process, this);
			std::string filter_str = filter.Get();
			Debug() << "HTTP: start thread (" << url << ")" << (!filter_str.empty() ? ", " + filter_str : "");
		}
	}

	void HTTPStreamer::Stop()
	{
		if (running)
		{

			running = false;
			terminate = true;
			run_thread.join();

			Debug() << "HTTP: stop thread (" << url << ").";
		}
	}

	void HTTPStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{

		for (int i = 0; i < len; i++)
		{
			const AIS::Message &msg = *(AIS::Message *)data[i].binary;
			if (filter.include(msg))
			{
				const std::lock_guard<std::mutex> lock(msg_list_mutex);
				count(msg.type());

				if (protocol == PROTOCOL::NMEA)
				{
					for (const auto &nmea : msg.sentences())
						enqueue(nmea);
				}
				else
				{
					std::string s;
					builder.stringify(data[i], s);
					enqueue(std::move(s));
				}
			}
		}
	}

	void HTTPStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (!filter.includeGPS() || len <= 0)
			return;
		const std::lock_guard<std::mutex> lock(msg_list_mutex);
		lat = std::to_string(data[len - 1].getLat());
		lon = std::to_string(data[len - 1].getLon());
	}

	void HTTPStreamer::post()
	{
		// Post even when send_list is empty: the station heartbeat (lat/lon,
		// receiver info) keeps aggregators alive between message bursts.
		std::list<std::string> send_list;
		std::string lat_snap, lon_snap;
		int msg_snap, pos_snap;

		{
			const std::lock_guard<std::mutex> lock(msg_list_mutex);
			send_list.splice(send_list.begin(), msg_list);
			lat_snap = lat;
			lon_snap = lon;
			msg_snap = msg_count;
			pos_snap = pos_count;
			msg_count = pos_count = 0;
		}

		post_body.clear();
		int r;

		if (protocol == PROTOCOL::AISCATCHER || protocol == PROTOCOL::AIRFRAMES)
		{
			const std::string now = Util::Convert::toTimeStr(std::time(0));

			JSON::Writer w(post_body);
			w.beginObject()
				.kv("protocol", protocol_string)
				.kv("encodetime", now)
				.kv_raw("stationid", stationid)
				.kv_raw("station_lat", lat_snap)
				.kv_raw("station_lon", lon_snap)
				.key("receiver")
				.beginObject()
				.kv("description", "AIS-catcher " VERSION)
				.kv("version", VERSION_NUMBER)
				.kv_raw("engine", model)
				.kv_raw("setting", model_setting)
				.endObject()
				.key("device")
				.beginObject()
				.kv_raw("product", product)
				.kv_raw("vendor", vendor)
				.kv_raw("serial", serial)
				.kv_raw("setting", device_setting)
				.endObject()
				.key("msgs")
				.beginArray();
			for (const auto &m : send_list)
				w.raw_val(m);
			w.endArray().endObject().finish();

			r = http.Post(post_body, gzip, false, "");
		}
		else if (PROTOCOL::APRS == protocol)
		{
			const std::string now = Util::Convert::toTimeStr(std::time(0));

			JSON::Writer w(post_body);
			w.beginObject().kv("protocol", "jsonais").kv("encodetime", now).key("groups").beginArray().beginObject().key("path").beginArray().beginObject().kv_raw("name", stationid).kv_raw("url", url_json).endObject().endArray().key("msgs").beginArray();

			for (const auto &m : send_list)
				w.raw_val(m);

			w.endArray().endObject().endArray().endObject().finish();

			r = http.Post(post_body, gzip, true, "jsonais");
		}
		else
		{
			for (const auto &m : send_list)
			{
				post_body += m;
				post_body += '\n';
			}

			r = http.Post(post_body, gzip, false, "");
		}

		if (r < 200 || r > 299)
			Error() << "HTTP Client [" << url << "]: return code " << r << " (" << msg_snap << " messages, " << pos_snap << " positions)";
		else if (show_response)
			Info() << "HTTP Client [" << url << "]: return code " << r << " (" << msg_snap << " messages, " << pos_snap << " positions)";
	}

	void HTTPStreamer::process()
	{
		do
		{
			for (int i = 0; i < INTERVAL && !terminate; i++)
				SleepSystem(1000);
			if (!url.empty())
				post();
		} while (!terminate);
	}

	Setting &HTTPStreamer::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_URL:
			url = arg;
			url_json = JSON::Writer::escape(arg);
			http.setURL(url);
			break;
		case AIS::KEY_SETTING_USERPWD:
			http.setUserPwd(arg);
			userpwd = arg;
			break;
		case AIS::KEY_SETTING_SSL_VERIFY:
		{
			bool v = Util::Parse::Switch(arg);
			http.setVerifyCertificates(v);
			if (!v)
				Warning() << "HTTP [" << url << "]: TLS certificate verification disabled (ssl_verify off)";
			break;
		}
		case AIS::KEY_SETTING_ID:
		case AIS::KEY_SETTING_CALLSIGN:
		case AIS::KEY_SETTING_STATIONID:
			stationid = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_INTERVAL:
			INTERVAL = Util::Parse::Integer(arg, 1, 60 * 60 * 24);
			break;
		case AIS::KEY_SETTING_TIMEOUT:
			TIMEOUT = Util::Parse::Integer(arg, 1, 30);
			break;
		case AIS::KEY_SETTING_MODEL:
			model = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_MODEL_SETTING:
			model_setting = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_PRODUCT:
			product = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_VENDOR:
			vendor = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_SERIAL:
			serial = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_LAT:
			lat = std::to_string(Util::Parse::Float(arg));
			break;
		case AIS::KEY_SETTING_LON:
			lon = std::to_string(Util::Parse::Float(arg));
			break;
		case AIS::KEY_SETTING_DEVICE_SETTING:
			device_setting = JSON::Writer::escape(arg);
			break;
		case AIS::KEY_SETTING_GZIP:
			gzip = Util::Parse::Switch(arg);
			if (gzip && !zip.installed())
				throw std::runtime_error("HTTP: ZLIB not installed");
			break;
		case AIS::KEY_SETTING_RESPONSE:
			show_response = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_PROTOCOL:
		{
			std::string a = arg;
			Util::Convert::toUpper(a);

			if (a == "AISCATCHER")
			{
				protocol_string = "jsonaiscatcher";
				protocol = PROTOCOL::AISCATCHER;
			}
			else if (a == "MINIMAL")
			{
				builder.setMap(JSON_DICT_MINIMAL);
				protocol_string = "jsonaiscatcher";
				protocol = PROTOCOL::AISCATCHER;
			}
			else if (a == "AIRFRAMES")
			{
				builder.setMap(JSON_DICT_MINIMAL);
				protocol_string = "airframes";
				protocol = PROTOCOL::AIRFRAMES;
				gzip = zip.installed();
				INTERVAL = 30;
			}
			else if (a == "LIST")
			{
				protocol = PROTOCOL::LIST;
			}
			else if (a == "NMEA")
			{
				protocol = PROTOCOL::NMEA;
			}
			else if (a == "APRS")
			{
				builder.setMap(JSON_DICT_APRS);
				protocol = PROTOCOL::APRS;
			}
			else
				throw std::runtime_error("HTTP: error - unknown protocol");
			break;
		}
		default:
			if (!setOptionKey(key, arg))
				throw std::runtime_error(std::string("HTTP output - unknown option: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			break;
		}
		return *this;
	}

	UDPStreamer::~UDPStreamer()
	{
		Stop();
	}

	bool UDPStreamer::applySocketOptions()
	{
		if (!Net::setNonBlocking(sock))
			return false;
#ifndef _WIN32
		if (broadcast)
		{
			int broadcastEnable = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcastEnable, sizeof(broadcastEnable)) < 0)
				return false;
		}
#endif
		return true;
	}

	void UDPStreamer::ResetIfNeeded()
	{
		if (reset > 0)
		{
			long now = (long)std::time(nullptr);
			if ((now - last_reconnect) > 60 * reset)
			{
				Info() << "UDP: recreate socket (" << host << ":" << port << ")";

				Net::closeSocket(sock);
				sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

				if (sock == -1 || !applySocketOptions())
				{
					Critical() << "UDP: cannot recreate socket (" << host << ":" << port << "). Requesting termination.";
					Net::closeSocket(sock);
					sock = -1;
					StopRequest();
					return;
				}
				last_reconnect = now;
			}
		}
	}

	void UDPStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{

		ResetIfNeeded();

		if (sock == -1)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.includeGPS())
			{
				if (fmt != MessageFormat::JSON_NMEA)
					SendTo((data[i].getNMEA() + "\r\n").c_str());
				else
					SendTo((data[i].getJSON() + "\r\n").c_str());
			}
		}
	}

	void UDPStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{

		ResetIfNeeded();

		if (sock == -1)
			return;

		for (int i = 0; i < len; i++)
		{
			if (!filter.include(data[i]))
				continue;

			formatInto(data[i], tag, include_sample_start, uuid, "\r\n");
			SendTo(json.data(), (int)json.size());
		}
	}

	void UDPStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{

		ResetIfNeeded();

		if (sock == -1)
			return;

		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json, "\r\n");
				SendTo(json.data(), (int)json.size());
			}
		}
	}

	void UDPStreamer::Start()
	{
		std::stringstream ss;
		ss << "UDP: open socket for host: " << host << ", port: " << port;
		ss << ", msgformat: " << Util::Convert::toString(fmt);
		if (broadcast)
			ss << ", broadcast: true";
		if (reset > 0)
			ss << ", reset: " << reset;
		if (!uuid.empty())
			ss << ", uuid: " << uuid;
		std::string filter_str = filter.Get();
		if (!filter_str.empty())
			ss << ", " << filter_str;
		ss << ", " << getSourcesStr();

		Info() << ss.str();

		if (sock != -1)
		{
			throw std::runtime_error("UDP: internal error, socket already defined.");
		}

		struct addrinfo h;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);

		if (code != 0 || address == NULL)
		{
			std::string error = (const char *)gai_strerror(code);
			throw std::runtime_error("UDP network address and/or port not valid: " + host + " port " + port + " code: " + error);
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1)
		{
			throw std::runtime_error("cannot create socket for UDP " + host + " port " + port);
		}

		if (!applySocketOptions())
			throw std::runtime_error("cannot configure UDP socket for " + host + " port " + port);

		if (reset > 0)
			last_reconnect = (long)std::time(nullptr);
	}

	void UDPStreamer::Stop()
	{
		Debug() << "UDP: close socket for host: " << host << ", port: " << port;

		if (sock != -1)
		{
			Net::closeSocket(sock);
			sock = -1;
		}
		if (address != NULL)
		{
			freeaddrinfo(address);
			address = NULL;
		}
	}

	Setting &UDPStreamer::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_HOST:
			host = arg;
			break;
		case AIS::KEY_SETTING_PORT:
			port = arg;
			break;
		case AIS::KEY_SETTING_BROADCAST:
			broadcast = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_RESET:
			reset = Util::Parse::Integer(arg, 1, 24 * 60);
			break;
		case AIS::KEY_SETTING_UUID:
			if (Util::Helper::isUUID(arg))
				uuid = arg;
			else
				throw std::runtime_error("UDP: invalid UUID: " + arg);
			break;
		case AIS::KEY_SETTING_INCLUDE_SAMPLE_START:
			include_sample_start = Util::Parse::Switch(arg);
			break;
		default:
			if (!setOptionKey(key, arg))
				throw std::runtime_error(std::string("UDP output - unknown option: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			break;
		}
		return *this;
	}

	// TCP output to server

	void TCPClientStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{

		if (!filter.includeGPS())
			return;

		for (int i = 0; i < len; i++)
		{
			const std::string line = (fmt == MessageFormat::NMEA ? data[i].getNMEA() : data[i].getJSON()) + "\r\n";

			if (SendTo(line.c_str()) < 0 && !persistent && !stop_requested)
			{
				Critical() << "TCP feed: requesting termination.";
				stop_requested = true;
				StopRequest();
			}
		}
	}

	void TCPClientStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (!filter.include(data[i]))
				continue;

			formatInto(data[i], tag, include_sample_start, uuid, "\r\n");

			if (SendTo(json.data(), (int)json.size()) < 0 && !persistent && !stop_requested)
			{
				Critical() << "TCP feed: requesting termination.";
				stop_requested = true;
				StopRequest();
			}
		}
	}

	void TCPClientStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{

		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json, "\r\n");
				if (SendTo(json.data(), (int)json.size()) < 0)
					if (!persistent && !stop_requested)
					{
						Critical() << "TCP feed: requesting termination.";
						stop_requested = true;
						StopRequest();
					}
			}
		}
	}

	void TCPClientStreamer::Start()
	{
		std::stringstream ss;

		ss << "TCP feed: open socket for host: " << host << ", port: " << port;
		ss << ", persist: " << Util::Convert::toString(persistent);
		ss << ", keep_alive: " << Util::Convert::toString(keep_alive);
		if (reset > 0)
			ss << ", reset: " << reset;
		if (!uuid.empty())
			ss << ", uuid: " << uuid;

		ss << ", msgformat: " << Util::Convert::toString(fmt);
		std::string filter_str = filter.Get();
		if (!filter_str.empty())
			ss << ", " << filter_str;
		ss << ", " << getSourcesStr();
		ss << ", status: ";

		// Set up TCP connection
		tcp.setStats(&stats);
		tcp.setOptionKey(AIS::KEY_SETTING_HOST, host);
		tcp.setOptionKey(AIS::KEY_SETTING_PORT, port);
		tcp.setOptionKey(AIS::KEY_SETTING_PERSIST, Util::Convert::toString(persistent));
		tcp.setOptionKey(AIS::KEY_SETTING_TIMEOUT, "0");
		tcp.setOptionKey(AIS::KEY_SETTING_KEEP_ALIVE, Util::Convert::toString(keep_alive));
		if (reset > 0)
			tcp.setOptionKey(AIS::KEY_SETTING_RESET, std::to_string(reset));

		connection = &tcp;

		if (connection->connect())
			ss << "connected";
		else
		{
			if (!persistent)
			{
				ss << "failed";
				throw std::runtime_error("TCP feed cannot connect to " + host + " port " + port);
			}
			else
				ss << "pending";
		}

		Info() << ss.str();
	}

	void TCPClientStreamer::Stop()
	{
		if (connection)
			connection->disconnect();
	}

	Setting &TCPClientStreamer::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_HOST:
			host = arg;
			break;
		case AIS::KEY_SETTING_PORT:
			port = arg;
			break;
		case AIS::KEY_SETTING_KEEP_ALIVE:
			keep_alive = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_PERSIST:
			persistent = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_RESET:
			reset = Util::Parse::Integer(arg, 0, 3600);
			break;
		case AIS::KEY_SETTING_UUID:
			if (Util::Helper::isUUID(arg))
				uuid = arg;
			else
				throw std::runtime_error("TCP client: invalid UUID: " + arg);
			break;
		case AIS::KEY_SETTING_INCLUDE_SAMPLE_START:
			include_sample_start = Util::Parse::Switch(arg);
			break;
		default:
			if (!setOptionKey(key, arg))
				throw std::runtime_error(std::string("TCP client - unknown option: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			break;
		}
		return *this;
	}

	void TCPlistenerStreamer::Start()
	{
		std::stringstream ss;
		ss << "TCP listener: open at port " << port;
		std::string filter_str = filter.Get();
		if (!filter_str.empty())
			ss << ", " << filter_str;
		ss << ", msgformat: " << Util::Convert::toString(fmt) << ".";

		Info() << ss.str();

		TCPServer::setStats(&stats);

		if (!TCPServer::start(port))
		{
			throw std::runtime_error("TCP listener: cannot start server at port " + std::to_string(port) + ".");
		}
	}

	Setting &TCPlistenerStreamer::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_PORT:
			port = Util::Parse::Integer(arg, 0, 0xFFFF);
			break;
		case AIS::KEY_SETTING_TIMEOUT:
			timeout = Util::Parse::Integer(arg);
			break;
		case AIS::KEY_SETTING_INCLUDE_SAMPLE_START:
			include_sample_start = Util::Parse::Switch(arg);
			break;
		default:
			if (!setOptionKey(key, arg))
				throw std::runtime_error(std::string("TCP listener - unknown option: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			break;
		}
		return *this;
	}

	void TCPlistenerStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (!filter.includeGPS())
			return;

		for (int i = 0; i < len; i++)
			SendAllDirect((fmt == MessageFormat::NMEA ? data[i].getNMEA() : data[i].getJSON()) + "\r\n");
	}

	void TCPlistenerStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (!filter.include(data[i]))
				continue;

			formatInto(data[i], tag, include_sample_start, "", "\r\n");
			SendAllDirect(json.data(), (int)json.size());
		}
	}

	void TCPlistenerStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json, "\r\n");
				SendAllDirect(json.data(), (int)json.size());
			}
		}
	}

	void MQTTStreamer::Stop()
	{
		session->disconnect();
	}

	void MQTTStreamer::Start()
	{
		std::stringstream ss;
		ss << Util::Convert::toString(Protocol) << " feed: " << session->getHost() << ", port: " << session->getPort();
		ss << ", msgformat: " << Util::Convert::toString(fmt);
		std::string filter_str = filter.Get();
		if (!filter_str.empty())
			ss << ", " << filter_str;
		tcp.setOptionKey(AIS::KEY_SETTING_PERSIST, "on");
		tcp.setStats(&stats);
		tls.setStats(&stats);

		switch (Protocol)
		{

		case PROTOCOL::TLS:
			session = tcp.add(&tls);
			break;
		case PROTOCOL::MQTT:
			session = tcp.add(&mqtt);
			break;
		case PROTOCOL::MQTTS:
			session = tcp.add(&tls);
			session = tls.add(&mqtt);
			break;
		case PROTOCOL::WS:
			session = tcp.add(&ws);
			break;
		case PROTOCOL::WSMQTT:

			session = tcp.add(&ws);
			session = ws.add(&mqtt);

			ws.setOptionKey(AIS::KEY_SETTING_PROTOCOLS, "mqtt");
			ws.setOptionKey(AIS::KEY_SETTING_BINARY, "on");
			break;
		case PROTOCOL::WSSMQTT:

			session = tcp.add(&tls);
			session = tls.add(&ws);
			session = ws.add(&mqtt);

			ws.setOptionKey(AIS::KEY_SETTING_PROTOCOLS, "mqtt");
			ws.setOptionKey(AIS::KEY_SETTING_BINARY, "on");
			break;
		default:
			break;
		}

		if (!session->connect())
		{
			throw std::runtime_error("MQTT: cannot connect to " + session->getHost() + " port " + session->getPort());
		}
		Info() << ss.str();
	}

	void MQTTStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (!filter.include(data[i]))
				continue;

			formatInto(data[i], tag, false, "", "\r\n");
			topic_template.write(tag, data[i], topic_buf);
			((Protocol::MQTT *)session)->send(json.data(), (int)json.size(), topic_buf);
		}

		session->read(nullptr, 0, 0, false);
	}

	void MQTTStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json, "\r\n");
				topic_template.write(tag, *((AIS::Message *)data[i].binary), topic_buf);
				((Protocol::MQTT *)session)->send(json.data(), (int)json.size(), topic_buf);
			}
		}

		session->read(nullptr, 0, 0, false);
	}

	Setting &MQTTStreamer::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_URL:
		{
			std::string prot, host, port, path, username, password;
			Util::Parse::URL(arg, prot, username, password, host, port, path);

			if (!host.empty())
				SetKey(AIS::KEY_SETTING_HOST, host);
			if (!port.empty())
				SetKey(AIS::KEY_SETTING_PORT, port);
			if (!prot.empty())
				SetKey(AIS::KEY_SETTING_PROTOCOL, prot);
			if (!username.empty())
				SetKey(AIS::KEY_SETTING_USERNAME, username);
			if (!password.empty())
				SetKey(AIS::KEY_SETTING_PASSWORD, password);
			break;
		}
		case AIS::KEY_SETTING_PROTOCOL:
		{
			std::string a = arg;
			if (!Util::Parse().Protocol(a, Protocol))
				throw std::runtime_error("MQTT output: unknown protocol: " + arg);

			switch (Protocol)
			{
			case PROTOCOL::NONE:
			case PROTOCOL::MQTT:
			case PROTOCOL::WS:
			case PROTOCOL::WSMQTT:
			case PROTOCOL::TXT:
			case PROTOCOL::TLS:
			case PROTOCOL::TCP:
			case PROTOCOL::WSSMQTT:
			case PROTOCOL::MQTTS:
				break;
			default:
				throw std::runtime_error("MQTT output: unsupported protocol: " + arg);
			}
			break;
		}
		case AIS::KEY_SETTING_TOPIC:
			mqtt.setOptionKey(AIS::KEY_SETTING_TOPIC, arg);
			topic_template.set(arg);
			break;
		default:
			if (!tcp.setOptionKey(key, arg) && !mqtt.setOptionKey(key, arg) && !ws.setOptionKey(key, arg) && !setOptionKey(key, arg))
				throw std::runtime_error(std::string("MQTT output - unknown option: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
			break;
		}
		return *this;
	}
}
