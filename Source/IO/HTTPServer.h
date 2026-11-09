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
#include <atomic>
#include <chrono>
#include <time.h>

#include "SocketUtil.h"

#include "Stream.h"
#include "Common.h"
#include "AIS.h"
#include "Keys.h"
#include "TCPServer.h"
#include "Parse.h"
#include "Library/ZIP.h"

namespace IO
{

	// Holds a generational handle to a connection, not a pin. The connection lives
	// in the server's fixed slot array so the pointer is always valid; the stored
	// generation tells us whether the slot is still OUR incarnation or has since
	// been closed/reused by a different client. All the sends below run on the
	// server's Run() thread (upgrade, ping, cleanup, and the posted fan-out), so
	// they honour the single-owner discipline.
	class SSEConnection
	{
	protected:
		IO::TCPServerConnection *connection;
		uint32_t gen;
		uint32_t mask = 0;

		IO::TCPServerConnection *live()
		{
			return (connection && connection->isConnected() && connection->getGeneration() == gen) ? connection : nullptr;
		}

	public:
		SSEConnection(IO::TCPServerConnection *c, uint32_t m) : connection(c), gen(c->getGeneration()), mask(m) { c->setVerbosity(false); }
		~SSEConnection() { Close(); }

		bool subscribed(int id)
		{
			return (mask >> id) & 1;
		}

		void Ping()
		{
			static const char ping[] = ": ping\r\n\r\n";
			if (auto *c = live())
				c->Send(ping, sizeof(ping) - 1);
		}

		void Start()
		{
			auto *c = live();
			if (!c)
				return;

			c->setNoTimeout();
			c->setSendLimit(Limits::STREAM_SEND_LIMIT);
			c->close_after_send = false;

			std::string headers = "HTTP/1.1 200 OK\r\n";
			headers += "Content-Type: text/event-stream\r\n";
			headers += "Cache-Control: no-cache\r\n";
			headers += "X-Accel-Buffering: no\r\n";
			headers += "Connection: keep-alive\r\n\r\n";

			c->Send(headers.c_str(), headers.length());
		}

		bool isConnected()
		{
			return live() != nullptr;
		}

		void Close()
		{
			if (auto *c = live())
			{
				c->Send("\r\n", 2);
				c->Close();
			}
			connection = nullptr;
		}

		// Append an already-formatted SSE frame (fan-out builds it once).
		void SendFrame(const std::string &frame)
		{
			if (auto *c = live())
				c->Send(frame.c_str(), (int)frame.length());
		}

		// The one place that knows the SSE wire format.
		static std::string frame(const std::string &eventName, const std::string &eventData, const std::string &eventId = "")
		{
			std::string s = "event: " + eventName + "\r\n";
			if (!eventId.empty())
				s += "id: " + eventId + "\r\n";
			return s + "data: " + eventData + "\r\n\r\n";
		}

		void SendEvent(const std::string &eventName, const std::string &eventData, const std::string &eventId = "")
		{
			SendFrame(frame(eventName, eventData, eventId));
		}

		uint32_t topics() const { return mask; }
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
		// the client a proxy on this machine forwards for: X-Forwarded-For's first address, or X-Real-IP
		std::string forwarded_for;
		std::string authorization;
		std::string setup_code;
		std::string peer;
		// from this machine itself
		bool local = false;
		bool forwarded = false;
		bool owner = false;
		bool keep_alive = true;
		bool accept_gzip = false;
		bool expect_continue = false;
		std::size_t content_length = 0;

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

		static std::string queryParam(const std::string &q, const std::string &name) { return Util::Parse::QueryParam(q, name); }

		std::string queryParam(const std::string &name) const
		{
			return queryParam(query(), name);
		}
	};

	// One mask carries every server's topics, so the ids may not collide.
	namespace SSE
	{
		const int ACTIVITY = 1;
		const int STATUS = 2;
		const int LOG = 3;
		const int NMEA = 4;
		const int SIGNAL = 5;
		const int VIEWER_LOG = 6;
	}

	// how a response goes out: its status, whether the body may be gzipped (Response) or
	// is (ResponseRaw), whether it may be cached and read by any origin, and one more header line
	struct Reply
	{
		int status = 200;
		bool gzip = false, cache = false, cors = false;
		std::string header;
		explicit Reply(int s = 200, bool g = false) : status(s), gzip(g) {}
		Reply &with(const std::string &h)
		{
			header = h;
			return *this;
		}
	};

	class HTTPServer : public IO::TCPServer
	{
	public:
		HTTPServer() { max_per_address = Limits::PER_ADDRESS_HTTP; }
		uint64_t bytesOut() const { return bytes_out.load(std::memory_order_relaxed); }
		virtual void Request(IO::TCPServerConnection &c, const HTTPRequest &r, bool accept_gzip);
		// what a path takes: 204 with Allow; a subclass that knows its paths answers for each
		virtual void Options(IO::TCPServerConnection &c, const HTTPRequest &r);
		// 404 and close; the fallback for any path no subclass handles
		void NotFound(IO::TCPServerConnection &c);

		void Response(IO::TCPServerConnection &c, const std::string &type, const std::string &content, const Reply &reply = Reply());
		void Response(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, const Reply &reply = Reply());
		void ResponseRaw(IO::TCPServerConnection &c, const std::string &type, const char *data, int len, const Reply &reply = Reply());

		// Serve another server's routes under a path prefix, so one listener can
		// front both. A server can be mounted once: its subscribers are held by
		// whichever server fronts it. Mounting while this server answers is safe.
		bool mount(const std::string &prefix, HTTPServer *target)
		{
			if (!target || target == this || target->isMounted() || prefix.empty() || prefix[0] != '/')
				return false;

			std::lock_guard<std::mutex> lock(mounts_mtx);
			for (auto &m : mounts)
				if (m.first == prefix)
				{
					m.second = target;
					target->setMountedOn(this);
					return true;
				}
			mounts.push_back({prefix, target});
			target->setMountedOn(this);
			return true;
		}

		bool isMounted() const { return mounted_on != nullptr; }

		// The sse list is touched only by the Run() thread: every path in reaches it
		// through processClients() or drainCommands(), so it needs no lock.
		bool hasSSEClients() const { return !sse.empty(); }
		// how many streams at once, of the server that fronts the connections; set before it starts
		void setMaxSSE(int n) { max_sse = n; }

		void closeAllSSE()
		{
			sse.clear();
			republishTopics();
		}

		// A producer posting while this runs lands in both the backlog snapshot and
		// the command queue, so a subscriber can see one duplicated event.
		void upgradeSSE(IO::TCPServerConnection &c, uint32_t mask, const std::string &topic = "",
						const std::function<std::vector<std::string>()> &backlog = nullptr)
		{
			// Only the loop that owns the socket may hold a subscriber.
			HTTPServer *front = mounted_on;
			if (front && !owns(c))
			{
				front->upgradeSSE(c, mask, topic, backlog);
				return;
			}

			cleanupSSE();

			if ((int)sse.size() >= max_sse)
			{
				c.close_after_send = true;
				Response(c, "text/plain", std::string("Too many live streams."), Reply(503).with("Retry-After: 30"));
				return;
			}

			sse.emplace_back(&c, mask);
			auto &connection = sse.back();
			connection.Start();
			if (backlog)
				for (const auto &data : backlog())
					connection.SendEvent(topic, data);
			republishTopics();
		}

		// Lets a producer skip building a payload nobody is listening for.
		bool sseSubscribed(int id) const
		{
			if ((topics.load() >> id) & 1)
				return true;
			HTTPServer *front = mounted_on;
			return front && front->sseSubscribed(id);
		}

		// Producers (decode/log threads) only enqueue a pre-formatted frame; the
		// Run() loop performs the actual fan-out, so no producer writes a socket.
		void sendSSE(int id, const std::string &event, const std::string &data)
		{
			HTTPServer *front = mounted_on;
			if (front)
				front->sendSSE(id, event, data);

			if (!((topics.load() >> id) & 1))
				return;

			post({Command::Derived, id, SSEConnection::frame(event, data)});
		}

		// the policy's parts; set while the server answers, so under a lock of their own
		void setFrameAncestors(const std::string &v)
		{
			std::lock_guard<std::mutex> lock(headers_mtx);
			frame_ancestors = v;
			common_headers.clear();
		}
		void setScriptSrc(const std::string &v)
		{
			std::lock_guard<std::mutex> lock(headers_mtx);
			csp_script_src = v;
			common_headers.clear();
		}
		void setFrameSrc(const std::string &v)
		{
			std::lock_guard<std::mutex> lock(headers_mtx);
			frame_src = v;
			common_headers.clear();
		}

		// back to the defaults documented below, for a reconfigure
		void resetFrameAncestors()
		{
			setFrameAncestors("*");
			setFrameSrc("'self'");
		}

	protected:
		// whether a request this server fronts for its mount comes from whom the mount's owner is
		virtual bool ownerOfMount(const HTTPRequest &) { return false; }

	private:
		// Default `*` permits any embedding — AIS-catcher is typically
		// self-hosted on a LAN and is commonly iframed by a control app on a
		// different port. Tighten with the `frame_ancestors` setting if the
		// instance is exposed beyond a trusted network.
		std::mutex headers_mtx;
		std::string frame_ancestors = "*";
		std::string frame_src = "'self'";
		// what script-src allows; a page with inline script (or onclick=) must
		// widen this via setScriptSrc
		std::string csp_script_src = "'self'";
		std::string common_headers;
		// a scanner's probes would fill the log
		LogMinute rejects;
		std::list<IO::SSEConnection> sse;
		int max_sse = Limits::STREAMS;
		// union of the subscriber masks, read by producers on other threads
		std::atomic<uint32_t> topics{0};
		std::atomic<std::size_t> bytes_out{0};
		std::chrono::steady_clock::time_point last_sse_ping{};
		// below this size the gzip header/CPU overhead outweighs the savings
		static const size_t GZIP_MIN_LENGTH = 1024;
		static const std::size_t MAX_REASON_SIZE = 120;

		void cleanupSSE()
		{
			const std::size_t before = sse.size();
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
			if (sse.size() != before)
				republishTopics();
		}

		void republishTopics()
		{
			uint32_t m = 0;
			for (const auto &c : sse)
				m |= c.topics();
			topics.store(m);
		}

		// Single parse pass over msg[0, header_end): request line + headers,
		// including framing validation. Returns 0 on success or the HTTP
		// status the request must be rejected with, with `error` set.
		int parseHeaders(const std::string &msg, std::size_t header_end, HTTPRequest &r, std::string &error);
		// the next complete request in the connection's buffer and what it takes of the buffer:
		// false while it has not fully arrived, or when it was rejected and the connection closes
		bool nextRequest(IO::TCPServerConnection &c, HTTPRequest &request, std::size_t &length);
		// one request answered: by a mount its path falls under, the subclass, or 405
		void answer(IO::TCPServerConnection &c, HTTPRequest &request);
		void reject(IO::TCPServerConnection &c, int status, const std::string &reason);
		bool localOnly() const;
		static bool addressedHere(const std::string &host);
		static bool isOwner(const HTTPRequest &r);
		bool dispatchMount(IO::TCPServerConnection &c, const HTTPRequest &r, bool accept_gzip);
		bool owns(const IO::TCPServerConnection &c) const { return c.owner == this; }

		void setMountedOn(HTTPServer *front) { mounted_on = front; }

		std::vector<std::pair<std::string, HTTPServer *>> mounts;
		std::mutex mounts_mtx;
		std::atomic<HTTPServer *> mounted_on{nullptr};
		std::string commonHeaders();

		ZIP zip;

	protected:
		void processClients() override;

		// Run() thread: reap dead subscribers, then fan the frame out to the rest.
		void onCommand(int id, const std::string &frame) override
		{
			cleanupSSE();
			for (auto &c : sse)
				if (c.subscribed(id))
					c.SendFrame(frame);
		}
	};
}
