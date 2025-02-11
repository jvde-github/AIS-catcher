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

#include "HTTPClient.h"

#ifdef HASOPENSSL
static class SSLContext {
	SSL_CTX* ctx = nullptr;

public:
	SSL_CTX* getContext() {
		if (!ctx) {

			(void)SSL_library_init();
			SSL_load_error_strings();

			ctx = SSL_CTX_new(SSLv23_client_method());

			if (!ctx) {
				throw std::runtime_error("SSL_CTX_new() failed.");
			}
		}
		return ctx;
	}
	~SSLContext() {
		if (ctx) SSL_CTX_free(ctx);
	}
} _sse_context;
#endif

namespace IO {

	void HTTPClient::parseResponse(HTTPResponse& result, const std::string& response) {

		try {
			std::istringstream iss(response);

			std::string httpVersion, statusMessage;
			int contentlength = 0;

			iss >> httpVersion >> result.status;

			std::getline(iss, statusMessage);

			std::string line;
			bool isChunked = false;

			// Parse headers
			while (std::getline(iss, line) && line != "\r" && !line.empty()) {
				if (line.find("Content-Length:") != std::string::npos) {
					contentlength = std::stoi(line.substr(16));
				}
				else if (line.find("Transfer-Encoding: chunked") != std::string::npos || line.find("Transfer-Encoding:chunked") != std::string::npos) {
					isChunked = true;
				}
				else if (line.find("Transfer-Encoding:") != std::string::npos) {
					Error() << "HTTP Client [" << host << "]: Transfer-Encoding other than chunked not supported." ;
					result.status = -2;
					return;
				}
			}

			if (isChunked) {
				std::getline(iss, line);
				contentlength = std::stoi(line, nullptr, 16);
			}

			if (contentlength > 0) {
				result.message.resize(contentlength);
				iss.read(&result.message[0], contentlength);
			}
		}
		catch (const std::exception&) {
			result.status = -3;
			result.message = "Error parsing response.";
			return;
		}
	}

	void HTTPClient::createMessageBody(const std::string& msg, bool gzip, bool multipart, const std::string& copyname) {

		// multipart
		if (multipart) {
			message = "--" + boundary + "\n";
			message += "Content-Disposition: form-data; name=\"" + copyname + "\"\n";
			message += "Content-Type: application/json\n\n";
			message += msg;
			message += "\n--" + boundary + "--\n";

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

		header = "POST " + path + " HTTP/1.1\r\nHost: " + host + ":" + port + "\r\nAccept: */*\r\n";
		if (!userpwd.empty()) {
			header += "Authorization: Basic " + Util::Convert::BASE64toString(userpwd) + "\r\n";
		}

		if (!multipart) {
			header += "Content-Type: application/json\r\n";
			if (gzip) header += "Content-Encoding: gzip\r\n";
		}
		else {
			header += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
		}

		header += "Content-Length: " + std::to_string(msg_length) + "\r\n\r\n";
	}

	bool HTTPClient::Handshake(TCP::Client &client) {
#ifdef HASOPENSSL
		ssl = SSL_new(_sse_context.getContext());
		if (!ssl) {
			Error() << "HTTP Client [" << host << "]: error during SSL new - error code : " << ERR_get_error() ;
			return false;
		}

		SSL_set_tlsext_host_name(ssl, host.c_str());
		SSL_set_fd(ssl, client.getSocket());

		if (SSL_connect(ssl) != 1) {
			Error() << "HTTP Client [" << host << "]: error during SSL handshake - error code : " << ERR_get_error() ;
			return false;
		}
		return true;
#else
		Error() << "HTTP Client [" << host << "]: error during Handshake, SSL not supported in this build." ;

		return false;
#endif
	}

	HTTPResponse HTTPClient::Post(const std::string& msg, bool gzip, bool multipart, const std::string& copyname) {

		TCP::Client client;
		HTTPResponse response;

		createMessageBody(msg, gzip, multipart, copyname);
		createHeader(gzip, multipart);

		client.setVerbosity(false);
		if (!client.connect(host, port, false, 1)) {
			Error() << "HTTP Client [" << host << "]: error connecting to server." ;
			return HTTPResponse();
		}

		std::memset(buffer, 0, sizeof(buffer));

		if (secure) {
#ifdef HASOPENSSL

			int r;

			if (!Handshake(client)) {
				freeSSL();
				return response;
			}

			r = SSL_write(ssl, header.c_str(), header.length());
			if (r <= 0) {
				Error() << "HTTP Client [" << host << "]: SSL write failed - error code : " << ERR_get_error() ;
				freeSSL();
				return response;
			}

			r = SSL_write(ssl, msg_ptr, msg_length);
			if (r <= 0) {
				Error() << "HTTP Client [" << host << "]: SSL write failed - error code : " << ERR_get_error() ;
				freeSSL();
				return response;
			}

			r = SSL_read(ssl, buffer, sizeof(buffer));
			if (r <= 0) {
				Error() << "HTTP Client [" << host << "]: SSL read failed - error code : " << ERR_get_error() ;
				freeSSL();
				return response;
			}

			freeSSL();
#else
			Error() << "HTTP Client [" << host << "]: SSL not supported." ;
			return response;
#endif
		}
		else {

			if (client.send(header.c_str(), header.length()) < 0) {
				Error() << "HTTP Client [" << host << "]: write failed" ;
				return response;
			}
			if (client.send(msg_ptr, msg_length) < 0) {
				Error() << "HTTP Client [" << host << "]: write failed" ;
				return response;
			}

			client.read(buffer, sizeof(buffer), 2, false);
		}

		parseResponse(response, std::string(buffer));

		return response;
	}
}
