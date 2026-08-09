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

#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <iostream>
#include <fstream>
#include <memory>
#include <cctype>
#include <string>
#include <vector>

#include "AIS-catcher.h"

#ifdef HASWEBVIEWER
#include "WebViewer.h"
#endif
#include "CommandLine.h"
#include "ManagedMain.h"
#include "Logger.h"

std::atomic<bool> stop;
std::atomic<bool> stop_process;

#ifdef HASWEBVIEWER
WebViewer *managed_viewer = nullptr;
#endif

void StopRequest()
{
	stop = true;
}
#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
	{
		stop = true;
		stop_process = true;
	}
	return TRUE;
}
#else
// only async-signal-safe calls allowed here
static void consoleHandler(int signal)
{
	if (signal == SIGPIPE)
		return;

	if (signal != SIGINT)
	{
		static const char msg[] = "Termination request received\n";
		ssize_t rc = write(STDERR_FILENO, msg, sizeof(msg) - 1);
		(void)rc;
	}

	stop = true;
	stop_process = true;
}
#endif

// Expand @filename response-file arguments (gcc/clang convention).
// Each line is whitespace-split; lines whose first non-whitespace
// character is '#' are skipped, as are blank lines. CR endings are
// stripped. Nested @file is allowed up to a small depth limit.
static void expandResponseFiles(int argc, char *argv[],
								std::vector<std::string> &out, int depth = 0)
{
	for (int i = 0; i < argc; i++)
	{
		const char *s = argv[i];
		if (!s || s[0] == '\0') continue;
		if (s[0] != '@')
		{
			out.emplace_back(s);
			continue;
		}
		if (depth >= 8)
			throw std::runtime_error(std::string("Response file recursion too deep: ") + s);

		std::ifstream f(s + 1);
		if (!f)
			throw std::runtime_error(std::string("Cannot open response file: ") + (s + 1));

		std::vector<std::string> tokens;
		std::string line;
		while (std::getline(f, line))
		{
			if (!line.empty() && line.back() == '\r') line.pop_back();

			size_t p = 0;
			while (p < line.size() && std::isspace((unsigned char)line[p])) p++;
			if (p >= line.size() || line[p] == '#') continue;

			while (p < line.size())
			{
				size_t e = p;
				while (e < line.size() && !std::isspace((unsigned char)line[e])) e++;
				tokens.emplace_back(line.substr(p, e - p));
				p = e;
				while (p < line.size() && std::isspace((unsigned char)line[p])) p++;
			}
		}

		std::vector<char *> tok_argv;
		tok_argv.reserve(tokens.size());
		for (auto &t : tokens) tok_argv.push_back(&t[0]);
		expandResponseFiles((int)tok_argv.size(), tok_argv.data(), out, depth + 1);
	}
}


int main(int argc, char *argv[])
{
	int cb = -1;

	try
	{
		Logger::getInstance().setMaxBufferSize(200);
		cb = Logger::getInstance().addConsoleListener();

#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("could not set control handler");
#else
		signal(SIGINT, consoleHandler);
		signal(SIGTERM, consoleHandler);
		signal(SIGHUP, consoleHandler);
		signal(SIGPIPE, consoleHandler);
#endif

		std::vector<std::string> args;
		expandResponseFiles(argc, argv, args);

		if (Managed::isInvocation(args))
		{
			CommandLine::printVersion();
			return Managed::run(args);
		}

		return CommandLine::run(args, cb);
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		return -1;
	}
}
