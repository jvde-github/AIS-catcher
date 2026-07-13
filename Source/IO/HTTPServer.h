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
#include <list>
#include <thread>
#include <mutex>
#include <chrono>
#include <time.h>

#include "SocketUtil.h"

#include "Stream.h"
#include "Common.h"
#include "AIS.h"
#include "Keys.h"
#include "TCPServer.h"
#include "Library/ZIP.h"

namespace IO
{

	class SSEConnection
	{
	protected:
		IO::TCPServerConnection *connection;
		uint32_t mask = 0;

	public:
		SSEConnection(IO::TCPServerConnection *c, uint32_t m) : connection(c), mask(m) { c->setVerbosity(false); }
		~SSEConnection() { Close(); }

		bool subscribed(int id)
		{
			return (mask >> id) & 1;
		}

		void Ping()
		{
			static const char ping[] = ": ping\r\n\r\n";
			if (connection)
				connection->SendDirect(ping, sizeof(ping) - 1);
		}

		void Start()
		{
			if (!connection)
				return;

			connection->Lock();

			std::string headers = "HTTP/1.1 200 OK\r\n";
			headers += "Content-Type: text/event-stream\r\n";
			headers += "Cache-Control: no-cache\r\n";
			headers += "X-Accel-Buffering: no\r\n";
			headers += "Connection: keep-alive\r\n\r\n";

			connection->SendDirect(headers.c_str(), headers.length());
		}

		bool isConnected()
		{
			return connection && connection->isConnected();
		}

		void Close()
		{
			// close before unlock, or the slot can be re-accepted while still referenced here
			if (connection)
			{
				connection->SendDirect("\r\n", 2);
				connection->Close();
				connection->Unlock();
				connection = nullptr;
			}
		}

		void SendEvent(const std::string &eventName, const std::string &eventData, const std::string &eventId = "")
		{
			if (connection)
			{
				std::string eventStr = "event: " + eventName + "\r\n";
				if (!eventId.empty())
				{
					eventStr += "id: " + eventId + "\r\n";
				}
				eventStr += "data: " + eventData + "\r\n\r\n";

				connection->SendDirect(eventStr.c_str(), eventStr.length());
			}
		}
	};

	struct HTTPRequest
	{
		std::string method;
		std::string target;
		std::string body;
		std::string cookie;
		std::string host;
		std::string origin;
		std::string forwarded_host;

		std::string path() const
		{
			std::string::size_type pos = target.find('?');
			return pos == std::string::npos ? target : target.substr(0, pos);
		}

		std::string query() const
		{
			std::string::size_type pos = target.find('?');
			return pos == std::string::npos ? "" : target.substr(pos + 1);
		}

		static std::string queryParam(const std::string &q, const std::string &name)
		{
			std::string::size_type pos = 0;
			while (pos < q.size())
			{
				std::string::size_type amp = q.find('&', pos);
				if (amp == std::string::npos)
					amp = q.size();
				std::string::size_type eq = q.find('=', pos);
				if (eq != std::string::npos && eq < amp && q.compare(pos, eq - pos, name) == 0)
					return q.substr(eq + 1, amp - eq - 1);
				pos = amp + 1;
			}
			return "";
		}

		std::string queryParam(const std::string &name) const
		{
			return queryParam(query(), name);
		}
	};

	class HTTPServer : public IO::TCPServer
	{
	public:
		virtual void Request(IO::TCPServerConnection &c, const std::string &msg, bool accept_gzip);
		virtual void Request(IO::TCPServerConnection &c, const HTTPRequest &r, bool accept_gzip);

		void Response(IO::TCPServerConnection &c, const std::string &type, const std::string &content, bool gzip = false, bool cache = false, bool cors = false, int status = 200);
		void Response(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false, bool cors = false, int status = 200);
		void ResponseRaw(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false, bool cors = false, int status = 200);

		void cleanupSSE()
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			cleanupSSE_locked();
		}

		bool hasSSEClients()
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			return !sse.empty();
		}

		void closeAllSSE()
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			sse.clear();
		}

		// backlog is built under sse_mtx so no event can fall between snapshot and registration
		void upgradeSSE(IO::TCPServerConnection &c, uint32_t mask, const std::string &topic = "",
						const std::function<std::vector<std::string>()> &backlog = nullptr)
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			cleanupSSE_locked();

			sse.emplace_back(&c, mask);
			auto &connection = sse.back();
			connection.Start();
			if (backlog)
				for (const auto &data : backlog())
					connection.SendEvent(topic, data);
		}

		void sendSSE(int id, const std::string &event, const std::string &data)
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			for (auto it = sse.begin(); it != sse.end(); ++it)
			{
				if (it->subscribed(id))
					it->SendEvent(event, data);
			}
			cleanupSSE_locked();
		}

		void setFrameAncestors(const std::string &v) { frame_ancestors = v; }
		void setFrameSrc(const std::string &v) { frame_src = v; }

		void setExtraHeader(const std::string &h) { extra_header = h; }

	private:
		std::string ret, header;
		// Default `*` permits any embedding — AIS-catcher is typically
		// self-hosted on a LAN and is commonly iframed by a control app on a
		// different port. Tighten with the `frame_ancestors` setting if the
		// instance is exposed beyond a trusted network.
		std::string frame_ancestors = "*";
		std::string frame_src = "'self'";
		std::string extra_header;
		std::list<IO::SSEConnection> sse;
		std::mutex sse_mtx;
		std::chrono::steady_clock::time_point last_sse_ping{};
		static const int SSE_PING_INTERVAL = 20;

		void cleanupSSE_locked()
		{
			for (auto it = sse.begin(); it != sse.end();)
			{
				if (!it->isConnected())
				{
					it->Close();
					it = sse.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		void Parse(const std::string &s, HTTPRequest &r, bool &accept_gzip);

		ZIP zip;

	protected:
		void processClients() override;
	};
}
