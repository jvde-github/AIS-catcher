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

#include <cstdlib>

#include "HTTPClient.h"
#include "Convert.h"
#include "Logger.h"

namespace IO
{

	// the body of a chunked reply; false when it stops before its last chunk
	static bool dechunk(const std::string &in, std::string &out)
	{
		out.clear();
		std::size_t at = 0;
		while (true)
		{
			const std::size_t eol = in.find("\r\n", at);
			if (eol == std::string::npos)
				return false;
			const std::string line = in.substr(at, eol - at);
			char *end = nullptr;
			const unsigned long size = std::strtoul(line.c_str(), &end, 16);
			if (end == line.c_str())
				return false;
			at = eol + 2;
			if (size == 0)
				return true;
			if (in.size() - at < 2 || size > in.size() - at - 2)
				return false;
			out.append(in, at, size);
			at += size + 2;
		}
	}

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
			content_type.clear();
			bool chunked = false, has_length = false;
			unsigned long length = 0;
			while (std::getline(iss, line) && line != "\r" && !line.empty())
			{
				std::string name = line.substr(0, line.find(':'));
				Util::Convert::toLower(name);
				if (name == "content-type" && line.size() > 13)
				{
					content_type = line.substr(13);
					content_type.erase(0, content_type.find_first_not_of(' '));
					if (!content_type.empty() && content_type.back() == '\r')
						content_type.pop_back();
				}
				else if (name == "content-length" && line.size() > 15)
				{
					has_length = true;
					length = std::strtoul(line.c_str() + 15, nullptr, 10);
				}
				else if (name == "transfer-encoding")
				{
					std::string value = line.substr(name.size() + 1);
					Util::Convert::toLower(value);
					if (value.find("chunked") == std::string::npos)
					{
						Error() << "HTTP Client [" << host << "]: Transfer-Encoding other than chunked not supported.";
						return HTTP_UNSUPPORTED_ENCODING;
					}
					chunked = true;
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

			// the body as sent, or no answer when it came short of what its framing says
			if (chunked)
			{
				std::string body;
				if (!dechunk(message, body))
				{
					Error() << "HTTP Client [" << host << "]: chunked response ended early.";
					return HTTP_RESPONSE_INCOMPLETE;
				}
				message.swap(body);
			}
			else if (has_length)
			{
				if (message.size() < length)
				{
					Error() << "HTTP Client [" << host << "]: response ended after " << message.size() << " of " << length << " bytes.";
					return HTTP_RESPONSE_INCOMPLETE;
				}
				message.resize(length);
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
			message = "--" + boundary + "\r\n";
			message += "Content-Disposition: form-data; name=\"" + copyname + "\"\r\n";
			message += "Content-Type: application/json\r\n\r\n";
			message += msg;
			message += "\r\n--" + boundary + "--\r\n";
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

		header = "POST " + path + " HTTP/1.1\r\nHost: " + (host.find(':') == std::string::npos ? host : "[" + host + "]") + ":" + port + "\r\nAccept: */*\r\nConnection: close\r\n";
		header += Util::Convert::authorizationHeader(username, password, basic);
		header += extra_headers;

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
		if (multipart) gzip = false;
		createMessageBody(msg, gzip, multipart, copyname);
		createHeader(gzip, multipart);
		return exchange(true, gzip);
	}

	// one GET, its reply's body in getResponse() and its type in content_type
	int HTTPClient::Get()
	{
		message.clear();
		header = "GET " + path + " HTTP/1.1\r\nHost: " + (host.find(':') == std::string::npos ? host : "[" + host + "]") + ":" + port + "\r\nAccept: */*\r\nConnection: close\r\n";
		header += Util::Convert::authorizationHeader(username, password, basic);
		header += extra_headers;
		header += "\r\n";
		return exchange(false, false);
	}

	int HTTPClient::exchange(bool body, bool gzip)
	{
		// Set up protocol chain: TCP -> TLS (if secure)
		tcp.setOptionKey(AIS::KEY_SETTING_HOST, host);
		tcp.setOptionKey(AIS::KEY_SETTING_PORT, port);
		tcp.setOptionKey(AIS::KEY_SETTING_TIMEOUT, std::to_string(timeout));

		if (secure)
		{
			connection = tcp.add(&tls);
		}
		else
		{
			connection = &tcp;
		}

		// bounded, so a stalled nameserver cannot hold up a stop; long enough for the lookup, an
		// address that does not answer and the next one
		if (!connection->connect() || !connection->waitConnected(3 * (timeout > 0 ? timeout : 10)))
		{
			Error() << "HTTP Client [" << host << "]: error connecting to server.";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		buffer.clear();
		buffer.resize(8192);

		// Send header
		if (!connection->sendAll(header.c_str(), (int)header.length()))
		{
			Error() << "HTTP Client [" << host << "]: write failed";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		// Send body
		if (body && !connection->sendAll(getMessagePtr(gzip), (int)getMessageLength(gzip)))
		{
			Error() << "HTTP Client [" << host << "]: write failed";
			connection->disconnect();
			return HTTP_CONNECTION_FAILED;
		}

		// Read response - read until connection closes or timeout
		message.clear();
		int totalBytes = 0;
		int emptyReads = 0;
		bool too_large = false;
		const Net::Clock::time_point start = Net::Clock::now();
		// the first byte may take the whole timeout; a reply under way ends after five quiet seconds
		while ((totalBytes == 0 || emptyReads < 5) && Net::secondsSince(start) < timeout)
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
					too_large = true;
					break;
				}
			}
			else if (bytes_read == 0)
			{
				// one request per connection: a close is the end of the reply
				if (!connection->isConnected())
					break;
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

		connection->disconnect();
		return too_large ? HTTP_RESPONSE_INCOMPLETE : parseResponse();
	}
}
