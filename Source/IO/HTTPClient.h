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

#include <iostream>
#include <string>

#ifdef HASOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "Library/ZIP.h"
#include "Protocol.h"
#include "Parse.h"
#include "OutputStats.h"

namespace IO
{

	enum HTTPStatus
	{
		HTTP_NO_RESPONSE = -1,
		HTTP_UNSUPPORTED_ENCODING = -2,
		HTTP_PARSE_ERROR = -3,
		HTTP_CONNECTION_FAILED = -4
	};

	class HTTPClient
	{
		ZIP zip;
		std::string boundary = "------------------------2e45e7d128457b6d";
		std::string message, header;
		std::string buffer;

		Protocol::TCP tcp;
		Protocol::TLS tls;
		Protocol::ProtocolBase *connection = nullptr;

		void createMessageBody(const std::string &msg, bool gzip, bool multipart, const std::string &copyname);
		const void *getMessagePtr(bool gzip) const { return gzip ? zip.getOutputPtr() : message.c_str(); }
		size_t getMessageLength(bool gzip) const { return gzip ? zip.getOutputLength() : message.length(); }
		void createHeader(bool gzip, bool multipart);
		int parseResponse();

	public:
		std::string protocol, host, port, path, userpwd;
		bool secure = false;

		const std::string &getResponse() const { return message; }

		void setUserPwd(const std::string &up)
		{
			userpwd = up;
		}

		void setURL(const std::string &url)
		{
			Util::Parse::HTTP_URL(url, protocol, host, port, path);

			if (protocol.empty())
				throw std::runtime_error("Invalid URL, missing protocol.");

			if (protocol != "http" && protocol != "https")
				throw std::runtime_error("Invalid protocol: " + protocol);

			secure = protocol == "https";

			if (port.empty())
				port = secure ? "443" : "80";

#ifndef HASOPENSSL
			if (secure)
				throw std::runtime_error("SSL not supported in this build");
#endif
		}

		int Post(const std::string &msg, bool gzip = false, bool multipart = false, const std::string &copyname = "");

		void setStats(IO::OutputStats *s) { tcp.setStats(s); }
	};
}
