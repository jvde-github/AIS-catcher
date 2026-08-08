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

#include "SQLite.h"
#include "Logger.h"

#ifdef HASSQLITE

namespace IO
{
	// $1 and ?1 are both one-based: swapping the sigil is the whole dialect difference
	static std::string toSQLiteParams(std::string sql)
	{
		for (auto &c : sql)
			if (c == '$')
				c = '?';
		return sql;
	}

	SQLite::~SQLite()
	{
		// before finalizing anything: the worker calls back into this object
		stopWorker();
		closeDB();
	}

	void SQLite::connectDB()
	{
		Debug() << "Opening SQLite database: \"" + conn_string + "\"";

		if (sqlite3_open_v2(conn_string.c_str(), &db,
							SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
		{
			stats.connect_fail++;
			std::string err = db ? sqlite3_errmsg(db) : "out of memory";
			sqlite3_close(db);
			db = nullptr;
			throw std::runtime_error("DBMS: cannot open database \"" + conn_string + "\": " + err);
		}

		// per connection and off by default; without it ON DELETE CASCADE silently does nothing
		run("PRAGMA foreign_keys = ON");

		// WAL keeps readers going; NORMAL trades an fsync per commit for one per checkpoint
		run("PRAGMA journal_mode = WAL");
		run("PRAGMA synchronous = NORMAL");
		run("PRAGMA busy_timeout = 5000");
	}

	void SQLite::closeDB()
	{
		for (int i = 0; i < ST_COUNT; i++)
		{
			if (stmt[i])
			{
				sqlite3_finalize(stmt[i]);
				stmt[i] = nullptr;
			}
		}

		if (db)
		{
			sqlite3_close(db);
			db = nullptr;
		}
	}

	bool SQLite::run(const char *cmd)
	{
		char *err = nullptr;
		if (sqlite3_exec(db, cmd, nullptr, nullptr, &err) == SQLITE_OK)
			return true;

		Error() << "DBMS: " << cmd << " failed: " << (err ? err : "unknown error");
		sqlite3_free(err);
		return false;
	}

	bool SQLite::prepareAll()
	{
		for (int i = 0; i < ST_COUNT; i++)
		{
			const std::string sql = toSQLiteParams(sqlTemplate(i));

			if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt[i], nullptr) != SQLITE_OK)
			{
				Error() << "DBMS: cannot prepare statement " << i << ": " << sqlite3_errmsg(db);
				return false;
			}
		}

		return true;
	}

	bool SQLite::bindAndStep(int st, const std::vector<const char *> &params)
	{
		sqlite3_stmt *s = stmt[st];

		// no clear_bindings: every call rebinds all parameters, nulls included
		sqlite3_reset(s);

		for (size_t i = 0; i < params.size(); i++)
		{
			// text plus column affinity; the strings outlive the step, hence SQLITE_STATIC
			int rc = params[i] ? sqlite3_bind_text(s, (int)i + 1, params[i], -1, SQLITE_STATIC)
							   : sqlite3_bind_null(s, (int)i + 1);

			if (rc != SQLITE_OK)
			{
				Error() << "DBMS: bind failed on statement " << st << ": " << sqlite3_errmsg(db);
				return false;
			}
		}

		if (sqlite3_step(s) != SQLITE_DONE)
		{
			Error() << "DBMS: statement " << st << " failed: " << sqlite3_errmsg(db);
			return false;
		}

		return true;
	}

	bool SQLite::exec(int st, const std::vector<const char *> &params)
	{
		return bindAndStep(st, params);
	}

	bool SQLite::execReturningId(int st, const std::vector<const char *> &params, std::string &id)
	{
		if (!bindAndStep(st, params))
			return false;

		id = std::to_string((long long)sqlite3_last_insert_rowid(db));
		return true;
	}
}

#endif
