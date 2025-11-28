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

#pragma once

#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <functional>
#include <cstring>
#include <iomanip>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define SOCKET int
#define SOCKADDR struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#ifdef HASOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "Parse.h"
#include "Convert.h"
#include "Common.h"

namespace Protocol
{
	class AddressInfo
	{
	public:
		AddressInfo(const std::string &host, const std::string &service, const struct addrinfo *hints = nullptr) : address(nullptr), msg("")
		{
			if (address != nullptr)
			{
				freeaddrinfo(address);
				address = nullptr;
			}

			int result = getaddrinfo(host.c_str(), service.c_str(), hints, &address);
			if (result != 0)
			{
#ifdef _WIN32
				msg = std::to_string(result);
#else
				msg = gai_strerror(result);
#endif
			}
		}

		~AddressInfo()
		{
			if (address != nullptr)
			{
				freeaddrinfo(address);
			}
		}

		struct addrinfo *get() const
		{
			return address;
		}

		bool isSuccess() const
		{
			return address != nullptr;
		}

		std::string getErrorMessage() const
		{
			return msg;
		}

	private:
		struct addrinfo *address;
		std::string msg;
	};

	class ProtocolBase
	{

	protected:
		ProtocolBase *prev = nullptr;
		ProtocolBase *next = nullptr;

		std::string layer = "*";

	public:
		ProtocolBase(std::string l) : layer(l) {};
		virtual ~ProtocolBase() {}

		ProtocolBase *getPrev() { return prev; }
		ProtocolBase *getNext() { return next; }

		std::string getLayer() { return layer; }

		virtual void disconnect()
		{
			if (prev)
				prev->disconnect();
		}

		virtual bool connect()
		{

			if (prev)
				return prev->connect();

			return false;
		}

		virtual bool isConnected()
		{
			if (prev)
				return prev->isConnected();

			return false;
		}

		virtual std::string getHost()
		{
			if (prev)
				return prev->getHost();

			return "";
		}

		virtual std::string getPort()
		{
			if (prev)
				return prev->getPort();

			return "";
		}

		virtual SOCKET getSocket()
		{
			if (prev)
				return prev->getSocket();

			return -1;
		}

		virtual int read(void *data, int length, int t = 1, bool wait = false)
		{
			if (prev)
				return prev->read(data, length, t, wait);

			return -1;
		}

		virtual int send(const void *data, int length)
		{
			if (prev)
				return prev->send(data, length);

			return -1;
		}

		virtual void onConnect()
		{
			if (next)
				next->onConnect();
		}

		virtual void onDisconnect()
		{
			if (next)
				next->onDisconnect();
		}

		ProtocolBase *add(ProtocolBase *p)
		{
			next = p;
			p->prev = this;
			p->next = nullptr;

			return p;
		}

		virtual bool setValue(const std::string &key, const std::string &value) { return false; }
		virtual std::string getValues() { return ""; }
	};

	class TCP : public ProtocolBase
	{
		const int RECONNECT_TIME = 10;

	public:
		enum State
		{
			DISCONNECTED,
			CONNECTING,
			READY
		};

		TCP() : ProtocolBase("TCP") {};
		void disconnect() override;
		bool connect() override;

		int read(void *data, int length, int t, bool wait) override;
		int send(const void *data, int length) override;

		bool isConnected() override
		{
			if (state == READY)
				return true;

			updateState();
			return state == READY;
		}

		bool setValue(const std::string &key, const std::string &value) override
		{
			if (key == "HOST")
				host = value;
			else if (key == "PORT")
				port = value;
			else if (key == "RESET")
				reset_time = Util::Parse::Integer(value, 0, 3600, key);
			else if (key == "PERSISTENT")
				persistent = Util::Parse::Switch(value);
			else if (key == "TIMEOUT")
				timeout = std::stoi(value);
			else if (key == "KEEP_ALIVE")
				keep_alive = Util::Parse::Switch(value);
			else
				return false;

			return true;
		}

		std::string getValues() override
		{
			return "host " + host + " port " + port + " timeout " + std::to_string(timeout) + " persist " + Util::Convert::toString(persistent) + " keep_alive " + Util::Convert::toString(keep_alive);
		}

		std::string getHost() override
		{
			return host;
		}

		std::string getPort() override
		{
			return port;
		}

		SOCKET getSocket() override
		{
			return sock;
		}

		enum State getState() { return state; }

	private:
		std::string host;
		std::string port;

		bool persistent = true;
		int reset_time = -1;
		int timeout = 0;
		bool keep_alive = true;

		int sock = -1;
		State state = DISCONNECTED;
		time_t stamp = 0;

		void updateState();
		bool isConnected(int t);

		bool reconnect()
		{
			std::cerr << "TCP: Reconnecting to " << host << ":" << port << std::endl;
			disconnect();
			return connect();
		}

		int handleNetworkError(const char *operation, int error_code, int partial_bytes_processed)
		{
			Error() << "TCP (" << host << ":" << port << "): " << operation << " error " << error_code
					<< " (" << strerror(error_code) << ")." << (persistent ? " Reconnecting." : " Failed.");

			// if (persistent)
			//	reconnect();
			disconnect();
			return partial_bytes_processed > 0 ? partial_bytes_processed : (persistent ? 0 : -1);
		}
	};

#ifdef HASOPENSSL

	class TLS : public ProtocolBase
	{
	private:
		SSL_CTX *ctx = nullptr;
		SSL *ssl = nullptr;
		bool handshake_complete = false;

		static std::once_flag ssl_init_flag;

		bool verify_certificates = true;

		enum TLS_STATE
		{
			TLS_DISCONNECTED,
			TLS_HANDSHAKING,
			TLS_CONNECTED
		} tls_state = TLS_DISCONNECTED;

	public:
		TLS() : ProtocolBase("TLS")
		{
			initializeSSL();
		}

		~TLS()
		{
			disconnect();
			// OpenSSL cleanup handled by OS at process exit
		}

		static void initializeSSL();

		void onConnect() override;
		void onDisconnect() override;
		bool isConnected() override;
		int send(const void *data, int length) override;

		int read(void *data, int length, int timeout = 0, bool wait = false) override;
		void updateState();

	private:
		bool performHandshake();
		std::string getSSLErrorString(int error);
	};
#else
	class TLS : public ProtocolBase
	{
	public:
		TLS() : ProtocolBase("TLS") {};

		bool connect() override
		{
			Error() << "TLS: OpenSSL support not available. Cannot connect.";
			return false;
		}
	};
#endif

	// http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718023

	class MQTT : public ProtocolBase
	{
		std::vector<uint8_t> packet, buffer;
		int buffer_ptr = 0;

		enum class PacketType
		{
			RESERVED_0 = 0 << 4,   // 0
			CONNECT = 1 << 4,	   // 16
			CONNACK = 2 << 4,	   // 32
			PUBLISH = 3 << 4,	   // 48
			PUBACK = 4 << 4,	   // 64
			PUBREC = 5 << 4,	   // 80
			PUBREL = 6 << 4,	   // 96
			PUBCOMP = 7 << 4,	   // 112
			SUBSCRIBE = 8 << 4,	   // 128
			SUBACK = 9 << 4,	   // 144
			UNSUBSCRIBE = 10 << 4, // 160
			UNSUBACK = 11 << 4,	   // 176
			PINGREQ = 12 << 4,	   // 192
			PINGRESP = 13 << 4,	   // 208
			DISCONNECT = 14 << 4,  // 224
			RESERVED_15 = 15 << 4  // 240
		};

		std::string username, password;
		std::string topic = "ais/data";
		std::string client_id;

		int qos = 0;
		int packet_id = 1;

		bool connected = false;
		bool subscribe = false;

		void pushVariableLength(int length);
		void pushByte(uint8_t byte);
		void pushInt(int length);
		void createPacket(PacketType type, uint8_t flags);
		void pushString(const std::string &str);
		void subscribePacket();
		void connectPacket();
		int readRemainingLength();
		void performHandshake();
		bool readPacket(uint8_t &type, int &length);

	public:
		MQTT() : ProtocolBase("MQTT") {};
		void onConnect() override;
		void onDisconnect() override;
		bool setValue(const std::string &key, const std::string &value) override;
		bool isConnected() override;
		int send(const void *str, int length, const std::string &tpc);
		int send(const void *str, int length) override;
		int read(void *data, int data_len, int t = 1, bool wait = false) override;

		std::string getValues() override;
	};

	class GPSD : public ProtocolBase
	{
		void onConnect() override
		{
			const std::string str = "?WATCH={\"enable\":true,\"json\":true,\"nmea\":false}\n";
			if (prev)
			{
				int len = prev->send(str.c_str(), str.size());
				if (len != str.size())
				{
					Error() << "GPSD: no or invalid response, likely not a gpsd server.";
					disconnect();
				}
			}

			ProtocolBase::onConnect();
		}

		void onDisconnect() override
		{
			ProtocolBase::onDisconnect();

			if (prev)
			{
				const std::string str = "?WATCH={\"enable\":false}\n";
				prev->send(str.c_str(), str.size());
			}
		}

	public:
		GPSD() : ProtocolBase("GPSD") {};
	};

	class RTLTCP : public ProtocolBase
	{

	protected:
		bool connected = false;

		void sendParam(uint8_t c, uint32_t p)
		{
			char instruction[5];

			instruction[0] = c;
			instruction[4] = p;
			instruction[3] = p >> 8;
			instruction[2] = p >> 16;
			instruction[1] = p >> 24;
			if (prev)
				prev->send(instruction, 5);
		}

		FLOAT32 freq_offset = 0;
		uint32_t frequency = 0;
		uint32_t sample_rate = 0;
		bool tuner_AGC = true;
		bool RTL_AGC = false;
		FLOAT32 tuner_Gain = 33.0;

		void applySettings()
		{
			std::cerr << "RTLTCP: setting parameters" << std::endl;
			sendParam(5, freq_offset);
			sendParam(3, tuner_AGC ? 0 : 1);

			if (!tuner_AGC)
				sendParam(4, tuner_Gain * 10);
			if (RTL_AGC)
				sendParam(8, 1);

			sendParam(2, sample_rate);
			sendParam(1, frequency);
		}

	public:
		RTLTCP() : ProtocolBase("RTLTCP") {};

		void onConnect() override
		{
			applySettings();

			struct
			{
				uint32_t magic = 0, tuner = 0, gain = 0;
			} dongle;

			int len = prev->read((char *)&dongle, 12, 5);
			if (len != 12 || dongle.magic != 0x304C5452)
			{
				Error() << "RTLTCP: no or invalid response, likely not an rtl-tcp server.";
				disconnect();
			}
			connected = true;
			ProtocolBase::onConnect();
		}

		void onDisconnect() override
		{
			ProtocolBase::onDisconnect();
			connected = false;
		}

		int read(void *data, int data_len, int t = 1, bool wait = false) override
		{
			if (prev && connected)
				return prev->read(data, data_len, t, wait);

			return 0;
		}

		bool setValue(const std::string &key, const std::string &value) override
		{
			if (key == "TUNER")
			{
				double temp;
				tuner_AGC = Util::Parse::AutoFloat(value, 0, 50, temp);
				tuner_Gain = (FLOAT32)temp;
			}
			else if (key == "RTLAGC")
			{
				RTL_AGC = Util::Parse::Switch(value);
			}
			else
			{
				if (key == "RATE" || key == "SAMPLE_RATE")
				{
					sample_rate = ((Util::Parse::Integer(value, 0, 20000000)));
				}
				else if (key == "FREQOFFSET")
				{
					freq_offset = Util::Parse::Float(value, -150, 150);
				}
				else if (key == "FREQUENCY")
				{
					frequency = Util::Parse::Integer(value, 0, 2000000000);
				}

				return false;
			}

			return true;
		}

		std::string getValues() override
		{
			std::string str = " tuner " + Util::Convert::toString(tuner_AGC, tuner_Gain);
			str += " rtlagc " + Util::Convert::toString(RTL_AGC);
			str += " frequency " + std::to_string(frequency) + " freqoffset " + std::to_string(freq_offset);
			str += " rate " + std::to_string(sample_rate);
			return str;
		}
	};

	class WebSocket : public ProtocolBase
	{
	private:
		enum class OPCODE
		{
			CONTINUATION = 0x0,
			TEXT = 0x1,
			BINARY = 0x2,
			CLOSE = 0x8,
			PING = 0x9,
			PONG = 0xA
		};

		const int MAX_PACKET_SIZE = 16384;
		bool connected = false;
		bool binary = false;

		std::vector<uint8_t> buffer;
		std::vector<uint8_t> received;
		std::vector<uint8_t> frame;

		int buffer_ptr = 0;
		int received_ptr = 0;

		std::string path = "/";
		std::string origin;
		std::string protocols;

		bool performHandshake();

		int getFrames(void *data, int length, int t = 1, bool wait = false);
		int populateData(void *data, int length);

		std::string sha1Hash(const std::string &data);

	public:
		WebSocket() : ProtocolBase("WS") {};

		void onConnect() override;
		void onDisconnect() override;
		bool isConnected() override;

		int send(const void *data, int length) override;
		int read(void *data, int length, int t = 1, bool wait = false) override;

		bool setValue(const std::string &key, const std::string &value) override;
		std::string getValues() override;
	};
}
