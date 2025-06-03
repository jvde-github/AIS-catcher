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

#include "Utilities.h"
#include "Common.h"
#include <iomanip>

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
	};

#ifdef HASOPENSSL

	class TLS : public ProtocolBase
	{
	private:
		SSL_CTX *ctx = nullptr;
		SSL *ssl = nullptr;
		bool handshake_complete = false;

		static bool ssl_initialized;
		static int ssl_ref_count;
		bool verify_certificates = false;

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
			cleanupSSL();
		}

		// Initialize OpenSSL library (call once globally)
		static void initializeSSL()
		{
			if (!ssl_initialized)
			{
				SSL_load_error_strings();
				SSL_library_init();
				OpenSSL_add_all_algorithms();
				ssl_initialized = true;
			}
			ssl_ref_count++;
		}

		// Cleanup OpenSSL library
		static void cleanupSSL()
		{
			ssl_ref_count--;
			if (ssl_ref_count <= 0 && ssl_initialized)
			{
				EVP_cleanup();
				ERR_free_strings();
				ssl_initialized = false;
			}
		}

		void onConnect() override
		{
			// Create SSL context
			ctx = SSL_CTX_new(TLS_client_method());
			if (!ctx)
			{
				Error() << "TLS: Failed to create SSL context for " << getHost() << ":" << getPort();
			}

			if (verify_certificates)
			{
				SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
				SSL_CTX_set_default_verify_paths(ctx);
			}
			else
			{
				SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
				Warning() << "TLS: Certificate verification DISABLED for " << getHost() << ":" << getPort();
			}

			// Create SSL object
			ssl = SSL_new(ctx);
			if (!ssl)
			{
				Error() << "TLS: Failed to create SSL object for " << getHost() << ":" << getPort();
				disconnect();
			}

			// Create custom BIO that uses our TCP connection
			BIO *bio = BIO_new(BIO_s_socket());
			BIO_set_fd(bio, getSocket(), BIO_NOCLOSE);
			SSL_set_bio(ssl, bio, bio);

			tls_state = TLS_HANDSHAKING;
			performHandshake();
		}

		void onDisconnect() override
		{
			ProtocolBase::onDisconnect();

			if (ssl)
			{
				if (handshake_complete)
				{
					SSL_shutdown(ssl);
					Info() << "TLS (" + getHost() + ":" + getPort() + "): Disconnected";
				}
				SSL_free(ssl);
				ssl = nullptr;
			}

			if (ctx)
			{
				SSL_CTX_free(ctx);
				ctx = nullptr;
			}

			handshake_complete = false;
			tls_state = TLS_DISCONNECTED;
		}

		bool isConnected() override
		{
			bool prev_connected = prev ? prev->isConnected() : false;

			if (prev_connected && !handshake_complete)
			{
				updateState();
			}

			return prev_connected && handshake_complete;
		}

		int send(const void *data, int length) override
		{
			if (!isConnected())
				return 0;

			int sent = SSL_write(ssl, data, length);
			if (sent <= 0)
			{
				int error = SSL_get_error(ssl, sent);

				switch (error)
				{
				case SSL_ERROR_WANT_WRITE:
				case SSL_ERROR_WANT_READ:
					// Non-blocking operation would block
					return 0;

				case SSL_ERROR_ZERO_RETURN:
					// Connection closed cleanly
					Warning() << "TLS (" << getHost() << ":" << getPort() << "): Connection closed by peer";
					disconnect();
					return -1;

				case SSL_ERROR_SYSCALL:
				case SSL_ERROR_SSL:
				default:
					Error() << "TLS (" << getHost() << ":" << getPort() << "): Send error: " << getSSLErrorString(error);
					disconnect();
					return -1;
				}
			}

			return sent;
		}

		int read(void *data, int length, int timeout = 0, bool wait = false) override
		{
			if (!isConnected())
				return 0;

			// do not block if we are not waiting
			if (!wait || timeout <= 0)
			{
				int pending = SSL_pending(ssl);
				if (pending > 0)
				{
					int to_read = std::min(pending, length);
					int r = SSL_read(ssl, data, to_read);
					return (r > 0 ? r : 0);
				}

				return 0;
			}

			SOCKET sock = getSocket();
			int time = 0;
			int total_received = 0;
			char *buf_ptr = (char *)data;

			while (time < timeout && total_received < length)
			{
				int pending = SSL_pending(ssl);
				if (pending > 0)
				{
					int to_read = std::min(pending, length - total_received);
					int r = SSL_read(ssl, buf_ptr + total_received, to_read);
					if (r > 0)
					{
						total_received += r;
						if (total_received >= length)
							return total_received;

						continue;
					}
				}

				// No pending data: wait up to 1 second on the socket for readability.
				fd_set readfds;
				FD_ZERO(&readfds);
				FD_SET(sock, &readfds);

				struct timeval tv;
				tv.tv_sec = 1;
				tv.tv_usec = 0;

				int rv = select(sock + 1, &readfds, nullptr, nullptr, &tv);
				if (rv < 0)
				{
					if (errno == EINTR)
						continue;

					// Fatal select() error:
					perror("TLS select");
					disconnect();
					return -1;
				}
				if (rv == 0)
				{
					time++;
					continue;
				}

				int to_read = length - total_received;
				int r = SSL_read(ssl, buf_ptr + total_received, to_read);
				if (r > 0)
				{
					total_received += r;
					if (total_received >= length)
					{
						return total_received;
					}
					time++;
					continue;
				}

				int err = SSL_get_error(ssl, r);
				switch (err)
				{
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					time ++;
					continue;

				case SSL_ERROR_ZERO_RETURN:
					disconnect();
					return (total_received > 0 ? total_received : -1);

				case SSL_ERROR_SYSCALL:
				case SSL_ERROR_SSL:
				default:
					std::cerr << "TLS: Read error: " << ERR_error_string(ERR_get_error(), nullptr) << "\n";
					disconnect();
					return -1;
				}
			}

			// Either we've done `timeout` rounds, or total_received >= length.
			return total_received;
		}

		void updateState()
		{
			if (tls_state == TLS_HANDSHAKING)
			{
				performHandshake();
			}
		}

	private:
		bool performHandshake()
		{
			if (!ssl)
				return false;

			int result = SSL_connect(ssl);

			if (result == 1)
			{
				// Handshake completed successfully
				handshake_complete = true;
				tls_state = TLS_CONNECTED;

				Info() << "TLS: Connected to " << getHost() << ":" << getPort() << " using " << SSL_get_version(ssl) << " with " << SSL_get_cipher(ssl);

				ProtocolBase::onConnect();
				return true;
			}

			int error = SSL_get_error(ssl, result);

			switch (error)
			{
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				return false;

			case SSL_ERROR_SYSCALL:
			case SSL_ERROR_SSL:
			default:
				Error() << "TLS (" << getHost() << ":" << getPort() << "): Handshake failed: " << getSSLErrorString(error);

				// Print detailed SSL error
				unsigned long ssl_err = ERR_get_error();
				if (ssl_err != 0)
				{
					char err_buf[256];
					ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
					Error() << "TLS (" << getHost() << ":" << getPort() << "): SSL Error details: " << err_buf;
				}

				disconnect();
				return false;
			}
		}

		std::string getSSLErrorString(int error)
		{
			switch (error)
			{
			case SSL_ERROR_NONE:
				return "No error";
			case SSL_ERROR_SSL:
				return "SSL protocol error";
			case SSL_ERROR_WANT_READ:
				return "Want read";
			case SSL_ERROR_WANT_WRITE:
				return "Want write";
			case SSL_ERROR_SYSCALL:
				return "System call error";
			case SSL_ERROR_ZERO_RETURN:
				return "Connection closed";
			default:
				char buffer[256];
				ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
				return std::string(buffer);
			}
		}
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

		// DEBUG REMOVE
		void debugDecodePacket(uint8_t packet_type, const std::vector<uint8_t> &packet_data)
		{
			std::cerr << "=== MQTT PACKET DEBUG ===" << std::endl;

			// Decode packet type
			uint8_t msg_type = (packet_type & 0xF0) >> 4;
			uint8_t flags = packet_type & 0x0F;

			std::cerr << "Packet Type: 0x" << std::hex << (int)packet_type << std::dec;

			switch (msg_type)
			{
			case 1:
				std::cerr << " (CONNECT)";
				break;
			case 2:
				std::cerr << " (CONNACK)";
				break;
			case 3:
				std::cerr << " (PUBLISH)";
				break;
			case 4:
				std::cerr << " (PUBACK)";
				break;
			case 5:
				std::cerr << " (PUBREC)";
				break;
			case 6:
				std::cerr << " (PUBREL)";
				break;
			case 7:
				std::cerr << " (PUBCOMP)";
				break;
			case 8:
				std::cerr << " (SUBSCRIBE)";
				break;
			case 9:
				std::cerr << " (SUBACK)";
				break;
			case 10:
				std::cerr << " (UNSUBSCRIBE)";
				break;
			case 11:
				std::cerr << " (UNSUBACK)";
				break;
			case 12:
				std::cerr << " (PINGREQ)";
				break;
			case 13:
				std::cerr << " (PINGRESP)";
				break;
			case 14:
				std::cerr << " (DISCONNECT)";
				break;
			default:
				std::cerr << " (UNKNOWN)";
				break;
			}
			std::cerr << ", Flags: 0x" << std::hex << (int)flags << std::dec << std::endl;

			std::cerr << "Payload Length: " << packet_data.size() << " bytes" << std::endl;

			if (packet_data.empty())
			{
				std::cerr << "No payload data" << std::endl;
				return;
			}

			// Decode specific packet types
			switch (msg_type)
			{
			case 1: // CONNECT
				debugDecodeConnect(packet_data);
				break;
			case 2: // CONNACK
				debugDecodeConnack(packet_data);
				break;
			case 3: // PUBLISH
				debugDecodePublish(packet_data, flags);
				break;
			default:
				// For other packet types, show hex dump
				std::cerr << "Raw payload (first 50 bytes):" << std::endl;
				for (size_t i = 0; i < std::min((size_t)50, packet_data.size()); i++)
				{
					if (i % 16 == 0)
						std::cerr << std::endl
								  << "  ";
					std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)packet_data[i] << " ";
				}
				std::cerr << std::dec << std::endl;
				break;
			}
			std::cerr << "=========================" << std::endl;
		}

		void debugDecodeConnect(const std::vector<uint8_t> &data)
		{
			std::cerr << "--- CONNECT Packet Details ---" << std::endl;

			if (data.size() < 10)
			{
				std::cerr << "ERROR: CONNECT packet too short (" << data.size() << " bytes)" << std::endl;
				return;
			}

			size_t pos = 0;

			// Protocol name length
			uint16_t proto_len = (data[pos] << 8) | data[pos + 1];
			pos += 2;

			std::cerr << "Protocol Name Length: " << proto_len << std::endl;

			if (pos + proto_len > data.size())
			{
				std::cerr << "ERROR: Invalid protocol name length" << std::endl;
				return;
			}

			// Protocol name
			std::string protocol_name(data.begin() + pos, data.begin() + pos + proto_len);
			pos += proto_len;
			std::cerr << "Protocol Name: \"" << protocol_name << "\"" << std::endl;

			if (pos >= data.size())
				return;

			// Protocol version
			uint8_t version = data[pos++];
			std::cerr << "Protocol Version: " << (int)version;
			switch (version)
			{
			case 3:
				std::cerr << " (MQTT 3.1)";
				break;
			case 4:
				std::cerr << " (MQTT 3.1.1)";
				break;
			case 5:
				std::cerr << " (MQTT 5.0)";
				break;
			default:
				std::cerr << " (Unknown)";
				break;
			}
			std::cerr << std::endl;

			if (pos >= data.size())
				return;

			// Connect flags
			uint8_t flags = data[pos++];
			std::cerr << "Connect Flags: 0x" << std::hex << (int)flags << std::dec << std::endl;
			std::cerr << "  Username Flag: " << ((flags & 0x80) ? "YES" : "NO") << std::endl;
			std::cerr << "  Password Flag: " << ((flags & 0x40) ? "YES" : "NO") << std::endl;
			std::cerr << "  Will Retain: " << ((flags & 0x20) ? "YES" : "NO") << std::endl;
			std::cerr << "  Will QoS: " << ((flags & 0x18) >> 3) << std::endl;
			std::cerr << "  Will Flag: " << ((flags & 0x04) ? "YES" : "NO") << std::endl;
			std::cerr << "  Clean Session: " << ((flags & 0x02) ? "YES" : "NO") << std::endl;

			if (pos + 1 >= data.size())
				return;

			// Keep alive
			uint16_t keep_alive = (data[pos] << 8) | data[pos + 1];
			pos += 2;
			std::cerr << "Keep Alive: " << keep_alive << " seconds" << std::endl;

			// Client ID
			if (pos + 1 < data.size())
			{
				uint16_t client_id_len = (data[pos] << 8) | data[pos + 1];
				pos += 2;

				if (pos + client_id_len <= data.size())
				{
					std::string client_id(data.begin() + pos, data.begin() + pos + client_id_len);
					pos += client_id_len;
					std::cerr << "Client ID: \"" << client_id << "\"" << std::endl;
				}
			}

			// Will topic and message (if will flag is set)
			if (flags & 0x04)
			{
				if (pos + 1 < data.size())
				{
					uint16_t will_topic_len = (data[pos] << 8) | data[pos + 1];
					pos += 2;
					if (pos + will_topic_len <= data.size())
					{
						std::string will_topic(data.begin() + pos, data.begin() + pos + will_topic_len);
						pos += will_topic_len;
						std::cerr << "Will Topic: \"" << will_topic << "\"" << std::endl;
					}
				}

				if (pos + 1 < data.size())
				{
					uint16_t will_msg_len = (data[pos] << 8) | data[pos + 1];
					pos += 2;
					if (pos + will_msg_len <= data.size())
					{
						std::string will_msg(data.begin() + pos, data.begin() + pos + will_msg_len);
						pos += will_msg_len;
						std::cerr << "Will Message: \"" << will_msg << "\"" << std::endl;
					}
				}
			}

			// Username (if username flag is set)
			if (flags & 0x80)
			{
				if (pos + 1 < data.size())
				{
					uint16_t username_len = (data[pos] << 8) | data[pos + 1];
					pos += 2;
					if (pos + username_len <= data.size())
					{
						std::string username(data.begin() + pos, data.begin() + pos + username_len);
						pos += username_len;
						std::cerr << "Username: \"" << username << "\"" << std::endl;
					}
				}
			}

			// Password (if password flag is set)
			if (flags & 0x40)
			{
				if (pos + 1 < data.size())
				{
					uint16_t password_len = (data[pos] << 8) | data[pos + 1];
					pos += 2;
					if (pos + password_len <= data.size())
					{
						std::cerr << "Password: [" << password_len << " bytes] (hidden for security)" << std::endl;
					}
				}
			}
		}

		void debugDecodeConnack(const std::vector<uint8_t> &data)
		{
			std::cerr << "--- CONNACK Packet Details ---" << std::endl;

			if (data.size() < 2)
			{
				std::cerr << "ERROR: CONNACK packet too short (" << data.size() << " bytes)" << std::endl;
				return;
			}

			uint8_t flags = data[0];
			uint8_t return_code = data[1];

			std::cerr << "Session Present: " << ((flags & 0x01) ? "YES" : "NO") << std::endl;
			std::cerr << "Return Code: " << (int)return_code << " - ";

			switch (return_code)
			{
			case 0:
				std::cerr << "Connection Accepted";
				break;
			case 1:
				std::cerr << "Unacceptable Protocol Version";
				break;
			case 2:
				std::cerr << "Identifier Rejected";
				break;
			case 3:
				std::cerr << "Server Unavailable";
				break;
			case 4:
				std::cerr << "Bad Username or Password";
				break;
			case 5:
				std::cerr << "Not Authorized";
				break;
			default:
				std::cerr << "Unknown Error";
				break;
			}
			std::cerr << std::endl;
		}

		void debugDecodePublish(const std::vector<uint8_t> &data, uint8_t flags)
		{
			std::cerr << "--- PUBLISH Packet Details ---" << std::endl;

			uint8_t qos = (flags & 0x06) >> 1;
			bool dup = flags & 0x08;
			bool retain = flags & 0x01;

			std::cerr << "DUP: " << (dup ? "YES" : "NO") << std::endl;
			std::cerr << "QoS: " << (int)qos << std::endl;
			std::cerr << "RETAIN: " << (retain ? "YES" : "NO") << std::endl;

			if (data.size() < 2)
			{
				std::cerr << "ERROR: PUBLISH packet too short" << std::endl;
				return;
			}

			size_t pos = 0;

			// Topic name
			uint16_t topic_len = (data[pos] << 8) | data[pos + 1];
			pos += 2;

			if (pos + topic_len > data.size())
			{
				std::cerr << "ERROR: Invalid topic length" << std::endl;
				return;
			}

			std::string topic(data.begin() + pos, data.begin() + pos + topic_len);
			pos += topic_len;
			std::cerr << "Topic: \"" << topic << "\"" << std::endl;

			// Packet ID (only for QoS > 0)
			if (qos > 0)
			{
				if (pos + 1 < data.size())
				{
					uint16_t packet_id = (data[pos] << 8) | data[pos + 1];
					pos += 2;
					std::cerr << "Packet ID: " << packet_id << std::endl;
				}
			}

			// Payload
			if (pos < data.size())
			{
				size_t payload_len = data.size() - pos;
				std::cerr << "Payload Length: " << payload_len << " bytes" << std::endl;

				// Try to show as text if printable
				bool is_text = true;
				for (size_t i = pos; i < data.size() && i < pos + 50; i++)
				{
					if (data[i] < 32 && data[i] != 9 && data[i] != 10 && data[i] != 13)
					{
						is_text = false;
						break;
					}
				}

				if (is_text)
				{
					std::string payload(data.begin() + pos, data.begin() + std::min(pos + 100, data.size()));
					std::cerr << "Payload (text): \"" << payload << "\"" << std::endl;
				}
				else
				{
					std::cerr << "Payload (hex): ";
					for (size_t i = pos; i < data.size() && i < pos + 50; i++)
					{
						std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
					}
					std::cerr << std::dec << std::endl;
				}
			}
		}

		// END DEBUG REMOVE
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

		void pushVariableLength(int length)
		{
			if (length >= 128 * 128 * 128 * 128)
			{
				Error() << "MQTT: Length encoding error: " << length;
				disconnect();
				return;
			}

			do
			{
				uint8_t b = length & 127;

				length >>= 7;
				if (length)
					b |= 128;

				packet.push_back(b);

			} while (length);
		}

		void pushByte(uint8_t byte)
		{
			packet.push_back(byte);
		}

		void pushInt(int length)
		{
			packet.push_back(length >> 8);
			packet.push_back(length & 0xFF);
		}

		void createPacket(PacketType type, uint8_t flags)
		{
			packet.resize(0);
			packet.push_back((uint8_t)type | flags);
		}

		void pushString(const std::string &str)
		{
			pushInt(str.length());
			packet.insert(packet.end(), str.begin(), str.end());
		}

		void subscribePacket()
		{

			int packet_length = 2 + 2 + topic.length() + 1;

			createPacket(PacketType::SUBSCRIBE, 2);
			pushVariableLength(packet_length);

			pushInt(packet_id++);
			pushString(topic);
			pushByte(qos);
		}

		void connectPacket()
		{
			createPacket(PacketType::CONNECT, 0);

			int length = 12 + client_id.length();
			uint8_t flags = 0x02;

			if (!username.empty())
			{
				length += 2 + username.length();
				flags |= 0x80;
			}
			if (!password.empty())
			{
				length += 2 + password.length();
				flags |= 0x40;
			}

			pushVariableLength(length);

			pushString("MQTT");
			pushByte(0x04); // MQTT 3.1.1
			pushByte(flags);

			// keep alive is off
			pushInt(0);

			pushString(client_id);

			if (!username.empty())
				pushString(username);
			if (!password.empty())
				pushString(password);
		}

		int readRemainingLength()
		{
			int length = 0;
			int shift = 0;
			uint8_t b;

			do
			{
				if (shift >= 28)
					return -1;

				if (prev->read((char *)&b, 1) != 1)
					return -1;

				length |= (b & 127) << shift;
				shift += 7;

			} while ((b & 128) != 0);

			return length;
		}

		void performHandshake()
		{

			uint8_t b;
			int length;

			Info() << "MQTT: Starting Handshake with broker " << getHost() << ":" << getPort();

			connected = false;

			if (!prev)
				return;

			connectPacket();

			if (prev->send(packet.data(), packet.size()) < 0)
			{
				Error() << "MQTT: Failed to send CONNECT packet";
				disconnect();
				return;
			}
			int l = readPacket(b, length);
			if (!l || packet.size() < 2 || (b & 0xF0) != (uint8_t)(PacketType::CONNACK))
			{
				Error() << "MQTT: Failed to read CONNACK packet l = " << l << " size = " << packet.size() << " type = 0x" << std::hex << (int)b;
				Error() << "MQTT: Expected CONNACK but received different packet type";
				disconnect();
				return;
			}

			if (packet[1] != 0)
			{
				switch (packet[1])
				{
				case 0x01:
					Error() << "MQTT: Connection Refused - Unacceptable Protocol Version. Server does not support requested MQTT protocol level.";
					break;
				case 0x02:
					Error() << "MQTT: Connection Refused - Identifier Rejected. Client identifier is valid UTF-8 but not allowed by server.";
					break;
				case 0x03:
					Error() << "MQTT: Connection Refused - Server Unavailable. Network connected but MQTT service unavailable.";
					break;
				case 0x04:
					Error() << "MQTT: Connection Refused - Bad Username or Password. Malformed credentials.";
					break;
				case 0x05:
					Error() << "MQTT: Connection Refused - Not Authorized. Client lacks authorization to connect.";
					break;
				default:
					Error() << "MQTT: Connection Refused - Unknown Return Code: 0x" << std::hex << (int)packet[1];
				}
				disconnect();
				return;
			}

			if (subscribe)
			{
				subscribePacket();

				if (prev->send((char *)packet.data(), packet.size()) < 0)
				{
					Error() << "MQTT: Failed to send SUBSCRIBE packet";
					disconnect();
					return;
				}
			}
			Info() << "MQTT: Connected to broker " << (subscribe ? "and subscribed" : "");
			connected = true;
		}

		bool readPacket(uint8_t &type, int &length)
		{
			if (prev->read(&type, 1, 5, true) != 1)
				return false;

			length = readRemainingLength();

			if (length < 0)
				return false;

			packet.resize(length);

			if (prev->read(packet.data(), length, 5, true) != length)
				return false;

			return true;
		}

	public:
		MQTT() : ProtocolBase("MQTT") {};
		void onConnect() override
		{
			buffer_ptr = 0;

			performHandshake();

			ProtocolBase::onConnect();
		}

		void onDisconnect() override
		{
			ProtocolBase::onDisconnect();

			connected = false;
		}

		bool setValue(const std::string &key, const std::string &value) override
		{
			if (key == "TOPIC")
				topic = value;
			else if (key == "CLIENT_ID")
				client_id = value;
			else if (key == "USERNAME")
				username = value;
			else if (key == "PASSWORD")
				password = value;
			else if (key == "QOS")
				qos = Util::Parse::Integer(value, 0, 2, key);
			else if (key == "SUBSCRIBE")
				subscribe = Util::Parse::Switch(value);
			else
				return false;

			return true;
		}

		bool isConnected() override
		{
			bool prev_connected = prev ? prev->isConnected() : false;

			return prev_connected && connected;
		}

		int send(const void *str, int length) override
		{
			if (!isConnected())
				return 0;

			if (length > 2048)
			{
				Warning() << "MQTT: message too long, skipped";
				return -1;
			}

			createPacket(PacketType::PUBLISH, qos << 1);

			int packet_length = 2 + topic.length() + length + (qos > 0 ? 2 : 0);

			pushVariableLength(packet_length);
			pushString(topic);

			if (qos > 0)
				pushInt(packet_id++);

			packet.insert(packet.end(), (char *)str, (char *)str + length);

			return prev->send(packet.data(), packet.size());
		}

		int read(void *data, int data_len, int t = 1, bool wait = false) override
		{
			if (!isConnected())
				return 0;

			if (buffer.empty())
				buffer.resize(16384);

			int len = prev->read((char *)buffer.data() + buffer_ptr, buffer.size() - buffer_ptr, t, wait);

			if (len <= 0)
				return len;

			buffer_ptr += len;

			while (true)
			{
				if (buffer_ptr < 2)
					return 0;

				int length = 0;
				int shift = 0;

				int i = 1;
				do
				{
					if (i >= buffer_ptr)
						return 0;

					if (shift >= 28)
					{
						Error() << "MQTT: size of remaining length packet is at most 4 bytes.";
						disconnect();
						return -1;
					}

					length |= (buffer[i] & 127) << shift;
					shift += 7;

				} while ((buffer[i++] & 128) != 0);

				if (buffer_ptr < length + i)
					return 0;

				// we have a message
				int data_returned = 0;

				switch ((PacketType)(buffer[0] & 0xF0))
				{

				case PacketType::PUBLISH:
				{
					if (data_len < length)
						break;

					int topic_len = (buffer[i] << 8) + buffer[i + 1];
					int q = (buffer[0] >> 1) & 0x03;

					if (q == 0)
					{
						data_returned = length - 2 - topic_len;
						memcpy(data, buffer.data() + i + 2 + topic_len, data_returned);
					}
					else if (q == 1 || q == 2)
					{
						uint16_t packet_id = (buffer[i + 2 + topic_len] << 8) + buffer[i + 2 + topic_len + 1];
						data_returned = length - 4 - topic_len;
						memcpy(data, buffer.data() + i + 4 + topic_len, data_returned);

						createPacket((q == 1) ? PacketType::PUBACK : PacketType::PUBREC, 0);
						pushVariableLength(2);
						pushInt(packet_id);
						prev->send(packet.data(), packet.size());
					}
					break;
				}
				case PacketType::PINGREQ:
					createPacket(PacketType::PINGRESP, 0);
					pushVariableLength(0);
					prev->send(packet.data(), packet.size());
					break;
				case PacketType::PUBREC:
					createPacket(PacketType::PUBREL, 2);
					pushVariableLength(2);
					pushByte(buffer[i]);
					pushByte(buffer[i + 1]);
					if (prev->send(packet.data(), packet.size()) != packet.size())
						return -1;

					break;
				case PacketType::PUBACK:
				case PacketType::DISCONNECT:
				case PacketType::PUBCOMP:
				case PacketType::SUBACK:
					break;
				default:
					Warning() << "MQTT: packet received: " << ((int)buffer[0] >> 4);
					break;
				}

				buffer_ptr -= length + i;
				memmove(buffer.data(), buffer.data() + length + i, buffer_ptr);

				if (data_returned)
					return data_returned;
			}
		}

		std::string getValues() override
		{
			return "topic " + topic + " client_id " + client_id + " username " + username + " password " + password + " qos " + std::to_string(qos);
		}
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
		int tuner_bandwidth = 0;
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
			else if (key == "RATE" || key == "SAMPLE_RATE")
			{
				sample_rate = ((Util::Parse::Integer(value, 0, 20000000)));
			}
			else if (key == "BW" || key == "BANDWIDTH")
			{
				tuner_bandwidth = Util::Parse::Integer(value, 0, 1000000);
			}
			else if (key == "FREQOFFSET")
			{
				freq_offset = Util::Parse::Float(value, -150, 150);
			}
			else if (key == "FREQUENCY")
			{
				frequency = Util::Parse::Integer(value, 0, 2000000000);
			}
			else
				return false;

			return true;
		}

		std::string getValues() override
		{
			return "frequency " + std::to_string(frequency) + " freqoffset " + std::to_string(freq_offset) + " rate " + std::to_string(sample_rate) + " bandwidth " + std::to_string(tuner_bandwidth) + " tuner " + std::to_string(tuner_Gain) + " rtlagc " + Util::Convert::toString(RTL_AGC);
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
