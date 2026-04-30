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

#include "AIS-catcher.h"
#include "DBMS/PostgreSQL.h"

namespace IO
{

#ifdef HASPSQL

	// Key sets for each table
	static const int keys_vessel_pos[] = {AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_MMSI, AIS::KEY_STATUS, AIS::KEY_TURN, AIS::KEY_HEADING, AIS::KEY_COURSE, AIS::KEY_SPEED};
	static const int keys_vessel_static[] = {AIS::KEY_MMSI, AIS::KEY_IMO, AIS::KEY_SHIPNAME, AIS::KEY_CALLSIGN, AIS::KEY_TO_BOW, AIS::KEY_TO_STERN, AIS::KEY_TO_STARBOARD, AIS::KEY_TO_PORT, AIS::KEY_DRAUGHT, AIS::KEY_SHIPTYPE, AIS::KEY_DESTINATION, AIS::KEY_ETA};
	static const int keys_basestation[] = {AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_MMSI};
	static const int keys_sar[] = {AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_ALT, AIS::KEY_COURSE, AIS::KEY_MMSI, AIS::KEY_SPEED};
	static const int keys_aton[] = {AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_NAME, AIS::KEY_TO_BOW, AIS::KEY_TO_STERN, AIS::KEY_TO_STARBOARD, AIS::KEY_TO_PORT, AIS::KEY_AID_TYPE, AIS::KEY_MMSI};
	static const int keys_vessel[] = {AIS::KEY_MMSI, AIS::KEY_IMO, AIS::KEY_SHIPNAME, AIS::KEY_CALLSIGN, AIS::KEY_TO_BOW, AIS::KEY_TO_STERN, AIS::KEY_TO_STARBOARD, AIS::KEY_TO_PORT, AIS::KEY_DRAUGHT, AIS::KEY_SHIPTYPE, AIS::KEY_DESTINATION, AIS::KEY_ETA, AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_STATUS, AIS::KEY_TURN, AIS::KEY_PPM, AIS::KEY_SIGNAL_POWER, AIS::KEY_HEADING, AIS::KEY_ALT, AIS::KEY_AID_TYPE, AIS::KEY_COURSE, AIS::KEY_SPEED};

	void PostgreSQL::post()
	{
		if (PQstatus(con) != CONNECTION_OK)
		{
			stats.connected = 0;
			Warning() << "DBMS: Connection to PostgreSQL lost. Attempting to reset...";
			PQreset(con);

			if (PQstatus(con) != CONNECTION_OK)
			{
				Error() << "DBMS: Could not reset connection. Aborting post.";
				conn_fails++;
				stats.connect_fail++;
				return;
			}
			else
			{
				Warning() << "DBMS: Connection successfully reset.";
				conn_fails = 0;
				stats.connected = 1;
				stats.connect_ok++;
				stats.reconnects++;
			}
		}

		std::vector<QueuedEntry> batch;
		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			batch.swap(message_queue);
		}

		if (batch.empty())
			return;

		PGresult *res = PQexec(con, "BEGIN");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			Error() << "DBMS: BEGIN failed: " << PQerrorMessage(con);
			PQclear(res);
			conn_fails++;
			return;
		}
		PQclear(res);

		bool ok = true;

		for (const auto &entry : batch)
		{
			if (!ok)
				break;

			std::string msg_id;

			// 1. Insert ais_message
			if (MSGS)
			{
				const char *params[] = {
					entry.mmsi.c_str(), entry.station_id.c_str(), entry.msg_type.c_str(),
					entry.timestamp.c_str(), entry.channel.c_str(), entry.level.c_str(), entry.ppm.c_str()};

				res = PQexecParams(con,
								   "INSERT INTO ais_message (mmsi, station_id, type, received_at, channel, signal_level, ppm) "
								   "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
								   7, nullptr, params, nullptr, nullptr, 0);

				if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
				{
					msg_id = PQgetvalue(res, 0, 0);
					for (const char *p : params)
						stats.bytes_out += strlen(p);
				}
				else
				{
					Error() << "DBMS: ais_message insert failed: " << PQerrorMessage(con);
					ok = false;
				}
				PQclear(res);
			}

			const char *msg_id_ptr = MSGS && !msg_id.empty() ? msg_id.c_str() : nullptr;

			// 2. Insert ais_nmea
			if (ok && NMEA)
			{
				for (const auto &nmea : entry.nmea_lines)
				{
					const char *params[] = {
						msg_id_ptr, entry.station_id.c_str(), entry.mmsi.c_str(),
						entry.timestamp.c_str(), nmea.c_str()};

					res = PQexecParams(con,
									   "INSERT INTO ais_nmea (msg_id, station_id, mmsi, received_at, nmea) "
									   "VALUES ($1, $2, $3, $4, $5)",
									   5, nullptr, params, nullptr, nullptr, 0);

					if (PQresultStatus(res) != PGRES_COMMAND_OK)
					{
						Error() << "DBMS: ais_nmea insert failed: " << PQerrorMessage(con);
						ok = false;
					}
					else
					{
						stats.bytes_out += nmea.length();
					}
					PQclear(res);
					if (!ok)
						break;
				}
			}

			// 3. Table-specific inserts
			if (ok)
			{
				switch (entry.msg_type_int)
				{
				case 1:
				case 2:
				case 3:
				case 18:
				case 27:
					if (VP)
						ok = execTableInsert("ais_vessel_pos", entry, keys_vessel_pos, sizeof(keys_vessel_pos) / sizeof(int), msg_id_ptr);
					break;
				case 4:
					if (BS)
						ok = execTableInsert("ais_basestation", entry, keys_basestation, sizeof(keys_basestation) / sizeof(int), msg_id_ptr);
					break;
				case 5:
				case 24:
					if (VS)
						ok = execTableInsert("ais_vessel_static", entry, keys_vessel_static, sizeof(keys_vessel_static) / sizeof(int), msg_id_ptr);
					break;
				case 9:
					if (SAR)
						ok = execTableInsert("ais_sar_position", entry, keys_sar, sizeof(keys_sar) / sizeof(int), msg_id_ptr);
					break;
				case 19:
					if (VP)
						ok = execTableInsert("ais_vessel_pos", entry, keys_vessel_pos, sizeof(keys_vessel_pos) / sizeof(int), msg_id_ptr);
					if (ok && VS)
						ok = execTableInsert("ais_vessel_static", entry, keys_vessel_static, sizeof(keys_vessel_static) / sizeof(int), msg_id_ptr);
					break;
				case 21:
				case 28:
					if (ATON)
						ok = execTableInsert("ais_aton", entry, keys_aton, sizeof(keys_aton) / sizeof(int), msg_id_ptr);
					break;
				default:
					break;
				}
			}

			// 4. Vessel upsert (requires identifiable mmsi)
			if (ok && VD && entry.mmsi != "0")
				ok = execVessel(entry, msg_id_ptr);

			// 5. Property inserts
			if (ok)
			{
				for (const auto &prop : entry.properties)
				{
					const char *params[] = {msg_id_ptr, prop.db_key.c_str(), prop.value.c_str()};

					res = PQexecParams(con,
									   "INSERT INTO ais_property (msg_id, key, value) VALUES ($1, $2, $3)",
									   3, nullptr, params, nullptr, nullptr, 0);

					if (PQresultStatus(res) != PGRES_COMMAND_OK)
					{
						Error() << "DBMS: ais_property insert failed: " << PQerrorMessage(con);
						ok = false;
					}
					PQclear(res);
					if (!ok)
						break;
				}
			}
		}

		res = PQexec(con, ok ? "COMMIT" : "ROLLBACK");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			Error() << "DBMS: " << (ok ? "COMMIT" : "ROLLBACK") << " failed: " << PQerrorMessage(con);
		PQclear(res);

		if (!ok)
			conn_fails++;
		else
			conn_fails = 0;
	}

	bool PostgreSQL::execTableInsert(const char *table, const QueuedEntry &entry, const int *keys, int nkeys, const char *msg_id)
	{
		std::string cols, placeholders;
		std::vector<const char *> params;
		int idx = 1;

		for (const auto &kv : entry.kvs)
		{
			for (int i = 0; i < nkeys; i++)
			{
				if (kv.key == keys[i])
				{
					if (idx > 1)
					{
						cols += ',';
						placeholders += ',';
					}
					cols += AIS::KeyMap[kv.key][JSON_DICT_FULL];
					placeholders += "$" + std::to_string(idx++);
					params.push_back(kv.value.c_str());
					break;
				}
			}
		}

		if (idx > 1)
		{
			cols += ',';
			placeholders += ',';
		}

		cols += "msg_id,station_id,received_at";
		placeholders += "$" + std::to_string(idx) + ",$" + std::to_string(idx + 1) + ",$" + std::to_string(idx + 2);
		params.push_back(msg_id);
		params.push_back(entry.station_id.c_str());
		params.push_back(entry.timestamp.c_str());

		std::string query = "INSERT INTO " + std::string(table) + " (" + cols + ") VALUES (" + placeholders + ")";

		PGresult *res = PQexecParams(con, query.c_str(), (int)params.size(), nullptr, params.data(), nullptr, nullptr, 0);
		bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
		if (!success)
			Error() << "DBMS: " << table << " insert failed: " << PQerrorMessage(con);
		PQclear(res);
		return success;
	}

	bool PostgreSQL::execVessel(const QueuedEntry &entry, const char *msg_id)
	{
		std::string cols, placeholders, on_conflict;
		std::vector<const char *> params;
		int idx = 1;
		bool first = true;

		for (const auto &kv : entry.kvs)
		{
			for (int i = 0; i < (int)(sizeof(keys_vessel) / sizeof(keys_vessel[0])); i++)
			{
				if (kv.key == keys_vessel[i])
				{
					if (!first)
					{
						cols += ',';
						placeholders += ',';
						on_conflict += ',';
					}
					first = false;

					const std::string &name = AIS::KeyMap[kv.key][JSON_DICT_FULL];
					cols += name;
					placeholders += "$" + std::to_string(idx++);
					on_conflict += name + "=EXCLUDED." + name;
					params.push_back(kv.value.c_str());
					break;
				}
			}
		}

		if (!first)
		{
			cols += ',';
			placeholders += ',';
			on_conflict += ',';
		}

		cols += "msg_id,station_id,received_at,count,msg_types,channels";

		std::string p_mid = "$" + std::to_string(idx);
		std::string p_sid = "$" + std::to_string(idx + 1);
		std::string p_ts = "$" + std::to_string(idx + 2);
		std::string p_types = "$" + std::to_string(idx + 3);
		std::string p_channels = "$" + std::to_string(idx + 4);

		placeholders += p_mid + "," + p_sid + "," + p_ts + ",1," + p_types + "," + p_channels;

		params.push_back(msg_id);
		params.push_back(entry.station_id.c_str());
		params.push_back(entry.timestamp.c_str());
		params.push_back(entry.type_bit.c_str());
		params.push_back(entry.channel_bit.c_str());

		on_conflict += "msg_id=EXCLUDED.msg_id,station_id=EXCLUDED.station_id,received_at=EXCLUDED.received_at,"
					   "count=ais_vessel.count+1,"
					   "msg_types=EXCLUDED.msg_types|ais_vessel.msg_types,"
					   "channels=EXCLUDED.channels|ais_vessel.channels";

		std::string query = "INSERT INTO ais_vessel (" + cols + ") VALUES (" + placeholders + ") ON CONFLICT (mmsi) DO UPDATE SET " + on_conflict;

		PGresult *res = PQexecParams(con, query.c_str(), (int)params.size(), nullptr, params.data(), nullptr, nullptr, 0);
		bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
		if (!success)
			Error() << "DBMS: ais_vessel upsert failed: " << PQerrorMessage(con);
		PQclear(res);
		return success;
	}
#endif
	PostgreSQL::~PostgreSQL()
	{
#ifdef HASPSQL
		if (running)
		{

			running = false;
			terminate = true;
			run_thread.join();

			Debug() << "DBMS: stop thread and database closed.";
		}
		if (con != nullptr)
			PQfinish(con);
#endif
	}

#ifdef HASPSQL

	void PostgreSQL::process()
	{
		while (!terminate)
		{
			for (int i = 0; !terminate && i < (conn_fails == 0 ? INTERVAL : 2) && message_queue.size() < MAX_QUEUE_SIZE / 2; i++)
			{
				SleepSystem(1000);
			}

			if (!message_queue.empty())
				post();

			if (terminate)
				break;

			if (MAX_FAILS < 1000 && conn_fails > MAX_FAILS)
			{
				Error() << "DBMS: max attempts reached to connect to DBMS. Terminating.";
				StopRequest();
			}
		}
	}
#endif

	void PostgreSQL::setup()
	{
#ifdef HASPSQL

		db_keys.resize(AIS::KEY_COUNT, -1);
		Debug() << "Connecting to ProgreSQL database: \"" + conn_string + "\"\n";
		con = PQconnectdb(conn_string.c_str());

		if (con == nullptr || PQstatus(con) != CONNECTION_OK)
		{
			stats.connect_fail++;
			throw std::runtime_error("DBMS: cannot open database :" + std::string(PQerrorMessage(con)));
		}

		conn_fails = 0;
		stats.connected = 1;
		stats.connect_ok++;

		PGresult *res = PQexec(con, "SELECT key_id, key_str FROM ais_keys");
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			throw std::runtime_error("DBMS: error fetching ais_keys table: " + std::string(PQerrorMessage(con)));
		}

		int key_count = 0;
		for (int row = 0; row < PQntuples(res); row++)
		{
			int id = atoi(PQgetvalue(res, row, 0));
			std::string name = PQgetvalue(res, row, 1);
			bool found = false;
			for (int i = 0; i < db_keys.size(); i++)
			{
				if (AIS::KeyMap[i][JSON_DICT_FULL] == name)
				{
					db_keys[i] = id;
					found = true;
					key_count++;
					break;
				}
			}
			if (!found)
				throw std::runtime_error("DBMS: The requested key \"" + name + "\" in ais_keys is not defined.");
		}

		if (key_count > 0 && !MSGS)
		{
			Info() << "DBMS: no messages logged in combination with property logging. MSGS ON auto activated.";
			MSGS = true;
		}

		if (!running)
		{

			running = true;
			terminate = false;

			run_thread = std::thread(&PostgreSQL::process, this);

			Debug() << "DBMS: start thread, filter: " << Util::Convert::toString(filter.isOn());
			
			if (filter.isOn())
				Debug() << ", Allowed: " << filter.getAllowed();

			Debug() << ", V " << Util::Convert::toString(VD)
					<< ", VP " << Util::Convert::toString(VP)
					<< ", MSGS " << Util::Convert::toString(MSGS)
					<< ", VS " << Util::Convert::toString(VS)
					<< ", BS " << Util::Convert::toString(BS)
					<< ", SAR " << Util::Convert::toString(SAR)
					<< ", ATON " << Util::Convert::toString(ATON)
					<< ", NMEA " << Util::Convert::toString(NMEA);
		}
#else
		throw std::runtime_error("DBMS: no support for PostgeSQL build in.");
#endif
	}

#ifdef HASPSQL

	static std::string valueToParam(const JSON::Value &v)
	{
		if (v.isString())
			return v.getString();
		if (v.isInt())
			return std::to_string(v.getInt());
		if (v.isFloat())
			return std::to_string(v.getFloat());
		if (v.isBool())
			return v.getBool() ? "true" : "false";
		return "";
	}

	void PostgreSQL::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		const std::lock_guard<std::mutex> lock(queue_mutex);

		if (message_queue.size() > MAX_QUEUE_SIZE)
		{
			Warning() << "DBMS: writing to database slow or failed, data lost.";
			message_queue.clear();
		}

		const JSON::JSON &json = data[0];
		const AIS::Message &msg = *(AIS::Message *)json.binary;

		if (!filter.include(msg))
			return;

		QueuedEntry entry;
		entry.mmsi = std::to_string(msg.mmsi());
		entry.station_id = std::to_string(station_id ? station_id : msg.getStation());
		entry.msg_type = std::to_string(msg.type());
		entry.msg_type_int = msg.type();
		entry.timestamp = Util::Convert::toTimestampStr(msg.getRxTimeUnix());
		entry.channel = std::string(1, (char)msg.getChannel());
		entry.level = std::to_string(tag.level);
		entry.ppm = std::to_string(tag.ppm);

		if (NMEA)
		{
			for (const auto &s : msg.sentences())
				entry.nmea_lines.push_back(s);
		}

		// Extract JSON key-value pairs for table inserts
		for (const auto &p : json.getMembers())
		{
			int k = p.Key();
			if (k < 0 || k >= AIS::KEY_COUNT)
				continue;

			std::string val = valueToParam(p.Get());
			if (val.empty() && !p.Get().isString())
				continue;

			entry.kvs.push_back({k, std::move(val)});
		}

		// Properties for ais_property table
		for (const auto &p : json.getMembers())
		{
			if (p.Key() >= 0 && p.Key() < (int)db_keys.size() && db_keys[p.Key()] != -1)
			{
				std::string val = valueToParam(p.Get());
				if (val.size() > 20)
					val.resize(20);
				entry.properties.push_back({std::to_string(db_keys[p.Key()]), std::move(val)});
			}
		}

		// Compute type/channel bitmasks for ais_vessel upsert
		int type = 1 << msg.type();
		int ch = msg.getChannel() - 'A';
		if (ch < 0 || ch > 4)
			ch = 4;
		ch = 1 << ch;
		entry.type_bit = std::to_string(type);
		entry.channel_bit = std::to_string(ch);

		message_queue.push_back(std::move(entry));
	}
#endif

	Setting &PostgreSQL::SetKey(AIS::Keys key, const std::string &arg)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_CONN_STR:
			conn_string = arg;
			break;
		case AIS::KEY_SETTING_GROUPS_IN:
			StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
			break;
		case AIS::KEY_SETTING_STATION_ID:
			station_id = Util::Parse::Integer(arg);
			break;
		case AIS::KEY_SETTING_INTERVAL:
			INTERVAL = Util::Parse::Integer(arg, 5, 1800);
			break;
		case AIS::KEY_SETTING_MAX_FAILS:
			MAX_FAILS = Util::Parse::Integer(arg);
			break;
		case AIS::KEY_SETTING_NMEA:
			NMEA = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_MSGS:
			MSGS = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_VP:
			VP = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_V:
			VD = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_VS:
			VS = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_BS:
			BS = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_ATON:
			ATON = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_SAR:
			SAR = Util::Parse::Switch(arg);
			break;
		default:
			if (!setOptionKey(key, arg) && !filter.SetOptionKey(key, arg))
				throw std::runtime_error("DBMS: unknown option.");
			break;
		}
		return *this;
	}
}
