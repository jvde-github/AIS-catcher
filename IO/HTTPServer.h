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
#include <list>
#include <thread>
#include <mutex>
#include <time.h>

#ifdef HASCURL
#include <curl/curl.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET		int
#define closesocket close
#endif

#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#include "Stream.h"
#include "Common.h"
#include "AIS.h"
#include "Keys.h"
#include "TCP.h"
#include "Utilities.h"
#include "Library/ZIP.h"

namespace IO {

	class HTTPServer : public TCP::Server {
	public:
		virtual void Request(TCP::Socket& c, const std::string& msg, bool accept_gzip);

		void Response(TCP::Socket& c, std::string type, const std::string& content, bool gzip = false);
		void Response(TCP::Socket& c, std::string type, char* data, int len, bool gzip = false);

	private:
		std::string ret, header;

		void Parse(const std::string& s, std::string& get, bool& accept_gzip);
		void processClients();

		ZIP zip;
	};
}
