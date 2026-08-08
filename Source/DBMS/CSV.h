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

#include <fstream>
#include <map>

#include "DatabaseOutput.h"
#include "SlotTable.h"

namespace IO
{

	// Same tables as files: three daily-rotating append logs, plus state and
	// stats snapshots held in memory and rewritten whole on each flush.
	class CSV : public DatabaseOutput
	{
		static const int N_COLUMNS = N_POSITION + N_STATIC;

		struct StateRow
		{
			char first_seen[24];
			char received_at[24];
			char station_id[12];
			char level[16];
			char ppm[16];
			char col[N_COLUMNS][24];
			int count;
			int msg_types;
			int channels;
		};

		std::string dir;
		std::ofstream log[3]; // ST_MESSAGE, ST_POSITION, ST_STATIC
		std::string log_day[3];
		long long next_id = 0;

		std::time_t day_stamp = -1; // days since epoch of day_str
		std::string day_str;

		SlotTable<StateRow, uint32_t> state;
		bool state_dirty = false;
		size_t state_bytes = 0; // seeds the reserve

		// keyed on bucket so a partial hour is replaced, not appended twice
		std::map<std::string, std::string> stats_rows;
		bool stats_dirty = false;

		static void escape(std::string &out, const char *v);
		static std::string join(const std::vector<const char *> &params);
		static std::string header(int st);

		const std::string &today();
		bool openLog(int st, const std::string &day);
		bool appendLine(int st, const std::string &line);
		void mergeState(const std::vector<const char *> &params);
		void writeSnapshot(const char *name, const std::string &content);
		void writeStateFile();
		void writeStatsFile();
		void closeDB();

	protected:
		void connectDB() override;

		bool ensureConnection() override { return !dir.empty(); }
		bool prepareAll() override { return true; }

		bool exec(int st, const std::vector<const char *> &params) override;
		bool execReturningId(int st, const std::vector<const char *> &params, std::string &id) override;

		// no transactions: the base skips failed entries instead of replaying
		bool begin() override { return true; }
		bool commit() override { return true; }
		bool rollback() override { return true; }
		bool transactional() const override { return false; }

		void flushed() override;

	public:
		CSV() : DatabaseOutput("CSV") { conn_string = "."; }
		~CSV();
	};
}
