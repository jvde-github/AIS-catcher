/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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
#include "Utilities.h"
#include "Receiver.h"

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
		if (!running)
		{

			running = true;
			terminate = false;

			run_thread = std::thread(&HTTPStreamer::process, this);
			Info() << "HTTP: start thread (" << url << "), filter: " << Util::Convert::toString(filter.isOn()) << (filter.isOn() ? ", Allowed: " + filter.getAllowed() : "");
		}
	}

	void HTTPStreamer::Stop()
	{
		if (running)
		{

			running = false;
			terminate = true;
			run_thread.join();

			Info() << "HTTP: stop thread (" << url << ").";
		}
	}

	void HTTPStreamer::post()
	{
		if (!queue.size())
			return;

		std::list<std::string> send_list;

		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			send_list.splice(send_list.begin(), queue);
		}

		msg.clear();
		HTTPResponse r;

		std::time_t now = std::time(0);

		if (protocol == PROTOCOL::AISCATCHER || protocol == PROTOCOL::AIRFRAMES)
		{

			msg += "{\n\t\"protocol\": \"" + protocol_string + "\",";
			msg += "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			msg += "\n\t\"stationid\": ";
			builder.stringify(stationid, msg);
			msg += ",";
			msg += "\n\t\"station_lat\": " + (lat == 0.0 ? "null" : std::to_string(lat)) + ",";
			msg += "\n\t\"station_lon\": " + (lon == 0.0 ? "null" : std::to_string(lon)) + ",";
			msg += "\n\t\"receiver\":\n\t\t{\n\t\t\"description\": \"AIS-catcher " VERSION "\",";
			msg += "\n\t\t\"version\": " + std::to_string(VERSION_NUMBER) + ",\n\t\t\"engine\": ";
			builder.stringify(model, msg);
			msg += ",\n\t\t\"setting\": ";
			builder.stringify(model_setting, msg);
			msg += "\n\t\t},\n\t\"device\":\n\t\t{\n\t\t\"product\": ";
			builder.stringify(product, msg);
			msg += ",\n\t\t\"vendor\": ";
			builder.stringify(vendor, msg);
			msg += ",\n\t\t\"serial\": ";
			builder.stringify(serial, msg);
			msg += ",\n\t\t\"setting\": ";
			builder.stringify(device_setting, msg);
			msg += "\n\t\t},\n\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it)
			{
				msg = msg + delim + "\n\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t]\n}\n";

			r = http.Post(msg, gzip, false, "");
		}
		else if (PROTOCOL::AIRFRAMES == protocol)
		{
			msg += "{\n\t\"app\": {\n\t\t\"name\": \"AIS-Catcher\",\n\t\t\"ver\": \"" VERSION "\"";
			msg += "\n\t},\n\t\"source\": {\n\t\t\"transport\": \"vhf\",\n\t\t\"protocol\": \"ais\",\n\t\t\"station_id\": ";
			builder.stringify(stationid, msg);
			msg += ",\n\t\t\"lat\": " + std::to_string(lat);
			msg += ",\n\t\t\"lon\": " + std::to_string(lon);
			msg += "\n\t\t},\n\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it)
			{
				msg = msg + delim + "\n\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t]\n}\n";

			r = http.Post(msg, gzip, false, "");
		}
		else if (PROTOCOL::APRS == protocol)
		{
			msg += "{\n\t\"protocol\": \"jsonais\",";
			msg += "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			msg += "\n\t\"groups\": [\n\t{\n\t\t\"path\": [{ \"name\": ";
			builder.stringify(stationid, msg);
			msg += ", \"url\" : ";
			builder.stringify(url, msg);
			msg += " }],\n\t\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it)
			{
				msg = msg + delim + "\n\t\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t\t]\n\t}]\n}";

			r = http.Post(msg, gzip, true, "jsonais");
		}
		else if (PROTOCOL::LIST == protocol)
		{

			for (auto it = send_list.begin(); it != send_list.end(); ++it)
			{
				msg += std::string(*it) + "\n";
			}

			r = http.Post(msg, gzip, false, "");
		}

		if (r.status < 200 || r.status > 299)
			Error() << "HTTP Client [" << url << "]: return code " << r.status << " msg: " << r.message;
		else if (show_response)
			Info() << "HTTP Client [" << url << "]: return code " << r.status << " msg: " << r.message;
	}

	void HTTPStreamer::process()
	{

		while (!terminate)
		{

			for (int i = 0; i < INTERVAL; i++)
			{
				SleepSystem(1000);
				if (terminate)
					break;
			}
			if (!url.empty())
				post();
		}
	}

	Setting &HTTPStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "URL")
		{
			url = arg;
			http.setURL(url);
		}
		else if (option == "USERPWD")
		{
			http.setUserPwd(arg);
			userpwd = arg;
		}
		else if (option == "STATIONID" || option == "ID" || option == "CALLSIGN")
		{
			stationid = arg;
		}
		else if (option == "INTERVAL")
		{
			INTERVAL = Util::Parse::Integer(arg, 1, 60 * 60 * 24, option);
		}
		else if (option == "TIMEOUT")
		{
			TIMEOUT = Util::Parse::Integer(arg, 1, 30, option);
		}
		else if (option == "MODEL")
		{
			model = arg;
		}
		else if (option == "MODEL_SETTING")
		{
			model_setting = arg;
		}
		else if (option == "PRODUCT")
		{
			product = arg;
		}
		else if (option == "VENDOR")
		{
			vendor = arg;
		}
		else if (option == "SERIAL")
		{
			serial = arg;
		}
		else if (option == "LAT")
		{
			lat = Util::Parse::Float(arg);
		}
		else if (option == "GROUPS_IN")
		{
			StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "LON")
		{
			lon = Util::Parse::Float(arg);
		}
		else if (option == "DEVICE_SETTING")
		{
			device_setting = arg;
		}
		else
		{
			Util::Convert::toUpper(arg);

			if (option == "GZIP")
			{
				gzip = Util::Parse::Switch(arg);
				if (gzip && !zip.installed())
					throw std::runtime_error("HTTP: ZLIB not installed");
			}
			else if (option == "RESPONSE")
			{
				show_response = Util::Parse::Switch(arg);
			}
			else if (option == "PROTOCOL")
			{

				if (arg == "AISCATCHER")
				{
					builder.setMap(JSON_DICT_FULL);
					protocol_string = "jsonaiscatcher";
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "MINIMAL")
				{
					builder.setMap(JSON_DICT_MINIMAL);
					protocol_string = "jsonaiscatcher";
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "AIRFRAMES")
				{
					builder.setMap(JSON_DICT_MINIMAL);
					protocol_string = "airframes";
					protocol = PROTOCOL::AIRFRAMES;
					gzip = zip.installed();
					INTERVAL = 30;
				}
				else if (arg == "LIST")
				{
					builder.setMap(JSON_DICT_FULL);
					protocol = PROTOCOL::LIST;
				}
				else if (arg == "APRS")
				{
					builder.setMap(JSON_DICT_APRS);
					protocol = PROTOCOL::APRS;
				}
				else
					throw std::runtime_error("HTTP: error - unknown protocol");
			}
			else if (!filter.SetOption(option, arg))
			{
				throw std::runtime_error("HTTP output - unknown option: " + option);
			}
		}

		return *this;
	}

	UDPStreamer::UDPStreamer() {}

	UDPStreamer::~UDPStreamer()
	{
		Stop();
	}

	void UDPStreamer::ResetIfNeeded()
	{
		if (reset > 0)
		{
			long now = (long)std::time(nullptr);
			if ((now - last_reconnect) > 60 * reset)
			{

				Info() << "UDP: recreate socket (" << host << ":" << port << ")";

				closesocket(sock);
				sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

				if (sock == -1)
				{
					Error() << "UDP: cannot recreate socket. Requesting termination.";
					StopRequest();
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
				if (!JSON)
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

		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				for (const auto &s : data[i].NMEA)
					SendTo((s + "\r\n").c_str());
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				if (filter.include(data[i]))
					SendTo((data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, include_sample_start, tag.ipv4, uuid) + "\r\n").c_str());
			}
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
				builder.stringify(data[i], json);
				SendTo((json + "\r\n").c_str());
			}
		}
	}

	void UDPStreamer::Start()
	{
		std::stringstream ss;
		ss << "UDP: open socket for host: " << host << ", port: " << port << ", filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn())
			ss << ", allowed: {" << filter.getAllowed() << "}";
		ss << ", JSON: " << Util::Convert::toString(JSON || JSON_input) << (JSON_input ? " (FULL)" : "");
		if (broadcast)
			ss << ", broadcast: true";
		if (reset > 0)
			ss << ", reset: " << reset;
		if (!uuid.empty())
			ss << ", uuid: " << uuid;

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

#ifndef _WIN32
		int r = fcntl(sock, F_GETFL, 0);
		r = fcntl(sock, F_SETFL, r | O_NONBLOCK);

		if (r < 0)
			throw std::runtime_error("cannot make UDP socket non-blocking for " + host + " port " + port);
#else
		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode);
#endif

#ifndef _WIN32
		if (broadcast)
		{
			int broadcastEnable = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcastEnable, sizeof(broadcastEnable)) < 0)
			{
				throw std::runtime_error("UDP: cannot set broadcast option for socket.");
			}
		}
#endif

		if (reset > 0)
			last_reconnect = (long)std::time(nullptr);
	}

	void UDPStreamer::Stop()
	{
		Info() << "UDP: close socket for host: " << host << ", port: " << port;

		if (sock != -1)
		{
			closesocket(sock);
			sock = -1;
		}
		if (address != NULL)
		{
			freeaddrinfo(address);
			address = NULL;
		}
	}

	Setting &UDPStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "HOST")
		{
			host = arg;
		}
		else if (option == "PORT")
		{
			port = arg;
		}
		else if (option == "JSON")
		{
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "BROADCAST")
		{
			broadcast = Util::Parse::Switch(arg);
		}
		else if (option == "GROUPS_IN")
		{
			StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "RESET")
		{
			reset = Util::Parse::Integer(arg, 1, 24 * 60, option);
		}
		else if (option == "UUID")
		{
			if (Util::Helper::isUUID(arg))
				uuid = arg;
			else
				throw std::runtime_error("UDP: invalid UUID: " + arg);
		}
		else if (option == "INCLUDE_SAMPLE_START")
		{
			include_sample_start = Util::Parse::Switch(arg);
		}
		else if (!OutputMessage::setOption(option, arg))
		{
			throw std::runtime_error("UDP output - unknown option: " + option);
		}

		return *this;
	}

	// TCP output to server

	void TCPClientStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{

		if (!filter.includeGPS())
			return;

		if (!JSON)
		{

			for (int i = 0; i < len; i++)
			{

				if (SendTo((data[i].getNMEA() + "\r\n").c_str()) < 0)
				{
					if (!persistent)
					{
						Error() << "TCP feed: requesting termination.";
						StopRequest();
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{

				if (SendTo((data[i].getJSON() + "\r\n").c_str()) < 0)
				{
					if (!persistent)
					{
						Error() << "TCP feed: requesting termination.";
						StopRequest();
					}
				}
			}
		}
	}

	void TCPClientStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				for (const auto &s : data[i].NMEA)
				{
					if (SendTo((s + "\r\n").c_str()) < 0)
						if (!persistent)
						{
							Error() << "TCP feed: requesting termination.";
							StopRequest();
						}
				}
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				if (SendTo((data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, include_sample_start, tag.ipv4, uuid) + "\r\n").c_str()) < 0)
					if (!persistent)
					{
						Error() << "TCP feed: requesting termination.";
						StopRequest();
					}
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
				builder.stringify(data[i], json);
				if (SendTo((json + "\r\n").c_str()) < 0)
					if (!persistent)
					{
						Error() << "TCP feed: requesting termination.";
						StopRequest();
					}
			}
		}
	}

	void TCPClientStreamer::Start()
	{
		std::stringstream ss;

		ss << "TCP feed: open socket for host: " << host << ", port: " << port << ", filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn())
		{
			ss << ", gps: " << Util::Convert::toString(filter.includeGPS());
			ss << ", allowed: {" << filter.getAllowed() << "}";
		}
		ss << ", PERSIST: " << Util::Convert::toString(persistent);
		ss << ", KEEP_ALIVE: " << Util::Convert::toString(keep_alive);
		if (!uuid.empty())
			ss << ", UUID: " << uuid;

		ss << ", JSON: " << Util::Convert::toString(JSON || JSON_input) << (JSON_input ? " (FULL)" : "") << ", status: ";

		if (tcp.connect(host, port, persistent, 0, keep_alive))
			ss << "connected\n";
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
		tcp.disconnect();
	}

	Setting &TCPClientStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "HOST")
		{
			host = arg;
		}
		else if (option == "PORT")
		{
			port = arg;
		}
		else if (option == "GROUPS_IN")
		{
			StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "KEEP_ALIVE")
		{
			keep_alive = Util::Parse::Switch(arg);
		}
		else if (option == "JSON")
		{
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "PERSIST")
		{
			persistent = Util::Parse::Switch(arg);
		}
		else if (option == "UUID")
		{
			if (Util::Helper::isUUID(arg))
				uuid = arg;
			else
				throw std::runtime_error("UDP: invalid UUID: " + arg);
		}
		else if (option == "INCLUDE_SAMPLE_START")
		{
			include_sample_start = Util::Parse::Switch(arg);
		}
		else if (!OutputMessage::setOption(option, arg))
		{
			throw std::runtime_error("TCP client - unknown option: " + option);
		}
		return *this;
	}

	void TCPlistenerStreamer::Start()
	{
		std::stringstream ss;
		ss << "TCP listener: open at port " << port << ", filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn())
			ss << ", allowed: {" << filter.getAllowed() << "}";

		ss << ", JSON: " << Util::Convert::toString(JSON || JSON_input) << (JSON_input ? " (FULL)" : "") << ".";

		Info() << ss.str();
		Server::start(port);
	}

	Setting &TCPlistenerStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "PORT")
		{
			port = Util::Parse::Integer(arg, 0, 0xFFFF, option);
		}
		else if (option == "TIMEOUT")
		{
			timeout = Util::Parse::Integer(arg);
		}
		else if (option == "GROUPS_IN")
		{
			StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "JSON")
		{
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "INCLUDE_SAMPLE_START")
		{
			include_sample_start = Util::Parse::Switch(arg);
		}
		else if (!OutputMessage::setOption(option, arg))
		{
			throw std::runtime_error("TCP listener - unknown option: " + option);
		}

		return *this;
	}

	void TCPlistenerStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (!filter.includeGPS())
			return;

		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				SendAll(data[i].getNMEA() + "\r\n");
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				SendAll((data[i].getJSON() + "\r\n").c_str());
			}
		}
	}

	void TCPlistenerStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				for (const auto &s : data[i].NMEA)
				{
					SendAll(s + "\r\n");
				}
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				SendAll((data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, include_sample_start, tag.ipv4) + "\r\n").c_str());
			}
		}
	}

	void TCPlistenerStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json);
				SendAll((json + "\r\n").c_str());
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
		ss << Util::Convert::toString(Protocol) << " feed: " << session->getHost() << ", port: " << session->getPort() << ", filter: " << Util::Convert::toString(filter.isOn());
		tcp.setValue("PERSISTENT", "on");

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

			ws.setValue("PROTOCOLS", "mqtt");
			ws.setValue("BINARY", "on");
			break;
		case PROTOCOL::WSSMQTT:

			session = tcp.add(&tls);
			session = tls.add(&ws);
			session = ws.add(&mqtt);

			ws.setValue("PROTOCOLS", "mqtt");
			ws.setValue("BINARY", "on");
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

			if (!JSON_NMEA)
			{
				for (const auto &s : data[i].NMEA)
				{
					((Protocol::MQTT *)session)->send((s + "\r\n").c_str(), s.length() + 2, topic_template.get(tag, data[0]));
				}
			}
			else
			{
				std::string s = data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, tag.ipv4) + "\r\n";
				((Protocol::MQTT *)session)->send(s.c_str(), s.length(), topic_template.get(tag, data[0]));
			}
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
				builder.stringify(data[i], json);
				json += "\r\n";
				((Protocol::MQTT *)session)->send(json.c_str(), json.length(), topic_template.get(tag, *((AIS::Message *)data[0].binary)));
			}
		}

		session->read(nullptr, 0, 0, false);
	}

	Setting &MQTTStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "URL")
		{
			std::string prot, host, port, path, username, password;
			Util::Parse::URL(arg, prot, username, password, host, port, path);

			if (!host.empty())
				Set("HOST", host);
			if (!port.empty())
				Set("PORT", port);
			if (!prot.empty())
				Set("PROTOCOL", prot);
			if (!username.empty())
				Set("USERNAME", username);
			if (!password.empty())
				Set("PASSWORD", password);
		}
		else if (option == "MSGFORMAT")
		{
			if (!Util::Parse::OutputFormat(arg, fmt))
				throw std::runtime_error("MQTT: unknown message format: " + arg);

			switch (fmt)
			{
			case MessageFormat::JSON_FULL:
				JSON_input = true;
				break;
			case MessageFormat::JSON_NMEA:
				JSON_NMEA = true;
				JSON_input = false;
				break;
			case MessageFormat::NMEA:
				JSON_input = false;
				JSON_NMEA = false;
				break;
			default:
				throw std::runtime_error("MQTT: unsupported message format");
				break;
			}
		}
		else if (option == "PROTOCOL")
		{
			if (!Util::Parse().Protocol(arg, Protocol))
				throw std::runtime_error("TCP output: unknown protocol: " + arg);

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
				throw std::runtime_error("TCO output: unsupported protocol: " + arg);
			}
		}
		else if (option == "TOPIC")
		{
			mqtt.setValue("TOPIC", arg);
			topic_template.set(arg);
		}
		else if (!tcp.setValue(option, arg) && !mqtt.setValue(option, arg) && !ws.setValue(option, arg) && !OutputMessage::setOption(option, arg))
		{
			throw std::runtime_error("MQTT output - unknown option: " + option);
		}

		return *this;
	}

	void BluetoothStreamer::Start()
	{
		std::stringstream ss;
		ss << "Bluetooth listener: auto-selecting channel, filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn())
			ss << ", allowed: {" << filter.getAllowed() << "}";

		ss << ", JSON: " << Util::Convert::toString(JSON || JSON_input) << (JSON_input ? " (FULL)" : "") << ".";

		Info() << ss.str();

		if (channel == -1)
		{
			// Auto-select available channel
			if (!Bluetooth::Server::startAuto())
			{
				throw std::runtime_error("Bluetooth listener: no available channels found");
			}
			channel = getChannel(); // Get the auto-selected channel
			Info() << "Bluetooth listener: auto-selected channel " << channel;
		}
		else
		{
			// Use specified channel
			if (!Bluetooth::Server::start(channel))
			{
				throw std::runtime_error("Bluetooth listener: cannot start on channel " + std::to_string(channel));
			}
		}
	}

	Setting &BluetoothStreamer::Set(std::string option, std::string arg)
	{
		Util::Convert::toUpper(option);

		if (option == "CHANNEL")
		{
			if (arg == "AUTO")
			{
				channel = -1; // Signal to use auto-selection
			}
			else
			{
				channel = Util::Parse::Integer(arg, 1, 30, option);
			}
		}
		else if (option == "GROUPS_IN")
		{
			StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
			StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
		}
		else if (option == "JSON")
		{
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "INCLUDE_SAMPLE_START")
		{
			include_sample_start = Util::Parse::Switch(arg);
		}
		else if (!OutputMessage::setOption(option, arg))
		{
			throw std::runtime_error("Bluetooth listener - unknown option: " + option);
		}

		return *this;
	}

	void BluetoothStreamer::Receive(const AIS::GPS *data, int len, TAG &tag)
	{
		if (!filter.includeGPS())
			return;

		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				SendAllDirect(data[i].getNMEA() + "\r\n");
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				SendAllDirect((data[i].getJSON() + "\r\n").c_str());
			}
		}
	}

	void BluetoothStreamer::Receive(const AIS::Message *data, int len, TAG &tag)
	{
		if (!JSON)
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				for (const auto &s : data[i].NMEA)
				{
					SendAll(s + "\r\n");
				}
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				if (!filter.include(data[i]))
					continue;

				SendAll((data[i].getNMEAJSON(tag.mode, tag.level, tag.ppm, tag.status, tag.hardware, tag.version, tag.driver, include_sample_start, tag.ipv4) + "\r\n").c_str());
			}
		}
	}

	void BluetoothStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		for (int i = 0; i < len; i++)
		{
			if (filter.include(*(AIS::Message *)data[i].binary))
			{
				json.clear();
				builder.stringify(data[i], json);
				SendAll((json + "\r\n").c_str());
			}
		}
	}
}
