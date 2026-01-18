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

#include <cstring>

#include "HTTPClient.h"
#include "Convert.h"

namespace IO
{

	int HTTPClient::parseResponse()
	{

		try
		{
			std::istringstream iss(message);

			std::string httpVersion, statusMessage;
			int status = HTTP_NO_RESPONSE;

			iss >> httpVersion >> status;

			std::getline(iss, statusMessage);

			std::string line;
			std::streampos bodyStart;

			// Parse headers
			while (std::getline(iss, line) && line != "\r" && !line.empty())
			{
				if (line.find("Transfer-Encoding:") != std::string::npos)
				{
					if (line.find("chunked") == std::string::npos)
					{
						Error() << "HTTP Client [" << host << "]: Transfer-Encoding other than chunked not supported.";
						return HTTP_UNSUPPORTED_ENCODING;
					}
				}
			}

			// Store position after headers for body extraction
			bodyStart = iss.tellg();
			if (bodyStart != std::streampos(-1))
			{
				// Extract body and update message to contain only the body
				message = message.substr(bodyStart);
			}
			else
			{
				// No body
				message.clear();
			}

			return status;
		}
		catch (const std::exception &)
		{
			return HTTP_PARSE_ERROR;
		}
	}

	void HTTPClient::createMessageBody(const std::string &msg, bool gzip, bool multipart, const std::string &copyname)
	{

		// multipart
		if (multipart)
		{
			message = "--" + boundary + "\n";
			message += "Content-Disposition: form-data; name=\"" + copyname + "\"\n";
			message += "Content-Type: application/json\n\n";
			message += msg;
			message += "\n--" + boundary + "--\n";
			return;
		}
		// plain & zipped
		if (gzip)
		{
			zip.zip(msg);
			return;
		}

		// plain
		message = msg;
	}

	void HTTPClient::createHeader(bool gzip, bool multipart)
	{

		header = "POST " + path + " HTTP/1.1\r\nHost: " + host + ":" + port + "\r\nAccept: */*\r\n";
		if (!userpwd.empty())
		{
			header += "Authorization: Basic " + Util::Convert::BASE64toString(userpwd) + "\r\n";
		}

		if (!multipart)
		{
			header += "Content-Type: application/json\r\n";
			if (gzip)
				header += "Content-Encoding: gzip\r\n";
		}
		else
		{
			header += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
		}

		header += "Content-Length: " + std::to_string(getMessageLength(gzip)) + "\r\n\r\n";
	}

	int HTTPClient::Post(const std::string &msg, bool gzip, bool multipart, const std::string &copyname)
	{

		createMessageBody(msg, gzip, multipart, copyname);
		createHeader(gzip, multipart);

		// Set up protocol chain: TCP -> TLS (if secure)
		tcp.setValue("HOST", host);
		tcp.setValue("PORT", port);
		tcp.setValue("PERSISTENT", "false");
		tcp.setValue("TIMEOUT", "1");

		if (secure)
		{
			connection = tcp.add(&tls);
		}
		else
		{
			connection = &tcp;
		}

		if (!connection->connect())
		{
			Error() << "HTTP Client [" << host << "]: error connecting to server.";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		buffer.clear();
		buffer.resize(8192); // Start with 8KB buffer

		// Send header
		if (connection->send(header.c_str(), header.length()) < 0)
		{
			Error() << "HTTP Client [" << host << "]: write failed";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		// Send body
		if (connection->send(getMessagePtr(gzip), (int)getMessageLength(gzip)) < 0)
		{
			Error() << "HTTP Client [" << host << "]: write failed";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		// Read response - read until connection closes or timeout
		message.clear();
		int totalBytes = 0;
		int emptyReads = 0;
		while (emptyReads < 5)
		{
			int bytes_read = connection->read(&buffer[0], buffer.size(), 1, true);
			if (bytes_read > 0)
			{
				message.append(buffer.data(), bytes_read);
				totalBytes += bytes_read;
				emptyReads = 0;
				if (totalBytes > 1 * 1024 * 1024)
				{
					Error() << "HTTP Client [" << host << "]: response too large";
					break;
				}
			}
			else if (bytes_read == 0)
			{
				emptyReads++;
			}
			else
			{
				// Error or connection closed
				break;
			}
		}

		if (totalBytes == 0)
		{
			Error() << "HTTP Client [" << host << "]: read failed";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		int status = parseResponse();
		connection->disconnect();

		return status;
	}
}
