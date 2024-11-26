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

#include "Protocol.h"

namespace Protocol {
	void TCP::disconnect() {

		if (sock != -1) {
			onDisconnect();
			closesocket(sock);
		}

		if (state == READY)
			Info() << "TCP (" << host << ":" << port << "): disconnected.";

		sock = -1;
		state = DISCONNECTED;
	}

	bool TCP::connect() {

		state = DISCONNECTED;
		stamp = time(nullptr);

		int r;
		struct addrinfo h;
		struct addrinfo* address = nullptr;

		std::memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;

#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);
		if (code != 0 || address == nullptr)
			return false;

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1) {
			freeaddrinfo(address);
			return false;
		}

#ifndef _WIN32
		if (keep_alive) {
			int optval = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
				freeaddrinfo(address);
				disconnect();
				return false;
			}
		}
#endif

		if (persistent) {
#ifndef _WIN32
			r = fcntl(sock, F_GETFL, 0);
			if (r == -1) { // Fixed: Check return value of fcntl()
				freeaddrinfo(address);
				disconnect();
				Error() << "TCP (" << host << ":" << port << "): fcntl F_GETFL failed: " << strerror(errno);
				return false;
			}
			r = fcntl(sock, F_SETFL, r | O_NONBLOCK);
			if (r == -1) {
				freeaddrinfo(address);
				disconnect();
				Error() << "TCP (" << host << ":" << port << "): fcntl F_SETFL failed: " << strerror(errno);
				return false;
			}
#else
			u_long mode = 1;							  // 1 to enable non-blocking socket
			if (ioctlsocket(sock, FIONBIO, &mode) != 0) { // Fixed: Check return value of ioctlsocket()
				freeaddrinfo(address);
				disconnect();
				int error_code = WSAGetLastError();
				Error() << "TCP (" << host << ":" << port << "): ioctlsocket failed. Error code: " << error_code;
				return false;
			}
#endif
		}

		r = ::connect(sock, address->ai_addr, (int)address->ai_addrlen);
		freeaddrinfo(address);

		if (r != -1) {
			state = READY;

			Info() << "TCP (" << host << ":" << port << "): connected.";
			onConnect();

			return true;
		}

#ifndef _WIN32

		if (errno != EINPROGRESS) {
			disconnect();
			return false;
		}
#else
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			disconnect();
			return false;
		}
#endif

		return isConnected(timeout) || persistent;
	}


	bool TCP::isConnected(int t) {

		if (state == READY)
			return true;

		if (sock == -1)
			return false;

		fd_set fdr, fdw;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		timeval to = { t, 1 }; // t seconds and 1 microsecond

		if (select(sock + 1, &fdr, &fdw, nullptr, &to) > 0) {
			int error = -1;
			socklen_t len = sizeof(error);

			if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) != 0) {
				Error() << "TCP (" << host << ":" << port << "): getsockopt failed.";
				disconnect();
				return false;
			}

			if (error != 0) {
				Error() << "TCP (" << host << ":" << port << "): connection failed with error " << error << " (" << strerror(error) << ").";
				disconnect();
				return false;
			}

			state = READY;

			Info() << "TCP (" << host << ":" << port << "): connected.";
			onConnect();

			return true;
		}


		state = CONNECTING;
		return false;
	}

	void TCP::updateState() {

		if (state == READY && reset_time > 0 && std::difftime(time(nullptr), stamp) > reset_time * 60) {
			Warning() << "TCP (" << host << ":" << port << "): connection expired, reconnect.";
			reconnect();
		}

		else if (state == DISCONNECTED) {
			if (std::difftime(time(nullptr), stamp) > RECONNECT_TIME) {
				Warning() << "TCP (" << host << ":" << port << "): not connected, reconnecting.";
				reconnect();
			}
		}

		else if (state == CONNECTING) {
			bool connected = isConnected(0);

			if (!connected && std::difftime(time(nullptr), stamp) > RECONNECT_TIME) {
				Warning() << "TCP (" << host << ":" << port << "): timeout connecting to server, reconnect.";
				reconnect();

				return;
			}
		}
	}

	int TCP::send(const void* data, int length) {
		if (state == READY) {

			int sent = ::send(sock, (char*)data, length, 0);

			if (sent < length) {
				int error_code = errno;
#ifdef _WIN32
				if (error_code == WSAEWOULDBLOCK)
					return 0;

#else
				if (error_code == EAGAIN || error_code == EWOULDBLOCK || error_code == EINPROGRESS) {
					Error() << "TCP (" << host << ":" << port << "): message might be lost. Error code: " << error_code << " (" << strerror(error_code) << ").";
					return 0;
				}

#endif
				Error() << "TCP (" << host << ":" << port << "): send error. Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");
				if (persistent) {
					reconnect();
					return 0;
				}
				else {
					return -1;
				}
			}
			return sent;
		}

		else
			updateState();


		return 0;
	}

	int TCP::read(void* data, int length, int t, bool wait) {
		if (state == READY) {
			fd_set fd, fe;

			FD_ZERO(&fd);
			FD_SET(sock, &fd);

			FD_ZERO(&fe);
			FD_SET(sock, &fe);

			timeval to = { t, 0 };

			if (select(sock + 1, &fd, nullptr, &fe, &to) < 0)
				return 0;


			if (FD_ISSET(sock, &fd) || FD_ISSET(sock, &fe)) {
				int retval = recv(sock, (char*)data, length, wait ? MSG_WAITALL : 0);

				if (retval <= 0) {
					int error_code = errno;
#ifdef _WIN32
					if (error_code == WSAEWOULDBLOCK)
						return 0;
#else
					if (error_code == EAGAIN || error_code == EWOULDBLOCK)
						return 0;
#endif

					Info() << "TCP (" << host << ":" << port << ") receive error with recv returns " << retval << ". Error code: " << error_code << " (" << strerror(error_code) << ")." << (persistent ? " Reconnect." : " Failed.");

					if (persistent) {
						reconnect();
						return 0;
					}
					else {
						return -1;
					}
				}

				return retval;
			}
		}
		else {
			updateState();
			return 0;
		}

		return 0;
	}

	void WebSocket::onConnect() {
		if (!performHandshake()) {
			Error() << "WebSocket: Handshake failed.";
			disconnect();
			return;
		}

		connected = true;
		ProtocolBase::onConnect();
	}

	bool WebSocket::performHandshake() {
		if (!prev) {
			Error() << "WebSocket: No underlying connection.";
			return false;
		}

		// Generate a random Sec-WebSocket-Key
		std::array<uint8_t, 16> key_bytes;
		for (auto& byte : key_bytes) {
			byte = rand() % 256;
		}
		std::string secWebSocketKey = base64Encode(key_bytes.data(), key_bytes.size());

		// Build the handshake request
		std::ostringstream request;
		request << "GET " << path << " HTTP/1.1\r\n";
		request << "Host: " << getHost() << "\r\n";
		request << "Upgrade: websocket\r\n";
		request << "Connection: Upgrade\r\n";
		request << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n";
		request << "Sec-WebSocket-Version: 13\r\n";

		if (!origin.empty()) {
			request << "Origin: " << origin << "\r\n";
		}
		if (!protocols.empty()) {
			request << "Sec-WebSocket-Protocol: " << protocols << "\r\n";
		}
		request << "\r\n";

		std::string request_str = request.str();

		// Send the handshake request
		if (prev->send(request_str.c_str(), request_str.size()) != (int)request_str.size()) {
			Error() << "WebSocket: Failed to send handshake request.";
			return false;
		}

		// Read the response
		char response[2048];
		int len = prev->read(response, sizeof(response), 5, true);
		if (len <= 0) {
			Error() << "WebSocket: No response to handshake request.";
			return false;
		}

		std::string response_str(response, len);

		// Parse the response
		if (response_str.find("HTTP/1.1 101") == std::string::npos &&
			response_str.find("HTTP/1.0 101") == std::string::npos) {
			Error() << "WebSocket: Invalid handshake response.";
			return false;
		}

		if (response_str.find("Upgrade: websocket") == std::string::npos ||
			response_str.find("Connection: Upgrade") == std::string::npos) {
			Error() << "WebSocket: Missing required headers in handshake response.";
			return false;
		}

		// Validate Sec-WebSocket-Accept
		std::string accept_key;
		std::istringstream resp_stream(response_str);
		std::string line;
		while (std::getline(resp_stream, line)) {
			if (line.find("Sec-WebSocket-Accept:") != std::string::npos) {
				accept_key = line.substr(line.find(":") + 2); // Add 2 to skip ": "
				// Remove trailing whitespace and CR/LF
				accept_key = accept_key.substr(0, accept_key.find_last_not_of(" \r\n") + 1);
				break;
			}
		}

		if (accept_key.empty()) {
			Error() << "WebSocket: Missing Sec-WebSocket-Accept header.";
			return false;
		}

		// Compute the expected accept key
		std::string concatenated = secWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string expected_accept_key = base64Encode(reinterpret_cast<const uint8_t*>(sha1Hash(concatenated).c_str()), 20);

		// Trim both keys to ensure exact comparison
		accept_key.erase(std::remove_if(accept_key.begin(), accept_key.end(), ::isspace), accept_key.end());
		expected_accept_key.erase(std::remove_if(expected_accept_key.begin(), expected_accept_key.end(), ::isspace), expected_accept_key.end());

		if (accept_key != expected_accept_key) {
			Error() << "WebSocket: Invalid Sec-WebSocket-Accept value: '" << accept_key << "' expected: '" << expected_accept_key << "'";
			return false;
		}

		handshakeDone = true;
		return true;
	}

	int WebSocket::send(const void* data, int length) {
		if (!connected || !prev) {
			Error() << "WebSocket: Not connected.";
			return -1;
		}

		std::vector<uint8_t> frame = createFrame(data, length);

		int sent = prev->send(frame.data(), frame.size());
		if (sent != (int)frame.size()) {
			Error() << "WebSocket: Failed to send frame.";
			return -1;
		}

		return length;
	}

	std::vector<uint8_t> WebSocket::createFrame(const void* data, int length) {
		std::vector<uint8_t> frame;
		uint8_t opcode = 0x1; // Text frame

		frame.push_back(0x80 | opcode); // FIN bit set and opcode

		if (length <= 125) {
			frame.push_back(0x80 | length); // Mask bit set and payload length
		}
		else if (length <= 65535) {
			frame.push_back(0x80 | 126);
			frame.push_back((length >> 8) & 0xFF);
			frame.push_back(length & 0xFF);
		}
		else {
			frame.push_back(0x80 | 127);
			for (int i = 7; i >= 0; --i) {
				frame.push_back((length >> (8 * i)) & 0xFF);
			}
		}

		// Generate a random masking key
		uint8_t masking_key[4];
		for (int i = 0; i < 4; ++i) {
			masking_key[i] = rand() % 256;
		}
		frame.insert(frame.end(), masking_key, masking_key + 4);

		// Mask the payload data
		const uint8_t* data_bytes = reinterpret_cast<const uint8_t*>(data);
		for (int i = 0; i < length; ++i) {
			frame.push_back(data_bytes[i] ^ masking_key[i % 4]);
		}

		return frame;
	}

	int WebSocket::read(void* data, int length, int t, bool wait) {
		if (!connected || !prev) {
			return -1;
		}

		uint8_t tmp_buf[2048];
		int len = prev->read(tmp_buf, sizeof(tmp_buf), t, wait);
		if (len <= 0) {
			return len;
		}

		buffer.insert(buffer.end(), tmp_buf, tmp_buf + len);

		std::vector<uint8_t> frame(buffer.begin(), buffer.end());
		int payload_length = parseFrame(frame, data, length);
		if (payload_length < 0) {
			return 0;
		}
		else {
			buffer.erase(buffer.begin(), buffer.begin() + frame.size());
			return payload_length;
		}
	}

	int WebSocket::parseFrame(std::vector<uint8_t>& frame, void* data, int data_len) {
		if (frame.size() < 2) {
			return -1; // Need more data
		}

		uint8_t b2 = frame[1];

		bool mask = b2 & 0x80;
		uint64_t payload_len = b2 & 0x7F;

		size_t pos = 2;

		if (payload_len == 126) {
			if (frame.size() < pos + 2) {
				return -1; // Need more data
			}
			payload_len = (frame[pos] << 8) | frame[pos + 1];
			pos += 2;
		}
		else if (payload_len == 127) {
			if (frame.size() < pos + 8) {
				return -1; // Need more data
			}
			payload_len = 0;
			for (int i = 0; i < 8; ++i) {
				payload_len = (payload_len << 8) | frame[pos + i];
			}
			pos += 8;
		}

		if (mask) {
			if (frame.size() < pos + 4) {
				return -1; // Need more data
			}
		}

		if (frame.size() < pos + (mask ? 4 : 0) + payload_len) {
			return -1; // Need more data
		}

		uint8_t masking_key[4];
		if (mask) {
			masking_key[0] = frame[pos++];
			masking_key[1] = frame[pos++];
			masking_key[2] = frame[pos++];
			masking_key[3] = frame[pos++];
		}

		if (payload_len > (uint64_t)data_len) {
			Error() << "WebSocket: Payload too large.";
			return -2;
		}

		uint8_t* data_bytes = reinterpret_cast<uint8_t*>(data);

		for (uint64_t i = 0; i < payload_len; ++i) {
			data_bytes[i] = frame[pos + i];
			if (mask) {
				data_bytes[i] ^= masking_key[i % 4];
			}
		}

		return payload_len;
	}

	bool WebSocket::isConnected() {

		if (connected)
			return true;

		if (prev)
			connected = prev->isConnected();

		return false;
	}


	void WebSocket::onDisconnect() {
		connected = false;
		handshakeDone = false;
		buffer.clear();
		ProtocolBase::onDisconnect();
	}

	bool WebSocket::setValue(const std::string& key, const std::string& value) {

		if (key == "ORIGIN") {
			origin = value;
		}
		else if (key == "PROTOCOLS") {
			protocols = value;
		}
		else {
			return false;
		}
		return true;
	}

	std::string WebSocket::getValues() {
		return "host " + getHost() + " path " + path + " origin " + origin + " protocols " + protocols;
	}

	std::string WebSocket::sha1Hash(const std::string& d) {

		unsigned char hash[20];

		uint32_t h0 = 0x67452301;
		uint32_t h1 = 0xEFCDAB89;
		uint32_t h2 = 0x98BADCFE;
		uint32_t h3 = 0x10325476;
		uint32_t h4 = 0xC3D2E1F0;

		// Pre-processing
		uint64_t original_bit_len = d.length() * 8;
		std::vector<uint8_t> msg(d.data(), d.data() + d.length());

		// Append the bit '1'
		msg.push_back(0x80);

		// Append zeros
		while ((msg.size() % 64) != 56) {
			msg.push_back(0x00);
		}

		// Append length
		for (int i = 7; i >= 0; --i) {
			msg.push_back((original_bit_len >> (i * 8)) & 0xFF);
		}

		// Process message in 512-bit chunks
		auto leftrotate = [](uint32_t x, uint32_t c) -> uint32_t {
			return (x << c) | (x >> (32 - c));
		};

		for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
			uint32_t w[80];

			// Break chunk into sixteen 32-bit words
			for (int i = 0; i < 16; i++) {
				w[i] = (msg[chunk + 4 * i] << 24) |
					   (msg[chunk + 4 * i + 1] << 16) |
					   (msg[chunk + 4 * i + 2] << 8) |
					   (msg[chunk + 4 * i + 3]);
			}

			// Extend the sixteen 32-bit words into eighty 32-bit words
			for (int i = 16; i < 80; i++) {
				w[i] = leftrotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
			}

			// Initialize hash values for this chunk
			uint32_t a = h0;
			uint32_t b = h1;
			uint32_t c = h2;
			uint32_t d = h3;
			uint32_t e = h4;

			// Main loop
			for (int i = 0; i < 80; i++) {
				uint32_t f, k;

				if (i < 20) {
					f = (b & c) | ((~b) & d);
					k = 0x5A827999;
				}
				else if (i < 40) {
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				}
				else if (i < 60) {
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				}
				else {
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}

				uint32_t temp = leftrotate(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = leftrotate(b, 30);
				b = a;
				a = temp;
			}

			// Add this chunk's hash to result
			h0 += a;
			h1 += b;
			h2 += c;
			h3 += d;
			h4 += e;
		}


		// Store hash as big-endian bytes
		hash[0] = (h0 >> 24) & 0xFF;
		hash[1] = (h0 >> 16) & 0xFF;
		hash[2] = (h0 >> 8) & 0xFF;
		hash[3] = h0 & 0xFF;

		hash[4] = (h1 >> 24) & 0xFF;
		hash[5] = (h1 >> 16) & 0xFF;
		hash[6] = (h1 >> 8) & 0xFF;
		hash[7] = h1 & 0xFF;

		hash[8] = (h2 >> 24) & 0xFF;
		hash[9] = (h2 >> 16) & 0xFF;
		hash[10] = (h2 >> 8) & 0xFF;
		hash[11] = h2 & 0xFF;

		hash[12] = (h3 >> 24) & 0xFF;
		hash[13] = (h3 >> 16) & 0xFF;
		hash[14] = (h3 >> 8) & 0xFF;
		hash[15] = h3 & 0xFF;

		hash[16] = (h4 >> 24) & 0xFF;
		hash[17] = (h4 >> 16) & 0xFF;
		hash[18] = (h4 >> 8) & 0xFF;
		hash[19] = h4 & 0xFF;

		// Return raw bytes as string
		return std::string(reinterpret_cast<char*>(hash), 20);
	}

	std::string WebSocket::base64Encode(const uint8_t* data, size_t len) {
		static const char base64_chars[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string encoded;
		encoded.reserve(((len + 2) / 3) * 4);

		size_t i = 0;
		while (i < len) {
			uint32_t octet_a = i < len ? data[i++] : 0;
			uint32_t octet_b = i < len ? data[i++] : 0;
			uint32_t octet_c = i < len ? data[i++] : 0;

			uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

			encoded += base64_chars[(triple >> 18) & 0x3F];
			encoded += base64_chars[(triple >> 12) & 0x3F];
			encoded += base64_chars[(triple >> 6) & 0x3F];
			encoded += base64_chars[triple & 0x3F];
		}

		if (len % 3 == 1) {
			encoded[encoded.length() - 1] = '=';
			encoded[encoded.length() - 2] = '=';
		}
		else if (len % 3 == 2) {
			encoded[encoded.length() - 1] = '=';
		}

		return encoded;
	}
}
