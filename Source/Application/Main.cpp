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

#include <atomic>
#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "AIS-catcher.h"

#include "CommandLine.h"
#include "ManagedMain.h"
#include "Logger.h"

std::atomic<bool> stop;
std::atomic<bool> stop_process;

void StopRequest()
{
	stop = true;
}
#ifdef _WIN32
#include <windows.h>

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
// Each line is whitespace-split; lines whose first word starts with '#'
// are skipped, as are blank lines. Nested @file is allowed up to a small
// depth limit.
static void expandResponseFiles(const std::vector<std::string> &args,
								std::vector<std::string> &out, int depth = 0)
{
	for (const std::string &s : args)
	{
		if (s.empty()) continue;
		if (s[0] != '@')
		{
			out.push_back(s);
			continue;
		}
		if (depth >= 8)
			throw std::runtime_error("Response file recursion too deep: " + s);

		std::ifstream f(s.substr(1));
		if (!f)
			throw std::runtime_error("Cannot open response file: " + s.substr(1));

		std::vector<std::string> tokens;
		std::string line, word;
		while (std::getline(f, line))
		{
			std::istringstream words(line);
			if (!(words >> word) || word[0] == '#') continue;
			do
				tokens.push_back(word);
			while (words >> word);
		}
		expandResponseFiles(tokens, out, depth + 1);
	}
}

int main(int argc, char *argv[])
{
	try
	{
		Logger::getInstance().setMaxBufferSize(200);
		Logger::getInstance().addConsoleListener();

#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("could not set control handler");
#else
		signal(SIGINT, consoleHandler);
		signal(SIGTERM, consoleHandler);
		signal(SIGHUP, consoleHandler);
		signal(SIGPIPE, SIG_IGN);
#endif

		std::vector<std::string> args;
		expandResponseFiles(std::vector<std::string>(argv, argv + argc), args);

		if (Managed::isInvocation(args))
		{
			CommandLine::printVersion();
			return Managed::run(args);
		}

		return CommandLine::run(args);
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		return -1;
	}
}
