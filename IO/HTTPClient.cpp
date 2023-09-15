/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#include "HTTPClient.h"

#ifdef HASOPENSSL
static class SSLContext {
	    SSL_CTX* ctx = nullptr;

	public:

		SSL_CTX* getContext() {
			if(!ctx) {

				(void)SSL_library_init();
				SSL_load_error_strings();

				ctx = SSL_CTX_new(SSLv23_client_method());

				if (!ctx) {
					throw "SSL_CTX_new() failed.";
				}
			}
			return ctx;
		}
		~SSLContext() { 
			if(ctx) SSL_CTX_free(ctx);
		 }
} _sse_context;
#endif

namespace IO {
	void HTTPClient::createMessageBody(const std::string &msg, bool gzip, bool multipart, const std::string &copyname) {

		// multipart
		if(multipart) {
			message = "--" + boundary + "\n";
			message += "Content-Disposition: form-data; name=\"" + copyname + "\"\n";
			message += "Content-Type: application/json\n\n";
			message += msg;
			message += "\n--"+boundary+"--\n";

			msg_ptr = (void*)message.c_str();
			msg_length = message.length();

			return;
		}
		// plain & zipped
		if (gzip) {
			zip.zip(msg);

			msg_length = zip.getOutputLength();
			msg_ptr = zip.getOutputPtr();
			return;
		}

		// plain 
		message = msg;
		msg_ptr = (void*)message.c_str();
		msg_length = message.length();
	}

	void HTTPClient::createHeader(bool gzip, bool multipart) {
		if(!multipart) {
			header = "POST " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
			if (!userpwd.empty()) {
				header += "Authorization: Basic " + base64_encode(userpwd) + "\r\n";
			}
			header += "Accept: */*\r\nContent-Type: application/json\r\n";
			if (gzip) header += "Content-Encoding: gzip\r\n";
			header += "Content-Length: " + std::to_string(msg_length) + "\r\n\r\n";

			return;
		}

		header = "POST " + path + " HTTP/1.1\r\n";
		header += "Host: " + host + "\r\n";
		header += "Accept: */*\r\n";
		header += "Content-Length: " + std::to_string(msg_length) + "\r\n";
		header += "Content-Type: multipart/form-data; boundary="+boundary+"\r\n\r\n";
	} 

	bool HTTPClient::Handshake() {
#ifdef HASOPENSSL
		ssl = SSL_new(_sse_context.getContext());
		if (!ssl) {
			std::cerr << "HTTP Client [" << host << "]: error during SSL new - error code : " << ERR_get_error() << std::endl;
			return false;
		}

		SSL_set_tlsext_host_name(ssl, host.c_str());
		SSL_set_fd(ssl, client.getSocket());

		if (SSL_connect(ssl) != 1) {
			std::cerr << "HTTP Client [" << host << "]: error during SSL handshake - error code : " << ERR_get_error() << std::endl;
			return false;
		}
		return true;
#else
		std::cerr << "HTTP Client [" << host << "]: error during Handshake, SSL not supported in this build." << std::endl;

		return false;
#endif
	}

	bool HTTPClient::Post(const std::string &msg, std::string &reply, bool gzip, bool multipart, const std::string &copyname) {
		int r;

		createMessageBody(msg, gzip, multipart, copyname);
		createHeader(gzip, multipart);

		if (!client.connect(host, port, false, 1)) {
			std::cerr << "HTTP Client [" << host << "]: error connecting to server." << std::endl;
			return false;
		}

		std::memset(response, 0, sizeof(response));

		if(secure) {
#ifdef HASOPENSSL
			std::cerr << " HTTP Client [" << host << "]: SSL enabled." << std::endl;
			if(!Handshake()) return false;

			r = SSL_write(ssl, header.c_str(), header.length());
			if (r <= 0) {
				std::cerr << "HTTP Client [" << host << "]: SSL write failed - error code : " << ERR_get_error() << std::endl;
				return false;
			}
			
			r = SSL_write(ssl, msg_ptr, msg_length);
			if (r <= 0) {
				std::cerr << "HTTP Client [" << host << "]: SSL write failed - error code : " << ERR_get_error() << std::endl;
				return false;
			}

			r = SSL_read(ssl, response, sizeof(response));
			if (r <= 0) {
				std::cerr << "HTTP Client [" << host << "]: SSL read failed - error code : " << ERR_get_error() << std::endl;
				return false;
			}

			r = SSL_shutdown(ssl);
			if (r < 0) {
				std::cerr << "HTTP Client [" << host << "]: SSL shutdown failed - error code : " << ERR_get_error() << std::endl;
			}

	        SSL_free(ssl);
			ssl = nullptr;
#else
			std::cerr << "HTTP Client [" << host << "]: SSL not supported." << std::endl;
			return false;
#endif
		}
		else {

			if(client.send(header.c_str(), header.length()) < 0) {
				std::cerr << "HTTP Client [" << host << "]: write failed" << std::endl;
				return false;
			}
			if(client.send(msg_ptr, msg_length) < 0) {
				std::cerr << "HTTP Client [" << host << "]: write failed" << std::endl;
				return false;
			}

			client.read(response, sizeof(response), 1, false);
		}
		reply = std::string(response);
		client.disconnect();

		return true;
	}

	std::string HTTPClient::base64_encode(const std::string &in) {
		const char base64_chars[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789+/";
			
		std::string out;
		int val = 0, valb = -6;
		for (uint8_t c : in) {
			val = (val << 8) + c;
			valb += 8;
			while (valb >= 0) {
				out.push_back(base64_chars[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}
		if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
		while (out.size() % 4) out.push_back('=');
		return out;
	}

}