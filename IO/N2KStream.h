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
#include <N2kMessages.h>
#endif

#include "MsgOut.h"
#include "AIS.h"

namespace IO {

	class N2KStreamer : public IO::OutputJSON {
		AIS::Filter filter;
		std::string dev = "can0";

#ifdef HASNMEA2000

	public:
		virtual ~N2KStreamer() { Stop(); }

		void Start();
		void Stop();

		void pushQueue(tN2kMsg* N2kMsg);
		
		void sendType123(const AIS::Message& ais, const JSON::JSON* data);

		void sendType4(const AIS::Message& ais, const JSON::JSON* data);
		void sendType5(const AIS::Message& ais, const JSON::JSON* data);
		void sendType9(const AIS::Message& ais, const JSON::JSON* data);
		void sendType14(const AIS::Message& ais, const JSON::JSON* data);
		void sendType18(const AIS::Message& ais, const JSON::JSON* data);
		void sendType19(const AIS::Message& ais, const JSON::JSON* data);
		void sendType21(const AIS::Message& ais, const JSON::JSON* data);
		void sendType24(const AIS::Message& ais, const JSON::JSON* data);

		void Receive(const JSON::JSON* data, int ln, TAG& tag);
		Setting& Set(std::string option, std::string arg);
#else
	public:
		void Start() { std::cout << "NMEA2000 support not included in this build." << std::endl; }
		Setting& Set(std::string option, std::string arg) {
			std::cout << "NMEA2000 support not included in this build." << std::endl;
			return *this;
		}
#endif
	};
}
