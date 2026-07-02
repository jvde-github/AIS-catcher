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

#include <random>

#include "ControlServer.h"
#include "Logger.h"
#include "Writer.h"
#include "Convert.h"

static std::string randomToken()
{
	static const char *digits = "0123456789abcdef";

	std::random_device rd;
	std::string token;
	while (token.size() < 48)
	{
		uint32_t r = rd();
		for (int j = 28; j >= 0; j -= 4)
			token += digits[(r >> j) & 0xF];
	}
	return token;
}

void ControlServer::start()
{
	if (!HTTPServer::start(core.getControlPort()))
		throw std::runtime_error("Control: cannot start control server at port " + std::to_string(core.getControlPort()));

	log_listener = Logger::getInstance().addLogListener([this](const LogMessage &m)
														{ sendSSE(3, "log", m.toJSON()); });

	Info() << "Control: control server running at port " << core.getControlPort();
}

void ControlServer::close()
{
	if (log_listener >= 0)
	{
		Logger::getInstance().removeLogListener(log_listener);
		log_listener = -1;
	}
	stopThread();
}

std::string ControlServer::createSession()
{
	std::string token = randomToken();

	std::lock_guard<std::mutex> lock(auth_mtx);
	sessions[token] = std::time(nullptr) + SESSION_LIFETIME;
	return token;
}

std::string ControlServer::sessionFromCookie(const std::string &cookie)
{
	const std::string name = "aiscontrol=";

	std::string::size_type pos = 0;
	while ((pos = cookie.find(name, pos)) != std::string::npos)
	{
		if (pos == 0 || cookie[pos - 1] == ' ' || cookie[pos - 1] == ';')
		{
			std::string::size_type start = pos + name.length();
			std::string::size_type end = cookie.find(';', start);
			return cookie.substr(start, end == std::string::npos ? std::string::npos : end - start);
		}
		pos += name.length();
	}
	return "";
}

bool ControlServer::checkSession(const std::string &cookie)
{
	std::string token = sessionFromCookie(cookie);
	if (token.empty())
		return false;

	std::lock_guard<std::mutex> lock(auth_mtx);

	auto it = sessions.find(token);
	if (it == sessions.end())
		return false;

	std::time_t now = std::time(nullptr);
	if (it->second < now)
	{
		sessions.erase(it);
		return false;
	}

	it->second = now + SESSION_LIFETIME;
	return true;
}

void ControlServer::destroySession(const std::string &cookie)
{
	std::lock_guard<std::mutex> lock(auth_mtx);
	sessions.erase(sessionFromCookie(cookie));
}

void ControlServer::destroyAllSessions()
{
	std::lock_guard<std::mutex> lock(auth_mtx);
	sessions.clear();
}

bool ControlServer::loginBlocked(int &seconds_left)
{
	std::lock_guard<std::mutex> lock(auth_mtx);

	std::time_t now = std::time(nullptr);
	if (login_block_until > now)
	{
		seconds_left = (int)(login_block_until - now);
		return true;
	}
	return false;
}

void ControlServer::loginFailed()
{
	std::lock_guard<std::mutex> lock(auth_mtx);

	login_failures++;
	int block = 1 << MIN(login_failures, 8);
	login_block_until = std::time(nullptr) + MIN(block, MAX_LOGIN_BLOCK);
}

void ControlServer::loginSucceeded()
{
	std::lock_guard<std::mutex> lock(auth_mtx);
	login_failures = 0;
	login_block_until = 0;
}

void ControlServer::sendStatus(IO::TCPServerConnection &c, bool authenticated)
{
	std::string s;
	JSON::Writer w(s);
	w.beginObject();

	if (!authenticated)
	{
		w.kv("auth", core.hasPassword() ? "login" : "setup");
	}
	else
	{
		bool running = core.getEngineState() == ControlCore::EngineState::Running;
		w.kv("auth", "ok")
			.kv("engine", running ? "running" : "stopped")
			.kv("uptime", core.getUptime());
	}

	w.endObject().finish();
	Response(c, "application/json", s);
}

void ControlServer::sendOK(IO::TCPServerConnection &c)
{
	Response(c, "application/json", std::string("{\"status\":true}"));
}

void ControlServer::sendError(IO::TCPServerConnection &c, const std::string &message, int status)
{
	std::string s;
	JSON::Writer w(s);
	w.beginObject().kv("status", false).kv("error", message).endObject().finish();
	Response(c, "application/json", s, false, false, false, status);
}

void ControlServer::Request(IO::TCPServerConnection &c, const IO::HTTPRequest &r, bool accept_gzip)
{
	const std::string path = r.path();
	const bool authenticated = checkSession(r.cookie);

	const std::string cookie_attr = "; Path=/; Max-Age=" + std::to_string(SESSION_LIFETIME) + "; HttpOnly; SameSite=Strict";

	if (path == "/api/status")
	{
		sendStatus(c, authenticated);
	}
	else if (path == "/api/setup" && r.method == "POST")
	{
		if (core.hasPassword())
			sendError(c, "password already set", 403);
		else if (r.body.length() < 6)
			sendError(c, "password needs at least 6 characters");
		else
		{
			core.setPassword(r.body);
			setExtraHeader("Set-Cookie: aiscontrol=" + createSession() + cookie_attr);
			sendOK(c);
		}
	}
	else if (path == "/api/login" && r.method == "POST")
	{
		int wait = 0;

		if (!core.hasPassword())
			sendError(c, "no password set", 403);
		else if (loginBlocked(wait))
			sendError(c, "too many attempts, retry in " + std::to_string(wait) + "s", 429);
		else if (core.verifyPassword(r.body))
		{
			loginSucceeded();
			setExtraHeader("Set-Cookie: aiscontrol=" + createSession() + cookie_attr);
			sendOK(c);
		}
		else
		{
			loginFailed();
			sendError(c, "invalid password", 401);
		}
	}
	else if (!authenticated)
	{
		sendError(c, "authentication required", 401);
	}
	else if (path == "/api/logout" && r.method == "POST")
	{
		destroySession(r.cookie);
		setExtraHeader("Set-Cookie: aiscontrol=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
		sendOK(c);
	}
	else if (path == "/api/password" && r.method == "POST")
	{
		if (r.body.length() < 6)
			sendError(c, "password needs at least 6 characters");
		else
		{
			core.setPassword(r.body);
			destroyAllSessions();
			setExtraHeader("Set-Cookie: aiscontrol=" + createSession() + cookie_attr);
			sendOK(c);
		}
	}
	else if (path == "/api/config" && r.method == "GET")
	{
		try
		{
			Response(c, "application/json", core.getConfig(), accept_gzip);
		}
		catch (const std::exception &e)
		{
			sendError(c, e.what(), 500);
		}
	}
	else if (path == "/api/config" && r.method == "POST")
	{
		std::string error;
		if (core.setConfig(r.body, error))
			sendOK(c);
		else
			sendError(c, error);
	}
	else if (path == "/api/engine" && r.method == "POST")
	{
		std::string action = r.body;
		Util::Convert::toLower(action);

		if (action == "start")
			core.startEngine();
		else if (action == "stop")
			core.stopEngine();
		else if (action == "restart")
			core.restartEngine();
		else
		{
			sendError(c, "unknown action, use start/stop/restart");
			return;
		}
		sendOK(c);
	}
	else if (path == "/api/devices" && r.method == "GET")
	{
		Response(c, "application/json", core.getDeviceListJSON());
	}
	else if (path == "/api/log" && r.method == "GET")
	{
		IO::SSEConnection *s = upgradeSSE(c, 3);
		if (s)
			for (auto &m : Logger::getInstance().getLastMessages(25))
				s->SendEvent("log", m.toJSON());
	}
	else
	{
		HTTPServer::Request(c, path, false);
	}
}
