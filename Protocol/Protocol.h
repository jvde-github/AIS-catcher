/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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
		ProtocolBase() = default;
		virtual ~ProtocolBase() {}

		ProtocolBase* getPrev() { return prev; }
		ProtocolBase* getNext() { return next; }

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
		const int RECONNECT_TIME = 10;

	public:
		void disconnect() override;
		bool connect() override;

		int read(void* data, int length, int t, bool wait) override;
		int send(const void* data, int length) override;

		bool isConnected() override {
			if (state == READY)
				return true;

			updateState();
			return state == READY;
		}

		bool setValue(const std::string& key, const std::string& value) override {

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

		std::string getValues() override {
			return "host " + host + " port " + port + " persist " + Util::Convert::toString(persistent) + " keep_alive " + Util::Convert::toString(keep_alive);
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

		std::vector<uint8_t> packet, buffer;
		int buffer_ptr = 0;

		enum class PacketType {
			CONNECT = 1,
			CONNACK = 2,
			PUBLISH = 3,
			PUBACK = 4,
			SUBSCRIBE = 8,
			SUBACK = 9,
			DISCONNECT = 14
		};

		std::string host, port, username, password;
		std::string topic = "ais/data";
		std::string client_id;

		int qos = 0;
		int packet_id = 1;

		bool connected = false;
		bool subscribe = false;

		void pushVariableLength(int length) {
			if (length >= 128 * 128 * 128 * 128) {
				Error() << "MQTT: Length encoding error: " << length;
				disconnect();
				return;
			}

			do {
				uint8_t encodedByte = length % 128;

				length /= 128;
				if (length > 0) {
					encodedByte |= 128;
				}

				packet.push_back(encodedByte);

			} while (length > 0);
		}

		void pushInt(int length) {
			packet.push_back(length >> 8);
			packet.push_back(length & 0xFF);
		}

		void pushByte(uint8_t byte) {
			packet.push_back(byte);
		}

		void createPacket(uint8_t type) {
			packet.resize(0);
			packet.push_back(type);
		}

		void pushString(const std::string& str) {
			pushInt(str.length());
			packet.insert(packet.end(), str.begin(), str.end());
		}

		void subscribePacket() {

			int packet_length = 2 + 2 + topic.length() + 1;

			createPacket(0x82);
			pushVariableLength(packet_length);

			pushInt(1);
			pushString(topic);
			pushByte(qos);
		}

		void connectPacket() {

			packet.push_back(0x10);
			createPacket(0x10);

			int length = 12 + client_id.length();
			uint8_t flags = 0x02;

			if (!username.empty()) {
				length += 2 + username.length();
				flags |= 0x80;
			}
			if (!password.empty()) {
				length += 2 + password.length();
				flags |= 0x40;
			}

			pushVariableLength(length);

			pushString("MQTT");
			pushByte(0x04); // MQTT 3.1.1
			pushByte(flags);

			// keep alive is set to two minutes
			pushInt(120);

			pushString(client_id);

			if (!username.empty()) pushString(username);
			if (!password.empty()) pushString(password);
		}


		int readRemainingLength() {
			int length = 0;
			int multiplier = 1;
			uint8_t b;

			do {
				if (prev->read((char*)&b, 1) != 1)
					return -1;

				length += (b & 127) * multiplier;

				multiplier *= 128;

				if (multiplier > 128 * 128 * 128)
					return -1;

			} while ((b & 128) != 0);


			return length;
		}


		void handshake() {

			uint8_t b;
			int length;

			Info() << "MQTT: Starting Handshake with broker " << host << ":" << port;

			connected = false;

			if (!prev) return;

			connectPacket();

			if (prev->send(packet.data(), packet.size()) < 0) {
				Error() << "MQTT: Failed to send CONNECT packet";
				disconnect();
				return;
			}

			if (!readPacket(b, length) || packet.size() < 2 || (b >> 4) != (uint8_t)(PacketType::CONNACK)) {
				Error() << "MQTT: Failed to read CONNACK fixed header.";
				disconnect();
				return;
			}

			if (packet[1] != 0) {
				switch (packet[1]) {
				case 1:
					Error() << "MQTT: Connection Refused - Unacceptable Protocol Version.";
					break;
				case 2:
					Error() << "MQTT: Connection Refused - Identifier Rejected.";
					break;
				case 3:
					Error() << "MQTT: Connection Refused - Server Unavailable.";
					break;
				case 4:
					Error() << "MQTT: Connection Refused - Not Authorized.";
					break;
				case 5:
					Error() << "MQTT: Connection Refused - Bad Username or Password.";
					break;
				case 6:
					Error() << "MQTT: Connection Refused - Not Authorized (Enhanced).";
					break;
				default:
					Error() << "MQTT: Connection Refused - Unknown Return Code: " << (int)packet[1];
				}
				disconnect();
				return;
			}


			if (subscribe) {
				subscribePacket();

				if (prev->send((char*)packet.data(), packet.size()) < 0) {
					Error() << "MQTT: Failed to send SUBSCRIBE packet";
					disconnect();
					return;
				}

				if (!readPacket(b, length)) {
					Error() << "MQTT: Failed to read SUBACK packet";
					disconnect();
					return;
				}
			}
			Info() << "MQTT: Connected to broker " << (subscribe ? "and subscribed" : "");
			connected = true;
		}

		bool readPacket(uint8_t& type, int& length) {
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
		void onConnect() override {
			if (!connected)
				handshake();

			buffer_ptr = 0;

			ProtocolBase::onConnect();
		}

		void onDisconnect() override {
			ProtocolBase::onDisconnect();

			connected = false;
			buffer_ptr = 0;
		}

		bool setValue(const std::string& key, const std::string& value) override {
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
			else if (key == "HOST")
				host = value;
			else if (key == "PORT")
				port = value;
			else
				return false;

			return true;
		}

		bool isConnected() override {
			if (connected)
				return true;

			if (prev && prev->isConnected()) {
				return connected;
			}

			return false;
		}

		int send(const void* str, int length) override {
			if (!isConnected())
				return 0;

			if (length > 2048) {
				Warning() << "MQTT: message too long, skipped";
				return -1;
			}

			createPacket(0x30 | (qos << 1));

			int packet_length = 2 + topic.length() + length + (qos > 0 ? 2 : 0);

			pushVariableLength(packet_length);
			pushString(topic);

			if (qos > 0) pushInt(packet_id++);

			packet.insert(packet.end(), (char*)str, (char*)str + length);

			return prev->send(packet.data(), packet.size());
		}

		int read(void* data, int data_len, int t = 1, bool wait = false) override {

			if (!isConnected())
				return 0;

			if (buffer.empty())
				buffer.resize(16384);

			int len = prev->read((char*)buffer.data() + buffer_ptr, buffer.size() - buffer_ptr, t, wait);
			if (len <= 0)
				return len;

			buffer_ptr += len;

			if (buffer_ptr < 2)
				return 0;

			int length = 0;
			int multiplier = 1;

			int i = 1;
			do {
				if (i >= buffer_ptr)
					return 0;

				length += (buffer[i] & 127) * multiplier;

				if (length > data_len) {
					disconnect();
					Error() << "MQTT: Invalid length";
					return -1;
				}

				multiplier *= 128;
				i++;

			} while ((buffer[i - 1] & 128) != 0);

			if (buffer_ptr < length + i)
				return 0;

			int topic_len = (buffer[i] << 8) + buffer[i + 1];

			length -= topic_len + 2;
			i += 2 + topic_len;

			if (length < 0) {
				Error() << "MQTT: Invalid length";
				disconnect();
				return -1;
			}

			// check header
			if ((buffer[0] & 0xF0) == 0x30) {
				memcpy(data, buffer.data() + i, length);
				memmove(buffer.data(), buffer.data() + length + i, buffer_ptr - length - i);
				buffer_ptr -= length + i;
				return length;
			}
			else {
				std::cerr << "MQTT: packet received: " << ((int)buffer[0] >> 4) << std::endl;
				memmove(buffer.data(), buffer.data() + length + i, buffer_ptr - length - i);
				buffer_ptr -= length + i;
			}

			return 0;
		}

		std::string getValues() override {
			return "topic " + topic + " client_id " + client_id + " username " + username + " password " + password + " qos " + std::to_string(qos);
		}
	};

	class GPSD : public ProtocolBase {
		void onConnect() override {
			const std::string str = "?WATCH={\"enable\":true,\"json\":true,\"nmea\":false}\n";
			if (prev) {
				int len = prev->send(str.c_str(), str.size());
				if (len != str.size()) {
					Error() << "GPSD: no or invalid response, likely not a gpsd server.";
					disconnect();
				}
			}

			ProtocolBase::onConnect();
		}

		void onDisconnect() override {
			ProtocolBase::onDisconnect();

			if (prev) {
				const std::string str = "?WATCH={\"enable\":false}\n";
				prev->send(str.c_str(), str.size());
			}
		}
	};

	class RTLTCP : public ProtocolBase {

	protected:
		bool connected = false;

		void sendParam(uint8_t c, uint32_t p) {
			char instruction[5];

			instruction[0] = c;
			instruction[4] = p;
			instruction[3] = p >> 8;
			instruction[2] = p >> 16;
			instruction[1] = p >> 24;
			if (prev) prev->send(instruction, 5);
		}

		FLOAT32 freq_offset = 0;
		int tuner_bandwidth = 0;
		uint32_t frequency = 0;
		uint32_t sample_rate = 0;
		bool tuner_AGC = true;
		bool RTL_AGC = false;
		FLOAT32 tuner_Gain = 33.0;

		void applySettings() {

			std::cerr << "RTLTCP: setting parameters" << std::endl;
			sendParam(5, freq_offset);
			sendParam(3, tuner_AGC ? 0 : 1);

			if (!tuner_AGC) sendParam(4, tuner_Gain * 10);
			if (RTL_AGC) sendParam(8, 1);

			sendParam(2, sample_rate);
			sendParam(1, frequency);
		}


	public:
		void onConnect() override {
			applySettings();

			struct {
				uint32_t magic = 0, tuner = 0, gain = 0;
			} dongle;

			int len = prev->read((char*)&dongle, 12, 5);
			if (len != 12 || dongle.magic != 0x304C5452) {
				Error() << "RTLTCP: no or invalid response, likely not an rtl-tcp server.";
				disconnect();
			}
			connected = true;
			ProtocolBase::onConnect();
		}

		void onDisconnect() override {
			ProtocolBase::onDisconnect();
			connected = false;
		}

		int read(void* data, int data_len, int t = 1, bool wait = false) override {
			if (prev && connected) return prev->read(data, data_len, t, wait);

			return 0;
		}

		bool setValue(const std::string& key, const std::string& value) override {
			if (key == "TUNER") {
				tuner_AGC = Util::Parse::AutoFloat(value, 0, 50, tuner_Gain);
			}
			else if (key == "RTLAGC") {
				RTL_AGC = Util::Parse::Switch(value);
			}
			else if (key == "RATE" || key == "SAMPLE_RATE") {
				sample_rate = ((Util::Parse::Integer(value, 0, 20000000)));
			}
			else if (key == "BW" || key == "BANDWIDTH") {
				tuner_bandwidth = Util::Parse::Integer(value, 0, 1000000);
			}
			else if (key == "FREQOFFSET") {
				freq_offset = Util::Parse::Float(value, -150, 150);
			}
			else if (key == "FREQUENCY") {
				frequency = Util::Parse::Integer(value, 0, 2000000000);
			}
			else
				return false;

			return true;
		}

		std::string getValues() override {
			return "frequency " + std::to_string(frequency) + " freqoffset " + std::to_string(freq_offset) + " rate " + std::to_string(sample_rate) + " bandwidth " + std::to_string(tuner_bandwidth) + " tuner " + std::to_string(tuner_Gain) + " rtlagc " + Util::Convert::toString(RTL_AGC);
		}
	};
}
