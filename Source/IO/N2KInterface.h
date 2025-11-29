/*
	Copyright(c) 2024 jvde.github@gmail.com

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
#include <fstream>
#include <iostream>
#include <iomanip>

#include <thread>
#include <mutex>
#include <list>
#include <chrono>
#include <condition_variable>

#ifdef HASNMEA2000
#include <NMEA2000.h>
#include <N2kMsg.h>
#endif

#include "Common.h"

namespace N2K
{

#ifdef HASNMEA2000

	// based on NMEA2000_SocketCAN with additions to support the N2KHub class
	class tNMEA2000_SKTCAN : public tNMEA2000
	{
	protected:
		bool CANOpen();
		bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent);
		bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf);

		int skt = -1;
		std::string CANinterface;

		bool OpenInterface(const std::string &port);

	public:
		virtual ~tNMEA2000_SKTCAN() {}

		// additions to support the N2KHub class
		void waitForFrame(int m);
		void Close();

		void setNetwork(std::string n) { CANinterface = n; }
	};

	extern tNMEA2000_SKTCAN NMEA2000;

	class N2KHubInterfaceHub
	{
		std::string network = "";
		std::thread run_thread;
		std::mutex mtx;
		std::condition_variable fifo_cond;

		Callback<tN2kMsg> *input = nullptr;

		tNMEA2000_SKTCAN NMEA2000;

		bool running = false;
		bool output = false;

		bool connected = false;

		void onOpen();
		static void onOpenStatic();

		void onMsg(const tN2kMsg &N2kMsg);
		static void onMsgStatic(const tN2kMsg &N2kMsg);

		void run();

	public:
		virtual ~N2KHubInterfaceHub() { Stop(); }

		void Start();
		void Stop();

		void addInput(Callback<tN2kMsg> *cb)
		{
			if (running)
			{
				Critical() << "NMEA2000: addOutput: cannot add input while running";
				StopRequest();
				return;
			}
			input = cb;
		}

		void addOutput()
		{
			if (running)
			{
				Error() << "NMEA2000: addOutput: cannot add output while running";
				StopRequest();
				return;
			}
			if (output)
				throw std::runtime_error("NMEA2000: Max one NMEA2000 output channel is supported.");

			output = true;
		}

		void setNetwork(std::string n)
		{
			if (!network.empty() && n != network)
			{
				throw std::runtime_error("NMEA2000: only one network supported (" + network + " set and " + n + " requested");
			}
			network = n;
		}

		void sendMsg(const tN2kMsg &N2kMsg);
	};

#else
	class N2KHubInterfaceHub
	{
	public:
		void Start() {}
		void Stop() {}
	};

#endif

	extern N2KHubInterfaceHub N2KInterface;
}
