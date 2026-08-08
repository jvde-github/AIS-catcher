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

#ifdef HASSQLITE

#include <sqlite3.h>

#include "DatabaseOutput.h"

namespace IO
{

	class SQLite : public DatabaseOutput
	{
		sqlite3 *db = nullptr;
		sqlite3_stmt *stmt[ST_COUNT] = {nullptr};

		bool run(const char *cmd);
		bool bindAndStep(int st, const std::vector<const char *> &params);
		void closeDB();

	protected:
		void connectDB() override;

		// a local file needs no reconnect, and the statements outlive the loop
		bool ensureConnection() override { return db != nullptr; }
		bool prepareAll() override;

		bool exec(int st, const std::vector<const char *> &params) override;
		bool execReturningId(int st, const std::vector<const char *> &params, std::string &id) override;

		void collectVesselsSince(const std::string &since, std::set<uint32_t> &out) override;
		long execDelete(const char *sql, const char *param) override;

		bool begin() override { return run("BEGIN"); }
		bool commit() override { return run("COMMIT"); }
		bool rollback() override { return run("ROLLBACK"); }

	public:
		SQLite() : DatabaseOutput("SQLite") { conn_string = "ais.db"; }
		~SQLite();
	};
}

#else // HASSQLITE

namespace IO
{
	class SQLite : public OutputUnavailable
	{
	public:
		SQLite() : OutputUnavailable("SQLite", "HASSQLITE") {}
	};
}

#endif // HASSQLITE
