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

#include "PostgreSQL.h"
#include "Logger.h"

#ifdef HASPSQL

namespace IO
{
	static const char *statement_name[DatabaseOutput::ST_COUNT] = {"ais_msg", "ais_pos", "ais_sta", "ais_state", "ais_stats"};

	PostgreSQL::~PostgreSQL()
	{
		// before PQfinish: the worker calls back into this object
		stopWorker();
		closeDB();
	}

	void PostgreSQL::connectDB()
	{
		Debug() << "Connecting to PostgreSQL database: \"" + conn_string + "\"";
		con = PQconnectdb(conn_string.c_str());

		if (con == nullptr || PQstatus(con) != CONNECTION_OK)
		{
			stats.connect_fail++;
			throw std::runtime_error("DBMS: cannot open database :" + std::string(PQerrorMessage(con)));
		}

		initSession();
	}

	void PostgreSQL::initSession()
	{
		// timestamps are bound as zone-naive UTC strings; the session must be at UTC
		run("SET TIME ZONE 'UTC'");
	}

	void PostgreSQL::closeDB()
	{
		if (con != nullptr)
		{
			PQfinish(con);
			con = nullptr;
		}
	}

	bool PostgreSQL::ensureConnection()
	{
		if (PQstatus(con) != CONNECTION_OK)
		{
			stats.connected = 0;
			Warning() << "DBMS: connection to PostgreSQL lost, attempting to reset";
			PQreset(con);

			if (PQstatus(con) != CONNECTION_OK)
			{
				Error() << "DBMS: could not reset connection, aborting post";
				stats.connect_fail++;
				return false;
			}

			Warning() << "DBMS: connection successfully reset";
			stats.connected = 1;
			stats.connect_ok++;
			stats.reconnects++;

			// session state - prepared statements, timezone - went away with it
			prepared = false;
			initSession();
		}

		return prepared || prepareAll();
	}

	bool PostgreSQL::prepareAll()
	{
		// a partial earlier attempt left names registered; re-preparing one fails (42P05)
		run("DEALLOCATE ALL");

		for (int i = 0; i < ST_COUNT; i++)
		{
			std::string sql = sqlTemplate(i);
			if (i == ST_MESSAGE)
				sql += " RETURNING id";

			PGresult *res = PQprepare(con, statement_name[i], sql.c_str(), 0, nullptr);
			bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;

			if (!ok)
				Error() << "DBMS: cannot prepare " << statement_name[i] << ": " << PQerrorMessage(con);

			PQclear(res);

			if (!ok)
				return false;
		}

		prepared = true;
		return true;
	}

	bool PostgreSQL::run(const char *cmd)
	{
		PGresult *res = PQexec(con, cmd);
		bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;

		if (!ok)
			Error() << "DBMS: " << cmd << " failed: " << PQerrorMessage(con);

		PQclear(res);
		return ok;
	}

	bool PostgreSQL::execPrepared(int st, const std::vector<const char *> &params, std::string *id)
	{
		PGresult *res = PQexecPrepared(con, statement_name[st], (int)params.size(),
									   params.data(), nullptr, nullptr, 0);
		const bool ok = id ? PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0
						   : PQresultStatus(res) == PGRES_COMMAND_OK;

		if (!ok)
			Error() << "DBMS: " << statement_name[st] << " failed: " << PQerrorMessage(con);
		else if (id)
			*id = PQgetvalue(res, 0, 0);

		PQclear(res);
		return ok;
	}

	bool PostgreSQL::exec(int st, const std::vector<const char *> &params)
	{
		return execPrepared(st, params, nullptr);
	}

	bool PostgreSQL::execReturningId(int st, const std::vector<const char *> &params, std::string &id)
	{
		return execPrepared(st, params, &id);
	}
}

#endif
