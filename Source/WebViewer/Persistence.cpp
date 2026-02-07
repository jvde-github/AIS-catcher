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

#include "WebViewer.h"

bool WebViewer::Save()
{
	try
	{
		std::ofstream infile(backup_filename, std::ios::binary);
		if (!counter.Save(infile))
			return false;
		if (!hist_second.Save(infile))
			return false;
		if (!hist_minute.Save(infile))
			return false;
		if (!hist_hour.Save(infile))
			return false;
		if (!hist_day.Save(infile))
			return false;

		if (!ships.Save(infile))
			return false;

		infile.close();
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}
	return true;
}

void WebViewer::Clear()
{
	counter.Clear();
	counter_session.Clear();

	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

bool WebViewer::Load()
{
	if (backup_filename.empty())
		return false;

	Info() << "Server: reading statistics from " << backup_filename;
	try
	{
		std::ifstream infile(backup_filename, std::ios::binary);

		if (!counter.Load(infile))
			return false;
		if (!hist_second.Load(infile))
			return false;
		if (!hist_minute.Load(infile))
			return false;
		if (!hist_hour.Load(infile))
			return false;
		if (!hist_day.Load(infile))
			return false;
		// Load ship database if available
		if (infile.peek() != EOF)
		{
			if (!ships.Load(infile))
				Warning() << "Server: Could not load ship database from backup";
		}

		infile.close();
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}

	return true;
}

void WebViewer::BackupService()
{
	try
	{
		Info() << "Server: starting backup service every " << backup_interval << " minutes.";
		while (true)
		{
			std::unique_lock<std::mutex> lock(m);

			if (cv.wait_for(lock, std::chrono::minutes(backup_interval), [&]
							{ return !run; }))
			{
				break;
			}

			if (!Save())
				Error() << "Server failed to write backup.";
		}
	}
	catch (std::exception &e)
	{
		Error() << "WebClient BackupService: " << e.what();
		std::terminate();
	}

	Info() << "Server: stopping backup service.";
}
