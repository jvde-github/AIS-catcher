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

#include <functional>

#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>

#include "HTTPServer.h"
#include "ControlCore.h"

class ControlServer : public IO::HTTPServer
{
public:
	ControlServer(ControlCore &core) : core(core)
	{
		setFrameSrc("'self' http: https:");
		setFrameAncestors("'self'");
	}
	~ControlServer() { close(); }

	void start();
	void close();

	void Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip) override;
	// the /viewer mount: whoever is logged in here is its owner
	bool ownerOfMount(const IO::HTTPRequest &r) override { return sameOrigin(r) && checkSession(r.cookie); }

	// The site's answers, set before start(): what the stations are doing now,
	// for the Stations tab; where the API tester reaches the site's server on
	// a port, false when there is none; the ports the servers listen on
	void setStationsLive(std::function<std::string()> f) { stations_live = std::move(f); }
	void setServerAddress(std::function<bool(int, std::string &)> f) { server_address = std::move(f); }
	void setServerPorts(std::function<std::vector<int>()> f) { server_ports = std::move(f); }
	void setServerTLS(std::function<std::vector<std::pair<int, long long>>()> f) { server_tls = std::move(f); }

protected:
	void processClients() override;

private:
	ControlCore &core;
	std::function<std::string()> stations_live;
	std::function<bool(int, std::string &)> server_address;
	std::function<std::vector<int>()> server_ports;
	std::function<std::vector<std::pair<int, long long>>()> server_tls;

	// The endpoints that answer without a session, method null meaning any.
	// Everything else is private: a mistake here locks an endpoint down
	// rather than exposing it.
	struct PublicEndpoint
	{
		const char *path;
		const char *method;
	};

	static const PublicEndpoint public_endpoints[];
	static bool isPublic(const std::string &path, const std::string &method);

	// serve the bundled control frontend, or 404
	void serveStatic(IO::TCPServerConnection &c, const std::string &path);
	std::string cookieAttributes() const;


	static const int SESSION_LIFETIME = 7 * 24 * 3600;
	static const int MAX_LOGIN_BLOCK = 300;

	std::mutex auth_mtx;
	std::unordered_map<std::string, std::time_t> sessions;

	// wrong passwords and setup codes by address, so one address cannot lock out
	// another; the entry that frees first goes when a new address needs room
	struct LoginTries
	{
		int failures = 0;
		std::time_t block_until = 0;
	};
	static const std::size_t MAX_LOGIN_PEERS = 16;
	std::unordered_map<std::string, LoginTries> login_tries;

	int log_listener = -1;

	uint32_t activity_sent[4] = {0, 0, 0, 0};
	std::time_t activity_time = 0;

	static const uint32_t STATUS_UNSENT = 0xFFFFFFFF;
	uint32_t status_state = STATUS_UNSENT;

	std::string createSession();
	bool checkSession(const std::string &cookie);
	void destroySession(const std::string &cookie);
	void destroyAllSessions();
	static std::string sessionFromCookie(const std::string &cookie);

	static bool sameOrigin(const IO::HTTPRequest &r);

	bool loginBlocked(const std::string &peer, int &seconds_left);
	void loginFailed(const std::string &peer);
	void loginSucceeded(const std::string &peer);

	std::string statusJSON(const char *auth, bool authenticated);
	void sendStatus(IO::TCPServerConnection &c, bool authenticated);
	void sendOK(IO::TCPServerConnection &c);
	void sendError(IO::TCPServerConnection &c, const std::string &message, int status = 400);
};
