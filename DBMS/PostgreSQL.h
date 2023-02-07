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
#include <pqxx/pqxx>
#endif

#include "Stream.h"
#include "AIS.h"
#include "Utilities.h"


#include "JSON/JSON.h"
#include "JSON/StringBuilder.h"

namespace IO {

	class PostgreSQL : public StreamIn<JSON::JSON>, public Setting {
		JSON::StringBuilder builder;
		std::string json;
		AIS::Filter filter;
#ifdef HASPSQL
		pqxx::connection* con = NULL;
		std::vector<int> db_keys;
#endif
	public:
		PostgreSQL() : builder(&AIS::KeyMap, JSON_DICT_FULL) {}
		~PostgreSQL() { 
#ifdef HASPSQL
			delete con; 
#endif
		}

		void setup() {
#ifdef HASPSQL
			try {
				db_keys.resize(AIS::KeyMap.size(), -1);
				std::cerr << "Starting ProgreSQL database  \"ais\n";
				con = new pqxx::connection("dbname=ais");

				pqxx::work txn(*con);

				// figure out which properties we are asked to file
				// TO DO: save key as reference id not string
				// TO DO: populate additional tables collecting the status of a vessel
				// TO DO: staging

				pqxx::result r = txn.exec("SELECT key_id, key_str FROM ais_keys");

				for (pqxx::result::const_iterator row = r.begin(); row != r.end(); ++row) {
					int id = row[0].as<int>();
					std::string name = row[1].as<std::string>();
					bool found = false;
					for (int i = 0; i < db_keys.size(); i++) {
						if (AIS::KeyMap[i][JSON_DICT_FULL] == name) {
							db_keys[i] = id;
							found = true;
						}
					}
					if (!found)
						throw std::runtime_error("DBMS: The key \"" + name + "\" defined in ais_keys not availble.");
					std::cout << id << ": " << name << std::endl;
				}
			}
			catch (const std::exception& e) {
				throw std::runtime_error("DBMS: cannot open database :" + std::string(e.what()));
			}
#else
			throw std::runtime_error("DBMS: no support for PostgeSQL build in.");
#endif
		}
		void Start(){};
		void Receive(const JSON::JSON* data, int len, TAG& tag) {
#ifdef HASPSQL

			const AIS::Message* msg = (AIS::Message*)data[0].binary;

			try {
				std::string sql = std::string("DROP TABLE IF EXISTS _id;\nWITH cte AS (INSERT INTO ais_message (mmsi, type, timestamp, sender, channel, signal_level, ppm) "
											  "VALUES (") +
								  std::to_string(msg->mmsi()) + ',' + std::to_string(msg->type()) + ',' + std::to_string(msg->getRxTimeUnix()) + ',' +
								  std::to_string(0) + ",\'" + (char)msg->getChannel() + "\'," +
								  std::to_string(tag.level) + ',' + std::to_string(tag.ppm) +
								  ") RETURNING id)\nSELECT id INTO _id FROM cte;";


				// TO DO: remove, can be selected from JSON properties
				for (auto s : msg->NMEA) {
					sql += "INSERT INTO ais_nmea (id, nmea) VALUES ((SELECT id FROM _id),\'" + s + "\');\n";
				}

				// TO DO: types, etc
				for (const auto& p : data[0].getProperties()) {
					if (db_keys[p.Key()] != -1) {
						std::string temp;
						builder.to_string(temp, p.Get());
						temp = temp.substr(0, 20);
						std::cerr << AIS::KeyMap[p.Key()][JSON_DICT_FULL] << " " << temp << std::endl;
						sql += "INSERT INTO ais_property (id, key, value) VALUES ((SELECT id FROM _id),\'" + AIS::KeyMap[p.Key()][JSON_DICT_FULL] + "\',\'" + temp + "\');\n";
					}
				}

				pqxx::work txn(*con);
				txn.exec(sql);
				txn.commit();
			catch (const std::exception& e) {
				std::cerr << "DBMS: Error writing PostgreSQL: " << e.what() << std::endl;
			}
#endif
		}
		void setMap(int m) { builder.setMap(m); }

		Setting& Set(std::string option, std::string arg) {
			filter.Set(option, arg);
			return *this;
		}
	};
}
