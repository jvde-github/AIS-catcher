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

#ifdef HASPSQL
#include <libpq-fe.h>
#endif

#include "Stream.h"
#include "AIS.h"
#include "Utilities.h"
#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"


namespace IO {

	class PostgreSQL : public StreamIn<JSON::JSON>, public Setting {
		JSON::StringBuilder builder;
		std::string sql, sql_trans;
		AIS::Filter filter;

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

		bool write_nmea = false;
		std::string conn_string = "dbname=ais";
		std::thread run_thread;
		bool terminate = false, running = false;
		std::mutex queue_mutex;
		uint32_t msg_id = 0;

		int INTERVAL = 10;
#ifdef HASPSQL
		void post() {

			{
				const std::lock_guard<std::mutex> lock(queue_mutex);
				sql_trans = sql;
				sql.clear();
			}

			PGresult* res;
			res = PQexec(con, sql_trans.c_str());

			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				std::cerr << "DBMS: Error writing PostgreSQL: " << PQerrorMessage(con) << sql_trans << std::endl;
			}
			else {
				std::cerr << "DMBS: write completed (" << sql_trans.size() << " bytes)." << std::endl;
			}

			PQclear(res);
		}
#endif
	public:
		PostgreSQL() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}
		~PostgreSQL() {
#ifdef HASPSQL
			if (running) {

				post();
				running = false;
				terminate = true;
				run_thread.join();

				std::cerr << "DBMS: stop thread and database closed." << std::endl;
			}
			if (con != NULL) PQfinish(con);
#endif
		}

#ifdef HASPSQL

		void process() {
			int i = 0;

			while (!terminate) {

				for (int i = 0; i < INTERVAL && sql.size() < 32768 * 16; i++) {
					SleepSystem(1000);
					if (terminate) break;
				}
				if (!sql.empty()) post();
			}
		}
#endif

		void setup() {
#ifdef HASPSQL
			db_keys.resize(AIS::KeyMap.size(), -1);
			std::cerr << "Connecting to ProgreSQL database: \"" + conn_string + "\"\n";
			con = PQconnectdb(conn_string.c_str());

			if (PQstatus(con) != CONNECTION_OK)
				throw std::runtime_error("DBMS: cannot open database :" + std::string(PQerrorMessage(con)));

			PGresult* res = PQexec(con, "SELECT key_id, key_str FROM ais_keys");
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				throw std::runtime_error("DBMS: error fetching ais_keys table: " + std::string(PQerrorMessage(con)));
			}

			for (int row = 0; row < PQntuples(res); row++) {
				int id = atoi(PQgetvalue(res, row, 0));
				std::string name = PQgetvalue(res, row, 1);
				bool found = false;
				for (int i = 0; i < db_keys.size(); i++) {
					if (AIS::KeyMap[i][JSON_DICT_FULL] == name) {
						db_keys[i] = id;
						found = true;
						break;
					}
				}
				if (!found)
					throw std::runtime_error("DBMS: The requested key \"" + name + "\" in ais_keys is not defined.");
			}

			if (!running) {

				running = true;
				terminate = false;

				run_thread = std::thread(&PostgreSQL::process, this);
				std::cerr << "DBMS: start thread, filter: " << Util::Convert::toString(filter.isOn());
				if (filter.isOn()) std::cerr << ", Allowed: " << filter.getAllowed() << ".";
				std::cerr << std::endl;
			}
#else
			throw std::runtime_error("DBMS: no support for PostgeSQL build in.");
#endif
		}

		void Start(){};
		void Receive(const JSON::JSON* data, int len, TAG& tag) {
#ifdef HASPSQL
			const std::lock_guard<std::mutex> lock(queue_mutex);

			if (sql.size() > 32768 * 24) {
				std::cerr << "DBMS: writing to database too slow, data lost." << std::endl;
				sql.clear();
			}

			const AIS::Message* msg = (AIS::Message*)data[0].binary;

			sql += std::string("DROP TABLE IF EXISTS _id;\nWITH cte AS (INSERT INTO ais_message (mmsi, type, timestamp, sender, channel, signal_level, ppm) "
							   "VALUES (") +
				   std::to_string(msg->mmsi()) + ',' + std::to_string(msg->type()) + ',' + std::to_string(msg->getRxTimeUnix()) + ',' +
				   std::to_string(0) + ",\'" + (char)msg->getChannel() + "\'," +
				   std::to_string(tag.level) + ',' + std::to_string(tag.ppm) +
				   ") RETURNING id)\nSELECT id INTO _id FROM cte;";


			// TO DO: remove, can be selected from JSON properties
			if (write_nmea)
				for (auto s : msg->NMEA) {
					sql += "INSERT INTO ais_nmea (id, nmea) VALUES ((SELECT id FROM _id),\'" + s + "\');\n";
				}

			// TO DO: types, etc
			for (const auto& p : data[0].getProperties()) {
				if (db_keys[p.Key()] != -1) {
					if (p.Get().isString()) {
						sql += "INSERT INTO ais_property (id, key, value) VALUES ((SELECT id FROM _id),\'" + std::to_string(db_keys[p.Key()]) + "\',\'" + escape(p.Get().getString()) + "\');\n";
					}
					else {
						std::string temp;
						builder.to_string(temp, p.Get());
						temp = temp.substr(0, 20);
						sql += "INSERT INTO ais_property (id, key, value) VALUES ((SELECT id FROM _id),\'" + std::to_string(db_keys[p.Key()]) + "\',\'" + temp + "\');\n";
					}
				}
			}


#endif
		}
		void setMap(int m) { builder.setMap(m); }

		Setting& Set(std::string option, std::string arg) {

			Util::Convert::toUpper(option);

			if (option == "CONN_STR") {
				conn_string = arg;
			}
			else if (option == "WRITE_NMEA") {
				write_nmea = Util::Parse::Switch(arg);
			}
			else {
				filter.Set(option, arg);
			}
			return *this;
		}
	};
}
