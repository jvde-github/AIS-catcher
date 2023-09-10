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

	class SSEConnection {
	protected:
		bool running = false;
		TCP::ServerConnection *connection;
		int _id = 0;

	public:
		SSEConnection(TCP::ServerConnection *connection,int id) : connection(connection), _id(id) {
			std::cerr << "SSE Connection Constructor : " << connection->sock << "\n";			
		}

		~SSEConnection() { 
			std::cerr << "SSE Connection Destructor\n"; Close();
		}

		int getID() {
			return _id;
		}

		void Start() {
			if (!connection) return;

			std::cerr << "SSE start: " << connection->sock << std::endl;
			running = true;
			connection->Lock();

			std::string headers = "HTTP/1.1 200 OK\n";
			headers += "Content-Type: text/event-stream\n";
			headers += "Cache-Control: no-cache\n";
			headers += "X-Accel-Buffering: no\n";
			headers += "Connection: keep-alive\n\n\n";

			connection->SendDirect(headers.c_str(), headers.length());
		}

		bool isConnected() {
			return connection && connection->isConnected();
		}

		void Close() {

			if (connection) {
				std::cerr << "SSE close: " << connection->sock << std::endl;
				connection->SendDirect("\r\n", 1);
				connection->Unlock();
				connection->Close();
				connection = nullptr;
			}
		}

		void SendEvent(const std::string& eventName, const std::string& eventData, const std::string& eventId = "") {
			if (connection && running) {
				std::string eventStr = "event: " + eventName + "\r\n";
				if (!eventId.empty()) {
					eventStr += "id: " + eventId + "\r\n";
				}
				eventStr += "data: " + eventData + "\r\n\r\n";

				connection->SendDirect(eventStr.c_str(), eventStr.length());
			}
		}
	};

	class HTTPServer : public TCP::Server {
	public:
		virtual void Request(TCP::ServerConnection& c, const std::string& msg, bool accept_gzip);

		void Response(TCP::ServerConnection& c, std::string type, const std::string& content, bool gzip = false);
		void Response(TCP::ServerConnection& c, std::string type, char* data, int len, bool gzip = false);

		void cleanupSSE() {
			for (auto it = sse.begin(); it != sse.end(); ) {
				if (!it->isConnected()) {
					it->Close();
					it = sse.erase(it);
				}
				else {
					++it;
				}
			}
		}

		void upgradeSSE(TCP::ServerConnection& c, int id) {
			// temporary design
			std::cerr << "Upgrading to SSE connection\n";
			cleanupSSE();

			sse.emplace_back(&c, id);
			sse.back().Start();
		}

		void sendSSE(int id, const std::string& event, const std::string& data) {

			for (auto it = sse.begin(); it != sse.end(); ++it) {				
				if(it->getID() == id)
					it->SendEvent("nmea", data);				
			}
			cleanupSSE();
		}

	private:
		std::string ret, header;
		std::list<IO::SSEConnection> sse;

		void Parse(const std::string& s, std::string& get, bool& accept_gzip);
		void processClients();

		ZIP zip;
	};
}
