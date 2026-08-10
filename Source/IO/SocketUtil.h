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

#include <string>
#include <atomic>
#include <cstring>

// Centralized platform socket includes and compat defines. winsock2.h must
// precede windows.h to avoid the winsock1/2 redefinition clash.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <netinet/tcp.h>
#ifdef __ANDROID__
#include <netinet/in.h>
#endif

#define SOCKET int
#define SOCKADDR struct sockaddr
#define SOCKET_ERROR -1
#endif

// Platform-normalized socket helpers, collapsing the per-call-site #ifdef _WIN32
// blocks (error classification, close, non-blocking mode).
namespace Net
{
	inline int lastError()
	{
#ifdef _WIN32
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	// Numeric host:port, so errors name the address actually tried.
	inline std::string addressString(const struct addrinfo *p)
	{
		char h[NI_MAXHOST], s[NI_MAXSERV];
		if (p && getnameinfo(p->ai_addr, (socklen_t)p->ai_addrlen, h, sizeof(h), s, sizeof(s),
							 NI_NUMERICHOST | NI_NUMERICSERV) == 0)
			return std::string(h) + ":" + s;
		return "?";
	}

	inline bool waitReady(SOCKET s, short events, int timeout_ms)
	{
#ifdef _WIN32
		WSAPOLLFD pfd = {s, events, 0};
		return WSAPoll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & (events | POLLHUP | POLLERR));
#else
		struct pollfd pfd = {s, events, 0};
		return ::poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & (events | POLLHUP | POLLERR));
#endif
	}

	inline bool waitWritable(SOCKET s, int timeout_ms) { return waitReady(s, POLLOUT, timeout_ms); }

	inline bool wouldBlock(int e)
	{
#ifdef _WIN32
		return e == WSAEWOULDBLOCK;
#else
		return e == EWOULDBLOCK || e == EAGAIN;
#endif
	}

	inline bool peerGone(int e)
	{
#ifdef _WIN32
		return e == WSAECONNRESET || e == WSAECONNABORTED || e == WSAENOTCONN ||
			   e == WSAESHUTDOWN || e == WSAETIMEDOUT || e == WSAENETRESET;
#else
		return e == ECONNRESET || e == EPIPE || e == ENOTCONN ||
			   e == ECONNABORTED || e == ETIMEDOUT;
#endif
	}

	inline bool connectInProgress(int e)
	{
#ifdef _WIN32
		return e == WSAEWOULDBLOCK;
#else
		return e == EINPROGRESS;
#endif
	}

	inline std::string errorString(int e)
	{
#ifdef _WIN32
		// strerror() only understands C errno values, not WSA error codes.
		char *buf = nullptr;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
						   FORMAT_MESSAGE_IGNORE_INSERTS,
					   nullptr, (DWORD)e, 0, (LPSTR)&buf, 0, nullptr);
		std::string s = buf ? buf : "unknown error";
		if (buf)
			LocalFree(buf);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		return s;
#else
		return std::strerror(e);
#endif
	}

	inline bool setTCPKeepAlive(SOCKET sock, int idle, int interval, int count)
	{
		int yes = 1;
#ifdef _WIN32
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&yes, sizeof(yes)))
			return false;

		struct tcp_keepalive keepalive;
		keepalive.onoff = 1;
		keepalive.keepalivetime = idle * 1000;
		keepalive.keepaliveinterval = interval * 1000;
		DWORD br;
		if (WSAIoctl(sock, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &br, NULL, NULL) == SOCKET_ERROR)
			return false;
#else
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&yes, sizeof(yes)))
			return false;
#if defined(__APPLE__)
		if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)))
			return false;
#elif defined(__ANDROID__)
		if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) ||
			setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) ||
			setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)))
			return false;
#else
		if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) ||
			setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) ||
			setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count)))
			return false;
#endif
#endif
		return true;
	}

	// Error a wedged (half-open / zero-window) socket instead of hanging forever.
	inline bool setTCPUserTimeout(SOCKET sock, int timeout_ms)
	{
#if defined(TCP_USER_TIMEOUT)
		return setsockopt(sock, IPPROTO_TCP, TCP_USER_TIMEOUT, (const char *)&timeout_ms, sizeof(timeout_ms)) == 0;
#elif defined(_WIN32) && defined(TCP_MAXRT)
		DWORD maxrt_secs = (DWORD)(timeout_ms / 1000);
		return setsockopt(sock, IPPROTO_TCP, TCP_MAXRT, (const char *)&maxrt_secs, sizeof(maxrt_secs)) == 0;
#else
		return true;
#endif
	}

#ifdef _WIN32
	inline int closeSocket(SOCKET s) { return ::closesocket(s); }

	inline bool setNonBlocking(SOCKET s)
	{
		u_long mode = 1;
		return ioctlsocket(s, FIONBIO, &mode) == 0;
	}
#else
	inline int closeSocket(int s) { return ::close(s); }

	inline bool setNonBlocking(int s)
	{
		int fl = fcntl(s, F_GETFL, 0);
		return fl != -1 && fcntl(s, F_SETFL, fl | O_NONBLOCK) != -1;
	}
#endif

	// Cross-thread wake for a poll()/WSAPoll() loop: a reliable, pollable handle
	// the loop watches; another thread calls notify() to make the blocking poll
	// return at once. POSIX uses a self-pipe, Windows a loopback TCP socket pair
	// (WSAPoll accepts only SOCKETs) — both reliable, so no wake is ever dropped.
	// Coalesced via `pending` so a burst of notify()s can't flood the fd, and
	// degradable: if init() fails, fd() is -1 and the caller falls back to the
	// poll timeout — never a hard failure.
	class Wakeup
	{
		SOCKET rd = -1; // read end, added to the poll set
		SOCKET wr = -1; // write end, poked by notify()
		std::atomic<bool> pending{false};

	public:
		Wakeup() {}
		~Wakeup() { close(); }
		Wakeup(const Wakeup &) = delete;
		Wakeup &operator=(const Wakeup &) = delete;

		bool init()
		{
#ifdef _WIN32
			SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
			if (listener == INVALID_SOCKET)
				return false;
			struct sockaddr_in a;
			std::memset(&a, 0, sizeof(a));
			a.sin_family = AF_INET;
			a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			a.sin_port = 0;
			int len = sizeof(a);
			bool ok = ::bind(listener, (SOCKADDR *)&a, sizeof(a)) == 0 && ::listen(listener, 1) == 0 &&
					  ::getsockname(listener, (SOCKADDR *)&a, &len) == 0;
			if (ok)
				wr = ::socket(AF_INET, SOCK_STREAM, 0);
			ok = ok && wr != INVALID_SOCKET && ::connect(wr, (SOCKADDR *)&a, sizeof(a)) == 0;
			if (ok)
				rd = ::accept(listener, NULL, NULL);
			ok = ok && rd != INVALID_SOCKET;
			::closesocket(listener);
			if (!ok)
			{
				close();
				return false;
			}
#else
			int fds[2];
			if (::pipe(fds) != 0)
				return false;
			rd = fds[0];
			wr = fds[1];
#endif
			setNonBlocking(rd);
			setNonBlocking(wr);
			return true;
		}

		SOCKET fd() const { return rd; }
		bool valid() const { return rd != -1; }

		// Coalesced: only the first notify() since the last drain() writes a byte;
		// the rest see `pending` already set and return. Errors are ignored — a
		// full pipe or a closed peer at shutdown is harmless, the poll backstop
		// still fires.
		void notify()
		{
			bool expected = false;
			if (!pending.compare_exchange_strong(expected, true))
				return;
			if (wr == -1)
				return;
			char b = 1;
#ifdef _WIN32
			::send(wr, &b, 1, 0);
#else
			ssize_t n = ::write(wr, &b, 1);
			(void)n;
#endif
		}

		// Reset the flag, THEN empty the fd, and do it before the loop consumes its
		// queue: a notify() racing the drain then leaves its byte for the next
		// poll rather than being lost.
		void drain()
		{
			pending.store(false);
			if (rd == -1)
				return;
			char buf[256];
#ifdef _WIN32
			while (::recv(rd, buf, sizeof(buf), 0) > 0) {}
#else
			while (::read(rd, buf, sizeof(buf)) > 0) {}
#endif
		}

		void close()
		{
			if (rd != -1) { closeSocket(rd); rd = -1; }
			if (wr != -1) { closeSocket(wr); wr = -1; }
		}
	};
}
