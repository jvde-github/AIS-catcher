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

#include <iostream>
#include <string>

#ifdef HASOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "Library/ZIP.h"
#include "TCP.h"
#include "Utilities.h"

namespace IO {

	struct HTTPResponse {
		std::string message = "No response.";
		int status = -1;
	};

	class HTTPClient {
		ZIP zip;
		std::string boundary = "------------------------2e45e7d128457b6d";
		std::string message, header;

		char buffer[1024];

		void createMessageBody(const std::string& msg, bool gzip, bool multipart, const std::string& copyname);
		void createHeader(bool gzip, bool multipart);
		bool Handshake();
		void parseResponse(HTTPResponse& response, const std::string& msg);
		void freeSSL() {
#ifdef HASOPENSSL
			if (ssl) {
				SSL_shutdown(ssl);
				SSL_free(ssl);
				ssl = nullptr;
			}
#endif
		}

	public:
		TCP::Client client;
		std::string protocol, host, port, path, userpwd;

		void* msg_ptr = nullptr;
		int msg_length = 0;

		bool secure = false;

#ifdef HASOPENSSL
		SSL* ssl = nullptr;
#endif
		void validateLibs() {
#ifndef HASOPENSSL
			if (secure)
				throw std::runtime_error("SSL not supported in this build");
#endif
		}

		void setUserPwd(const std::string& up) {
			userpwd = up;
		}

		void setURL(const std::string& url) {
			Util::Parse::HTTP_URL(url, protocol, host, port, path);

			if(protocol.empty())
				throw std::runtime_error("Invalid URL, missing protocol.");

			if(protocol != "http" && protocol != "https")
				throw std::runtime_error("Invalid protocol: " + protocol);
				
			secure = protocol == "https";

			if (port.empty())
				port = secure ? "443" : "80";

			validateLibs();
		}

		HTTPResponse Post(const std::string& msg, bool gzip = false, bool multipart = false, const std::string& copyname = "");
	};
}
