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

#include <iostream>
#include <string.h>
#include <memory>

#include "AIS-catcher.h"

#include "Receiver.h"
#include "WebViewer.h"
#include "JSONConfig.h"
#include "CLI.h"
#include "JSON.h"
#include "N2KStream.h"
#include "PostgreSQL.h"
#include "Logger.h"
#include "Screen.h"
#include "File.h"
#include "CommunityStreamer.h"

static std::atomic<bool> stop;

void StopRequest()
{
	stop = true;
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
		stop = true;
	return TRUE;
}
#else
static void consoleHandler(int signal)
{
	if (signal == SIGPIPE)
	{
		// Info() << "Termination request SIGPIPE ignored" ;
		return;
	}
	if (signal != SIGINT)
		std::cerr << "Termination request: " << signal;

	stop = true;
}
#endif

// -------------------------------
// Main application functions

static int initializeSystem(ApplicationState &app)
{
	Logger::getInstance().setMaxBufferSize(50);
	app.cb = Logger::getInstance().addLogListener([](const LogMessage &msg)
											  { std::cerr << msg.message << "\n"; });

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
		throw std::runtime_error("could not set control handler");
#else
	signal(SIGINT, consoleHandler);
	signal(SIGTERM, consoleHandler);
	signal(SIGHUP, consoleHandler);
	signal(SIGPIPE, consoleHandler);
#endif

	return 0;
}

static bool handleInformationRequests(ApplicationState &app)
{
	if (app.show_copyright)
		CLI::printVersion();

	if (app.list_devices)
		app.receivers.back()->getDeviceManager().printAvailableDevices(app.list_devices_JSON);
	if (app.list_support)
		CLI::printBuildConfiguration();
	if (app.list_options)
		CLI::printUsage();

	return (app.list_devices || app.list_support || app.list_options || app.no_run);
}

static void setupReceiversAndOutputs(ApplicationState &app)
{
	app.stat.resize(app.receivers.size());

	int group = 0;

	for (int i = 0; i < app.receivers.size(); i++)
	{
		Receiver &r = *app.receivers[i];
		r.setOwnMMSI(app.own_mmsi);

		if (app.servers.size() > 0 && app.servers[0]->active())
			r.setTags("DTM");

		r.setupDevice();
		r.setupModel(group);

		for (auto &o : app.msg)
			o->Connect(r);
		for (auto &j : app.json)
			j->Connect(r);

		app.screen.Connect(r);

		for (auto &s : app.servers)
			if (s->active())
				s->connect(r);

		if (r.verbose || app.timeout_nomsg)
			app.stat[i].connect(r);
	}
}

static void startOutputServices(ApplicationState &app)
{
	for (auto &o : app.msg)
		o->Start();

	for (auto &j : app.json)
		j->Start();

	for (auto &s : app.servers)
		if (s->active())
		{
			if (app.own_mmsi != -1)
			{
				s->Set("SHARE_LOC", "true");
				s->Set("OWN_MMSI", std::to_string(app.own_mmsi));
			}
			s->setCommunityFeed(app.community_feed);
			s->start();
		}

	Debug() << "Starting statistics";
	for (auto &s : app.stat)
		s.start();

	Debug() << "Starting receivers";
	for (auto &r : app.receivers)
		r->play();
}

static int runMainLoop(ApplicationState &app)
{
	int exit_code = 0;
	stop = false;
	const int SLEEP = 50;
	auto time_start = high_resolution_clock::now();
	auto time_timeout_start = time_start;
	auto time_last = time_start;
	bool oneverbose = false, iscallback = true;

	for (auto &r : app.receivers)
	{
		oneverbose |= r->verbose;
		iscallback &= r->getDeviceManager().getDevice()->isCallback();
	}

	Debug() << "Entering main loop";
	while (!stop)
	{
		for (auto &r : app.receivers)
			stop = stop || !(r->getDeviceManager().getDevice()->isStreaming());

		if (iscallback)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

		if (!oneverbose && !app.timeout)
			continue;

		auto time_now = high_resolution_clock::now();

		if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >= app.screen.verboseUpdateTime)
		{
			time_last = time_now;

			for (int i = 0; i < app.receivers.size(); i++)
			{
				Receiver &r = *app.receivers[i];
				if (r.verbose)
				{
					for (int j = 0; j < r.Count(); j++)
					{
						app.stat[i].statistics[j].Stamp();
						std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
						Info() << "[" << name << "] " << std::string(37 - name.length(), ' ') << "received: " << app.stat[i].statistics[j].getDeltaCount() << " msgs, total: "
							   << app.stat[i].statistics[j].getCount() << " msgs, rate: " << app.stat[i].statistics[j].getRate() << " msg/s";
					}
				}
			}
		}

		if (app.timeout && app.timeout_nomsg)
		{
			bool one_stale = false;

			for (int i = 0; i < app.receivers.size(); i++)
			{
				if (app.stat[i].statistics[0].getCount() == app.msg_count[i])
					one_stale = true;

				app.msg_count[i] = app.stat[i].statistics[0].getCount();
			}

			if (!one_stale)
				time_timeout_start = time_now;
		}

		if (app.timeout && duration_cast<seconds>(time_now - time_timeout_start).count() >= app.timeout)
		{
			stop = true;
			if (app.timeout_nomsg)
				Warning() << "Stop triggered, no messages were received for " << app.timeout << " seconds.";
			else
			{
				exit_code = -2;
				Warning() << "Stop triggered by timeout after " << app.timeout << " seconds. (-T " << app.timeout << ")";
			}
		}
	}

	return exit_code;
}

static void printFinalStatistics(ApplicationState &app)
{
	std::stringstream ss;
	for (int i = 0; i < app.receivers.size(); i++)
	{
		Receiver &r = *app.receivers[i];
		r.stop();

		if (r.verbose)
		{
			ss << "----------------------\n";
			for (int j = 0; j < r.Count(); j++)
			{
				std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
				app.stat[i].statistics[j].Stamp();
				ss << "[" << name << "] " << std::string(37 - name.length(), ' ') << "total: " << app.stat[i].statistics[j].getCount() << " msgs" << "\n";
			}
		}

		if (r.Timing())
			for (int j = 0; j < r.Count(); j++)
			{
				std::string name = r.Model(j)->getName();
				ss << "[" << r.Model(j)->getName() << "]: " << std::string(37 - name.length(), ' ') << r.Model(j)->getTotalTiming() << " ms" << "\n";
			}
		Info() << ss.str();
	}
}

int main(int argc, char *argv[])
{
	ApplicationState app;

	try
	{
		initializeSystem(app);
		
		for (auto &r : app.receivers)
			r->getDeviceManager().refreshDevices();

		CLI::parseArguments(argc, argv, app);

		if (handleInformationRequests(app))
			return 0;

		app.msg_count.resize(app.receivers.size(), 0);

		setupReceiversAndOutputs(app);
		startOutputServices(app);

		int exit_code = runMainLoop(app);

		printFinalStatistics(app);

		for (auto &s : app.servers)
			s->close();
			
		return exit_code;
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		for (auto &r : app.receivers)
			r->stop();
		return -1;
	}
}
