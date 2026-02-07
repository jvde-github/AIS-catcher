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

#include "HTTPServer.h"
#include "Logger.h"

class WebViewerLogger
{
protected:
	IO::HTTPServer *server = nullptr;
	Logger::LogCallback logCallback;
	int cb = -1;

public:
	void Start()
	{
		logCallback = [this](const LogMessage &msg)
		{
			if (server)
			{
				server->sendSSE(3, "log", msg.toJSON());
			}
		};

		cb = Logger::getInstance().addLogListener(logCallback);
	}

	void Stop()
	{
		if (cb != -1)
		{
			Logger::getInstance().removeLogListener(cb);
			cb = -1;
		}
	}

	virtual ~WebViewerLogger() = default;

	void setSSE(IO::HTTPServer *s)
	{
		server = s;
	}
};
