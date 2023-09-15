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

#pragma once

#include <iostream>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ZIP.h"
#include "TCP.h"
#include "Utilities.h"

namespace IO {

	class HTTPClient {
		ZIP zip;
		std::string boundary = "------------------------2e45e7d128457b6d";
		std::string message, header;

		char response[1024];

		std::string base64_encode(const std::string &in);

	public:
		TCP::Client client;
		std::string protocol, host, port, path, userpwd;

		void *msg_ptr = nullptr;
		int msg_length = 0;

		bool secure = false;
		SSL* ssl = nullptr;

		void validateLibs() {
#ifndef HASOPENSSL
			if(secure)
				throw std::runtime_error("SSL not supported in this build");
#endif
		}
		
		void setUserPwd(const std::string &up) {
			userpwd = up;
		}

		void setURL(const std::string &url) {
			Util::Parse::URL(url, protocol, host, port, path);

			secure = protocol == "https";

			if(port.empty())
				port = secure ? "443" : "80";

			validateLibs();
		}	

		void createMessageBody(const std::string &msg, bool gzip, bool multipart, const std::string &copyname);
		void createHeader(bool gzip, bool multipart);
		bool Handshake();
		bool Post(const std::string &msg, std::string &reply, bool gzip = false, bool multipart = false, const std::string &copyname = "");

	};
}
