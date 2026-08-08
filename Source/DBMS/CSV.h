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

	// Same tables as files: daily-rotating append logs, while state and stats
	// journal changes to a sidecar merged into the clean main file at startup,
	// shutdown and day rollover; last row per key wins.
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
			bool dirty;
		};

		struct Journal
		{
			std::ofstream log;
			size_t bytes = 0, compacted = 0;

			// 4x live balances write amplification against replay; floor stops thrash
			bool due() const { return bytes > 1048576 && bytes > 4 * compacted; }
		};

		std::string dir;
		std::ofstream log[3]; // ST_MESSAGE, ST_POSITION, ST_STATIC
		std::string log_day[3];
		long long next_id = 0;

		std::time_t day_stamp = -1; // days since epoch of day_str
		std::string day_str;

		SlotTable<StateRow, uint32_t> state;
		std::vector<int> dirty_slots;
		Journal state_log, stats_log;

		// crash recovery is the only reader: the clock caps staleness, the cap loss
		static const int JOURNAL_INTERVAL = 600;
		static const size_t JOURNAL_MAX_PENDING = 4096;
		std::time_t journal_drained = 0;
		std::set<std::string> stats_dirty_keys;

		// deduplicated truth behind the stats journal, rewritten at compaction
		std::map<std::string, std::string> stats_rows;

		static void escape(std::string &out, const char *v);
		static std::string join(const std::vector<const char *> &params);
		static std::string header(int st);
		static bool splitLine(const std::string &line, std::vector<std::string> &out);

		const std::string &today();
		bool openLog(int st, const std::string &day);
		bool appendLine(int st, const std::string &line);
		bool appendJournal(Journal &j, int st, const std::string &row);
		void finishCompact(Journal &j, int st, const std::string &out);

		void loadState();
		void loadStateFile(const std::string &path);
		void loadStats();
		void loadStatsFile(const std::string &path);
		void mergeState(const std::vector<const char *> &params);
		void stateRow(std::string &out, int h);
		void writeSnapshot(const char *name, const std::string &content);
		void compactState();
		void compactStats();
		void pruneLogs();
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
		void maintain() override;
		void collectVesselsSince(const std::string &since, std::set<uint32_t> &out) override;

	public:
		CSV() : DatabaseOutput("CSV") { conn_string = "."; }
		~CSV();
	};
}
