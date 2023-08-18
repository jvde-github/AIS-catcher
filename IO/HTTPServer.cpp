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

#include "HTTPServer.h"

namespace IO {


	// HTTP Server
	void HTTPServer::processClients() {
		for (auto& c : client) {
			if (c.isConnected()) {

				std::size_t pos = c.msg.find("\r\n\r\n");
				if (pos != std::string::npos) {
					std::string request;
					bool gzip;
					Parse(c.msg, request, gzip);
					if (!request.empty())
						Request(c, request, gzip);

					c.msg.erase(0, pos + 4);
				}
				else if (c.msg.size() > 4096) {
					std::cerr << "Server: closing connection, client flooding server: " << c.sock << std::endl;
					c.Close();
				}
			}
		}
	}

	void HTTPServer::Request(TCP::Socket& c, const std::string& r, bool) {
		// TO DO: return 404 by default
	}


	void HTTPServer::Parse(const std::string& s, std::string& get, bool& accept_gzip) {
		int max = 100;

		get = "";
		accept_gzip = false;

		std::istringstream iss(s);
		std::string line;
		while (std::getline(iss, line) && !line.empty()) {
			std::istringstream line_stream(line);
			std::string key, value;
			std::getline(line_stream, key, ' ');
			Util::Convert::toUpper(key);

			if (key == "GET") {
				std::getline(line_stream, value, ' ');
				get = value;
			}
			else if (key == "ACCEPT-ENCODING:") {
				std::getline(line_stream, value);
				accept_gzip = value.find("gzip") != std::string::npos;
			}
		}
	}

	void HTTPServer::Response(TCP::Socket& c, std::string type, const std::string& content, bool gzip) {
#ifdef HASZLIB
		if (gzip) {
			zip.zip(content);
			Response(c, type, (char*)zip.getOutputPtr(), zip.getOutputLength(), true);
			return;
		}
#endif

		Response(c, type, (char*)content.c_str(), content.size());
	}

	void HTTPServer::Response(TCP::Socket& c, std::string type, char* data, int len, bool gzip) {

		std::string header = "HTTP/1.1 200 OK\r\nServer: AIS-catcher\r\nContent-Type: " + type;
		if (gzip) header += "\r\nContent-Encoding: gzip";
		header += "\r\nConnection: keep-alive\r\nContent-Length: " + std::to_string(len) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

		if (!Send(c.sock, header.c_str(), header.length())) {
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
		if (!Send(c.sock, data, len)) {
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
	}
}