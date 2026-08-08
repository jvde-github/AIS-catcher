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

#include <libpq-fe.h>

#include "DatabaseOutput.h"

namespace IO
{

	class PostgreSQL : public DatabaseOutput
	{
		PGconn *con = nullptr;
		bool prepared = false;

		bool run(const char *cmd);
		bool execPrepared(int st, const std::vector<const char *> &params, std::string *id);
		void initSession();
		void closeDB();

	protected:
		void connectDB() override;
		bool ensureConnection() override;
		bool prepareAll() override;

		bool exec(int st, const std::vector<const char *> &params) override;
		bool execReturningId(int st, const std::vector<const char *> &params, std::string &id) override;

		void collectVesselsSince(const std::string &since, std::set<uint32_t> &out) override;
		long execDelete(const char *sql, const char *param) override;

		bool begin() override { return run("BEGIN"); }
		bool commit() override { return run("COMMIT"); }
		bool rollback() override { return run("ROLLBACK"); }

	public:
		PostgreSQL() : DatabaseOutput("PostgreSQL") { conn_string = "dbname=ais"; }
		~PostgreSQL();
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
