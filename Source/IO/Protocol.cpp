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

#include "Protocol.h"
#include "Logger.h"
#include "N2K.h"
#include "AIS-catcher.h"

#include <sys/types.h>

#ifdef _WIN32
#include <mstcpip.h>
#else
#include <netinet/tcp.h>
#ifdef __ANDROID__
#include <android/log.h>
#include <dirent.h>
#endif
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#endif

#include <errno.h>
#include <algorithm>
#include <random>

#ifdef HASOPENSSL
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#endif

#if defined(HASOPENSSL) && defined(_WIN32)
// after the OpenSSL headers: wincrypt.h defines macros that clash with OpenSSL's type names
#include <wincrypt.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#undef X509_CERT_PAIR
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#endif

namespace Protocol
{
	const int ProtocolBase::SEND_STALL_MS;

	void TCP::disconnect()
	{
		// closed first: a goodbye a layer sends from onDisconnect() has nowhere to go
		bool was_ready = state == READY;
		state = DISCONNECTED;
		// a session this short was a rejection by a layer above, not a connection
		if (was_ready && Net::secondsSince(stamp) > 30)
			attempts = 0;
		reconnect_delay = jittered(backoff(attempts) * 1000L, 20) / 1000.0;
		addresses.reset();
		next_address = nullptr;

		if (was_ready && sock != -1 && !pending.empty())
			::send(sock, pending.data(), (int)pending.size(), 0);
		pending.clear();

		if (sock != -1)
		{
			if (stats)
				stats->connected = false;

			onDisconnect();
			Net::closeSocket(sock);
		}

		if (was_ready)
			Debug() << "TCP (" << host << ":" << port << "): Disconnected.";

		sock = -1;
	}

	bool TCP::connect()
	{
		disconnect();
		attempts++;
		stamp = Net::Clock::now();

		if (!resolver.start(host, port, Net::lookupHints(SOCK_STREAM, IPPROTO_TCP)))
			return connectFailed("cannot start a lookup.");

		state = RESOLVING;
		return true;
	}

	bool TCP::connectResolved(std::unique_ptr<Net::AddressInfo> ai)
	{
		if (!ai->isSuccess())
			return connectFailed("getaddrinfo failed: " + ai->getErrorMessage());

		addresses = std::move(ai);
		next_address = addresses->get();
		last_error.clear();
		return tryNextAddress();
	}

	bool TCP::tryNextAddress()
	{
		while (next_address)
		{
			struct addrinfo *p = next_address;
			next_address = p->ai_next;

			if (connectAddress(p))
				return true;
		}

		addresses.reset();
		state = DISCONNECTED;
		return connectFailed(last_error);
	}

	// One connect attempt against a single addrinfo entry. Returns true if this attempt
	// owns a live socket (READY, or CONNECTING with the connect in progress). Returns false
	// if this address failed: sock is closed and reset to -1 so the caller can try the next.
	bool TCP::connectAddress(struct addrinfo *p)
	{
		auto fail = [this, p](const char *op) -> bool
		{
			last_error = std::string(op) + " " + Net::addressString(p) + ": " + Net::errorString(Net::lastError());
			if (sock != -1)
			{
				Net::closeSocket(sock);
				sock = -1;
			}
			return false;
		};

		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock == -1)
			return fail("socket");

		int yes = 1;
#ifdef _WIN32
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes)) == -1)
#else
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&yes, sizeof(yes)) == -1)
#endif
			return fail("TCP_NODELAY");

		if (keep_alive)
		{
			const int idle = 20, interval = 5, count = 2;

			if (!Net::setTCPKeepAlive(sock, idle, interval, count))
				return fail("keepalive");

			if (!Net::setTCPUserTimeout(sock, (idle + interval * count) * 1000))
				Debug() << "TCP (" << host << ":" << port << "): user timeout not applied: " << Net::errorString(Net::lastError());
		}

		if (!Net::setNonBlocking(sock))
			return fail("set non-blocking");

		stamp = Net::Clock::now();
		int r = ::connect(sock, p->ai_addr, (int)p->ai_addrlen);

		if (r != -1)
		{
			ready();
			return true;
		}
		if (!Net::connectInProgress(Net::lastError()))
			return fail("connect");

		// one look now catches a refusal already in, as on localhost, so the next address
		// is tried without a wait; a layer that gave up in onConnect() is not a refusal
		state = CONNECTING;
		checkConnecting();
		return !(state == CONNECTING && sock == -1);
	}

	// a refusal closes the socket for the next address, the reason kept for the log
	bool TCP::checkConnecting()
	{
		if (state == READY)
			return true;

		if (sock == -1)
			return false;

		fd_set fdr, fdw, fde;

		FD_ZERO(&fdr);
		FD_SET(sock, &fdr);

		FD_ZERO(&fdw);
		FD_SET(sock, &fdw);

		// Winsock reports a failed connect in the exception set
		FD_ZERO(&fde);
		FD_SET(sock, &fde);

		timeval to = {0, 1};

		if (select(sock + 1, &fdr, &fdw, &fde, &to) <= 0)
			return false;

		int error = -1;
		socklen_t len = sizeof(error);

		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) != 0)
			error = -1;

		if (error != 0)
		{
			giveUp((error > 0 ? Net::errorString(error) : std::string("getsockopt failed")) + " (" + std::to_string(error) + ")");
			return false;
		}

		ready();
		return true;
	}

	// a connect that failed, false: the first of an outage is an error, the ones after it only debugging
	bool TCP::connectFailed(const std::string &why)
	{
		if (stats)
			stats->connect_fail++;
		(outage ? Debug() : Error()) << "TCP (" << host << ":" << port << "): " << why;
		outage = true;
		return false;
	}

	void TCP::giveUp(const std::string &why)
	{
		last_error = why;
		Debug() << "TCP (" << host << ":" << port << "): " << why << (next_address ? ", trying the next address." : "");
		Net::closeSocket(sock);
		sock = -1;
	}

	void TCP::ready()
	{
		state = READY;
		randomizeResetInterval();

		(outage ? Info() : Debug()) << "TCP (" << host << ":" << port << "): connected.";
		outage = false;

		if (stats)
		{
			stats->connect_ok++;
			stats->connected = true;
		}

		onConnect();
	}

	void TCP::randomizeResetInterval()
	{
		if (reset_time <= 0)
			return;

		reset_interval = jittered((long)reset_time * 60, 10);
		Debug() << "TCP (" << host << ":" << port << "): reset in " << reset_interval << "s.";
	}

	long TCP::jittered(long base, int percent)
	{
		long span = base * percent / 100;

		static thread_local std::mt19937 gen(std::random_device{}());
		std::uniform_int_distribution<long> dist(-span, span);

		return base + dist(gen);
	}

	void TCP::updateState()
	{
		Net::Clock::time_point now = Net::Clock::now();

		switch (state)
		{
		case RESOLVING:
		{
			std::unique_ptr<Net::AddressInfo> ai = resolver.take();
			if (ai)
			{
				state = DISCONNECTED;
				connectResolved(std::move(ai));

				// the pause counts from the failure, not from the start of the lookup
				if (state == DISCONNECTED)
					stamp = now;
			}
			else if (Net::secondsSince(stamp) > LOOKUP_TIME)
			{
				Warning() << "TCP (" << host << ":" << port << "): lookup not finished after " << LOOKUP_TIME << " s, still waiting.";
				stamp = now;
			}
			break;
		}

		case CONNECTING:
		{
			if (checkConnecting())
				break;

			if (sock != -1)
			{
				if (Net::secondsSince(stamp) <= (timeout > 0 ? timeout : RECONNECT_TIME))
					break;

				giveUp("connect timed out");
			}

			if (!tryNextAddress())
				stamp = now;
			break;
		}

		case READY:
			if (reset_time > 0 && Net::secondsSince(stamp) > reset_interval)
			{
				Debug() << "TCP (" << host << ":" << port << "): reset interval reached, refreshing connection.";
				reconnect();
			}
			break;

		case DISCONNECTED:
			if (Net::secondsSince(stamp) > reconnect_delay)
			{
				(outage ? Debug() : Warning()) << "TCP (" << host << ":" << port << "): not connected, reconnecting.";
				reconnect();
			}
			break;
		}
	}

	void TCP::poll()
	{
		updateState();

		if (state == READY)
			flushPending();
	}

	int TCP::send(const void *data, int length)
	{
		if (state != READY)
			return 0;

		return sendKept(data, length);
	}

	int TCP::sendSome(const char *p, int length, bool &progress)
	{
		int total = 0;
		while (total < length)
		{
			int sent = ::send(sock, p + total, length - total, 0);
			if (sent > 0)
			{
				total += sent;
				continue;
			}
			if (sent < 0)
			{
				int error_code = Net::lastError();
				if (!Net::wouldBlock(error_code))
				{
					if (stats)
						stats->bytes_out += total;
					handleNetworkError("send", error_code);
					return -1;
				}
			}
			break;
		}

		progress = total > 0;

		if (stats)
			stats->bytes_out += total;

		return total;
	}

	// nothing, not an error, while the connection is down: it is being made again
	int TCP::read(void *data, int length, int timeout, bool wait)
	{
		if (state != READY)
			return 0;

		char *buffer = (char *)data;
		int total_received = 0;
		int remaining_length = length;
		int remaining_time = timeout = MAX(timeout, 0);

		Net::Clock::time_point start_time = Net::Clock::now();

		while (remaining_length > 0)
		{
			if (timeout)
			{
				int elapsed = (int)Net::secondsSince(start_time);
				remaining_time = MAX(timeout - elapsed, 0);
				if (remaining_time <= 0)
					break;
			}

			fd_set readfds, exceptfds;
			FD_ZERO(&readfds);
			FD_ZERO(&exceptfds);
			FD_SET(sock, &readfds);
			FD_SET(sock, &exceptfds);

			timeval tv = {remaining_time, 1};
			int select_result = select(sock + 1, &readfds, nullptr, &exceptfds, &tv);

			if (select_result < 0)
			{
				int error_code = Net::lastError();
				if (error_code == EINTR)
					break;

				handleNetworkError("select()", error_code);
				return total_received;
			}

			if (select_result == 0 || (!FD_ISSET(sock, &readfds) && !FD_ISSET(sock, &exceptfds)))
				break;

			int received = recv(sock, buffer + total_received, remaining_length, 0);

			if (received == 0)
			{
				Debug() << "TCP (" << host << ":" << port << "): connection closed by peer.";
				disconnect();
				break;
			}

			if (received < 0)
			{
				int error_code = Net::lastError();

				if (Net::wouldBlock(error_code))
				{
					if (timeout)
						continue;
					else
						break;
				}

				handleNetworkError("receive", error_code);
				return total_received;
			}

			total_received += received;
			remaining_length -= received;

			if (!wait)
				break;
		}

		if (stats)
			stats->bytes_in += total_received;

		return total_received;
	}

#ifdef HASOPENSSL

	const int TLS::HANDSHAKE_TIME;

#ifdef __ANDROID__
	// Android ships no OpenSSL trust store, and names its CA files with the pre-1.0.0
	// subject hash, so a CApath lookup by the modern hash finds nothing. Read them once
	// and hand the certificates to every context we build.
	static const std::vector<X509 *> &androidCACertificates()
	{
		static std::vector<X509 *> certs;
		static std::once_flag flag;

		std::call_once(flag, []()
					   {
						   // the APEX store is the updatable one, /system the older location
						   for (const char *dir : {"/apex/com.android.conscrypt/cacerts", "/system/etc/security/cacerts"})
						   {
							   DIR *d = opendir(dir);
							   if (!d)
								   continue;

							   while (dirent *e = readdir(d))
							   {
								   if (e->d_name[0] == '.')
									   continue;

								   FILE *f = fopen((std::string(dir) + "/" + e->d_name).c_str(), "r");
								   if (!f)
									   continue;

								   while (X509 *c = PEM_read_X509(f, nullptr, nullptr, nullptr))
									   certs.push_back(c);
								   fclose(f);
							   }
							   closedir(d);

							   if (!certs.empty())
								   break;
						   }

						   Info() << "TLS: loaded " << certs.size() << " system CA certificates"; });

		return certs;
	}
#endif

#ifdef _WIN32
	// The Windows build ships OpenSSL from vcpkg, whose compiled-in certificate directory
	// is not on a user's machine, so the default verify paths find nothing. Read the
	// system ROOT store once instead; it follows Windows Update. SSL_CERT_FILE and
	// SSL_CERT_DIR stay honoured through the default paths added after these.
	static const std::vector<X509 *> &windowsCACertificates()
	{
		static std::vector<X509 *> certs;
		static std::once_flag flag;

		std::call_once(flag, []()
					   {
						   HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
						   if (store)
						   {
							   for (PCCERT_CONTEXT c = CertEnumCertificatesInStore(store, nullptr); c; c = CertEnumCertificatesInStore(store, c))
							   {
								   const unsigned char *der = c->pbCertEncoded;
								   if (X509 *x = d2i_X509(nullptr, &der, (long)c->cbCertEncoded))
									   certs.push_back(x);
							   }
							   CertCloseStore(store, 0);
						   }

						   Info() << "TLS: loaded " << certs.size() << " certificates from the Windows ROOT store"; });

		return certs;
	}
#endif

	void TLS::onConnect()
	{
		// Create SSL context
		ctx = SSL_CTX_new(TLS_client_method());
		if (!ctx)
		{
			Error() << "TLS: Failed to create SSL context for " << getHost() << ":" << getPort();
			disconnect();
			return;
		}

		// Disable weak SSL/TLS protocols (SSLv2, SSLv3, TLS 1.0, TLS 1.1)
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);

		if (verify_certificates)
		{
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
#if defined(__ANDROID__) || defined(_WIN32)
#ifdef __ANDROID__
			const std::vector<X509 *> &cas = androidCACertificates();
#else
			const std::vector<X509 *> &cas = windowsCACertificates();
#endif
			if (cas.empty())
				Error() << "TLS: no system CA certificates found, verification will fail";

			X509_STORE *store = SSL_CTX_get_cert_store(ctx);
			for (X509 *c : cas)
				X509_STORE_add_cert(store, c);
#endif
#ifndef __ANDROID__
			SSL_CTX_set_default_verify_paths(ctx);
#endif
		}
		else
		{
			SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
			if (!own_server)
				Warning() << "TLS: Certificate verification DISABLED for " << getHost() << ":" << getPort();
		}

		// Create SSL object
		ssl = SSL_new(ctx);
		if (!ssl)
		{
			Error() << "TLS: Failed to create SSL object for " << getHost() << ":" << getPort();
			disconnect();
			return;
		}

		// a write that did not finish is retried from the kept copy, not the caller's buffer
		SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

		// Set SNI (Server Name Indication) hostname
		SSL_set_tlsext_host_name(ssl, getHost().c_str());

		// Enable hostname verification (protection against MitM attacks)
		if (verify_certificates)
		{
			X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
			// an address is compared with the certificate's IP names, a name with its DNS names
			if (X509_VERIFY_PARAM_set1_ip_asc(param, getHost().c_str()) != 1)
				X509_VERIFY_PARAM_set1_host(param, getHost().c_str(), 0);
		}

		// Create custom BIO that uses our TCP connection
		BIO *bio = BIO_new(BIO_s_socket());
		BIO_set_fd(bio, getSocket(), BIO_NOCLOSE);
		SSL_set_bio(ssl, bio, bio);

		tls_state = TLS_HANDSHAKING;
		handshake_deadline = Net::Clock::now() + std::chrono::seconds(HANDSHAKE_TIME);
		performHandshake();
	}

	void TLS::onDisconnect()
	{
		ProtocolBase::onDisconnect();

		if (ssl)
		{
			if (handshake_complete)
			{
				SSL_shutdown(ssl);
				Debug() << "TLS (" + getHost() + ":" + getPort() + "): Disconnected";
			}
			SSL_free(ssl);
			ssl = nullptr;
		}
		if (ctx)
		{
			SSL_CTX_free(ctx);
			ctx = nullptr;
		}

		handshake_complete = false;
		tls_state = TLS_DISCONNECTED;
		pending.clear();
	}

	bool TLS::isConnected()
	{
		return prev && prev->isConnected() && handshake_complete;
	}

	void TLS::poll()
	{
		ProtocolBase::poll();

		if (!prev || !prev->isConnected())
			return;

		if (handshake_complete)
		{
			flushPending();
			return;
		}

		updateState();
		if (ssl && !handshake_complete && Net::Clock::now() > handshake_deadline)
		{
			Error() << "TLS (" << getHost() << ":" << getPort() << "): handshake not finished after " << HANDSHAKE_TIME << " s, disconnecting";
			disconnect();
		}
	}

	int TLS::send(const void *data, int length)
	{
		if (!isConnected())
			return 0;

		return sendKept(data, length);
	}

	// a record goes whole or not at all; progress when part of it went out
	int TLS::sendSome(const char *data, int length, bool &progress)
	{
		uint64_t before = BIO_number_written(SSL_get_wbio(ssl));
		int sent = SSL_write(ssl, data, length);
		uint64_t written = BIO_number_written(SSL_get_wbio(ssl)) - before;
		if (stats)
			stats->bytes_out += written;

		progress = written > 0;
		if (sent > 0)
			return length;

		int error = SSL_get_error(ssl, sent);
		if (error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_READ)
			return 0;

		if (error == SSL_ERROR_ZERO_RETURN)
			Warning() << "TLS (" << getHost() << ":" << getPort() << "): Connection closed by peer";
		else
			Error() << "TLS (" << getHost() << ":" << getPort() << "): Send error: " << getSSLErrorString(error);
		disconnect();
		return -1;
	}

	int TLS::read(void *data, int length, int timeout, bool wait)
	{
		if (!isConnected())
			return 0;

		uint64_t before = BIO_number_read(SSL_get_rbio(ssl));
		int result = 0;

		// do not block if we are not waiting
		if (!wait || timeout <= 0)
		{
			int avail = SSL_pending(ssl);
			if (avail == 0)
			{
				SOCKET sock = getSocket();
#ifdef _WIN32
				if (sock != INVALID_SOCKET)
#else
				if (sock >= 0)
#endif
				{
					fd_set readfds;
					FD_ZERO(&readfds);
					FD_SET(sock, &readfds);
					timeval tv = {timeout > 0 ? timeout : 0, 0};
					if (select(sock + 1, &readfds, nullptr, nullptr, &tv) > 0)
						avail = length;
				}
			}
			if (avail > 0)
			{
				int r = SSL_read(ssl, data, MIN(avail, length));
				if (r > 0)
					result = r;
				else
				{
					int err = SSL_get_error(ssl, r);
					if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
					{
						if (stats)
							stats->bytes_in += BIO_number_read(SSL_get_rbio(ssl)) - before;
						disconnect();
						return -1;
					}
				}
			}
		}
		else
		{
			SOCKET sock = getSocket();
#ifdef _WIN32
			if (sock == INVALID_SOCKET)
#else
			if (sock < 0)
#endif
			{
				disconnect();
				return -1;
			}

			int time = 0;
			int total_received = 0;
			char *buf_ptr = (char *)data;

			while (time < timeout && total_received < length)
			{
				int pending = SSL_pending(ssl);
				if (pending > 0)
				{
					int to_read = MIN(pending, length - total_received);
					int r = SSL_read(ssl, buf_ptr + total_received, to_read);
					if (r > 0)
					{
						total_received += r;
						if (total_received >= length)
							break;
						continue;
					}
				}

				fd_set readfds;
				FD_ZERO(&readfds);
				FD_SET(sock, &readfds);
				struct timeval tv = {1, 0};

				int rv = select(sock + 1, &readfds, nullptr, nullptr, &tv);
				if (rv < 0)
				{
#ifdef _WIN32
					int sock_err = WSAGetLastError();
					if (sock_err == WSAEINTR)
						continue;
					Error() << "TLS select failed. Error code: " << sock_err;
#else
					if (errno == EINTR)
						continue;
					Error() << "TLS select failed: " << strerror(errno);
#endif
					if (stats)
						stats->bytes_in += BIO_number_read(SSL_get_rbio(ssl)) - before;

					disconnect();
					return -1;
				}
				if (rv == 0)
				{
					time++;
					continue;
				}

				int r = SSL_read(ssl, buf_ptr + total_received, length - total_received);
				if (r > 0)
				{
					total_received += r;
					if (total_received >= length)
						break;
					time++;
					continue;
				}

				int err = SSL_get_error(ssl, r);
				switch (err)
				{
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					time++;
					continue;
				case SSL_ERROR_ZERO_RETURN:
					if (stats)
						stats->bytes_in += BIO_number_read(SSL_get_rbio(ssl)) - before;
					disconnect();
					return (total_received > 0 ? total_received : -1);
				case SSL_ERROR_SYSCALL:
				case SSL_ERROR_SSL:
				default:
					std::cerr << "TLS: Read error: " << ERR_error_string(ERR_get_error(), nullptr) << "\n";
					if (stats)
						stats->bytes_in += BIO_number_read(SSL_get_rbio(ssl)) - before;
					disconnect();
					return -1;
				}
			}
			result = total_received;
		}

		if (stats)
			stats->bytes_in += BIO_number_read(SSL_get_rbio(ssl)) - before;

		return result;
	}

	void TLS::updateState()
	{
		if (tls_state == TLS_HANDSHAKING)
		{
			performHandshake();
		}
	}

	bool TLS::performHandshake()
	{
		if (!ssl)
			return false;

		int result = SSL_connect(ssl);

		if (result == 1)
		{
			// Handshake completed successfully
			handshake_complete = true;
			tls_state = TLS_CONNECTED;

			Debug() << "TLS: Connected to " << getHost() << ":" << getPort() << " using " << SSL_get_version(ssl) << " with " << SSL_get_cipher(ssl);

			ProtocolBase::onConnect();
			return true;
		}
		int error = SSL_get_error(ssl, result);

		switch (error)
		{
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return false;

		case SSL_ERROR_SYSCALL:
		{
			Error() << "TLS (" << getHost() << ":" << getPort() << "): Handshake failed: " << getSSLErrorString(error);

			// Print detailed SSL error
			unsigned long ssl_err = ERR_get_error();
			if (ssl_err != 0)
			{
				char err_buf[256];
				ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
				Error() << "TLS (" << getHost() << ":" << getPort() << "): SSL Error details: " << err_buf;
			}
			else if (result == 0)
			{
				Error() << "TLS (" << getHost() << ":" << getPort() << "): Connection closed by peer during handshake";
			}
			else if (result == -1)
			{
				Error() << "TLS (" << getHost() << ":" << getPort() << "): System error during handshake: " << strerror(errno);
			}

			disconnect();
			return false;
		}
		case SSL_ERROR_SSL:
		default:
			Error() << "TLS (" << getHost() << ":" << getPort() << "): Handshake failed: " << getSSLErrorString(error);

			// Print detailed SSL error
			unsigned long ssl_err = ERR_get_error();
			if (ssl_err != 0)
			{
				char err_buf[256];
				ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
				Error() << "TLS (" << getHost() << ":" << getPort() << "): SSL Error details: " << err_buf;
			}

			disconnect();
			return false;
		}
	}

	std::string TLS::getSSLErrorString(int error)
	{
		switch (error)
		{
		case SSL_ERROR_NONE:
			return "No error";
		case SSL_ERROR_SSL:
			return "SSL protocol error";
		case SSL_ERROR_WANT_READ:
			return "Want read";
		case SSL_ERROR_WANT_WRITE:
			return "Want write";
		case SSL_ERROR_SYSCALL:
			return "System call error";
		case SSL_ERROR_ZERO_RETURN:
			return "Connection closed";
		default:
			char buffer[256];
			ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
			return std::string(buffer);
		}
	}
#endif // HASOPENSSL

	/// -------MQTT Implementation------
	void MQTT::pushVariableLength(int length)
	{
		if (length >= 128 * 128 * 128 * 128)
		{
			Error() << "MQTT: Length encoding error: " << length;
			disconnect();
			return;
		}

		do
		{
			uint8_t b = length & 127;

			length >>= 7;
			if (length)
				b |= 128;

			packet.push_back(b);

		} while (length);
	}

	void MQTT::pushByte(uint8_t byte)
	{
		packet.push_back(byte);
	}

	void MQTT::pushInt(int length)
	{
		packet.push_back(length >> 8);
		packet.push_back(length & 0xFF);
	}

	void MQTT::createPacket(PacketType type, uint8_t flags)
	{
		packet.resize(0);
		packet.push_back((uint8_t)type | flags);
	}

	void MQTT::pushString(const std::string &str)
	{
		pushInt(str.length());
		packet.insert(packet.end(), str.begin(), str.end());
	}

	void MQTT::subscribePacket()
	{

		int packet_length = 2 + 2 + topic.length() + 1;

		createPacket(PacketType::SUBSCRIBE, 2);
		pushVariableLength(packet_length);

		pushInt(nextPacketId());
		pushString(topic);
		pushByte(qos);
	}

	void MQTT::connectPacket()
	{
		createPacket(PacketType::CONNECT, 0);

		int length = 12 + client_id.length();
		uint8_t flags = 0x02;

		// the will, retained at QoS 0 (MQTT 3.1.1 3.1.2.5-3.1.2.7)
		if (!will_topic.empty())
		{
			length += 2 + will_topic.length() + 2 + will_message.length();
			flags |= 0x04 | 0x20;
		}

		if (!username.empty())
		{
			length += 2 + username.length();
			flags |= 0x80;
		}
		// a password goes with a user name, never alone (MQTT 3.1.1 3.1.2.9)
		if (!password.empty() && !username.empty())
		{
			length += 2 + password.length();
			flags |= 0x40;
		}

		pushVariableLength(length);

		pushString("MQTT");
		pushByte(0x04); // MQTT 3.1.1
		pushByte(flags);

		// off unless a will needs the broker to notice the connection is gone
		pushInt(keepalive);

		pushString(client_id);
		if (!will_topic.empty())
		{
			pushString(will_topic);
			pushString(will_message);
		}

		if (!username.empty())
			pushString(username);
		if (!password.empty() && !username.empty())
			pushString(password);
	}

	bool MQTT::sendConnect()
	{
		Debug() << "MQTT: Starting Handshake with broker " << getHost() << ":" << getPort();

		connected = false;
		if (!prev)
			return false;

		connectPacket();

		if (prev->send(packet.data(), packet.size()) <= 0)
		{
			Error() << "MQTT: Failed to send CONNECT packet";
			disconnect();
			return false;
		}
		return true;
	}

	bool MQTT::connackAccepted(uint8_t code)
	{
		switch (code)
		{
		case 0x00:
			return true;
		case 0x01:
			Error() << "MQTT: Connection Refused - Unacceptable Protocol Version. Server does not support requested MQTT protocol level.";
			break;
		case 0x02:
			Error() << "MQTT: Connection Refused - Identifier Rejected. Client identifier is valid UTF-8 but not allowed by server.";
			break;
		case 0x03:
			Error() << "MQTT: Connection Refused - Server Unavailable. Network connected but MQTT service unavailable.";
			break;
		case 0x04:
			Error() << "MQTT: Connection Refused - Bad Username or Password. Malformed credentials.";
			break;
		case 0x05:
			Error() << "MQTT: Connection Refused - Not Authorized. Client lacks authorization to connect.";
			break;
		default:
			Error() << "MQTT: Connection Refused - Unknown Return Code: 0x" << std::hex << (int)code;
		}
		return false;
	}

	bool MQTT::finishHandshake()
	{
		if (subscribe)
		{
			subscribePacket();

			if (prev->send((char *)packet.data(), packet.size()) <= 0)
			{
				Error() << "MQTT: Failed to send SUBSCRIBE packet";
				disconnect();
				return false;
			}
		}
		Debug() << "MQTT: Connected to broker " << (subscribe ? "and subscribed" : "");
		connected = true;
		return true;
	}

	void MQTT::onConnect()
	{
		buffer_ptr = 0;

		connack_len = 0;
		connack_deadline = Net::Clock::now() + std::chrono::seconds(5);
		awaiting_connack = sendConnect();
	}

	void ZMQ::onConnect()
	{
		// greeting (ZMTP 3.0, NULL security) and READY as SUB
		static const char ready[] = "\x04\x19\x05READY\x0bSocket-Type\0\0\0\x03SUB";
		std::string hello(64, '\0');
		hello[0] = '\xff';
		hello[9] = '\x7f';
		hello[10] = 3;
		hello.replace(12, 4, "NULL");
		hello.append(ready, sizeof(ready) - 1);

		head.clear();
		left = 0;
		greeted = command = subscribed = false;

		if (!prev || prev->send(hello.data(), (int)hello.size()) != (int)hello.size())
		{
			Error() << "ZMQ: cannot send the greeting to " << getHost() << ":" << getPort();
			disconnect();
			return;
		}
		ProtocolBase::onConnect();
	}

	int ZMQ::read(void *data, int length, int t, bool wait)
	{
		const int n = ProtocolBase::read(data, length, t, wait);
		char *p = (char *)data;
		int out = 0;

		for (int i = 0; i < n;)
		{
			if (left)
			{
				const int k = (int)MIN(left, (uint64_t)(n - i));
				if (!command)
				{
					std::memmove(p + out, p + i, k);
					out += k;
				}
				i += k;
				left -= k;

				// libzmq takes a subscription only after its READY
				if (command && !left && !subscribed)
				{
					static const char all[] = {0, 1, 1};
					subscribed = true;
					prev->send(all, sizeof(all));
				}
				continue;
			}

			head += p[i++];
			if (!greeted)
			{
				if (head.size() < 64)
					continue;
				if ((unsigned char)head[0] != 0xff || head[9] != 0x7f || head[10] < 3)
				{
					Error() << "ZMQ (" << getHost() << ":" << getPort() << "): not a ZeroMQ publisher of version 4 or later.";
					disconnect();
					return 0;
				}
				greeted = true;
				head.clear();
				continue;
			}

			// frame header: flags (2 = long size, 4 = command), size in 1 or 8 bytes
			const size_t size_bytes = head[0] & 2 ? 8 : 1;
			if (head.size() < 1 + size_bytes)
				continue;
			command = head[0] & 4;
			for (size_t j = 1; j <= size_bytes; j++)
				left = left << 8 | (unsigned char)head[j];
			head.clear();
		}
		return n < 0 ? n : out;
	}

	void MQTT::poll()
	{
		ProtocolBase::poll();

		// with a keep-alive: a ping when quiet for half of it, and what the
		// broker says back is read and let go
		if (connected && keepalive > 0 && !subscribe && prev && prev->isConnected())
		{
			if (Net::Clock::now() - last_sent >= std::chrono::seconds(keepalive / 2))
			{
				createPacket(PacketType::PINGREQ, 0);
				pushVariableLength(0);
				if (prev->send(packet.data(), packet.size()) > 0)
					last_sent = Net::Clock::now();
			}
			char sink[64];
			while (prev->read(sink, sizeof(sink), 0, false) > 0)
				;
		}

		if (!awaiting_connack || !prev || !prev->isConnected())
			return;

		// a CONNACK is four bytes; only those are read, the rest is left for read()
		int len = prev->read(connack + connack_len, 4 - connack_len, 0, false);
		if (len > 0)
			connack_len += len;

		if (len >= 0 && connack_len < 4 && Net::Clock::now() < connack_deadline)
			return;

		awaiting_connack = false;

		if (connack_len < 4 || (connack[0] & 0xF0) != (uint8_t)(PacketType::CONNACK) || connack[1] != 2)
		{
			Error() << "MQTT: failed to read CONNACK packet";
			disconnect();
			return;
		}

		if (!connackAccepted(connack[3]))
		{
			disconnect();
			return;
		}

		if (finishHandshake())
			ProtocolBase::onConnect();
	}

	// a DISCONNECT before the socket goes (MQTT 3.1.1 3.14)
	void MQTT::disconnect()
	{
		if (connected && prev)
		{
			connected = false;
			createPacket(PacketType::DISCONNECT, 0);
			pushVariableLength(0);
			prev->send(packet.data(), packet.size());
		}

		ProtocolBase::disconnect();
	}

	void MQTT::onDisconnect()
	{
		ProtocolBase::onDisconnect();

		connected = false;
		awaiting_connack = false;
	}

	bool MQTT::setOptionKey(AIS::Keys key, const std::string &value)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_TOPIC:
			topic = value;
			break;
		case AIS::KEY_SETTING_CLIENT_ID:
			client_id = value;
			break;
		case AIS::KEY_SETTING_USERNAME:
			username = value;
			break;
		case AIS::KEY_SETTING_PASSWORD:
			password = value;
			break;
		case AIS::KEY_SETTING_QOS:
			qos = Util::Parse::Integer(value, 0, 2);
			break;
		case AIS::KEY_SETTING_SUBSCRIBE:
			subscribe = Util::Parse::Switch(value);
			break;
		default:
			return false;
		}
		return true;
	}

	bool MQTT::isConnected()
	{
		bool prev_connected = prev ? prev->isConnected() : false;

		return prev_connected && connected;
	}

	int MQTT::send(const void *str, int length, const std::string &tpc)
	{
		return publish(str, length, tpc, false);
	}

	int MQTT::publish(const void *str, int length, const std::string &tpc, bool retain)
	{
		if (!isConnected())
			return 0;

		if (length > 2048)
		{
			Warning() << "MQTT: message too long, skipped";
			return -1;
		}

		last_sent = Net::Clock::now();
		createPacket(PacketType::PUBLISH, (qos << 1) | (retain ? 1 : 0));

		int packet_length = 2 + tpc.length() + length + (qos > 0 ? 2 : 0);

		pushVariableLength(packet_length);
		pushString(tpc);

		if (qos > 0)
			pushInt(nextPacketId());

		packet.insert(packet.end(), (char *)str, (char *)str + length);

		return prev->send(packet.data(), packet.size());
	}

	int MQTT::send(const void *str, int length)
	{
		return send(str, length, topic);
	}

	int MQTT::read(void *data, int data_len, int t, bool wait)
	{
		if (!isConnected())
			return 0;

		if (buffer.empty())
			buffer.resize(16384);

		int len = prev->read((char *)buffer.data() + buffer_ptr, buffer.size() - buffer_ptr, t, wait);

		if (len <= 0)
			return len;

		buffer_ptr += len;

		while (true)
		{
			if (buffer_ptr < 2)
				return 0;

			int length = 0;
			int shift = 0;

			int i = 1;
			do
			{
				if (i >= buffer_ptr)
					return 0;

				if (shift >= 28)
				{
					Error() << "MQTT: size of remaining length packet is at most 4 bytes.";
					disconnect();
					return -1;
				}

				length |= (buffer[i] & 127) << shift;
				shift += 7;

			} while ((buffer[i++] & 128) != 0);

			const int MAX_MQTT_PACKET_SIZE = 1024 * 1024;
			if (length > MAX_MQTT_PACKET_SIZE)
			{
				Error() << "MQTT: Packet size " << length << " exceeds maximum " << MAX_MQTT_PACKET_SIZE;
				disconnect();
				return -1;
			}

			if ((int)buffer.size() < length + i)
				buffer.resize(length + i);

			if (buffer_ptr < length + i)
				return 0;

			// we have a message
			int data_returned = 0;

			switch ((PacketType)(buffer[0] & 0xF0))
			{

			case PacketType::PUBLISH:
			{
				// Validate minimum length for topic length field
				if (length < 2)
				{
					Error() << "MQTT: PUBLISH packet too short";
					disconnect();
					return -1;
				}

				int topic_len = (buffer[i] << 8) + buffer[i + 1];
				int q = (buffer[0] >> 1) & 0x03;

				// Validate topic_len to prevent integer underflow
				int header_size = 2 + topic_len + (q > 0 ? 2 : 0);
				if (header_size > length)
				{
					Error() << "MQTT: Invalid topic length in PUBLISH packet";
					disconnect();
					return -1;
				}

				int payload_len = length - header_size;
				// a message is a line, whether its publisher ended it or not
				const int payload_at = i + 2 + topic_len + (q == 0 ? 0 : 2);
				const bool end_line = payload_len > 0 && buffer[payload_at + payload_len - 1] != '\n';
				if (data_len < payload_len + (end_line ? 1 : 0))
				{
					Warning() << "MQTT: Buffer too small for incoming message. Required: " << payload_len << ", provided: " << data_len;
					break;
				}

				if (q == 0)
				{
					data_returned = payload_len;
					memcpy(data, buffer.data() + i + 2 + topic_len, data_returned);
					if (end_line)
						((char *)data)[data_returned++] = '\n';
				}
				else if (q == 1 || q == 2)
				{
					uint16_t packet_id = (buffer[i + 2 + topic_len] << 8) + buffer[i + 2 + topic_len + 1];
					data_returned = payload_len;
					memcpy(data, buffer.data() + i + 4 + topic_len, data_returned);
					if (end_line)
						((char *)data)[data_returned++] = '\n';

					createPacket((q == 1) ? PacketType::PUBACK : PacketType::PUBREC, 0);
					pushVariableLength(2);
					pushInt(packet_id);
					prev->send(packet.data(), packet.size());
				}
				break;
			}
			case PacketType::PINGREQ:
				createPacket(PacketType::PINGRESP, 0);
				pushVariableLength(0);
				prev->send(packet.data(), packet.size());
				break;
			case PacketType::PUBREC:
				if (length < 2)
				{
					Error() << "MQTT: PUBREC packet too short";
					disconnect();
					return -1;
				}
				createPacket(PacketType::PUBREL, 2);
				pushVariableLength(2);
				pushByte(buffer[i]);
				pushByte(buffer[i + 1]);
				if (prev->send(packet.data(), packet.size()) != (int)packet.size())
					return -1;

				break;
			case PacketType::PUBREL:
				if (length < 2)
				{
					Error() << "MQTT: PUBREL packet too short";
					disconnect();
					return -1;
				}
				createPacket(PacketType::PUBCOMP, 0);
				pushVariableLength(2);
				pushByte(buffer[i]);
				pushByte(buffer[i + 1]);
				if (prev->send(packet.data(), packet.size()) != (int)packet.size())
					return -1;

				break;
			case PacketType::PUBACK:
			case PacketType::DISCONNECT:
			case PacketType::PUBCOMP:
			case PacketType::SUBACK:
				break;
			default:
				Warning() << "MQTT: packet received: " << ((int)buffer[0] >> 4);
				break;
			}

			buffer_ptr -= length + i;
			memmove(buffer.data(), buffer.data() + length + i, buffer_ptr);

			if (data_returned)
				return data_returned;
		}
	}

	std::string MQTT::getValues()
	{
		return "topic " + topic + " client_id " + client_id + " username " + username + " qos " + std::to_string(qos);
	}

	///  ------WebSocket Implementation------
	static void randomMaskingKey(uint8_t key[4])
	{
#ifdef HASOPENSSL
		if (RAND_bytes(key, 4) == 1)
			return;
#endif
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(0, 255);
		for (int i = 0; i < 4; ++i)
			key[i] = (uint8_t)dis(gen);
	}

	// a client's frame, masked: https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
	void WebSocket::buildFrame(OPCODE opcode, const uint8_t *payload, int length)
	{
		uint8_t key[4];
		randomMaskingKey(key);

		frame.resize(0);
		frame.push_back(0x80 | (uint8_t)opcode);
		if (length <= 125)
			frame.push_back(0x80 | length);
		else if (length <= 65535)
		{
			frame.push_back(0x80 | 126);
			frame.push_back((length >> 8) & 0xFF);
			frame.push_back(length & 0xFF);
		}
		else
		{
			frame.push_back(0x80 | 127);
			for (int i = 7; i >= 0; --i)
				frame.push_back(((uint64_t)length >> (8 * i)) & 0xFF);
		}
		frame.insert(frame.end(), key, key + 4);
		for (int i = 0; i < length; ++i)
			frame.push_back(payload[i] ^ key[i % 4]);
	}

	void WebSocket::onConnect()
	{
		buffer_ptr = 0;
		received_ptr = 0;
		close_sent = false;

		// a frame at its largest, with its longest header
		buffer.resize(MAX_PACKET_SIZE + 14);
		received.resize(16384);

		if (sendUpgrade())
		{
			upgrading = true;
			upgrade_deadline = Net::Clock::now() + std::chrono::seconds(5);
			return;
		}

		Error() << "WebSocket: Handshake failed.";
		disconnect();
	}

	void WebSocket::finishUpgrade()
	{
		connected = true;
		Debug() << "WebSocket: Connected to " << getHost() << ":" << getPort() << path;
		ProtocolBase::onConnect();
	}

	void WebSocket::poll()
	{
		ProtocolBase::poll();

		if (!upgrading || !prev || !prev->isConnected())
			return;

		int r = readUpgrade(0);
		if (r == 0 && Net::Clock::now() < upgrade_deadline)
			return;

		upgrading = false;

		if (r > 0)
		{
			finishUpgrade();
			return;
		}

		if (r == 0)
			Error() << "WebSocket: No response to handshake request.";
		Error() << "WebSocket: Handshake failed.";
		disconnect();
	}

	bool WebSocket::sendUpgrade()
	{
		if (!prev)
		{
			Error() << "WebSocket: No underlying connection.";
			return false;
		}

		// Generate a random Sec-WebSocket-Key
		std::string key;
		key.resize(16);
#ifdef HASOPENSSL
		// Use cryptographically secure random from OpenSSL
		if (RAND_bytes((unsigned char *)key.data(), 16) != 1)
		{
			Error() << "WebSocket: Failed to generate secure random key";
			return false;
		}
#else
		// Fallback to std::random_device (platform-dependent quality)
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);
		for (int i = 0; i < 16; i++)
		{
			key[i] = (char)dis(gen);
		}
#endif
		upgrade_key = Util::Convert::toBase64(key);
		upgrade_reply.clear();

		// Build the handshake request
		std::ostringstream request;
		request << "GET " << path << " HTTP/1.1\r\n";
		// the port goes with the host unless it is the scheme's default (RFC 6455 4.1)
		std::string port = getPort();
		request << "Host: " << getHost() << (port == "80" || port == "443" ? "" : ":" + port) << "\r\n";
		request << "Upgrade: websocket\r\n";
		request << "Connection: Upgrade\r\n";
		request << "Sec-WebSocket-Key: " << upgrade_key << "\r\n";
		request << "Sec-WebSocket-Version: 13\r\n";

		if (!origin.empty())
		{
			request << "Origin: " << origin << "\r\n";
		}
		request << Util::Convert::authorizationHeader(username, password, basic);
		if (!protocols.empty())
		{
			request << "Sec-WebSocket-Protocol: " << protocols << "\r\n";
		}
		request << "\r\n";

		std::string request_str = request.str();

		// Send the handshake request
		if (prev->send(request_str.c_str(), request_str.size()) != (int)request_str.size())
		{
			Error() << "WebSocket: Failed to send handshake request.";
			return false;
		}

		return true;
	}

	// 1 once the reply is in and accepted, 0 while more is to come, -1 on failure; a read
	// that waits would hold out for a full chunk, which a short reply never fills
	int WebSocket::readUpgrade(int t)
	{
		const size_t MAX_HEADER_SIZE = 16384;
		const std::string HEADER_END = "\r\n\r\n";

		char chunk[2048];
		int len = prev->read(chunk, sizeof(chunk), t, false);
		if (len < 0)
		{
			Error() << "WebSocket: connection lost before the upgrade reply.";
			return -1;
		}
		if (len == 0)
			return 0;

		// Track scan start so we don't re-scan the entire reply on each read,
		// while still catching the marker if it straddles two reads.
		size_t scan_from = upgrade_reply.size() >= 3 ? upgrade_reply.size() - 3 : 0;
		upgrade_reply.append(chunk, len);

		size_t header_end = upgrade_reply.find(HEADER_END, scan_from);
		if (header_end != std::string::npos)
			return checkUpgrade(header_end + HEADER_END.size()) ? 1 : -1;

		if (upgrade_reply.size() >= MAX_HEADER_SIZE)
		{
			Error() << "WebSocket: Handshake response headers exceeded " << MAX_HEADER_SIZE << " bytes.";
			return -1;
		}
		return 0;
	}

	bool WebSocket::checkUpgrade(size_t body_start)
	{
		std::string &response_str = upgrade_reply;

		// Preserve any frame bytes that arrived after the headers for the frame parser.
		if (body_start < response_str.size())
		{
			size_t extra = response_str.size() - body_start;
			if (extra > buffer.size())
				buffer.resize(extra);
			memcpy(buffer.data(), response_str.data() + body_start, extra);
			buffer_ptr = (int)extra;
			response_str.resize(body_start);
		}

		std::string rs_lower(response_str);

		// Parse the response
		if (response_str.find("HTTP/1.1 101") == std::string::npos &&
			response_str.find("HTTP/1.0 101") == std::string::npos)
		{
			Error() << "WebSocket (" << getHost() << ":" << getPort() << "): upgrade refused: " << response_str.substr(0, response_str.find("\r\n"));
			return false;
		}
		Util::Convert::toLower(rs_lower);

		if (rs_lower.find("upgrade: websocket") == std::string::npos ||
			rs_lower.find("connection: upgrade") == std::string::npos)
		{
			Error() << "WebSocket: Missing required headers in handshake response.";
			return false;
		}

		// Validate Sec-WebSocket-Accept
		std::string accept_key;
		std::istringstream resp_stream(response_str);
		std::string line;

		while (std::getline(resp_stream, line))
		{
			std::string lower(line);
			Util::Convert::toLower(lower);

			if (lower.find("sec-websocket-accept:") != std::string::npos)
			{
				const std::size_t value = line.find(":") + 2;
				accept_key = value < line.size() ? line.substr(value) : std::string();
				// Remove trailing whitespace and CR/LF
				accept_key.resize(accept_key.find_last_not_of(" \r\n") + 1);
				break;
			}
		}

		if (accept_key.empty())
		{
			Error() << "WebSocket: Missing Sec-WebSocket-Accept header.";
			return false;
		}

		// Compute the expected accept key
		std::string expected_accept_key = Util::Convert::toBase64(sha1Hash(upgrade_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

		accept_key.erase(std::remove_if(accept_key.begin(), accept_key.end(), ::isspace), accept_key.end());
		expected_accept_key.erase(std::remove_if(expected_accept_key.begin(), expected_accept_key.end(), ::isspace), expected_accept_key.end());

		if (accept_key != expected_accept_key)
		{
			Error() << "WebSocket: Invalid Sec-WebSocket-Accept value: '" << accept_key << "' expected: '" << expected_accept_key << "'";
			return false;
		}

		// the server may take one of the protocols offered, and nothing else (RFC 6455 4.1)
		std::string chosen;
		std::istringstream proto_stream(response_str);
		while (std::getline(proto_stream, line))
		{
			std::string lower(line);
			Util::Convert::toLower(lower);

			if (lower.find("sec-websocket-protocol:") == 0)
			{
				chosen = line.substr(line.find(":") + 1);
				chosen.erase(std::remove_if(chosen.begin(), chosen.end(), ::isspace), chosen.end());
			}
		}

		std::string offered = protocols;
		offered.erase(std::remove_if(offered.begin(), offered.end(), ::isspace), offered.end());

		if (!chosen.empty() && ("," + offered + ",").find("," + chosen + ",") == std::string::npos)
		{
			Error() << "WebSocket: server chose protocol '" << chosen << "', which was not offered.";
			return false;
		}

		return true;
	}

	// https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
	int WebSocket::send(const void *data, int length)
	{
		if (!isConnected())
			return 0;

		if (length > MAX_PACKET_SIZE)
		{
			Warning() << "WebSocket: message too long, skipped";
			return 0;
		}

		buildFrame(binary ? OPCODE::BINARY : OPCODE::TEXT, (const uint8_t *)data, length);

		int sent = prev->send(frame.data(), frame.size());
		if (sent == (int)frame.size())
			return length;

		if (sent > 0)
		{
			Error() << "WebSocket: incomplete frame send, disconnecting.";
			disconnect();
			return -1;
		}

		return sent;
	}

	int WebSocket::populateData(void *data, int length)
	{
		int l = MIN(received_ptr, length);

		if (l > 0)
		{
			memcpy(data, received.data(), l);
			memmove(received.data(), received.data() + l, received_ptr - l);
			received_ptr -= l;
		}
		return l;
	}

	int WebSocket::read(void *data, int data_len, int t, bool wait)
	{
		if (data_len > 0 && received_ptr >= data_len)
		{
			return populateData(data, data_len);
		}

		// a waiting read underneath would hold out to fill the whole frame buffer
		const Net::Clock::time_point deadline = Net::Clock::now() + std::chrono::seconds(MAX(t, 0));
		do
		{
			if (getFrames(data, data_len, wait ? MIN(t, 1) : t, false) < 0)
				break;
		} while (wait && received_ptr < data_len && Net::Clock::now() < deadline && isConnected());

		return populateData(data, data_len);
	}

	int WebSocket::getFrames(void *data, int data_len, int t, bool wait)
	{
		if (!isConnected())
			return 0;

		int len = prev->read(buffer.data() + buffer_ptr, buffer.size() - buffer_ptr, t, wait);
		if (len <= 0)
			return len;

		buffer_ptr += len;

		while (true)
		{
			if (buffer_ptr < 2)
				return 0;

			bool mask = buffer[1] & 0x80;
			int64_t length = buffer[1] & 0x7F;
			int ptr = 2;

			if (length == 126)
			{
				if (buffer_ptr < 4)
					return 0;

				length = (buffer[ptr] << 8) | buffer[ptr + 1];
				ptr += 2;
			}
			else if (length == 127)
			{
				if (buffer_ptr < ptr + 8)
					return 0;

				length = 0;
				for (int i = 0; i < 8; ++i)
					length = (length << 8) | buffer[ptr + i];

				ptr += 8;
			}

			if (length < 0 || length > MAX_PACKET_SIZE)
			{
				Warning() << "WebSocket (" << getHost() << ":" << getPort() << "): frame exceeds " << MAX_PACKET_SIZE << " bytes, disconnecting";
				disconnect();
				return -1;
			}

			uint8_t masking_key[4] = {0};

			// Validate we have enough data for mask (if present) and payload
			int required_len = ptr + (mask ? 4 : 0) + length;
			if (required_len < 0 || buffer_ptr < required_len)
				return 0;

			if (mask)
			{
				masking_key[0] = buffer[ptr++];
				masking_key[1] = buffer[ptr++];
				masking_key[2] = buffer[ptr++];
				masking_key[3] = buffer[ptr++];
			}

			switch ((OPCODE)(buffer[0] & 0x0F))
			{
			// the fragments of a message go up as they come (RFC 6455 5.4)
			case OPCODE::CONTINUATION:
			case OPCODE::TEXT:
			case OPCODE::BINARY:

				for (int i = 0; i < length; ++i)
				{
					if (mask)
						buffer[ptr + i] ^= masking_key[i % 4];
				}

				if (received_ptr + length <= received.size() && length <= MAX_PACKET_SIZE)
				{
					memmove(received.data() + received_ptr, buffer.data() + ptr, length);
					received_ptr += length;
				}
				else
				{
					Warning() << "WebSocket: package too large, dropped";
				}
				break;
			case OPCODE::CLOSE:
			{
				Warning() << "WebSocket: Connection closed by remote host.";

				// echo the close code back before tearing down (RFC 6455 5.5.1)
				int echo_len = MIN((int)length, 2);
				if (mask)
					for (int i = 0; i < echo_len; ++i)
						buffer[ptr + i] ^= masking_key[i % 4];

				buildFrame(OPCODE::CLOSE, buffer.data() + ptr, echo_len);
				prev->send(frame.data(), frame.size());

				close_sent = true;
				disconnect();
				break;
			}
			case OPCODE::PING:
			{
				// Control frames (like PING/PONG) must have payload <= 125 bytes per RFC 6455
				if (length > 125)
				{
					Warning() << "WebSocket: PING payload too large, ignoring";
					break;
				}

				// Unmask the incoming PING payload before echoing it back
				if (mask)
				{
					for (int i = 0; i < length; ++i)
						buffer[ptr + i] ^= masking_key[i % 4];
				}

				buildFrame(OPCODE::PONG, buffer.data() + ptr, (int)length);

				if (prev->send(frame.data(), frame.size()) != (int)frame.size())
				{
					Error() << "WebSocket: Failed to send pong.";
					return -1;
				}
				break;
			}
			case OPCODE::PONG:
				break;
			default:
				Warning() << "WebSocket: Unsupported opcode: " << (int)(buffer[0] & 0x0F);
				break;
			}

			memmove(buffer.data(), buffer.data() + ptr + length, buffer_ptr - ptr - length);
			buffer_ptr -= ptr + length;
		}
	}

	bool WebSocket::isConnected()
	{
		bool prev_connected = prev && prev->isConnected();

		return prev_connected && connected;
	}

	// a Close before the socket goes (RFC 6455 7.1.2)
	void WebSocket::disconnect()
	{
		if (connected && !close_sent && prev)
		{
			close_sent = true;

			const uint8_t normal[2] = {0x03, 0xE8};
			buildFrame(OPCODE::CLOSE, normal, 2);
			prev->send(frame.data(), (int)frame.size());
		}

		ProtocolBase::disconnect();
	}

	void WebSocket::onDisconnect()
	{
		connected = false;
		upgrading = false;

		ProtocolBase::onDisconnect();
	}

	bool WebSocket::setOptionKey(AIS::Keys key, const std::string &value)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_ORIGIN:
			origin = value;
			break;
		case AIS::KEY_SETTING_PROTOCOLS:
			protocols = value;
			break;
		case AIS::KEY_SETTING_BINARY:
			binary = Util::Parse::Switch(value);
			break;
		case AIS::KEY_SETTING_USERNAME:
			username = value;
			break;
		case AIS::KEY_SETTING_PASSWORD:
			password = value;
			basic = true;
			break;
		default:
			return false;
		}
		return true;
	}

	std::string WebSocket::getValues()
	{
		return "host " + getHost() + " path " + path + " origin " + origin + " protocols " + protocols;
	}

	std::string WebSocket::sha1Hash(const std::string &d)
	{
		unsigned char hash[20];

#ifdef HASOPENSSL
		SHA1((const unsigned char *)d.data(), d.length(), hash);
		return std::string(reinterpret_cast<char *>(hash), 20);
#else
		uint32_t h0 = 0x67452301;
		uint32_t h1 = 0xEFCDAB89;
		uint32_t h2 = 0x98BADCFE;
		uint32_t h3 = 0x10325476;
		uint32_t h4 = 0xC3D2E1F0;

		// Pre-processing
		uint64_t original_bit_len = d.length() * 8;
		std::vector<uint8_t> msg(d.data(), d.data() + d.length());

		// Append the bit '1'
		msg.push_back(0x80);

		// Append zeros
		while ((msg.size() % 64) != 56)
		{
			msg.push_back(0x00);
		}

		// Append length
		for (int i = 7; i >= 0; --i)
		{
			msg.push_back((original_bit_len >> (i * 8)) & 0xFF);
		}

		// Process message in 512-bit chunks
		auto leftrotate = [](uint32_t x, uint32_t c) -> uint32_t
		{
			return (x << c) | (x >> (32 - c));
		};

		for (size_t chunk = 0; chunk < msg.size(); chunk += 64)
		{
			uint32_t w[80];

			// Break chunk into sixteen 32-bit words
			for (int i = 0; i < 16; i++)
			{
				w[i] = (msg[chunk + 4 * i] << 24) |
					   (msg[chunk + 4 * i + 1] << 16) |
					   (msg[chunk + 4 * i + 2] << 8) |
					   (msg[chunk + 4 * i + 3]);
			}

			// Extend the sixteen 32-bit words into eighty 32-bit words
			for (int i = 16; i < 80; i++)
			{
				w[i] = leftrotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
			}

			// Initialize hash values for this chunk
			uint32_t a = h0;
			uint32_t b = h1;
			uint32_t c = h2;
			uint32_t d = h3;
			uint32_t e = h4;

			// Main loop
			for (int i = 0; i < 80; i++)
			{
				uint32_t f, k;

				if (i < 20)
				{
					f = (b & c) | ((~b) & d);
					k = 0x5A827999;
				}
				else if (i < 40)
				{
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				}
				else if (i < 60)
				{
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				}
				else
				{
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}

				uint32_t temp = leftrotate(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = leftrotate(b, 30);
				b = a;
				a = temp;
			}

			// Add this chunk's hash to result
			h0 += a;
			h1 += b;
			h2 += c;
			h3 += d;
			h4 += e;
		}

		// Store hash as big-endian bytes
		hash[0] = (h0 >> 24) & 0xFF;
		hash[1] = (h0 >> 16) & 0xFF;
		hash[2] = (h0 >> 8) & 0xFF;
		hash[3] = h0 & 0xFF;

		hash[4] = (h1 >> 24) & 0xFF;
		hash[5] = (h1 >> 16) & 0xFF;
		hash[6] = (h1 >> 8) & 0xFF;
		hash[7] = h1 & 0xFF;

		hash[8] = (h2 >> 24) & 0xFF;
		hash[9] = (h2 >> 16) & 0xFF;
		hash[10] = (h2 >> 8) & 0xFF;
		hash[11] = h2 & 0xFF;

		hash[12] = (h3 >> 24) & 0xFF;
		hash[13] = (h3 >> 16) & 0xFF;
		hash[14] = (h3 >> 8) & 0xFF;
		hash[15] = h3 & 0xFF;

		hash[16] = (h4 >> 24) & 0xFF;
		hash[17] = (h4 >> 16) & 0xFF;
		hash[18] = (h4 >> 8) & 0xFF;
		hash[19] = h4 & 0xFF;

		// Return raw bytes as string
		return std::string(reinterpret_cast<char *>(hash), 20);
#endif
	}

#if defined(__linux__) && !defined(__ANDROID__)
	bool SocketCAN::open()
	{
		struct ifreq ifr;
		struct sockaddr_can addr;
		const char *failed = nullptr;

		disconnect();
		std::memset(&ifr, 0, sizeof(ifr));
		std::memset(&addr, 0, sizeof(addr));
		std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

		if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
			failed = "cannot open a CAN socket";
		else if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
			failed = "no such interface";
		else
		{
			addr.can_family = AF_CAN;
			addr.can_ifindex = ifr.ifr_ifindex;
			if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
				failed = "cannot bind to the interface";
			else if (!Net::setNonBlocking(sock))
				failed = "cannot set the socket non-blocking";
		}

		if (!failed)
		{
			if (failures)
				Info() << "CAN (" << interface << "): open.";
			failures = 0;
			return true;
		}
		if (!failures++)
			Error() << "CAN (" << interface << "): " << failed << ", will keep trying.";
		disconnect();
		return false;
	}

	void SocketCAN::disconnect()
	{
		if (sock != -1)
			Net::closeSocket(sock);
		sock = -1;
		stamp = Net::Clock::now();
	}

	void SocketCAN::poll()
	{
		if (sock == -1 && Net::secondsSince(stamp) > backoff(failures ? failures - 1 : 0))
			open();
	}

	// the frames that have come, each as a line, after at most `t` seconds of waiting for one
	int SocketCAN::read(void *data, int length, int t, bool)
	{
		if (sock == -1)
			return 0;

		struct pollfd pfd = {sock, POLLIN, 0};
		if (::poll(&pfd, 1, t * 1000) <= 0)
			return 0;

		char *out = (char *)data;
		int n = 0;
		// a line is at most 8 digits, 8 bytes of 3 characters and the line end
		while (length - n >= 40)
		{
			struct can_frame frame;
			struct iovec iov = {&frame, sizeof(frame)};
			struct msghdr msg;
			std::memset(&msg, 0, sizeof(msg));
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;

			const ssize_t r = recvmsg(sock, &msg, 0);
			if (r < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
					break;
				disconnect();
				return -1;
			}
			// NMEA 2000 frames have the long identifier; MSG_DONTROUTE marks one sent from here
			if (r != (ssize_t)sizeof(frame) || !(frame.can_id & CAN_EFF_FLAG) || (frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) ||
				(msg.msg_flags & MSG_DONTROUTE) || frame.can_dlc < 1 || frame.can_dlc > 8)
				continue;

			n += std::snprintf(out + n, 10, "%08X", (unsigned)(frame.can_id & CAN_EFF_MASK));
			for (int i = 0; i < frame.can_dlc; i++)
				n += std::snprintf(out + n, 4, " %02X", frame.data[i]);
			out[n++] = '\r';
			out[n++] = '\n';
		}
		return n;
	}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
	// frame lines, each written to the interface; false as a count of none when it takes no more
	int SocketCAN::send(const void *data, int length)
	{
		if (sock == -1)
			return 0;

		const std::string text((const char *)data, length);
		for (size_t at = 0, end; (end = text.find('\n', at)) != std::string::npos; at = end + 1)
		{
			std::string line = text.substr(at, end - at);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			uint32_t id;
			struct can_frame frame;
			int len;
			std::memset(&frame, 0, sizeof(frame));
			if (!AIS::parseFrameLine(line.c_str(), id, frame.data, len))
				continue;
			frame.can_id = id | CAN_EFF_FLAG;
			frame.can_dlc = len;
			if (::write(sock, &frame, sizeof(frame)) != (ssize_t)sizeof(frame))
			{
				if (errno != ENOBUFS && errno != EAGAIN && errno != EWOULDBLOCK)
					disconnect();
				return 0;
			}
		}
		return length;
	}
#endif

	// A message of the node's own as frame lines: one frame, or a fast packet of several
	void N2KNode::message(uint32_t pgn, int priority, int destination, const std::string &data, bool fast)
	{
		const uint32_t id = (uint32_t)priority << 26 | pgn << 8 | (((pgn >> 8) & 0xff) < 240 ? (uint32_t)destination << 8 : 0) | (uint32_t)address;
		const int order = (sequence++ & 7) << 5;
		const int len = (int)data.size();
		std::string lines;
		char buf[32];

		for (int n = 0, at = 0; at < len; n++)
		{
			std::snprintf(buf, sizeof(buf), "%08X", (unsigned)id);
			lines += buf;
			int room = 8;
			if (fast)
			{
				std::snprintf(buf, sizeof(buf), " %02X", order | n);
				lines += buf;
				room--;
				if (n == 0)
				{
					std::snprintf(buf, sizeof(buf), " %02X", len);
					lines += buf;
					room--;
				}
			}
			for (; room > 0; room--, at++)
			{
				std::snprintf(buf, sizeof(buf), " %02X", at < len ? (unsigned char)data[at] : 0xff);
				lines += buf;
			}
			lines += "\r\n";
		}
		ProtocolBase::send(lines.data(), (int)lines.size());
	}

	// the address claim: the name, sent to all from the address wanted
	void N2KNode::claim()
	{
		std::string n(8, 0);
		for (int i = 0; i < 8; i++)
			n[i] = (char)(name >> (8 * i));
		message(60928, 6, 255, n, false);
	}

	// a frame from the bus: another's claim of our address, or a request
	void N2KNode::frame(uint32_t id, const unsigned char *data, int len)
	{
		const uint32_t pf = (id >> 16) & 0xff, ps = (id >> 8) & 0xff;
		const uint32_t pgn = ((id >> 24) & 1) << 16 | pf << 8 | (pf < 240 ? 0 : ps);
		const int from = id & 0xff, to = pf < 240 ? (int)ps : 255;

		if (pgn == 60928 && from == address && len == 8 && state != SILENT)
		{
			uint64_t other = 0;
			for (int i = 0; i < 8; i++)
				other |= (uint64_t)data[i] << (8 * i);

			// the lower name keeps the address; with the same name ours becomes another instance
			if (name > other)
				address = address < 251 ? address + 1 : 254;
			else if (name == other)
				name += 1ULL << 32;
			if (address == 254)
			{
				state = SILENT;
				Error() << "NMEA2000 (" << getHost() << "): no address left on the bus, nothing is sent.";
				claim();
				return;
			}
			if (name >= other)
			{
				state = CLAIMING;
				stamp = Net::Clock::now();
			}
			claim();
		}
		else if (pgn == 59904 && (to == address || to == 255) && len >= 3)
			request((uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16, from, to != 255);
	}

	void N2KNode::request(uint32_t pgn, int from, bool addressed)
	{
		// the messages this device sends, and the ones it takes
		static const uint32_t sent[] = {59392, 60928, 126464, 126993, 126996, 126998, 129038, 129039, 129040, 129041, 129793, 129794, 129798, 129802, 129809, 129810};
		static const uint32_t taken[] = {59904, 60928};

		if (state != UP)
			return;

		auto text = [](std::string &d, const char *s, size_t len)
		{
			const size_t n = std::strlen(s);
			d.append(s, MIN(n, len));
			d.append(len - MIN(n, len), (char)0xff);
		};
		auto list = [](unsigned char kind, const uint32_t *p, size_t n)
		{
			std::string d(1, (char)kind);
			for (size_t i = 0; i < n; i++)
				for (int k = 0; k < 3; k++)
					d += (char)(p[i] >> (8 * k));
			return d;
		};

		switch (pgn)
		{
		case 60928:
			claim();
			break;
		case 126996:
		{
			// product information: the standard's version 2.101, product 100, then four texts of 32
			std::string d = {(char)0x35, (char)0x08, (char)100, (char)0};
			text(d, "AIS-catcher", 32);
			text(d, VERSION, 32);
			text(d, VERSION, 32);
			text(d, "00000002", 32);
			d += (char)0; // not certified
			d += (char)1; // one load unit of 50 mA
			message(126996, 6, 255, d, true);
			break;
		}
		case 126998:
		{
			// configuration information: two installation notes, empty, and the maker's text
			std::string d = {2, 1, 2, 1};
			const std::string maker = "AIS-catcher";
			d += (char)(maker.size() + 2);
			d += (char)1;
			d += maker;
			message(126998, 6, 255, d, true);
			break;
		}
		case 126464:
			message(126464, 6, from, list(0, sent, sizeof(sent) / sizeof(sent[0])), true);
			message(126464, 6, from, list(1, taken, sizeof(taken) / sizeof(taken[0])), true);
			break;
		default:
			// a request to this device alone for what it does not have: declined
			if (addressed)
			{
				std::string d = {1, (char)0xff, (char)0xff, (char)0xff, (char)0xff};
				for (int k = 0; k < 3; k++)
					d += (char)(pgn >> (8 * k));
				message(59392, 6, from, d, false);
			}
			break;
		}
	}

	void N2KNode::poll()
	{
		ProtocolBase::poll();
		if (!ProtocolBase::isConnected())
		{
			state = DOWN;
			return;
		}
		if (state == DOWN)
		{
			state = CLAIMING;
			stamp = Net::Clock::now();
			in.clear();
			claim();
		}

		char buf[2048];
		int n;
		while ((n = ProtocolBase::read(buf, sizeof(buf), 0, false)) > 0)
			for (int i = 0; i < n; i++)
			{
				if (buf[i] == '\n')
				{
					uint32_t id;
					unsigned char data[8];
					int len;
					if (AIS::parseFrameLine(in.c_str(), id, data, len))
						frame(id, data, len);
					in.clear();
				}
				else if (buf[i] != '\r' && in.size() < 64)
					in += buf[i];
			}

		// the address is ours when nobody objected for a quarter of a second
		if (state == CLAIMING && Net::secondsSince(stamp) > 0.25)
		{
			state = UP;
			beat = Net::Clock::now() - std::chrono::seconds(60);
			Info() << "NMEA2000 (" << getHost() << "): on the bus with address " << address << ".";
		}
		if (state == UP && Net::secondsSince(beat) >= 60)
		{
			beat = Net::Clock::now();
			// heartbeat: its interval in ms, a count, the rest unused
			std::string d = {(char)0x60, (char)0xea, (char)(beats++ % 253), (char)0xff, (char)0xff, (char)0xff, (char)0xff, (char)0xff};
			message(126993, 7, 255, d, false);
		}
	}

	// frame lines of the sender: each goes out under our address
	int N2KNode::send(const void *data, int length)
	{
		if (state != UP)
			return 0;

		static const char hex[] = "0123456789ABCDEF";
		out.assign((const char *)data, length);
		for (size_t at = 0; at + 8 <= out.size(); at = out.find('\n', at) + 1)
		{
			out[at + 6] = hex[address >> 4];
			out[at + 7] = hex[address & 15];
			if (out.find('\n', at) == std::string::npos)
				break;
		}
		return ProtocolBase::send(out.data(), (int)out.size()) > 0 ? length : 0;
	}
}
