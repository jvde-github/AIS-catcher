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
		bool running = false;
		IO::TCPServerConnection *connection;
		int _id = 0;

	public:
		SSEConnection(IO::TCPServerConnection *c, int id) : connection(c), _id(id) { c->setVerbosity(false); }
		~SSEConnection() { Close(); }

		int getID()
		{
			return _id;
		}

		void Start()
		{
			if (!connection)
				return;

			running = true;
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

			if (connection)
			{
				connection->SendDirect("\r\n", 2);
				connection->Unlock();
				connection->Close();
				connection = nullptr;
			}
		}

		void SendEvent(const std::string &eventName, const std::string &eventData, const std::string &eventId = "")
		{
			if (connection && running)
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

	// Parsed HTTP request; target is the raw request target including the
	// query string
	struct HTTPRequest
	{
		std::string method;
		std::string target;
		std::string body;
		std::string cookie;
		std::string host;
		std::string origin;

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
	};

	class HTTPServer : public IO::TCPServer
	{
		std::array<std::string, 4> sse_topic = {"aiscatcher", "nmea", "nmea", "log"};

	public:
		virtual void Request(IO::TCPServerConnection &c, const std::string &msg, bool accept_gzip);
		// Default implementation folds the request into the legacy string form
		// (POST body appended as query string) and calls the overload above
		virtual void Request(IO::TCPServerConnection &c, const HTTPRequest &r, bool accept_gzip);

		void Response(IO::TCPServerConnection &c, const std::string &type, const std::string &content, bool gzip = false, bool cache = false, bool cors = false, int status = 200);
		void Response(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false, bool cors = false, int status = 200);
		void ResponseRaw(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false, bool cors = false, int status = 200);

		void cleanupSSE()
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			cleanupSSE_locked();
		}

		IO::SSEConnection *upgradeSSE(IO::TCPServerConnection &c, int id)
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			cleanupSSE_locked();

			sse.emplace_back(&c, id);
			auto &connection = sse.back();
			connection.Start();
			return &connection;
		}

		void sendSSE(int id, const std::string &event, const std::string &data)
		{
			std::lock_guard<std::mutex> lk(sse_mtx);
			for (auto it = sse.begin(); it != sse.end(); ++it)
			{
				if (it->getID() == id)
					it->SendEvent(sse_topic[MIN(id, 3)], data);
			}
			cleanupSSE_locked();
		}

		void setFrameAncestors(const std::string &v) { frame_ancestors = v; }
		void setSSETopic(int id, const std::string &topic) { sse_topic[MIN(id, 3)] = topic; }

		// one-shot header (e.g. Set-Cookie) included in the next response only
		void setExtraHeader(const std::string &h) { extra_header = h; }

	private:
		std::string ret, header;
		// Default `*` permits any embedding — AIS-catcher is typically
		// self-hosted on a LAN and is commonly iframed by a control app on a
		// different port. Tighten with the `frame_ancestors` setting if the
		// instance is exposed beyond a trusted network.
		std::string frame_ancestors = "*";
		std::string extra_header;
		std::list<IO::SSEConnection> sse;
		std::mutex sse_mtx;

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
