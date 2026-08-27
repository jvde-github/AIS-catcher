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

#include "HTTPServer.h"
#include "Logger.h"

namespace IO
{
	static std::string httpDate(time_t t)
	{
		struct tm tm_utc;
#ifdef _WIN32
		gmtime_s(&tm_utc, &t);
#else
		gmtime_r(&t, &tm_utc);
#endif
		char buf[40];
		strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_utc);
		return buf;
	}

	const int HTTPServer::SSE_PING_INTERVAL;

	// HTTP Server
	void HTTPServer::processClients()
	{
		static const std::string EOF_MSG = "\r\n\r\n";

		const std::time_t now_t = std::time(nullptr);

		for (auto &c : client)
		{
			if (c.isConnected() && !c.close_after_send)
			{
				// timer runs while a request is arriving but not yet complete
				if (c.msg.empty())
					c.request_start = 0;
				else if (c.request_start == 0)
					c.request_start = now_t;

				while (true)
				{
					// RFC 9112 2.2: empty lines before the request line are ignored,
					// not mistaken for the request itself
					std::size_t skip = 0;
					while (c.msg.compare(skip, 2, "\r\n") == 0)
						skip += 2;
					if (skip)
						c.msg.erase(0, skip);

					std::size_t pos = c.msg.find(EOF_MSG);
					if (pos == std::string::npos)
						break;

					HTTPRequest request;
					std::string error;
					int status = parseHeaders(c.msg, pos, request, error);
					if (status)
					{
						reject(c, status, error);
						break;
					}

					// Wait for the body if it has not fully arrived yet
					std::size_t required_length = pos + 4;
					if (required_length > MAX_REQUEST_SIZE || request.content_length > MAX_REQUEST_SIZE - required_length)
					{
						reject(c, 413, "total request size too large");
						break;
					}
					required_length += request.content_length;
					if (c.msg.size() < required_length)
					{
						// RFC 9110 10.1.1: the body only arrives once we say so
						if (request.expect_continue && !c.continue_sent)
						{
							static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
							c.continue_sent = true;
							Send(c, cont, sizeof(cont) - 1);
						}
						break;
					}

					if (request.method == "POST" && request.content_length > 0)
						request.body = c.msg.substr(pos + 4, request.content_length);

					c.close_after_send = !request.keep_alive;

					// HEAD is GET without the body; ResponseRaw suppresses it
					c.head_request = request.method == "HEAD";
					if (c.head_request)
						request.method = "GET";

					if (request.method == "GET" || request.method == "POST")
					{
						if (!dispatchMount(c, request, request.accept_gzip))
							Request(c, request, request.accept_gzip);
					}
					else
					{
						setExtraHeader("Allow: GET, HEAD, POST");
						Response(c, "text/plain", std::string("Method not allowed."), false, false, false, 405);
					}
					c.head_request = false;

					c.msg.erase(0, required_length);
					c.continue_sent = false;
					c.request_start = c.msg.empty() ? 0 : now_t;
					// closing after this response; ignore any pipelined requests
					if (c.close_after_send)
						break;
				}

				// Limit accumulated message size to prevent memory exhaustion
				if (c.msg.size() > MAX_REQUEST_SIZE)
					reject(c, 431, "accumulated message too large");
				// drop never-completing requests so slow senders can't exhaust slots (slowloris)
				else if (!c.close_after_send && !c.msg.empty() && c.request_start != 0 &&
						 (now_t - c.request_start) > REQUEST_TIMEOUT)
					reject(c, 408, "request header timeout");
			}
		}

		// reclaims dead SSE slots even when no events flow
		auto now = std::chrono::steady_clock::now();
		if (now - last_sse_ping >= std::chrono::seconds(SSE_PING_INTERVAL))
		{
			last_sse_ping = now;
			for (auto &s : sse)
				s.Ping();
			cleanupSSE();
		}
	}

	void HTTPServer::NotFound(IO::TCPServerConnection &c)
	{
		c.close_after_send = true;
		Response(c, "text/html", std::string("Page not found."), false, false, false, 404);
	}

	// Send an error response and drop the connection once it is flushed;
	// whatever is left in the input buffer cannot be trusted after a
	// malformed request.
	void HTTPServer::reject(IO::TCPServerConnection &c, int status, const std::string &reason)
	{
		Error() << "Server: closing connection, " << reason;
		c.close_after_send = true;
		Response(c, "text/plain", reason, false, false, false, status);
		c.msg.clear();
	}

	void HTTPServer::Request(IO::TCPServerConnection &c, const HTTPRequest &, bool)
	{
		NotFound(c);
	}

	bool HTTPServer::dispatchMount(IO::TCPServerConnection &c, const HTTPRequest &r, bool accept_gzip)
	{
		const std::string path = r.path();

		for (const auto &m : mounts)
		{
			const std::string &prefix = m.first;
			if (path.rfind(prefix, 0) != 0)
				continue;

			// What a mount serves is relative, so the browser must believe it is
			// inside a directory.
			if (path.size() == prefix.size())
			{
				setExtraHeader("Location: " + prefix + "/");
				Response(c, "text/plain", std::string(), false, false, false, 301);
				return true;
			}

			if (path[prefix.size()] != '/')
				continue;

			HTTPRequest sub = r;
			sub.target = r.target.substr(prefix.size());
			m.second->Request(c, sub, accept_gzip);
			return true;
		}
		return false;
	}

	int HTTPServer::parseHeaders(const std::string &msg, std::size_t header_end, HTTPRequest &r, std::string &error)
	{
		r = HTTPRequest();

		std::string version, connection, expect;
		std::size_t line_start = 0;
		bool first_line = true;
		bool seen_content_length = false;
		bool seen_host = false;

		while (line_start < header_end)
		{
			std::size_t line_end = msg.find("\r\n", line_start);
			if (line_end == std::string::npos || line_end > header_end)
				line_end = header_end;

			std::string line = msg.substr(line_start, line_end - line_start);
			line_start = line_end + 2;

			if (first_line)
			{
				first_line = false;

				std::size_t sp1 = line.find(' ');
				std::size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);

				// exactly three tokens: a space anywhere else is ambiguous framing, not a target we should guess at
				if (sp1 == std::string::npos || sp1 == 0 || sp2 == std::string::npos ||
					sp2 == sp1 + 1 || sp2 + 1 == line.size() || line.find(' ', sp2 + 1) != std::string::npos)
				{
					error = "malformed request line";
					return 400;
				}

				r.method = line.substr(0, sp1);
				r.target = line.substr(sp1 + 1, sp2 - sp1 - 1);
				version = line.substr(sp2 + 1);
				Util::Convert::toUpper(r.method);
				Util::Convert::toUpper(version);

				if (version.size() != 8 || version.compare(0, 7, "HTTP/1.") != 0 || version[7] < '0' || version[7] > '9')
				{
					error = "unsupported HTTP version: " + version;
					return 505;
				}

				if (r.target[0] != '/')
				{
					std::size_t scheme = r.target.find("://");
					if (scheme == std::string::npos)
					{
						error = "unsupported request target: " + r.target;
						return 400;
					}

					std::size_t slash = r.target.find('/', scheme + 3);
					r.target = slash == std::string::npos ? "/" : r.target.substr(slash);
				}
				continue;
			}

			// obs-fold: a continuation line can hide framing headers from this parser
			if (line.empty() || line[0] == ' ' || line[0] == '\t')
			{
				error = "malformed header field";
				return 400;
			}

			std::size_t colon = line.find(':');
			if (colon == std::string::npos || colon == 0)
			{
				error = "malformed header field";
				return 400;
			}

			std::string key = line.substr(0, colon);
			// whitespace before the colon is the classic request-smuggling desync
			if (key.back() == ' ' || key.back() == '\t')
			{
				error = "whitespace before header colon";
				return 400;
			}

			std::string value = line.substr(colon + 1);
			Util::Convert::toUpper(key);
			value.erase(0, value.find_first_not_of(" \t"));
			std::size_t value_end = value.find_last_not_of(" \t");
			value.erase(value_end == std::string::npos ? 0 : value_end + 1);

			if (key == "ACCEPT-ENCODING")
			{
				r.accept_gzip = value.find("gzip") != std::string::npos;
			}
			else if (key == "COOKIE")
			{
				r.cookie = value;
			}
			else if (key == "HOST")
			{
				if (seen_host)
				{
					error = "duplicate Host header";
					return 400;
				}
				seen_host = true;
				r.host = value;
			}
			else if (key == "ORIGIN")
			{
				r.origin = value;
			}
			else if (key == "X-FORWARDED-HOST")
			{
				r.forwarded_host = value;
			}
			else if (key == "CONNECTION")
			{
				connection = value;
				Util::Convert::toLower(connection);
			}
			else if (key == "EXPECT")
			{
				expect = value;
				Util::Convert::toLower(expect);
			}
			else if (key == "TRANSFER-ENCODING")
			{
				// unsupported framing must be rejected, not misparsed
				error = "Transfer-Encoding not supported";
				return 501;
			}
			else if (key == "CONTENT-LENGTH")
			{
				if (seen_content_length)
				{
					error = "duplicate Content-Length header";
					return 400;
				}
				seen_content_length = true;

				if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
				{
					error = "invalid Content-Length: " + value;
					return 400;
				}

				try
				{
					r.content_length = std::stoul(value);
				}
				catch (...)
				{
					error = "invalid Content-Length: " + value;
					return 400;
				}
				if (r.content_length > MAX_REQUEST_SIZE)
				{
					error = "Content-Length too large: " + value;
					return 413;
				}
			}
		}

		if (first_line)
		{
			error = "missing request line";
			return 400;
		}

		// Host is mandatory from HTTP/1.1 on, and 1.0 predates it
		if (version != "HTTP/1.0" && !seen_host)
		{
			error = "missing Host header";
			return 400;
		}

		r.expect_continue = expect.find("100-continue") != std::string::npos;

		// HTTP/1.0 is close-by-default, HTTP/1.1 keep-alive-by-default
		if (version == "HTTP/1.0")
			r.keep_alive = connection.find("keep-alive") != std::string::npos;
		else
			r.keep_alive = connection.find("close") == std::string::npos;

		return 0;
	}

	static const char *statusText(int status)
	{
		switch (status)
		{
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 408:
			return "Request Timeout";
		case 413:
			return "Content Too Large";
		case 429:
			return "Too Many Requests";
		case 431:
			return "Request Header Fields Too Large";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		default:
			return "OK";
		}
	}

	void HTTPServer::Response(IO::TCPServerConnection &c, const std::string &type, const std::string &content, bool gzip, bool cache, bool cors, int status)
	{
		Response(c, type, content.c_str(), (int)content.size(), gzip, cache, cors, status);
	}

	void HTTPServer::Response(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip, bool cache, bool cors, int status)
	{
#ifdef HASZLIB
		if (gzip && len >= (int)GZIP_MIN_LENGTH && zip.zip(data, len))
		{
			ResponseRaw(c, type, (const char *)zip.getOutputPtr(), zip.getOutputLength(), true, cache, cors, status);
			return;
		}
#endif

		ResponseRaw(c, type, data, len, false, cache, cors, status);
	}

	// Headers identical for every response, rebuilt only when the frame_*
	// settings change.
	const std::string &HTTPServer::commonHeaders()
	{
		if (common_headers.empty())
		{
			common_headers = "\r\nServer: AIS-catcher";
			common_headers += "\r\nContent-Security-Policy: default-src 'self'; "
				"script-src 'self'; "
				"style-src 'self' 'unsafe-inline'; "
				"img-src 'self' data: blob: http: https:; "
				"connect-src 'self' http: https: ws: wss:; "
				"font-src 'self' data:; "
				"frame-src " + frame_src + "; "
				"frame-ancestors " + frame_ancestors + "; "
				"base-uri 'self'";
			common_headers += "\r\nX-Content-Type-Options: nosniff";
			common_headers += "\r\nReferrer-Policy: strict-origin-when-cross-origin";
			common_headers += "\r\nVary: Accept-Encoding";
		}
		return common_headers;
	}

	void HTTPServer::ResponseRaw(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip, bool cache, bool cors, int status)
	{
		std::string header = "HTTP/1.1 " + std::to_string(status) + " " + statusText(status) +
							 "\r\nContent-Type: " + type + commonHeaders();
		header += "\r\nDate: " + httpDate(time(nullptr));

		std::string extra_header_sent;
		if (!extra_header.empty())
		{
			header += "\r\n" + extra_header;
			extra_header_sent.swap(extra_header);
		}

		if (cors)
			header += "\r\nAccess-Control-Allow-Origin: *";

		if (gzip)
			header += "\r\nContent-Encoding: gzip";

		// an extra header that sets Cache-Control speaks for the response
		bool cache_set = extra_header_sent.find("Cache-Control:") != std::string::npos;
		if (cache_set)
		{
		}
		else if (cache)
		{
			header += "\r\nCache-Control: max-age=31536000, stale-while-revalidate=604800, stale-if-error=604800";
			header += "\r\nExpires: " + httpDate(time(nullptr) + 31536000);
		}
		else
		{
			header += "\r\nCache-Control: private, no-store, max-age=0, s-maxage=0";
		}

		header += std::string("\r\nConnection: ") + (c.close_after_send ? "close" : "keep-alive") +
				  "\r\nContent-Length: " + std::to_string(len) + "\r\n\r\n";

		if (!Send(c, header.c_str(), header.length()))
		{
			Error() << "Server: closing client socket.";
			c.Close();
			return;
		}

		if (c.head_request)
			return;

		if (!Send(c, data, len))
		{
			Error() << "Server: closing client socket.";
			c.Close();
			return;
		}
	}
}