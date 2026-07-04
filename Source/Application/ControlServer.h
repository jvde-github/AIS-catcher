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
#include <unordered_map>
#include <mutex>
#include <ctime>

#include "HTTPServer.h"
#include "ControlCore.h"

// HTTP/JSON binding of ControlCore for managed mode (-E). Lives outside the
// per-run state so it survives engine stops and restarts. All endpoints
// except status, first-run setup and login require a session cookie.
class ControlServer : public IO::HTTPServer
{
public:
	ControlServer(ControlCore &core) : core(core) { setSSETopic(1, "activity"); }
	~ControlServer() { close(); }

	void start();
	void close();

	void Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip) override;

protected:
	// piggybacks the server loop to emit the channel-activity SSE tick
	void processClients() override;

private:
	ControlCore &core;

	static const int SESSION_LIFETIME = 7 * 24 * 3600;
	static const int MAX_LOGIN_BLOCK = 300;

	std::mutex auth_mtx;
	std::unordered_map<std::string, std::time_t> sessions;
	int login_failures = 0;
	std::time_t login_block_until = 0;

	int log_listener = -1;

	uint32_t activity_sent[4] = {0, 0, 0, 0};
	std::time_t activity_time = 0;

	std::string createSession();
	bool checkSession(const std::string &cookie);
	void destroySession(const std::string &cookie);
	void destroyAllSessions();
	static std::string sessionFromCookie(const std::string &cookie);

	bool loginBlocked(int &seconds_left);
	void loginFailed();
	void loginSucceeded();

	void sendStatus(IO::TCPServerConnection &c, bool authenticated);
	void sendOK(IO::TCPServerConnection &c);
	void sendError(IO::TCPServerConnection &c, const std::string &message, int status = 400);
};
