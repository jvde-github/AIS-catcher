/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#ifdef HASPSQL
#include <libpq-fe.h>
#endif

#include "Stream.h"
#include "JSON/JSON.h"
#include "Keys.h"
#include "AIS.h"
#include "Utilities.h"
#include "JSON/StringBuilder.h"

// Needs major clean up and simplification

namespace IO {

	class PostgreSQL : public StreamIn<JSON::JSON>, public Setting {
		JSON::StringBuilder builder;
		std::string sql_trans;
		std::stringstream sql;
		AIS::Filter filter;
		int station_id = 0;
		int conn_fails = 0;
		int MAX_FAILS = 10;

		std::string escape(const std::string& input) {
			std::string output;
			for (const char c : input) {
				if (c == '\'') output += c;

				output += c;
			}
			return output;
		}

#ifdef HASPSQL
		PGconn* con = NULL;
		std::vector<int> db_keys;
#endif

		bool MSGS = true, NMEA = false, VP = true, VS = true, BS = false, ATON = true, SAR = true;
		std::string conn_string = "dbname=ais";
		std::thread run_thread;
		bool terminate = false, running = false;
		std::mutex queue_mutex;
		uint32_t msg_id = 0;

		int INTERVAL = 10;
#ifdef HASPSQL
		void post();
#endif
	public:
		PostgreSQL() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}
		~PostgreSQL();

#ifdef HASPSQL
		void process();

		std::string addVesselPosition(const JSON::JSON* data, const AIS::Message* msg, const std::string &m, const std::string &s);
		std::string addVesselStatic(const JSON::JSON* data, const AIS::Message* msg, const std::string &m, const std::string &s);
		std::string addBasestation(const JSON::JSON* data, const AIS::Message* msg, const std::string &m, const std::string &s);
		std::string addSARposition(const JSON::JSON* data, const AIS::Message* msg, const std::string &m, const std::string &s);
		std::string addATON(const JSON::JSON* data, const AIS::Message* msg, const std::string &m, const std::string &s);

		void Receive(const JSON::JSON* data, int len, TAG& tag);
#endif

		void setup();

		void Start() {}
		void setMap(int m) { builder.setMap(m); }


		Setting& Set(std::string option, std::string arg);
	};
}
