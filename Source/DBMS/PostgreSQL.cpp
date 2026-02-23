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
	void PostgreSQL::post()
	{

		if (PQstatus(con) != CONNECTION_OK)
		{
			Warning() << "DBMS: Connection to PostgreSQL lost. Attempting to reset...";
			PQreset(con);

			if (PQstatus(con) != CONNECTION_OK)
			{
				Error() << "DBMS: Could not reset connection. Aborting post.";
				conn_fails++;
				return;
			}
			else
			{
				Warning() << "DBMS: Connection successfully reset.";
				conn_fails = 0;
			}
		}

		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			sql_trans = "DO $$\nDECLARE\n\tm_id INTEGER;\nBEGIN\n" + sql.str() + "\nEND $$;\n";
			sql.str("");
		}

		PGresult *res;
		res = PQexec(con, sql_trans.c_str());

		if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			Error() << "DBMS: Error writing PostgreSQL: " << PQerrorMessage(con);
			conn_fails = 1;
		}

		PQclear(res);
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

			for (int i = 0; !terminate && i < (conn_fails == 0 ? INTERVAL : 2) && sql.tellp() < 32768 * 16; i++)
			{
				SleepSystem(1000);
			}

			if (sql.tellp())
				post();

			if (terminate)
				break;

			if (MAX_FAILS < 1000 && conn_fails > MAX_FAILS)
			{
				Error() << "DBMS: max attemtps reached to connect to DBMS. Terminating.";
				StopRequest();
			}
		}
	}
#endif

	void PostgreSQL::setup()
	{
#ifdef HASPSQL

		db_keys.resize(AIS::KeyMap.size(), -1);
		Debug() << "Connecting to ProgreSQL database: \"" + conn_string + "\"\n";
		con = PQconnectdb(conn_string.c_str());

		if (con == nullptr || PQstatus(con) != CONNECTION_OK)
			throw std::runtime_error("DBMS: cannot open database :" + std::string(PQerrorMessage(con)));

		conn_fails = 0;

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
	std::string PostgreSQL::addVesselPosition(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!VP)
			return "";

		std::string keys = "";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;

			switch (p.Key())
			{
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
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

		return "\tINSERT INTO ais_vessel_pos (" + keys + ") VALUES (" + values + ");\n";
	}

	std::string PostgreSQL::addVessel(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!VD)
			return "";

		std::string keys = "";
		std::string set = "ON CONFLICT (mmsi) DO UPDATE SET ";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;

			switch (p.Key())
			{
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
			case AIS::KEY_LAT:
			case AIS::KEY_LON:
			case AIS::KEY_STATUS:
			case AIS::KEY_TURN:
			case AIS::KEY_PPM:
			case AIS::KEY_SIGNAL_POWER:
			case AIS::KEY_HEADING:
			case AIS::KEY_ALT:
			case AIS::KEY_AID_TYPE:
			case AIS::KEY_COURSE:
			case AIS::KEY_SPEED:
				keys += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";
				set += AIS::KeyMap[p.Key()][JSON_DICT_FULL] + "=EXCLUDED." + AIS::KeyMap[p.Key()][JSON_DICT_FULL] + ",";

				if (p.Get().isString())
				{
					values += "\'" + escape(p.Get().getString()) + "\'";
				}
				else
					builder.to_string(values, p.Get());
				values += ",";
				break;
			}
		}
		int type = 1 << msg->type();
		int ch = msg->getChannel() - 'A';
		if (ch < 0 || ch > 4)
			ch = 4;
		ch = 1 << ch;

		keys += "msg_id,station_id,received_at,count,msg_types,channels";
		set += "msg_id=EXCLUDED.msg_id,station_id=EXCLUDED.station_id,received_at=EXCLUDED.received_at,count=ais_vessel.count+1,msg_types=" + std::to_string(type) + "|ais_vessel.msg_types,channels=" + std::to_string(ch) + "|ais_vessel.channels";
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'' + ",1," + std::to_string(type) + "," + std::to_string(ch);

		return "\tINSERT INTO ais_vessel (" + keys + ") VALUES (" + values + ")" + set + "; \n";
	}

	std::string PostgreSQL::addVesselStatic(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!VS)
			return "";

		std::string keys = "";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;
			switch (p.Key())
			{
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
				if (p.Get().isString())
				{
					values += "\'" + escape(p.Get().getString()) + "\'";
				}
				else
					builder.to_string(values, p.Get());
				values += ",";
				break;
			}
		}
		keys += "msg_id,station_id,received_at";
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

		return "\tINSERT INTO ais_vessel_static (" + keys + ") VALUES (" + values + ");\n";
	}

	std::string PostgreSQL::addBasestation(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!BS)
			return "";

		std::string keys = "";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;
			switch (p.Key())
			{
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
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

		return "\tINSERT INTO ais_basestation (" + keys + ") VALUES (" + values + ");\n";
	}

	std::string PostgreSQL::addSARposition(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!SAR)
			return "";

		std::string keys = "";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;
			switch (p.Key())
			{
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
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

		return "\tINSERT INTO ais_sar_position (" + keys + ") VALUES (" + values + ");\n";
	}

	std::string PostgreSQL::addATON(const JSON::JSON *data, const AIS::Message *msg, const std::string &m, const std::string &s)
	{
		if (!ATON)
			return "";

		std::string keys = "";
		std::string values = "";

		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;
			switch (p.Key())
			{
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
				if (p.Get().isString())
				{
					values += "\'" + escape(p.Get().getString()) + "\'";
				}
				else
					builder.to_string(values, p.Get());
				values += ",";
				break;
			}
		}
		keys += "msg_id,station_id,received_at";
		values += m + ',' + s;
		values += ",\'" + Util::Convert::toTimestampStr(msg->getRxTimeUnix()) + '\'';

		return "\tINSERT INTO ais_aton (" + keys + ") VALUES (" + values + ");\n";
	}

	void PostgreSQL::Receive(const JSON::JSON *data, int len, TAG &tag)
	{

		const std::lock_guard<std::mutex> lock(queue_mutex);

		if (sql.tellp() > 32768 * 24)
		{
			Warning() << "DBMS: writing to database slow or failed, data lost.";
			sql.str("");
		}
		const AIS::Message *msg = (AIS::Message *)data[0].binary;

		if (!filter.include(*msg))
			return;

		std::string m_id = MSGS ? "m_id" : " NULL";
		std::string s_id = std::to_string(station_id ? station_id : msg->getStation());

		if (MSGS)
		{
			sql << "\tINSERT INTO ais_message (mmsi, station_id, type, received_at,channel, signal_level, ppm) "
				<< "VALUES (" << msg->mmsi() << ',' + s_id << ',' << msg->type() << ",\'" << Util::Convert::toTimestampStr(msg->getRxTimeUnix()) << "\',\'"
				<< (char)msg->getChannel() << "\'," << tag.level << ',' << tag.ppm
				<< ") RETURNING id INTO m_id;\n";
		}

		if (NMEA)
		{
			for (auto s : msg->NMEA)
			{

				sql << "\tINSERT INTO ais_nmea (msg_id,station_id,mmsi,received_at,nmea) VALUES (" << m_id << ',' << s_id << ',' << msg->mmsi() << ",\'" << Util::Convert::toTimestampStr(msg->getRxTimeUnix()) << "\',\'" << s << "\');\n";
			}
		}

		switch (msg->type())
		{
		case 1:
		case 2:
		case 3:
		case 27:
			sql << addVesselPosition(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 4:
			sql << addBasestation(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 5:
			sql << addVesselStatic(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 9:
			sql << addSARposition(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 18:
			sql << addVesselPosition(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 19:
			sql << addVesselPosition(data, msg, m_id, s_id);
			sql << addVesselStatic(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 21:
			sql << addATON(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		case 24:
			sql << addVesselStatic(data, msg, m_id, s_id);
			sql << addVessel(data, msg, m_id, s_id);
			break;
		default:
			break;
		}
		// TO DO: types, etc
		for (const auto &p : data[0].getProperties())
		{
			if (p.Key() >= 0 && p.Key() < db_keys.size() && db_keys[p.Key()] != -1)
			{
				if (p.Get().isString())
				{
					sql << "INSERT INTO ais_property (msg_id, key, value) VALUES (" << m_id << "\'" << db_keys[p.Key()] << "\',\'" << escape(p.Get().getString()) << "\');\n";
				}
				else
				{
					std::string temp;
					builder.to_string(temp, p.Get());
					temp = temp.substr(0, 20);
					sql << "INSERT INTO ais_property (msg_id, key, value) VALUES  (" << m_id << "\',\'" + temp + "\');\n";
				}
			}
		}
		sql << "\n";
	}
#endif

	Setting &PostgreSQL::Set(std::string option, std::string arg)
	{

		Util::Convert::toUpper(option);

		if (option == "CONN_STR")
			conn_string = arg;
		else if (option == "GROUPS_IN")
			StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
		else if (option == "STATION_ID")
			station_id = Util::Parse::Integer(arg);
		else if (option == "INTERVAL")
			INTERVAL = Util::Parse::Integer(arg, 5, 1800);
		else if (option == "MAX_FAILS")
			MAX_FAILS = Util::Parse::Integer(arg);
		else if (option == "NMEA")
			NMEA = Util::Parse::Switch(arg);
		else if (option == "VP")
			VP = Util::Parse::Switch(arg);
		else if (option == "V")
			VD = Util::Parse::Switch(arg);
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
		else
		{
			filter.SetOption(option, arg);
		}
		return *this;
	}
}
