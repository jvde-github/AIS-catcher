/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include <vector>
#include <memory>

#include "Receiver.h"
#include "WebViewer.h"
#include "Screen.h"
#include "MsgOut.h"

struct OutputStatistics;

class ApplicationContext
{
public:
	std::vector<std::unique_ptr<Receiver>> receivers;
	IO::ScreenOutput screen;
	std::vector<OutputStatistics> stat;
	std::vector<int> msg_count;
	std::vector<std::unique_ptr<WebViewer>> servers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;
	std::vector<std::unique_ptr<IO::OutputJSON>> json;

	int nrec = 0;
	int own_mmsi = -1;
	int group = 0;

	// Command line parsing results
	bool list_devices = false;
	bool list_support = false;
	bool list_options = false;
	bool list_devices_JSON = false;
	bool no_run = false;
	bool show_copyright = true;
	int timeout = 0;
	bool timeout_nomsg = false;
	std::string config_file;

	ApplicationContext()
	{
		receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
	}

	~ApplicationContext()
	{
		Stop();
		
		for (auto& s : servers)
			s->close();
	}

	// Prevent copying
	ApplicationContext(const ApplicationContext&) = delete;
	ApplicationContext& operator=(const ApplicationContext&) = delete;

	void Setup(WebViewer* webapp = nullptr)
	{
		stat.resize(receivers.size());
		msg_count.resize(receivers.size(), 0);

		for (int i = 0; i < receivers.size(); i++)
		{
			Receiver& r = *receivers[i];
			r.setOwnMMSI(own_mmsi);

			if ((servers.size() > 0 && servers[0]->active()) || webapp)
				r.setTags("DTM");

			r.setupDevice();
			// set up the decoding model(s), group is the last output group used
			r.setupModel(group);

			// set up all the output and connect to the receiver outputs
			for (auto& o : msg)
				o->Connect(r);
			for (auto& j : json)
				j->Connect(r);

			screen.Connect(r);

			for (auto& s : servers)
				if (s->active())
					s->connect(r);

			// Connect to persistent webapp if provided
			if (webapp && webapp->active())
				webapp->connect(r);

			if (r.verbose || timeout_nomsg)
				stat[i].connect(r);
		}
	}

	bool shouldRun() const
	{
		return !list_devices && !list_support && !list_options && !no_run;
	}

	bool isAnyVerbose() const
	{
		for (const auto& r : receivers)
			if (r->verbose)
				return true;
		return false;
	}

	bool isAllCallback() const
	{
		for (const auto& r : receivers)
			if (!r->getDeviceManager().getDevice()->isCallback())
				return false;
		return true;
	}

	void printVerboseStats()
	{
		for (int i = 0; i < receivers.size(); i++)
		{
			Receiver& r = *receivers[i];
			if (r.verbose)
			{
				for (int j = 0; j < r.Count(); j++)
				{
					stat[i].statistics[j].Stamp();
					std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
					Info() << "[" << name << "] " << std::string(37 - name.length(), ' ') << "received: " << stat[i].statistics[j].getDeltaCount() << " msgs, total: "
						   << stat[i].statistics[j].getCount() << " msgs, rate: " << stat[i].statistics[j].getRate() << " msg/s";
				}
			}
		}
	}

	void Stop()
	{
		for (auto& r : receivers)
			r->stop();
	}

	void printFinalStats()
	{
		std::stringstream ss;
		for (int i = 0; i < receivers.size(); i++)
		{
			Receiver& r = *receivers[i];

			if (r.verbose)
			{
				ss << "----------------------\n";
				for (int j = 0; j < r.Count(); j++)
				{
					std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
					stat[i].statistics[j].Stamp();
					ss << "[" << name << "] " << std::string(37 - name.length(), ' ') << "total: " << stat[i].statistics[j].getCount() << " msgs" << "\n";
				}
			}

			if (r.Timing())
				for (int j = 0; j < r.Count(); j++)
				{
					std::string name = r.Model(j)->getName();
					ss << "[" << r.Model(j)->getName() << "]: " << std::string(37 - name.length(), ' ') << r.Model(j)->getTotalTiming() << " ms" << "\n";
				}
		}
		if (!ss.str().empty())
			Info() << ss.str();
	}

	void Start()
	{
		for (auto& o : msg)
			o->Start();

		for (auto& j : json)
			j->Start();

		for (auto& s : servers)
			if (s->active())
			{
				if (own_mmsi != -1)
				{
					s->Set("SHARE_LOC", "true");
					s->Set("OWN_MMSI", std::to_string(own_mmsi));
				}
				s->start();
			}

		for (auto& s : stat)
			s.start();

		for (auto& r : receivers)
			r->play();
	}
};
