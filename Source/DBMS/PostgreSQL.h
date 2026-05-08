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

#include "MsgOut.h"

#ifdef HASPSQL

#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

#include <libpq-fe.h>

#include "Stream.h"
#include "Keys.h"
#include "AIS.h"
#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

namespace IO
{

	class PostgreSQL : public OutputMessage
	{
		int station_id = 0;
		int conn_fails = 0;
		int MAX_FAILS = 10;

		PGconn *con = nullptr;
		std::vector<int> db_keys;
		bool terminate = false, running = false;

		struct QueuedEntry
		{
			std::string mmsi, station_id, msg_type, timestamp, channel, level, ppm;
			int msg_type_int;
			std::vector<std::string> nmea_lines;
			struct KV
			{
				int key;
				std::string value;
			};
			std::vector<KV> kvs;
			struct Property
			{
				std::string db_key, value;
			};
			std::vector<Property> properties;
			std::string type_bit, channel_bit;
		};

		std::vector<QueuedEntry> message_queue;
		static const size_t MAX_QUEUE_SIZE = 2048;

		bool MSGS = false, NMEA = false, VP = false, VS = false, BS = false, ATON = false, SAR = false, VD = true;
		std::string conn_string = "dbname=ais";
		std::thread run_thread;

		std::mutex queue_mutex;

		int INTERVAL = 10;

		void post();
		bool execTableInsert(const char *table, const QueuedEntry &entry, const int *keys, int nkeys, const char *msg_id);
		bool execVessel(const QueuedEntry &entry, const char *msg_id);

	public:
		PostgreSQL() : OutputMessage("PostgreSQL") { fmt = MessageFormat::JSON_FULL; }
		~PostgreSQL();

		void process();
		using StreamIn<AIS::Message>::Receive;
		using StreamIn<AIS::GPS>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		void Receive(const JSON::JSON *data, int len, TAG &tag);

		void setup();

		void Start() { setup(); }

		Setting &SetKey(AIS::Keys key, const std::string &arg);
	};
}

#else // HASPSQL

namespace IO
{
	class PostgreSQL : public OutputUnavailable
	{
	public:
		PostgreSQL() : OutputUnavailable("PostgreSQL", "HASPSQL") {}
	};
}

#endif // HASPSQL
