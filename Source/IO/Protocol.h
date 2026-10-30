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

#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>
#include <array>
#include <vector>
#include <functional>
#include <cstring>
#include <iomanip>

#include "SocketUtil.h"

#ifdef HASOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "Parse.h"
#include "Convert.h"
#include "Common.h"
#include "OutputStats.h"
#include "Keys.h"
#include "Logger.h"

namespace Protocol
{
	// steady clock: a wall clock that steps would fire or hold every deadline at once
	typedef std::chrono::steady_clock Clock;

	inline double secondsSince(Clock::time_point t)
	{
		return std::chrono::duration<double>(Clock::now() - t).count();
	}

	class AddressInfo
	{
	public:
		AddressInfo(const std::string &host, const std::string &service, const struct addrinfo *hints = nullptr) : address(nullptr), msg("")
		{
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

		const std::string &getErrorMessage() const
		{
			return msg;
		}

	private:
		struct addrinfo *address;
		std::string msg;
	};

	// a lookup on a thread of its own, detached: it shares only the job, so a lookup
	// nobody collects still finishes and frees its result
	class Resolver
	{
		struct Job
		{
			std::mutex mtx;
			bool done = false;
			std::unique_ptr<AddressInfo> result;
		};
		std::shared_ptr<Job> job;

	public:
		// a lookup still running is waited for: another thread would hang on the same
		// nameserver, and they would pile up
		bool start(const std::string &host, const std::string &service, const struct addrinfo &hints)
		{
			if (job)
				return true;

			std::shared_ptr<Job> j = std::make_shared<Job>();
			job = j;

			try
			{
				std::thread([j, host, service, hints]()
							{
					std::unique_ptr<AddressInfo> r(new AddressInfo(host, service, &hints));
					std::lock_guard<std::mutex> lock(j->mtx);
					j->result = std::move(r);
					j->done = true; })
					.detach();
			}
			catch (const std::system_error &)
			{
				job.reset();
				return false;
			}
			return true;
		}

		std::unique_ptr<AddressInfo> take()
		{
			std::unique_ptr<AddressInfo> r;
			if (!job)
				return r;
			{
				std::lock_guard<std::mutex> lock(job->mtx);
				if (!job->done)
					return r;
				r = std::move(job->result);
			}
			job.reset();
			return r;
		}
	};

	class ProtocolBase
	{

	protected:
		ProtocolBase *prev = nullptr;
		ProtocolBase *next = nullptr;

		std::string layer = "*";

		// Nothing in a layer waits: connect(), send(), read() and isConnected() return at
		// once, and poll(), called by the owner from a loop of its own, moves the
		// connection on a step at a time. A caller that can wait loops poll() through
		// waitConnected() and sendAll().

		// One stall budget for every layer, so back-pressure behaves the same
		// whether or not TLS is in the chain.
		static const int SEND_STALL_MS = 500;

		// what the layer could not send now, sent ahead of anything else
		std::string pending;
		std::chrono::steady_clock::time_point pending_since;

		// what the peer takes without waiting, or -1 after an error the layer handled;
		// progress when some went out even if none of the caller's bytes count, as for
		// a TLS record
		virtual int sendSome(const char *data, int length, bool &progress) { return -1; }

		// true once nothing is kept back; a peer that takes none of it for the stall
		// budget is disconnected
		bool flushPending()
		{
			if (pending.empty())
				return true;

			bool progress = false;
			int sent = sendSome(pending.data(), (int)pending.size(), progress);
			if (sent < 0)
				return false;

			pending.erase(0, sent);
			if (pending.empty())
				return true;

			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			if (progress || sent > 0)
				pending_since = now;
			else if (now - pending_since > std::chrono::milliseconds(SEND_STALL_MS))
			{
				Warning() << getLayer() << " (" << getHost() << ":" << getPort()
						  << "): send blocked for " << SEND_STALL_MS << " ms, disconnecting";
				disconnect();
			}
			return false;
		}

		// never waits: what is not taken now is kept and goes out first, and a message
		// that comes while some is still kept is dropped
		int sendKept(const void *data, int length)
		{
			if (!flushPending())
				return 0;

			bool progress = false;
			int sent = sendSome((const char *)data, length, progress);
			if (sent < 0)
				return -1;

			if (sent < length)
			{
				pending.assign((const char *)data + sent, length - sent);
				pending_since = std::chrono::steady_clock::now();
			}
			return length;
		}

	public:
		ProtocolBase(const std::string &l) : layer(l) {};
		virtual ~ProtocolBase() {}

		ProtocolBase *getPrev() { return prev; }
		ProtocolBase *getNext() { return next; }

		const std::string &getLayer() { return layer; }

		// one step for each layer, from the bottom up
		virtual void poll()
		{
			if (prev)
				prev->poll();
		}

		virtual bool sendDone()
		{
			return pending.empty() && (!prev || prev->sendDone());
		}

		virtual bool isConnecting()
		{
			return prev && prev->isConnecting();
		}

		// until it is up or nothing is under way any more, each address and handshake
		// getting the layer's own time; or for the seconds a quick look can spare
		bool waitConnected(int seconds = 0)
		{
			std::chrono::steady_clock::time_point until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
			while (true)
			{
				poll();
				if (isConnected())
					return true;
				if (!isConnecting() || (seconds > 0 && std::chrono::steady_clock::now() >= until))
					return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		// for an owner that never reads: how a close by the peer is seen
		void drain()
		{
			char discard[64];
			read(discard, sizeof(discard), 0, false);
		}

		// the whole message, stepping the connection until it is all out
		bool sendAll(const void *data, int length)
		{
			if (send(data, length) <= 0 && length > 0)
				return false;

			while (isConnected() && !sendDone())
			{
				poll();
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			return isConnected();
		}

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

		virtual bool setOptionKey(AIS::Keys key, const std::string &value) { return false; }
		virtual std::string getValues() { return ""; }
	};

	class TCP : public ProtocolBase
	{
		const int RECONNECT_TIME = 10;
		// a lookup still running after this is logged
		const int LOOKUP_TIME = 30;

	public:
		enum State
		{
			DISCONNECTED,
			RESOLVING,
			CONNECTING,
			READY
		};

		TCP() : ProtocolBase("TCP") {};
		void disconnect() override;
		bool connect() override;
		void poll() override;

		int read(void *data, int length, int t, bool wait) override;
		int send(const void *data, int length) override;

		bool isConnected() override { return state == READY; }
		bool isConnecting() override { return state == RESOLVING || state == CONNECTING; }

		bool setOptionKey(AIS::Keys key, const std::string &value) override
		{
			switch (key)
			{
			case AIS::KEY_SETTING_HOST:
				host = value;
				break;
			case AIS::KEY_SETTING_PORT:
				port = value;
				break;
			case AIS::KEY_SETTING_RESET:
				reset_time = Util::Parse::Integer(value, 0, 3600);
				break;
			case AIS::KEY_SETTING_TIMEOUT:
				timeout = Util::Parse::Integer(value, 0, 3600);
				break;
			case AIS::KEY_SETTING_KEEP_ALIVE:
				keep_alive = Util::Parse::Switch(value);
				break;
			default:
				return false;
			}
			return true;
		}

		std::string getValues() override
		{
			return "host " + host + " port " + port + " timeout " + std::to_string(timeout) + " keep_alive " + Util::Convert::toString(keep_alive);
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

		unsigned long getBytesSent() { return bytes_sent; }
		unsigned long getBytesReceived() { return bytes_received; }
		void setStats(IO::OutputStats *s) { stats = s; }

	private:
		std::string host;
		std::string port;

		int reset_time = -1;
		// Captured at the point of failure: closeSocket() would overwrite errno.
		std::string last_error;
		// a connect attempt not through after this many seconds is given up on (0: RECONNECT_TIME)
		int timeout = 0;
		bool keep_alive = true;

		unsigned long bytes_sent = 0;
		unsigned long bytes_received = 0;

		IO::OutputStats *stats = nullptr;

		int sock = -1;
		State state = DISCONNECTED;
		Clock::time_point stamp;

		long reset_interval = 0; // jittered reset deadline in seconds, measured from stamp
		// jittered too, so stations that lost the same server do not all come back at once
		double reconnect_delay = RECONNECT_TIME;

		// the addresses the lookup gave, tried in turn
		Resolver resolver;
		std::unique_ptr<AddressInfo> addresses;
		struct addrinfo *next_address = nullptr;

		void updateState();
		void randomizeResetInterval();
		static long jittered(long base, int percent);
		bool checkConnecting();
		void giveUp(const std::string &why);
		void ready();
		bool connectAddress(struct addrinfo *p);
		bool connectResolved(std::unique_ptr<AddressInfo> ai);
		bool tryNextAddress();
		static struct addrinfo lookupHints();

		int sendSome(const char *data, int length, bool &progress) override;

		// from the top of the chain down, so the layers above say their goodbyes
		void reconnect()
		{
			if (stats)
				stats->reconnects++;

			Debug() << "TCP: Reconnecting to " << host << ":" << port;

			ProtocolBase *top = this;
			while (top->getNext())
				top = top->getNext();
			top->disconnect();

			connect();
		}

		void handleNetworkError(const char *operation, int error_code)
		{
			Error() << "TCP (" << host << ":" << port << "): " << operation << " error " << error_code
					<< " (" << Net::errorString(error_code) << ").";
			disconnect();
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
		IO::OutputStats *stats = nullptr;

		bool verify_certificates = true;
		bool own_server = false;

		enum TLS_STATE
		{
			TLS_DISCONNECTED,
			TLS_HANDSHAKING,
			TLS_CONNECTED
		} tls_state = TLS_DISCONNECTED;

		// a peer that accepts the socket and then says nothing
		static const int HANDSHAKE_TIME = 15;
		Clock::time_point handshake_deadline;

		int sendSome(const char *data, int length, bool &progress) override;

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
		bool isConnecting() override { return tls_state == TLS_HANDSHAKING || ProtocolBase::isConnecting(); }
		void poll() override;
		int send(const void *data, int length) override;

		int read(void *data, int length, int timeout = 0, bool wait = false) override;
		void updateState();

		void setStats(IO::OutputStats *s) { stats = s; }
		void setVerifyCertificates(bool v) { verify_certificates = v; }
		// this program's own server on this machine: nothing to verify and nothing to warn of
		void setOwnServer() { verify_certificates = false; own_server = true; }

		bool setOptionKey(AIS::Keys key, const std::string &value) override
		{
			switch (key)
			{
			case AIS::KEY_SETTING_SSL_VERIFY:
				verify_certificates = Util::Parse::Switch(value);
				break;
			default:
				return false;
			}
			return true;
		}

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

		void setStats(IO::OutputStats *s) {}
		void setVerifyCertificates(bool) {}
		void setOwnServer() {}
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
		int packet_id = 0;

		// a last will, retained, and the keep-alive that lets the broker notice
		// it is due; none unless set
		std::string will_topic, will_message;
		int keepalive = 0;
		Clock::time_point last_sent;
		int publish(const void *str, int length, const std::string &tpc, bool retain);

		// never zero (MQTT 3.1.1 2.3.1)
		int nextPacketId()
		{
			if (++packet_id > 65535)
				packet_id = 1;
			return packet_id;
		}

		bool connected = false;
		bool subscribe = false;

		bool awaiting_connack = false;
		Clock::time_point connack_deadline;
		uint8_t connack[4];
		int connack_len = 0;

		void pushVariableLength(int length);
		void pushByte(uint8_t byte);
		void pushInt(int length);
		void createPacket(PacketType type, uint8_t flags);
		void pushString(const std::string &str);
		void subscribePacket();
		void connectPacket();
		bool sendConnect();
		bool connackAccepted(uint8_t code);
		bool finishHandshake();

	public:
		MQTT() : ProtocolBase("MQTT") {};
		void onConnect() override;
		void onDisconnect() override;
		void disconnect() override;
		void poll() override;
		bool setOptionKey(AIS::Keys key, const std::string &value) override;
		bool isConnected() override;
		bool isConnecting() override { return awaiting_connack || ProtocolBase::isConnecting(); }
		int send(const void *str, int length, const std::string &tpc);
		int send(const void *str, int length) override;
		// a message the broker keeps for whoever subscribes later
		int sendRetained(const std::string &payload, const std::string &tpc) { return publish(payload.data(), (int)payload.size(), tpc, true); }
		// what the broker publishes, retained, when this connection dies
		void setWill(const std::string &tpc, const std::string &message, int keepalive_s = 60)
		{
			will_topic = tpc;
			will_message = message;
			keepalive = keepalive_s;
		}
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
					return;
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

	class Hub : public ProtocolBase
	{
		std::string uuid;
		bool connected = false;

		enum Flags : unsigned char
		{
			HELLO = 0x01,
			BYE = 0x02
		};

		// 0xAB version flags [uuidlen uuid] '\n'
		void sendControl(unsigned char flags)
		{
			std::string frame;
			frame += (char)0xab;
			frame += (char)0x00;
			frame += (char)flags;

			if (flags & HELLO)
			{
				frame += (char)uuid.size();
				frame += uuid;
			}
			frame += '\n';

			if (prev)
				prev->send(frame.data(), (int)frame.size());
		}

		void onConnect() override
		{
			connected = true;
			if (!uuid.empty())
				sendControl(HELLO);
			ProtocolBase::onConnect();
		}

		void onDisconnect() override
		{
			connected = false;
			ProtocolBase::onDisconnect();
		}

	public:
		Hub() : ProtocolBase("Hub") {};

		void setUUID(const std::string &u) { uuid = u; }
		void keepAlive() { sendControl(0); }

		void disconnect() override
		{
			if (connected)
				sendControl(BYE);
			ProtocolBase::disconnect();
		}
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
				return;
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

		bool setOptionKey(AIS::Keys key, const std::string &value) override
		{
			switch (key)
			{
			case AIS::KEY_SETTING_TUNER:
			{
				double temp;
				tuner_AGC = Util::Parse::AutoFloat(value, 0, 50, temp);
				tuner_Gain = (FLOAT32)temp;
				break;
			}
			case AIS::KEY_SETTING_RTLAGC:
				RTL_AGC = Util::Parse::Switch(value);
				break;
			case AIS::KEY_SETTING_SAMPLE_RATE:
				sample_rate = Util::Parse::Integer(value, 0, 20000000);
				return false;
			case AIS::KEY_SETTING_FREQOFFSET:
				freq_offset = Util::Parse::Float(value, -150, 150);
				return false;
			case AIS::KEY_SETTING_FREQUENCY:
				frequency = Util::Parse::Integer(value, 0, 2000000000);
				return false;
			default:
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
		std::string username, password;
		bool basic = false;
		std::string origin;
		std::string protocols;

		// the upgrade under way: the key sent and the reply so far
		std::string upgrade_key, upgrade_reply;
		bool upgrading = false;
		bool close_sent = false;
		Clock::time_point upgrade_deadline;

		bool sendUpgrade();
		int readUpgrade(int t);
		bool checkUpgrade(size_t body_start);
		void finishUpgrade();

		int getFrames(void *data, int length, int t = 1, bool wait = false);
		int populateData(void *data, int length);

		std::string sha1Hash(const std::string &data);

	public:
		WebSocket() : ProtocolBase("WS") {};

		void setPath(const std::string &p) { path = p.empty() ? "/" : p; }

		void onConnect() override;
		void onDisconnect() override;
		void disconnect() override;
		bool isConnected() override;
		bool isConnecting() override { return upgrading || ProtocolBase::isConnecting(); }
		void poll() override;

		int send(const void *data, int length) override;
		int read(void *data, int length, int t = 1, bool wait = false) override;

		bool setOptionKey(AIS::Keys key, const std::string &value) override;
		std::string getValues() override;
	};
}
