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
#include "Keys.h"
#include "JSON/StringBuilder.h"

// Needs major clean up and simplification

namespace IO {

	class PostgreSQL : public StreamIn<JSON::JSON>, public Setting {
		JSON::StringBuilder builder;
		std::string sql, sql_trans;
		AIS::Filter filter;
		int station_id = 0;

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
		void post() {

			{
				// std::cerr << sql << std::endl;
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
				std::cerr << "DBMS: write completed (" << sql_trans.size() << " bytes)." << std::endl;
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

			int key_count = 0;
			for (int row = 0; row < PQntuples(res); row++) {
				int id = atoi(PQgetvalue(res, row, 0));
				std::string name = PQgetvalue(res, row, 1);
				bool found = false;
				for (int i = 0; i < db_keys.size(); i++) {
					if (AIS::KeyMap[i][JSON_DICT_FULL] == name) {
						db_keys[i] = id;
						found = true;
						key_count++;
						break;
					}
				}
				if (!found)
					throw std::runtime_error("DBMS: The requested key \"" + name + "\" in ais_keys is not defined.");
			}

			if (key_count > 0 && !MSGS) {
				std::cerr << "DBMS: no messages logged in combination with property logging. MSGS ON auto activated." << std::endl;
				MSGS = true;
			}

			if (!running) {

				running = true;
				terminate = false;

				run_thread = std::thread(&PostgreSQL::process, this);
				std::cerr << "DBMS: start thread, filter: " << Util::Convert::toString(filter.isOn());
				if (filter.isOn()) std::cerr << ", Allowed: " << filter.getAllowed();
				std::cerr << ", VP " << Util::Convert::toString(VP);
				std::cerr << ", MSGS " << Util::Convert::toString(MSGS);
				std::cerr << ", VS " << Util::Convert::toString(VS);
				std::cerr << ", BS " << Util::Convert::toString(BS);
				std::cerr << ", SAR " << Util::Convert::toString(SAR);
				std::cerr << ", ATON " << Util::Convert::toString(ATON);
				std::cerr << ", NMEA " << Util::Convert::toString(NMEA);
				std::cerr << std::endl;
			}
#else
			throw std::runtime_error("DBMS: no support for PostgeSQL build in.");
#endif
		}

		std::string addVesselPosition(const JSON::JSON* data, const AIS::Message* msg) {
			if (!VP) return "";

			std::string keys = "";
			std::string values = "";

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_LAT:
				case AIS::KEY_LON:
				case AIS::KEY_MMSI:
				case AIS::KEY_STATUS:
				case AIS::KEY_TURN:
				case AIS::KEY_HEADING:
				case AIS::KEY_COURSE:
				case AIS::KEY_SPEED:
					keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
					builder.to_string(values, p.Get());
					values += ",";
					break;
				}
			}
			keys += "msg_id,station_id,received_at";
			values += (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
			values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

			return "INSERT INTO ais_vessel_pos (" + keys + ") VALUES (" + values + ");\n";
		}

		std::string addVesselStatic(const JSON::JSON* data, const AIS::Message* msg) {
			if (!VS) return "";

			std::string keys = "";
			std::string values = "";

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_MMSI:
				case AIS::KEY_IMO:
				case AIS::KEY_SHIPNAME:
				case AIS::KEY_CALLSIGN:
				case AIS::KEY_TO_BOW:
				case AIS::KEY_TO_STERN:
				case AIS::KEY_TO_STARBOARD:
				case AIS::KEY_TO_PORT:
				case AIS::KEY_DRAUGHT:
				case AIS::KEY_SHIPTYPE:
				case AIS::KEY_DESTINATION:
				case AIS::KEY_ETA:
					keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
					if (p.Get().isString()) {
						values += "\'" + escape(p.Get().getString()) + "\'";
					}
					else
						builder.to_string(values, p.Get());
					values += ",";
					break;
				}
			}
			keys += "msg_id,station_id,received_at";
			values += (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
			values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

			return "INSERT INTO ais_vessel_static (" + keys + ") VALUES (" + values + ");\n";
		}


		std::string addBasestation(const JSON::JSON* data, const AIS::Message* msg) {
			if (!BS) return "";

			std::string keys = "";
			std::string values = "";

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_LAT:
				case AIS::KEY_LON:
				case AIS::KEY_MMSI:
					keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
					builder.to_string(values, p.Get());
					values += ",";
					break;
				}
			}
			keys += "msg_id,station_id,received_at";
			values += (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
			values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

			return "INSERT INTO ais_basestation (" + keys + ") VALUES (" + values + ");\n";
		}

		std::string addSARposition(const JSON::JSON* data, const AIS::Message* msg) {
			if (!SAR) return "";

			std::string keys = "";
			std::string values = "";

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_LAT:
				case AIS::KEY_LON:
				case AIS::KEY_ALT:
				case AIS::KEY_COURSE:
				case AIS::KEY_MMSI:
				case AIS::KEY_SPEED:
					keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
					builder.to_string(values, p.Get());
					values += ",";
					break;
				}
			}
			keys += "msg_id,station_id,received_at";
			values += (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
			values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

			return "INSERT INTO ais_sar_position (" + keys + ") VALUES (" + values + ");\n";
		}

		std::string addATON(const JSON::JSON* data, const AIS::Message* msg) {
			if (!ATON) return "";

			std::string keys = "";
			std::string values = "";

			for (const auto& p : data[0].getProperties()) {
				switch (p.Key()) {
				case AIS::KEY_LAT:
				case AIS::KEY_LON:
				case AIS::KEY_NAME:
				case AIS::KEY_TO_BOW:
				case AIS::KEY_TO_STERN:
				case AIS::KEY_TO_STARBOARD:
				case AIS::KEY_TO_PORT:
				case AIS::KEY_AID_TYPE:
				case AIS::KEY_MMSI:
					keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
					if (p.Get().isString()) {
						values += "\'" + escape(p.Get().getString()) + "\'";
					}
					else
						builder.to_string(values, p.Get());
					values += ",";
					break;
				}
			}
			keys += "msg_id,station_id,received_at";
			values += (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
			values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

			return "INSERT INTO ais_aton (" + keys + ") VALUES (" + values + ");\n";
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

			if (!filter.include(*msg)) return;

			if (MSGS) {
				sql += std::string("DROP TABLE IF EXISTS _id;\nWITH cte AS (INSERT INTO ais_message (mmsi, station_id, type, received_at, published_at, channel, signal_level, ppm) "
								   "VALUES (") +
					   std::to_string(msg->mmsi()) + ',' + std::to_string(station_id) + ',' + std::to_string(msg->type()) + ",\'" + std::string(Util::Convert::toTimestampStr(msg->getRxTimeUnix())) + "\', current_timestamp,\'" +
					   (char)msg->getChannel() + "\'," +
					   std::to_string(tag.level) + ',' + std::to_string(tag.ppm) +
					   ") RETURNING id)\nSELECT id INTO _id FROM cte;";
			}

			if (NMEA) {
				std::string keys = "msg_id,station_id,received_at";
				std::string values = (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + std::to_string(station_id);
				values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';
				for (auto s : msg->NMEA) {

					sql += "INSERT INTO ais_nmea (" + keys + ", nmea) VALUES (" + values + ",\'" + s + "\');\n";
				}
			}

			switch (msg->type()) {
			case 1:
			case 2:
			case 3:
			case 27:
				sql += addVesselPosition(data, msg);
				break;
			case 4:
				sql += addBasestation(data, msg);
				break;
			case 5:
				sql += addVesselStatic(data, msg);
				break;
			case 9:
				sql += addSARposition(data, msg);
				break;
			case 18:
				sql += addVesselPosition(data, msg);
				break;
			case 19:
				sql += addVesselPosition(data, msg);
				sql += addVesselStatic(data, msg);
				break;
			case 21:
				sql += addATON(data, msg);
				break;
			case 24:
				sql += addVesselStatic(data, msg);
				break;
			default:
				break;
			}
			// TO DO: types, etc
			for (const auto& p : data[0].getProperties()) {
				if (db_keys[p.Key()] != -1) {
					if (p.Get().isString()) {
						sql += "INSERT INTO ais_property (msg_id, key, value) VALUES (" + (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + "\'" + std::to_string(db_keys[p.Key()]) + "\',\'" + escape(p.Get().getString()) + "\');\n";
					}
					else {
						std::string temp;
						builder.to_string(temp, p.Get());
						temp = temp.substr(0, 20);
						sql += "INSERT INTO ais_property (msg_id, key, value) VALUES  (" + (MSGS ? std::string("(SELECT id FROM _id),") : std::string("NULL,")) + "\',\'" + temp + "\');\n";
					}
				}
			}


#endif
		}
		void setMap(int m) { builder.setMap(m); }

		Setting& Set(std::string option, std::string arg) {

			Util::Convert::toUpper(option);

			if (option == "CONN_STR")
				conn_string = arg;
			else if (option == "STATION_ID")
				station_id = Util::Parse::Integer(arg);
			else if (option == "NMEA")
				NMEA = Util::Parse::Switch(arg);
			else if (option == "VP")
				VP = Util::Parse::Switch(arg);
			else if (option == "VS")
				VS = Util::Parse::Switch(arg);
			else if (option == "MSGS")
				MSGS = Util::Parse::Switch(arg);
			else if (option == "BS")
				BS = Util::Parse::Switch(arg);
			else if (option == "ATON")
				ATON = Util::Parse::Switch(arg);
			else if (option == "SAR")
				SAR = Util::Parse::Switch(arg);
			else {
				filter.Set(option, arg);
			}
			return *this;
		}
	};
}
