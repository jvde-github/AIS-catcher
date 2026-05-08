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

#include "MsgOut.h"
#include "AIS.h"

#ifdef HASNMEA2000

#include <fstream>
#include <iostream>
#include <iomanip>

#include <thread>
#include <mutex>
#include <list>
#include <chrono>
#include <condition_variable>

#include <N2kMessages.h>

namespace IO
{

	class N2KStreamer : public IO::OutputMessage
	{
		std::string dev = "can0";

	public:
		N2KStreamer() : OutputMessage("NMEA2000") { fmt = MessageFormat::JSON_FULL; }
		virtual ~N2KStreamer() { Stop(); }

		void Start();
		void Stop();

		void pushQueue(tN2kMsg *N2kMsg);

		void sendType123(const AIS::Message &ais, const JSON::JSON *data);

		void sendType4(const AIS::Message &ais, const JSON::JSON *data);
		void sendType5(const AIS::Message &ais, const JSON::JSON *data);
		void sendType9(const AIS::Message &ais, const JSON::JSON *data);
		void sendType14(const AIS::Message &ais, const JSON::JSON *data);
		void sendType18(const AIS::Message &ais, const JSON::JSON *data);
		void sendType19(const AIS::Message &ais, const JSON::JSON *data);
		void sendType21(const AIS::Message &ais, const JSON::JSON *data);
		void sendType24(const AIS::Message &ais, const JSON::JSON *data);

		void Receive(const JSON::JSON *data, int ln, TAG &tag);
		Setting &SetKey(AIS::Keys key, const std::string &arg);
	};
}

#else // HASNMEA2000

namespace IO
{
	class N2KStreamer : public IO::OutputUnavailable
	{
	public:
		N2KStreamer() : OutputUnavailable("NMEA2000", "HASNMEA2000") {}
	};
}

#endif // HASNMEA2000
