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

	inline bool wouldBlock(int e)
	{
#ifdef _WIN32
		return e == WSAEWOULDBLOCK;
#else
		return e == EWOULDBLOCK || e == EAGAIN;
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
}
