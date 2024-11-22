
/*

	Copyright(c) 2023-2024 jvde.github@gmail.com


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

#define SOCKET		 int
#define SOCKADDR	 struct sockaddr
#define SOCKET_ERROR -1

#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Utilities.h"
#include "Common.h"

namespace Protocol {
	class ProtocolBase {

	protected:
		ProtocolBase* prev = nullptr;
		ProtocolBase* next = nullptr;

	public:
		ProtocolBase() {}
		~ProtocolBase() { disconnect(); }

        ProtocolBase *getPrev() { return prev; }
        ProtocolBase *getNext() { return next; }

		virtual void disconnect() {
			if (prev)
				prev->disconnect();
		}

		virtual bool connect() {
			if (prev)
				return prev->connect();

			return false;
		}

		virtual bool isConnected() {
			if (prev)
				return prev->isConnected();

			return false;
		}

		virtual int read(void* data, int length, int t = 1, bool wait = false) {
			if (prev)
				return prev->read(data, length, t, wait);

			return -1;
		}

		virtual int send(const void* data, int length) {
			if (prev)
				return prev->send(data, length);

			return -1;
		}

		virtual void updateState() {
			if (prev)
				prev->updateState();
		}

		virtual void onConnect() {
			if (next)
				next->onConnect();
		}

		virtual void onDisconnect() {
			if (next)
				next->onDisconnect();
		}

		ProtocolBase* add(ProtocolBase* p) {
			next = p;
			p->prev = this;

            return p;
		}

		virtual bool setValue(const std::string& key, const std::string& value) { return false; }
		virtual std::string getValues() { return ""; }
	};


	class TCP : public ProtocolBase {

	public:
		void disconnect();
		bool connect();

		int read(void* data, int length, int t, bool wait);
		int send(const void* data, int length);

		bool isConnected() {
			if (state == READY)
				return true;

			updateState();
			return state == READY;
		}

		bool setValue(const std::string& key, const std::string& value) {
            
			if (key == "HOST")
				host = value;
			else if (key == "PORT")
				port = value;
			else if (key == "RESET")
				reset_time = std::stoi(value);
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

		std::string getValues() {
			return "HOST: " + host + " PORT: " + port + " PERSIST: " + Util::Convert::toString(persistent) + " KEEP_ALIVE: " + Util::Convert::toString(keep_alive);
		}

	private:
		enum State {
			DISCONNECTED,
			CONNECTING,
			READY
		};

		std::string host;
		std::string port;

		bool persistent = true;
		int reset_time = -1;
		int timeout = 0;
		bool keep_alive = false;

		int sock = -1;
		State state = DISCONNECTED;
		time_t stamp = 0;

		void updateState();
		bool isConnected(int t);

		bool reconnect() {
			disconnect();
			return connect();
		}
	};


	class MQTT : public ProtocolBase {

		std::vector<uint8_t> data, buffer;

		enum class PacketType {

			CONNECT = 1,

			CONNACK = 2,

			PUBLISH = 3,

			PUBACK = 4,

			SUBSCRIBE = 8,

			SUBACK = 9,

			DISCONNECT = 14

		};


		std::string broker_host = "127.0.0.1";

		std::string broker_port = "1883";

		std::string topic = "ais/data";

		std::string client_id;

		std::string username;

		std::string password;


		int qos = 0;

		bool connected = false;
		bool subscribe = false;

		void encodeLength(size_t length)

		{

			do

			{

				uint8_t encodedByte = length % 128;

				length /= 128;

				if (length > 0)

				{

					encodedByte |= 128;
				}

				data.push_back(encodedByte);

			} while (length > 0);
		}


		void subscribePacket()

		{

			data.resize(0);

			data.push_back(0x82);

			size_t remaining_length = 2 + 2 + topic.length() + 1;

			encodeLength(remaining_length);


			data.push_back(0x00);

			data.push_back(0x01);


			data.push_back((topic.length() >> 8) & 0xFF);

			data.push_back(topic.length() & 0xFF);


			data.insert(data.end(), topic.begin(), topic.end());


			data.push_back(static_cast<uint8_t>(qos));
		}


		void connectPacket()

		{

			data.resize(0);

			data.push_back(0x10);


			size_t remaining_length = 12 + client_id.length();

			if (!username.empty())

			{

				remaining_length += 2 + username.length();
			}


			if (!password.empty())

			{

				remaining_length += 2 + password.length();
			}


			encodeLength(remaining_length);


			data.push_back(0x00);

			data.push_back(0x04);

			data.push_back('M');

			data.push_back('Q');

			data.push_back('T');

			data.push_back('T');


			data.push_back(0x04);


			uint8_t flags = 0x02;

			if (!username.empty())

				flags |= 0x80;

			if (!password.empty())

				flags |= 0x40;

			data.push_back(flags);


			// keep alive is set to two minutes

			data.push_back(0x00);

			data.push_back(120);


			// Client ID

			data.push_back((client_id.length() >> 8) & 0xFF);

			data.push_back(client_id.length() & 0xFF);

			data.insert(data.end(), client_id.begin(), client_id.end());


			// Username if present

			if (!username.empty())

			{

				data.push_back((username.length() >> 8) & 0xFF);

				data.push_back(username.length() & 0xFF);

				data.insert(data.end(), username.begin(), username.end());
			}


			// Password if present

			if (!password.empty())

			{

				data.push_back((password.length() >> 8) & 0xFF);

				data.push_back(password.length() & 0xFF);

				data.insert(data.end(), password.begin(), password.end());
			}
		}


		int readRemainingLength()

		{

			int length = 0;

			size_t multiplier = 1;

			uint8_t encodedByte;

			do

			{

				if (prev->read((char*)&encodedByte, 1) != 1)

					return -1;

				length += (encodedByte & 127) * multiplier;

				multiplier *= 128;

				if (multiplier > 128 * 128 * 128)

					return -1;

			} while ((encodedByte & 128) != 0);


			return length;
		}


		void handshake()

		{

			Info() << "MQTT: Starting Handshake with broker " << broker_host << ":" << broker_port;

			connected = false;


			if (!prev)

				return;


			connectPacket();


			if (prev->send((char*)data.data(), data.size()) < 0)

			{

				Error() << "MQTT: Failed to send CONNECT packet";

				prev->disconnect();


				return;
			}


			uint8_t fixedHeader[1];


			if (prev->read((void*)fixedHeader, 1, 5) != 1)

			{

				Error() << "MQTT: Failed to read CONNACK fixed header, error ";

				prev->disconnect();


				return;
			}


			if ((fixedHeader[0] >> 4) != (uint8_t)(PacketType::CONNACK))

			{

				Error() << "MQTT: Expected CONNACK packet, but received different packet type";

				prev->disconnect();


				return;
			}


			int length = readRemainingLength();


			if (length < 0)

			{

				Error() << "MQTT: Failed to read CONNACK remaining length";

				prev->disconnect();

				return;
			}


			data.resize(length);


			int rv = prev->read((char*)data.data(), length, 5);

			if (rv != (int)length)

			{

				Error() << "MQTT: Failed to read CONNACK payload " << rv << " " << length;

				disconnect();

				return;
			}


			uint8_t connackReturnCode = data[1];


			if (connackReturnCode != 0)

			{

				disconnect();

				return;
			}


			connected = true;

			Info() << "MQTT: Connected to broker";


			if (subscribe)

			{
				subscribePacket();


				if (prev->send((char*)data.data(), data.size()) < 0)

				{

					Error() << "MQTT: Failed to send SUBSCRIBE packet";

					prev->disconnect();


					return;
				}


				if (!handleSubAck())

				{

					Error() << "MQTT: Failed to read SUBACK packet";

					prev->disconnect();

					return;
				}
			}
		}


		bool handleSubAck()

		{

			char h;

			if (prev->read(&h, 1, 5, true) != 1)

				return false;


			Info() << "MQTT: SUBACK packet received ";


			int length = readRemainingLength();

			if (length < 0)

				return false;


			data.resize(length);

			if (prev->read((char*)data.data(), length, 5, true) != length)

				return false;

			return true;
		}


	public:
		void onConnect()

		{

			if (!connected)

				handshake();


			ProtocolBase::onConnect();
		}


		void onDisconnect()

		{

			connected = false;

			ProtocolBase::onDisconnect();
		}


		bool setValue(const std::string& key, const std::string& value) {
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


		bool isConnected()

		{

			if (connected)

				return true;


			if (prev && prev->isConnected())

			{

				return connected;
			}


			return false;
		}


		int send(const void* str, int length) override

		{

			if (!isConnected())

				return 0;


			std::string msg = std::string((char*)str);


			int topic_len = topic.length();

			int total_len = 2 + topic_len + msg.length();


			uint8_t data[2048] = { 0 };


			if (msg.length() > sizeof(data) - 4 - topic_len)

			{

				Warning() << "MQTT: message too long, skipped";

				return -1;
			}


			data[0] = 0x30;

			data[0] |= (qos << 1) & 0x06;


			int pos = 1;

			do

			{

				uint8_t encoded_byte = total_len & 127;

				total_len = total_len >> 7;

				if (total_len > 0)

				{

					encoded_byte |= 128;
				}

				data[pos++] = encoded_byte;

			} while (total_len > 0);


			data[pos] = 0;

			data[pos + 1] = topic_len;


			memcpy(&data[pos + 2], topic.c_str(), topic_len);

			memcpy(&data[pos + 2 + topic_len], msg.c_str(), msg.length());


			return prev->send((char*)data, pos + 2 + topic_len + msg.length());
		}


		int read_ptr = 0;

		int read(void* data, int data_len, int t = 1, bool wait = false) override {
			if (!isConnected())
				return 0;

			if (buffer.empty())
				buffer.resize(16384);

			int len = prev->read((char*)buffer.data() + read_ptr, buffer.size() - read_ptr, t, wait);
			if (len <= 0)
				return len;

			read_ptr += len;

			if (read_ptr < 2)
				return 0;

			// check header
			if ((buffer[0] & 0xF0) != 0x30) {
				std::cerr << "MQTT: Invalid header" << std::endl;
				prev->disconnect();
				return -1;
			}

			int length = 0;
			int multiplier = 1;

			int i = 1;
			do {
				if (i >= read_ptr)
					return 0;

				length += (buffer[i] & 127) * multiplier;

				if (length > data_len) {
					prev->disconnect();
					Error() << "MQTT: Invalid length";
					return -1;
				}

				multiplier *= 128;
				i++;

			} while ((buffer[i - 1] & 128) != 0);

			if (read_ptr < length + i)
				return 0;

			int topic_len = (buffer[i] << 8) + buffer[i + 1];

			length -= topic_len + 2;
			i += 2 + topic_len;

			if (length < 0) {
				Error() << "MQTT: Invalid length";
				prev->disconnect();
				return -1;
			}

			memcpy(data, buffer.data() + i, length);
			memmove(buffer.data(), buffer.data() + length + i, read_ptr - length - i);

			read_ptr -= length + i;
			return length;
		}

		std::string getValues() {
			return "TOPIC: " + topic + " CLIENT_ID: " + client_id + " USERNAME: " + username + " PASSWORD: " + password + " QOS: " + std::to_string(qos);
		}
	};


}
