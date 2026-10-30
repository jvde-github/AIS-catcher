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

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "TCPServer.h"
#include "Logger.h"

#include <sys/stat.h>

#ifdef HASOPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

#ifdef _WIN32
#include <mstcpip.h>
#else
#include <arpa/inet.h> // For inet_addr() and INADDR_ANY
#include <netinet/tcp.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#endif

namespace IO
{

	const int TCPServer::MAX_CONN;
	std::vector<int> TCPServer::active_ports;

	void TCPServerConnection::Close()
	{
#ifdef HASOPENSSL
		if (ssl)
		{
			// says goodbye if the socket takes it, and does not wait for an answer
			ERR_clear_error();
			if (SSL_is_init_finished(ssl))
				SSL_shutdown(ssl);
			SSL_free(ssl);
			ssl = nullptr;
		}
#endif
		if (sock != -1)
		{
			Net::closeSocket(sock);
			sock = -1;
		}
		releaseBuffers();
	}

	void TCPServerConnection::Start(SOCKET s)
	{
		releaseBuffers();
		close_after_send = false;
		head_request = false;
		continue_sent = false;
		no_timeout = false;
		send_limit = MAX_BUFFER_SIZE;
		verbose = true;
		request_start = 0;
		ssl_want_write = false;
		stamp = std::time(nullptr);
		++generation; // new incarnation: invalidates any handle held to the old one
		sock = s;
	}

	void TCPServerConnection::startTLS(ssl_ctx_st *ctx)
	{
#ifdef HASOPENSSL
		ssl = SSL_new(ctx);
		if (!ssl || SSL_set_fd(ssl, (int)sock) != 1)
			Close();
#else
		(void)ctx;
#endif
	}

	bool TCPServerConnection::handshaking() const
	{
#ifdef HASOPENSSL
		return ssl && !SSL_is_init_finished(ssl);
#else
		return false;
#endif
	}

	// A handshake asks for the socket itself; what waits to go out waits for it
	short TCPServerConnection::pollEvents() const
	{
		return ssl_want_write || (hasSendBuffer() && !handshaking()) ? POLLIN | POLLOUT : POLLIN;
	}

	int TCPServerConnection::Inactive(std::time_t now) const
	{
		return (int)std::difftime(now, stamp);
	}

	void TCPServerConnection::take(const char *buffer, int nread)
	{
		if (msg.size() + nread > MAX_BUFFER_SIZE)
		{
			Warning() << "TCPServer: input buffer overflow, closing connection";
			Close();
		}
		else
		{
			msg.append(buffer, nread);
			// empty lines alone do not keep a connection alive
			if (std::find_if(buffer, buffer + nread, [](char ch) { return ch != '\r' && ch != '\n'; }) != buffer + nread)
				stamp = std::time(nullptr);
		}
	}

#ifdef HASOPENSSL
	// after a handshake or read that did not finish: wait for the socket, or the connection is over
	void TCPServerConnection::waitOrDropTLS(int result)
	{
		switch (SSL_get_error(ssl, result))
		{
		case SSL_ERROR_WANT_READ:
			break;
		case SSL_ERROR_WANT_WRITE:
			ssl_want_write = true;
			break;
		default:
			// no goodbye on a session that failed
			SSL_free(ssl);
			ssl = nullptr;
			Close();
		}
	}

	void TCPServerConnection::readTLS(char *buffer, int size)
	{
		ssl_want_write = false;
		ERR_clear_error();
		if (!SSL_is_init_finished(ssl))
		{
			const int r = SSL_accept(ssl);
			if (r != 1)
			{
				waitOrDropTLS(r);
				return;
			}
			stamp = std::time(nullptr);
		}
		// what OpenSSL holds of a record poll() cannot see, so it is taken now
		do
		{
			ERR_clear_error();
			const int nread = SSL_read(ssl, buffer, size);
			if (nread <= 0)
			{
				waitOrDropTLS(nread);
				return;
			}
			take(buffer, nread);
		} while (ssl && SSL_pending(ssl) > 0);
	}
#else
	void TCPServerConnection::waitOrDropTLS(int) {}
	void TCPServerConnection::readTLS(char *, int) {}
#endif

	void TCPServerConnection::Read()
	{
		char buffer[16384];

		if (isConnected())
		{
			if (ssl)
			{
				readTLS(buffer, sizeof(buffer));
				return;
			}

			int nread = recv(sock, buffer, sizeof(buffer), 0);

			if (nread > 0)
			{
				take(buffer, nread);
			}
			else if (nread == 0)
			{
				Close();
			}
			else
			{
				int e = Net::lastError();
				if (!Net::wouldBlock(e))
				{
					if (verbose && !Net::peerGone(e))
						Debug() << "Socket: read failed: " << Net::errorString(e) << ", sock = " << sock;

					Close();
				}
			}
		}
	}

	int TCPServerConnection::write(const char *data, int length)
	{
#ifdef HASOPENSSL
		if (ssl)
		{
			if (!SSL_is_init_finished(ssl))
				return 0;
			// a record at a time, as far as the socket takes them; what it does not take is
			// offered again from the front of out, as SSL_write needs it
			int sent = 0;
			while (sent < length)
			{
				ERR_clear_error();
				const int n = SSL_write(ssl, data + sent, length - sent);
				if (n <= 0)
				{
					// a write that waits for a read would be a renegotiation, and the context has none
					if (SSL_get_error(ssl, n) == SSL_ERROR_WANT_WRITE)
						return sent;
					SSL_free(ssl);
					ssl = nullptr;
					return -1;
				}
				sent += n;
			}
			return sent;
		}
#endif
		int bytes = ::send(sock, data, length, 0);

		if (bytes < 0)
		{
			int e = Net::lastError();
			if (Net::wouldBlock(e))
				return 0;
			if (verbose && !Net::peerGone(e))
				Error() << "TCP Connection: error message to client: " << Net::errorString(e);
			return -1;
		}
		return bytes;
	}

	void TCPServerConnection::SendBuffer()
	{
		if (isConnected() && hasSendBuffer())
		{
			int bytes = write(out.data() + out_pos, (int)pending());

			if (bytes < 0)
			{
				Close();
			}
			else
			{
				out_pos += bytes;
				if (out_pos == out.size())
				{
					shrink(out);
					out_pos = 0;
				}
			}
		}
	}
	bool TCPServerConnection::queue(const char *data, int length)
	{
		if (out_pos >= OUT_COMPACT_THRESHOLD)
			compact();

		if (pending() + length > (size_t)send_limit)
			return false;

		out.insert(out.end(), data, data + length);
		return true;
	}

	bool TCPServerConnection::Send(const char *data, int length)
	{
		if (!isConnected())
			return false;

		int bytes = 0;

		if (!hasSendBuffer())
		{
			bytes = write(data, length);

			if (bytes < 0)
			{
				Close();
				return false;
			}
		}

		// A client this far behind (e.g. a stalled SSE consumer) will not
		// recover; close instead of growing the buffer without bound.
		if (bytes < length && !queue(data + bytes, length - bytes))
		{
			if (verbose)
				Error() << "TCP Connection: send buffer limit exceeded, closing connection.";

			Close();
			return false;
		}

		return true;
	}

	// TLS of the server's port

#ifdef HASOPENSSL
	// a key with a passphrase fails here, rather than asking on the terminal
	static int noPassphrase(char *, int, int, void *) { return -1; }

	// why a file did not load: it does not open, or what OpenSSL made of it
	static std::string whyNot(const std::string &path)
	{
		FILE *f = fopen(path.c_str(), "rb");
		if (!f)
			return strerror(errno);
		fclose(f);
		const unsigned long e = ERR_peek_error();
		const char *reason = ERR_GET_LIB(e) == ERR_LIB_PEM ? nullptr : ERR_reason_error_string(e);
		return reason ? reason : "not a PEM file, or damaged";
	}

	// a context with this certificate and key, null with the reason
	static SSL_CTX *loadTLS(const std::string &cert, const std::string &key, std::string &error)
	{
		ERR_clear_error();
		SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
		if (!ctx)
		{
			error = "cannot make a TLS context";
			return nullptr;
		}
		// TLS 1.2 or later, as the client has it; without renegotiation a write never waits for a read
		long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
#ifdef SSL_OP_NO_RENEGOTIATION
		options |= SSL_OP_NO_RENEGOTIATION;
#endif
		SSL_CTX_set_options(ctx, options);
		// writes as a plain socket takes them, from a buffer that moves; an idle connection keeps no buffers
		SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_RELEASE_BUFFERS);
		// a client comes back with a ticket it holds, nothing is kept here for it
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
		SSL_CTX_set_default_passwd_cb(ctx, noPassphrase);

		// the key first: then a certificate of another key fails the check, in those words
		if (SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) != 1)
			error = "cannot read the key \"" + key + "\", a PEM file without a passphrase: " + whyNot(key);
		else if (SSL_CTX_use_certificate_chain_file(ctx, cert.c_str()) != 1)
			error = "cannot read the certificate \"" + cert + "\": " + whyNot(cert);
		else if (SSL_CTX_check_private_key(ctx) != 1)
			error = "the certificate \"" + cert + "\" and the key \"" + key + "\" do not match";
		ERR_clear_error();
		if (!error.empty())
		{
			SSL_CTX_free(ctx);
			return nullptr;
		}
		return ctx;
	}

	static long long notAfter(SSL_CTX *ctx)
	{
		int days = 0, seconds = 0;
		const X509 *x = SSL_CTX_get0_certificate(ctx);
		ASN1_TIME_diff(&days, &seconds, nullptr, x ? X509_get0_notAfter(x) : nullptr);
		return (long long)std::time(nullptr) + days * 86400LL + seconds;
	}

	static std::time_t modified(const std::string &path)
	{
		struct stat s;
		return stat(path.c_str(), &s) == 0 ? s.st_mtime : 0;
	}
#endif

	std::string TCPServer::checkTLS(const std::string &cert, const std::string &key)
	{
#ifdef HASOPENSSL
		std::string error;
		SSL_CTX *ctx = loadTLS(cert, key, error);
		if (ctx)
			SSL_CTX_free(ctx);
		return error;
#else
		(void)cert;
		(void)key;
		return "this build has no OpenSSL, so no HTTPS (cmake -DOPENSSL=ON).";
#endif
	}

	bool TCPServer::setTLS(const std::string &cert, const std::string &key, std::string &error)
	{
#ifdef HASOPENSSL
		SSL_CTX *fresh = nullptr;
		const std::time_t cert_time = modified(cert), key_time = modified(key);
		if (!cert.empty())
		{
			fresh = loadTLS(cert, key, error);
			if (!fresh)
				return false;
#ifndef _WIN32
			struct stat s;
			if (stat(key.c_str(), &s) == 0 && (s.st_mode & S_IROTH))
				Warning() << "Server: the key \"" << key << "\" can be read by every user of this machine: chmod o-r it";
#endif
		}
		std::lock_guard<std::mutex> lock(tls_mtx);
		if (tls_ctx)
			SSL_CTX_free(tls_ctx);
		tls_ctx = fresh;
		tls_cert = cert;
		tls_key = key;
		tls_cert_time = cert_time;
		tls_key_time = key_time;
		tls_warned = false;
		tls_until = fresh ? notAfter(fresh) : 0;
		if (fresh && tls_until < std::time(nullptr))
			Warning() << "Server: the certificate \"" << cert << "\" has expired, browsers will refuse it: renew it, the server takes the new one by itself";
		return true;
#else
		if (cert.empty())
			return true;
		error = checkTLS(cert, key);
		return false;
#endif
	}

	// On the Run() thread, about once a second. A renewal is taken when the files change; while
	// they do not load the server keeps the certificate it has, and they are tried again.
	void TCPServer::reloadTLS(std::time_t now)
	{
#ifdef HASOPENSSL
		if (!tls_until || now - tls_checked < TLS_CHECK_INTERVAL)
			return;
		std::lock_guard<std::mutex> lock(tls_mtx);
		if (!tls_ctx)
			return;
		tls_checked = now;
		const std::time_t cert_time = modified(tls_cert), key_time = modified(tls_key);
		if (cert_time == tls_cert_time && key_time == tls_key_time)
			return;
		std::string error;
		SSL_CTX *fresh = loadTLS(tls_cert, tls_key, error);
		if (!fresh)
		{
			if (!tls_warned)
				Warning() << "Server: port " << listening_port << " keeps the certificate it has, a new one does not load: " << error;
			tls_warned = true;
			return;
		}
		// the connections that have the old context keep it until they close
		SSL_CTX_free(tls_ctx);
		tls_ctx = fresh;
		tls_cert_time = cert_time;
		tls_key_time = key_time;
		tls_warned = false;
		tls_until = notAfter(fresh);
		Info() << "Server: port " << listening_port << " has a new certificate";
#else
		(void)now;
#endif
	}

	// TCP Server

	void TCPServer::stopThread()
	{
		stop = true;
		wakeup.notify(); // break out of poll() now rather than on the backstop

		if (run_thread.joinable())
			run_thread.join();
	}

	TCPServer::~TCPServer()
	{
		stopThread();

		for (auto &c : client)
		{
			c.Close();
		}

#ifdef HASOPENSSL
		if (tls_ctx)
			SSL_CTX_free(tls_ctx);
#endif

		if (sock != -1)
			Net::closeSocket(sock);

		if (listening_port != -1)
		{
			auto it = std::find(active_ports.begin(), active_ports.end(), listening_port);
			if (it != active_ports.end())
				active_ports.erase(it);
		}
	}

	int TCPServer::numberOfClients()
	{
		int n = 0;
		for (auto &c : client)
			if (c.isConnected())
				n++;
		return n;
	}

	int TCPServer::findFreeClient()
	{
		for (int i = 0; i < MAX_CONN; i++)
			if (!client[i].isConnected())
				return i;
		return -1;
	}

	void TCPServer::acceptClients()
	{
		sockaddr_in peer;
		int addrlen = sizeof(peer);
		SOCKET conn_socket;

		conn_socket = accept(sock, (SOCKADDR *)&peer, (socklen_t *)&addrlen);
#ifdef _WIN32
		if (conn_socket == SOCKET_ERROR)
#else
		if (conn_socket == -1)
#endif
		{
			int e = Net::lastError();
			if (!Net::wouldBlock(e) && !Net::peerGone(e))
				Error() << "TCP Server: error accepting connection: " << Net::errorString(e);
			return;
		}

		if (max_per_address && (ntohl(peer.sin_addr.s_addr) >> 24) != 127)
		{
			int held = 0;
			for (const auto &c : client)
				held += c.isConnected() && c.peer_ipv4 == peer.sin_addr.s_addr;
			if (held >= max_per_address)
			{
				Net::closeSocket(conn_socket);
				return;
			}
		}

		int ptr = findFreeClient();
		if (ptr == -1)
		{
			Error() << "TCP Server: max connections reached (" << MAX_CONN << "), closing socket.";
			Net::closeSocket(conn_socket);
			return;
		}

		// Configure fully before Start() publishes the socket to broadcast threads.
		// TCP_NODELAY is best-effort: a client that reset since accept() fails it,
		// and that is not a reason to drop the connection here.
		int flag = 1;
		setsockopt(conn_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

		const int idle = 60, interval = 20, count = 3;
		Net::setTCPKeepAlive(conn_socket, idle, interval, count);
		Net::setTCPUserTimeout(conn_socket, (idle + interval * count) * 1000);

		// not optional: a blocking client socket would stall the loop
		if (!Net::setNonBlocking(conn_socket))
		{
			int e = Net::lastError();
			if (!Net::peerGone(e))
				Error() << "TCP Server: cannot make client socket non-blocking: " << Net::errorString(e);
			Net::closeSocket(conn_socket);
			return;
		}

		client[ptr].peer_ipv4 = peer.sin_addr.s_addr;
		client[ptr].Start(conn_socket);

		if (tls_until)
		{
			std::lock_guard<std::mutex> lock(tls_mtx);
			if (tls_ctx)
				client[ptr].startTLS(tls_ctx);
		}
	}

	void TCPServer::cleanUp(std::time_t now)
	{
		if (tls_until)
			for (auto &c : client)
				if (c.handshaking() && c.Inactive(now) > HANDSHAKE_TIMEOUT)
					c.Close();

		if (!timeout)
			return;

		for (auto &c : client)
			if (c.isConnected() && c.Inactive(now) > timeout && !c.no_timeout)
			{
				c.Close();
			}
	}

	void TCPServer::readClients()
	{
		for (int i = 0; i < MAX_CONN; i++)
			if (pfds[i + 2].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL | (client[i].readWantsWrite() ? POLLOUT : 0)))
				client[i].Read();
	}

	void TCPServer::writeClients()
	{

		for (auto &c : client)
		{
			c.SendBuffer();

			if (c.close_after_send && c.isConnected() && !c.hasSendBuffer())
				c.Close();
		}
	}

	void TCPServer::processClients()
	{
		for (auto &c : client)
		{
			if (c.isConnected())
			{
				c.msg.clear();
			}
		}
	}

	void TCPServer::Run()
	{
		while (!stop)
		{
			SleepAndWait();

			try
			{
				// execute all queued cross-thread work here, on the sole owner
				// thread; drain the wake first so a command posted mid-cycle is
				// either already in the queue or re-wakes the next poll
				if (pfds[1].revents & POLLIN)
					wakeup.drain();
				drainCommands();
				if (pfds[0].revents & POLLIN)
					acceptClients();
				readClients();
				processClients();
				writeClients();

				// the loop wakes per command now, so keep these sweeps at ~1 Hz
				const std::time_t now = std::time(nullptr);
				if (now != last_housekeeping)
				{
					last_housekeeping = now;
					reloadTLS(now);
					cleanUp(now);

					if (pstats)
						pstats->connected = numberOfClients();
				}
			}
			catch (const std::exception &e)
			{
				Error() << "TCP Server: exception in main loop: " << e.what() << ", closing all connections";
				for (auto &c : client)
					c.Close();
			}
		}

		Debug() << "TCP Server: thread ending.";
	}

	void TCPServer::SleepAndWait()
	{
		// poll() rather than select(): keyed on fd values with no FD_SETSIZE ceiling,
		// so a high-numbered socket in a busy process can't overflow an fd_set.
		// Blocks until a socket is ready or 1s elapses; the revents left in pfds
		// tell Run() which sockets to accept/read.
		pfds[0].fd = sock;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		// the wake handle; -1 when unavailable so poll simply skips it
		pfds[1].fd = wakeup.valid() ? wakeup.fd() : -1;
		pfds[1].events = POLLIN;
		pfds[1].revents = 0;

		for (int i = 0; i < MAX_CONN; i++)
		{
			TCPServerConnection &c = client[i];
			pollfd &p = pfds[i + 2];

			p.fd = c.isConnected() ? c.sock : -1;
			p.events = c.pollEvents();
			p.revents = 0;
		}

		// With the wake handle latency comes from poll() returning on the wake, so
		// this timeout is just a liveness backstop — keep it long (1/s idle, gentle
		// on low-power boards). Only when the wake is unavailable does it also carry
		// command latency, so it tightens to 50 ms then.
		const int POLL_MS = wakeup.valid() ? 1000 : 50;
#ifdef _WIN32
		int r = WSAPoll(pfds.data(), (ULONG)pfds.size(), POLL_MS);
#else
		int r = poll(pfds.data(), (nfds_t)pfds.size(), POLL_MS);
#endif
		// on poll failure fall back to sweeping every socket rather than none
		if (r < 0)
			for (auto &p : pfds)
				p.revents = POLLIN;
	}

	void TCPServer::post(Command c)
	{
		{
			std::lock_guard<std::mutex> lk(cmd_mtx);
			cmds.push_back(std::move(c));
		}
		// wake the loop now; the byte is coalesced so a burst can't flood the fd
		wakeup.notify();
	}

	void TCPServer::drainCommands()
	{
		{
			std::lock_guard<std::mutex> lk(cmd_mtx);
			if (cmds.empty())
				return;
			cmd_scratch.swap(cmds);
		}
		for (auto &c : cmd_scratch)
		{
			switch (c.kind)
			{
			case Command::BroadcastRaw:
				for (auto &cl : client)
				{
					if (cl.isConnected() && !Send(cl, c.data.data(), (int)c.data.size()))
					{
						cl.Close();
						Error() << "TCP listener: client not reading, close connection.";
						if (pstats)
							pstats->dropped++;
					}
				}
				break;
			case Command::Derived:
				onCommand(c.id, c.data);
				break;
			case Command::CloseAll:
				for (auto &cl : client)
					cl.Close();
				break;
			}
		}
		cmd_scratch.clear();
	}

	// Cross-thread producers only enqueue; the Run() loop performs the actual
	// sends, so no other thread ever touches a connection.
	void TCPServer::SendAll(std::string m)
	{
		post({Command::BroadcastRaw, 0, std::move(m)});
	}

	bool TCPServer::start(int port)
	{
		for (auto &c : client)
			c.owner = this;

		for (const auto &p : active_ports)
		{
			if (port && p == port)
			{
				Error() << "TCP Server: port " << port << " is already in use by another server instance";
				return false;
			}
		}

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return false;

#ifndef _WIN32
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		}
		if (reuse_port)
		{
			int optval = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		}
#endif
		sockaddr_in service;
		memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;

		// Conditional binding based on IP_BIND
		if (!IP_BIND.empty())
		{
			service.sin_addr.s_addr = inet_addr(IP_BIND.c_str());
		}
		else
		{
			service.sin_addr.s_addr = htonl(INADDR_ANY);
		}

		service.sin_port = htons(port);

		int r = bind(sock, (SOCKADDR *)&service, sizeof(service));
		if (r == SOCKET_ERROR)
		{
			Net::closeSocket(sock);
			sock = -1;
			return false;
		}

		if (listen(sock, 511) < 0)
		{
			Net::closeSocket(sock);
			sock = -1;
			return false;
		}

		if (port == 0)
		{
			struct sockaddr_in bound;
			socklen_t len = sizeof(bound);
			if (getsockname(sock, (SOCKADDR *)&bound, &len) == 0)
				port = ntohs(bound.sin_port);
		}

		for (auto &c : client)
			c.Close();

		if (!Net::setNonBlocking(sock))
		{
			Error() << "TCP Server: cannot set socket to non-blocking";
		}
		stop = false;

		if (IP_BIND.empty())
			Debug() << "TCP Server: start thread at port " << port;
		else
			Debug() << "TCP Server: start thread at IP " << IP_BIND << " port " << port;

		listening_port = port;
		active_ports.push_back(port);

		// not fatal if it fails: the loop still drains on the poll timeout
		if (!wakeup.init())
			Debug() << "TCP Server: wake handle unavailable, relying on poll timeout.";

		run_thread = std::thread(&TCPServer::Run, this);

		return true;
	}
}