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

#include "BackupManager.h"
#include "Logger.h"
#include "Helper.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

void BackupManager::run()
{
	Debug() << "Server: starting backup service every " << interval << " minutes.";

	while (running)
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::minutes(interval), [this]
					{ return !running.load(); });

		// ships included: they carry the per-vessel dedup times the hourly
		// vessel counts need to stay exact across a crash restore
		if (running && !save())
			Error() << "Server: failed to write backup: " << filename;
	}

	Debug() << "Server: stopping backup service.";
}

void BackupManager::start()
{
	if (interval <= 0 || !tracker || filename.empty())
		return;

	running = true;
	thread = std::thread(&BackupManager::run, this);
}

void BackupManager::stop()
{
	if (running.load())
	{
		{
			// must hold mtx so the store cannot slip between the run thread's
			// predicate check and its wait, which would stall join() a full interval
			std::lock_guard<std::mutex> lock(mtx);
			running = false;
		}
		cv.notify_all();
		if (thread.joinable())
			thread.join();
	}
}


std::string BackupManager::base_dir;

static bool isAbsolutePath(const std::string &p)
{
	if (p.empty())
		return false;
	if (p[0] == '/' || p[0] == '\\')
		return true;
	return p.size() > 2 && p[1] == ':' && (p[2] == '/' || p[2] == '\\');
}

std::string BackupManager::resolve(const std::string &f)
{
	std::string path = f;

	if (path.size() > 1 && path[0] == '~' && (path[1] == '/' || path[1] == '\\'))
	{
		const char *home = std::getenv("HOME");
		if (home && *home)
			path = std::string(home) + path.substr(1);
		else
			Warning() << "Server: no home directory to expand \"" << f << "\"";
	}

	if (path.empty() || isAbsolutePath(path) || base_dir.empty())
		return path;

	return base_dir + "/" + path;
}

bool BackupManager::save(bool include_ships)
{
	if (filename.empty() || !tracker)
		return false;

	const std::string tmp = filename + ".tmp";

	try
	{
		std::ofstream outfile(tmp, std::ios::binary | std::ios::trunc);

		if (!outfile.is_open())
		{
			Error() << "Server: cannot open backup file for writing: "
					<< tmp << " (" << std::strerror(errno) << ")";
			return false;
		}

		outfile.exceptions(std::ios::failbit | std::ios::badbit);

		if (!tracker->save(outfile, include_ships))
		{
			outfile.close();
			std::remove(tmp.c_str());
			return false;
		}

		outfile.close();
	}
	catch (const std::exception &e)
	{
		Error() << "Server: write error on " << tmp << " (" << e.what() << ")";
		std::remove(tmp.c_str());
		return false;
	}

	// data must reach the disk before the rename makes it the live backup
	if (!Util::Helper::syncFile(tmp))
	{
		Error() << "Server: cannot sync " << tmp << " to disk (" << std::strerror(errno) << ")";
		std::remove(tmp.c_str());
		return false;
	}

#ifdef _WIN32
	std::remove(filename.c_str());
#endif
	if (std::rename(tmp.c_str(), filename.c_str()) != 0)
	{
		Error() << "Server: cannot rename " << tmp << " -> " << filename
				<< " (" << std::strerror(errno) << ")";
		std::remove(tmp.c_str());
		return false;
	}

	return true;
}

bool BackupManager::load()
{
	if (filename.empty() || !tracker)
		return false;

	Info() << "Server: reading statistics from " << filename;
	try
	{
		std::ifstream infile(filename, std::ios::binary);

		if (!infile.is_open())
		{
			Warning() << "Server: cannot open backup file for reading: "
					  << filename << " (" << std::strerror(errno) << ")";
			return false;
		}

		if (!tracker->load(infile))
		{
			Warning() << "Server: Could not load statistics from backup";
			return false;
		}
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}

	return true;
}
