/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET int
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

namespace IO
{

	class SSEConnection
	{
	protected:
		bool running = false;
		TCP::ServerConnection *connection;
		int _id = 0;

	public:
		SSEConnection(TCP::ServerConnection *c, int id) : connection(c), _id(id) { c->setVerbosity(false); }
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
			headers += "Connection: keep-alive\r\n\r\n\r\n";

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
				connection->SendDirect("\r\n", 1);
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

	class HTTPServer : public TCP::Server
	{
		std::array<std::string, 4> sse_topic = {"aiscatcher", "nmea", "nmea", "log"};

	public:
		virtual void Request(TCP::ServerConnection &c, const std::string &msg, bool accept_gzip);

		void Response(TCP::ServerConnection &c, const std::string &type, const std::string &content, bool gzip = false, bool cache = false);
		void Response(TCP::ServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false);
		void ResponseRaw(TCP::ServerConnection &c, const std::string &type, const char *data, int len, bool gzip = false, bool cache = false);

		void cleanupSSE()
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

		IO::SSEConnection *upgradeSSE(TCP::ServerConnection &c, int id)
		{
			cleanupSSE();

			sse.emplace_back(&c, id);
			auto &connection = sse.back();
			connection.Start();
			return &connection;
		}

		void sendSSE(int id, const std::string &event, const std::string &data)
		{

			for (auto it = sse.begin(); it != sse.end(); ++it)
			{
				if (it->getID() == id)
					it->SendEvent(sse_topic[MIN(id, 3)], data);
			}
			cleanupSSE();
		}

	private:
		std::string ret, header;
		std::list<IO::SSEConnection> sse;

		void Parse(const std::string &s, std::string &get, bool &accept_gzip);
		void processClients();

		ZIP zip;
	};
}
