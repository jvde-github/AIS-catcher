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

#include <cstring>

#include "HTTPServer.h"

namespace IO
{

	// HTTP Server
	void HTTPServer::processClients()
	{
		static const std::string EOF_MSG = "\r\n\r\n";

		for (auto &c : client)
		{
			if (c.isConnected())
			{

				std::size_t pos = c.msg.find(EOF_MSG);
				while (pos != std::string::npos)
				{
					std::string request;
					bool gzip;
					Parse(c.msg.substr(0, pos + 4), request, gzip);
					if (!request.empty())
						Request(c, request, gzip);

					c.msg.erase(0, pos + 4);
					pos = c.msg.find(EOF_MSG);
				}

				if (c.msg.size() > 8192)
				{
					std::cerr << "Server: closing connection, client flooding server: " << c.sock << std::endl;
					// std::cerr << "Server: " << c.msg << std::endl;
					c.Close();
				}
			}
		}
	}

	void HTTPServer::Request(TCP::ServerConnection &c, const std::string &, bool)
	{
		std::string r = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 15\r\nConnection: close\r\n\r\nPage not found.";
		Send(c, r.c_str(), r.length());
		c.Close();
	}

	void HTTPServer::Parse(const std::string &s, std::string &get, bool &accept_gzip)
	{

		get.clear();
		accept_gzip = false;

		std::istringstream iss(s);
		std::string line;
		while (std::getline(iss, line) && !line.empty())
		{
			std::istringstream line_stream(line);
			std::string key, value;
			std::getline(line_stream, key, ' ');
			Util::Convert::toUpper(key);

			if (key == "GET")
			{
				std::getline(line_stream, value, ' ');
				get = value;
			}
			else if (key == "ACCEPT-ENCODING:")
			{
				std::getline(line_stream, value);
				accept_gzip = value.find("gzip") != std::string::npos;
			}
		}
	}

	void HTTPServer::Response(TCP::ServerConnection &c, std::string type, const std::string &content, bool gzip, bool cache)
	{
#ifdef HASZLIB
		if (gzip)
		{
			zip.zip(content);
			ResponseRaw(c, type, (char *)zip.getOutputPtr(), zip.getOutputLength(), true, cache);
			return;
		}
#endif

		ResponseRaw(c, type, (char *)content.c_str(), content.size(), cache);
	}

	void HTTPServer::Response(TCP::ServerConnection &c, std::string type, char *data, int len, bool gzip, bool cache)
	{
#ifdef HASZLIB
		if (gzip)
		{
			zip.zip(data, len);
			ResponseRaw(c, type, (char *)zip.getOutputPtr(), zip.getOutputLength(), true, cache);
			return;
		}
#endif

		ResponseRaw(c, type, data, len);
	}

	void HTTPServer::ResponseRaw(TCP::ServerConnection &c, std::string type, char *data, int len, bool gzip, bool cache)
	{

		std::string header = "HTTP/1.1 200 OK\r\nServer: AIS-catcher\r\nContent-Type: " + type;
		if (gzip)
			header += "\r\nContent-Encoding: gzip";

		if (cache)
		{
			header += "\r\nCache-Control: public, max-age=31536000";
			time_t now = time(0) + 31536000;
			char buf[100];
			strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
			header += "\r\nExpires: " + std::string(buf);
		}
		else 
		{
			header += "\r\nCache-Control: no-cache";
		}

		header += "\r\nConnection: keep-alive\r\nContent-Length: " + std::to_string(len) + "\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

		if (!Send(c, header.c_str(), header.length()))
		{
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
		if (!Send(c, data, len))
		{
			std::cerr << "Server: closing client socket." << std::endl;
			c.Close();
			return;
		}
	}
}