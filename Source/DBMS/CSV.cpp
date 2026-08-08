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

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CSV.h"
#include "Logger.h"
#include "Parse.h"
#include "Convert.h"
#include "Helper.h"

namespace IO
{
	static const char *TABLE_NAME[] = {"ais_message", "ais_position", "ais_static", "ais_state", "ais_stats_hourly"};

	std::string CSV::header(int st)
	{
		// the message log carries its own generated id in front
		return st == ST_MESSAGE ? "id," + columnList(st) : columnList(st);
	}

	void CSV::escape(std::string &out, const char *v)
	{
		if (!v)
			return;

		// newlines become spaces so a record is always one line
		const bool quote = std::strpbrk(v, ",\"\n\r") != nullptr;
		if (!quote)
		{
			out += v;
			return;
		}

		out += '"';
		for (const char *p = v; *p; p++)
		{
			if (*p == '\n' || *p == '\r')
			{
				out += ' ';
				continue;
			}
			if (*p == '"')
				out += '"';
			out += *p;
		}
		out += '"';
	}

	std::string CSV::join(const std::vector<const char *> &params)
	{
		std::string line;
		line.reserve(128);
		for (size_t i = 0; i < params.size(); i++)
		{
			if (i > 0)
				line += ',';
			escape(line, params[i]);
		}
		return line;
	}

	const std::string &CSV::today()
	{
		const std::time_t t = std::time(nullptr);
		if (t / 86400 != day_stamp)
		{
			day_stamp = t / 86400;
			day_str = Util::Convert::toDateStr(t);
		}
		return day_str;
	}

	void CSV::connectDB()
	{
		// empty ("-D csv:") means cwd; dir doubles as the connected flag
		dir = conn_string.empty() ? "." : conn_string;
		if (dir[dir.size() - 1] != '/')
			dir += '/';

		// a write is the only reliable test that the directory is usable
		const std::string probe = dir + ".aiscatcher-csv";
		std::ofstream t(probe.c_str());
		if (!t.is_open())
		{
			stats.connect_fail++;
			throw std::runtime_error("DBMS: cannot write to directory \"" + dir + "\", does it exist?");
		}
		t.close();
		std::remove(probe.c_str());

		state.setup(capacity, capacity * 2 + 1);

		// a table that is switched off is neither loaded nor rewritten
		if (STATE)
			loadState();
		if (STATS)
			loadStats();

		Debug() << "DBMS: writing CSV files to " << dir;
	}

	// inverse of escape; false on an unterminated quote
	bool CSV::splitLine(const std::string &line, std::vector<std::string> &out)
	{
		out.clear();
		std::string field;
		bool quoted = false;

		for (size_t i = 0; i < line.size(); i++)
		{
			const char c = line[i];
			if (quoted)
			{
				if (c != '"')
					field += c;
				else if (i + 1 < line.size() && line[i + 1] == '"')
					field += line[i++];
				else
					quoted = false;
			}
			else if (c == '"')
				quoted = true;
			else if (c == ',')
			{
				out.push_back(field);
				field.clear();
			}
			else
				field += c;
		}
		out.push_back(field);
		return !quoted;
	}

	void CSV::loadState()
	{
		loadStateFile(dir + TABLE_NAME[ST_STATE] + ".csv");

		const std::string journal = dir + TABLE_NAME[ST_STATE] + ".csv.journal";
		if (std::ifstream(journal.c_str()).good())
		{
			loadStateFile(journal);
			state_log.bytes = 1; // marks a journal the next compaction must fold in
		}
	}

	void CSV::loadStateFile(const std::string &path)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return;

		// chronological file, last row per target wins, upsert restores LRU order
		std::vector<std::string> f;
		std::string line;
		std::getline(file, line); // header

		int restored = 0, skipped = 0;
		while (std::getline(file, line))
		{
			if (line.empty())
				continue;

			// a corrupt line must never block startup
			uint32_t mmsi = 0;
			try
			{
				if (!splitLine(line, f) || (int)f.size() != 6 + N_COLUMNS + 3 ||
					!(mmsi = (uint32_t)Util::Parse::Integer(f[0])))
				{
					skipped++;
					continue;
				}
			}
			catch (const std::exception &)
			{
				skipped++;
				continue;
			}

			int h = state.find(mmsi);
			if (h == SlotTable<StateRow, uint32_t>::NIL)
			{
				h = state.create(mmsi);
				restored++;
			}
			else
				state.touch(h);

			StateRow &r = state[h];
			std::snprintf(r.first_seen, sizeof(r.first_seen), "%s", f[1].c_str());
			std::snprintf(r.received_at, sizeof(r.received_at), "%s", f[2].c_str());
			std::snprintf(r.station_id, sizeof(r.station_id), "%s", f[3].c_str());
			std::snprintf(r.level, sizeof(r.level), "%s", f[4].c_str());
			std::snprintf(r.ppm, sizeof(r.ppm), "%s", f[5].c_str());
			for (int i = 0; i < N_COLUMNS; i++)
				std::snprintf(r.col[i], sizeof(r.col[i]), "%s", f[6 + i].c_str());

			r.count = r.msg_types = r.channels = 0;
			r.dirty = false;
			try
			{
				r.count = Util::Parse::Integer(f[6 + N_COLUMNS]);
				r.msg_types = Util::Parse::Integer(f[6 + N_COLUMNS + 1]);
				r.channels = Util::Parse::Integer(f[6 + N_COLUMNS + 2]);
			}
			catch (const std::exception &)
			{
			}
		}

		if (skipped)
			Warning() << "DBMS: skipped " << skipped << " corrupt lines in " << path;
		if (restored)
			Debug() << "DBMS: restored " << restored << " targets from " << path;
	}

	void CSV::loadStats()
	{
		loadStatsFile(dir + TABLE_NAME[ST_STATS] + ".csv");

		const std::string journal = dir + TABLE_NAME[ST_STATS] + ".csv.journal";
		if (std::ifstream(journal.c_str()).good())
		{
			loadStatsFile(journal);
			stats_log.bytes = 1;
		}
	}

	void CSV::loadStatsFile(const std::string &path)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return;

		std::vector<std::string> f;
		std::string line;
		std::getline(file, line); // header
		while (std::getline(file, line))
		{
			// keyed on the bucket; a corrupt line is dropped, not written back
			if (splitLine(line, f) && (int)f.size() == sqlParamCount(ST_STATS))
				stats_rows[f[1]] = line;
		}

		if (!stats_rows.empty())
			Debug() << "DBMS: restored " << stats_rows.size() << " stat hours from " << path;
	}

	// append mode; only a brand-new file gets the header
	static bool openAppend(std::ofstream &f, const std::string &path, const std::string &hdr)
	{
		std::ifstream probe(path.c_str());
		const bool fresh = !probe.good();
		probe.close();

		f.open(path.c_str(), std::ios::out | std::ios::app);
		if (!f.is_open())
		{
			Error() << "DBMS: cannot open " << path;
			return false;
		}

		if (fresh)
			f << hdr << '\n';

		return true;
	}

	bool CSV::openLog(int st, const std::string &day)
	{
		if (log[st].is_open())
			log[st].close();

		if (!openAppend(log[st], dir + TABLE_NAME[st] + "-" + day + ".csv", header(st)))
			return false;

		log_day[st] = day;
		return true;
	}

	bool CSV::appendLine(int st, const std::string &line)
	{
		const std::string &day = today();
		if (log_day[st] != day && !openLog(st, day))
			return false;

		log[st] << line << '\n';
		return log[st].good();
	}

	bool CSV::appendJournal(Journal &j, int st, const std::string &row)
	{
		if (!j.log.is_open() && !openAppend(j.log, dir + TABLE_NAME[st] + ".csv.journal", header(st)))
			return false;

		j.log << row << '\n';
		j.bytes += row.size() + 1;
		return j.log.good();
	}

	void CSV::mergeState(const std::vector<const char *> &params)
	{
		const uint32_t mmsi = (uint32_t)Util::Parse::Integer(params[0] ? params[0] : "0");
		if (!mmsi)
			return;

		int h = state.find(mmsi);
		const bool fresh = h == SlotTable<StateRow, uint32_t>::NIL;
		if (fresh)
			h = state.create(mmsi);

		StateRow &r = state[h];
		if (fresh)
		{
			std::memset(&r, 0, sizeof(r));
			std::snprintf(r.first_seen, sizeof(r.first_seen), "%s", params[1] ? params[1] : "");
		}

		std::snprintf(r.received_at, sizeof(r.received_at), "%s", params[1] ? params[1] : "");
		std::snprintf(r.station_id, sizeof(r.station_id), "%s", params[2] ? params[2] : "");

		// NULL never erases a held value - the COALESCE rule
		if (params[3])
			std::snprintf(r.level, sizeof(r.level), "%s", params[3]);
		if (params[4])
			std::snprintf(r.ppm, sizeof(r.ppm), "%s", params[4]);

		for (int i = 0; i < N_COLUMNS; i++)
			if (params[ST_STATE_FIXED + i])
				std::snprintf(r.col[i], sizeof(r.col[i]), "%s", params[ST_STATE_FIXED + i]);

		r.count++;
		r.msg_types |= Util::Parse::Integer(params[ST_STATE_FIXED + N_COLUMNS] ? params[ST_STATE_FIXED + N_COLUMNS] : "0");
		r.channels |= Util::Parse::Integer(params[ST_STATE_FIXED + N_COLUMNS + 1] ? params[ST_STATE_FIXED + N_COLUMNS + 1] : "0");

		if (!r.dirty)
		{
			r.dirty = true;
			dirty_slots.push_back(h);
		}

		state.touch(h);
	}

	bool CSV::exec(int st, const std::vector<const char *> &params)
	{
		if (st == ST_STATE)
		{
			mergeState(params);
			return true;
		}

		if (st == ST_STATS)
		{
			const std::string key = params[1] ? params[1] : "";
			stats_rows[key] = join(params);
			stats_dirty_keys.insert(key);
			return true;
		}

		return appendLine(st, join(params));
	}

	bool CSV::execReturningId(int st, const std::vector<const char *> &params, std::string &id)
	{
		// the counter restarts with the process, hence the dated log files
		id = std::to_string(++next_id);
		return appendLine(st, id + ',' + join(params));
	}

	void CSV::stateRow(std::string &out, int h)
	{
		const StateRow &r = state[h];
		out += std::to_string(state.key(h));
		out += ',';
		escape(out, r.first_seen);
		out += ',';
		escape(out, r.received_at);
		out += ',';
		escape(out, r.station_id);
		out += ',';
		escape(out, r.level);
		out += ',';
		escape(out, r.ppm);
		for (int i = 0; i < N_COLUMNS; i++)
		{
			out += ',';
			escape(out, r.col[i]);
		}
		out += ',' + std::to_string(r.count) + ',' + std::to_string(r.msg_types) + ',' + std::to_string(r.channels);
	}

	// atomic replace: a reader never sees a half written table
	void CSV::writeSnapshot(const char *name, const std::string &content)
	{
		std::string err;
		if (!Util::Helper::writeFileAtomic(dir + name + ".csv", content, err))
			Error() << "DBMS: " << err;
	}

	void CSV::compactState()
	{
		// a switched-off table is never rewritten, an unchanged one not again
		if (!STATE || (state_log.bytes == 0 && dirty_slots.empty()))
			return;

		const auto t0 = std::chrono::steady_clock::now();

		// oldest first, so the journal appends continue the chronological order
		std::vector<int> hs;
		hs.reserve(state.size());
		state.forEach([&](int h) {
			hs.push_back(h);
			return true;
		});

		std::string out;
		out.reserve(state_log.compacted ? state_log.compacted + state_log.compacted / 4 : 4096);
		out += header(ST_STATE);
		out += '\n';
		for (auto it = hs.rbegin(); it != hs.rend(); ++it)
		{
			stateRow(out, *it);
			out += '\n';
		}

		finishCompact(state_log, ST_STATE, out);

		// the snapshot holds every row, anything still pending is superseded
		for (int h : dirty_slots)
			state[h].dirty = false;
		dirty_slots.clear();

		Debug() << "DBMS: compacted " << TABLE_NAME[ST_STATE] << ".csv, " << hs.size() << " targets, "
				<< (out.size() >> 10) << " KB in " << Util::Helper::msSince(t0) << " ms";
	}

	// merge the journal chunk back into one clean file per table
	void CSV::finishCompact(Journal &j, int st, const std::string &out)
	{
		// the rename leaves an open append handle on the old inode
		if (j.log.is_open())
			j.log.close();

		writeSnapshot(TABLE_NAME[st], out);
		std::remove((dir + TABLE_NAME[st] + ".csv.journal").c_str());
		j.compacted = out.size();
		j.bytes = 0;
	}

	void CSV::compactStats()
	{
		if (!STATS)
			return;

		// compaction is also the pruning moment
		size_t pruned = 0;
		if (retention_days > 0)
		{
			const std::string cutoff = Util::Convert::toTimestampStr(retentionCutoff());
			while (!stats_rows.empty() && stats_rows.begin()->first < cutoff)
			{
				stats_rows.erase(stats_rows.begin());
				pruned++;
			}
		}

		if (!pruned && stats_log.bytes == 0 && stats_dirty_keys.empty())
			return;

		const auto t0 = std::chrono::steady_clock::now();

		std::string out;
		out.reserve(stats_log.compacted ? stats_log.compacted + stats_log.compacted / 4 : 4096);
		out += header(ST_STATS);
		out += '\n';
		for (const auto &kv : stats_rows)
		{
			out += kv.second;
			out += '\n';
		}

		finishCompact(stats_log, ST_STATS, out);
		stats_dirty_keys.clear();

		Debug() << "DBMS: compacted " << TABLE_NAME[ST_STATS] << ".csv, " << stats_rows.size() << " hours in "
				<< Util::Helper::msSince(t0) << " ms";
	}

	void CSV::collectVesselsSince(const std::string &since, std::set<uint32_t> &out)
	{
		// the LRU is in last-heard order, so stop at the first pre-hour target
		state.forEach([&](int h) {
			if (since.compare(state[h].received_at) > 0)
				return false;
			out.insert(state.key(h));
			return true;
		});
	}

	void CSV::flushed()
	{
		for (int i = 0; i < 3; i++)
			if (log[i].is_open())
				log[i].flush();

		if (std::time(nullptr) - journal_drained >= JOURNAL_INTERVAL || dirty_slots.size() >= JOURNAL_MAX_PENDING)
		{
			journal_drained = std::time(nullptr);

			// one line per changed target since the last drain
			std::string row;
			for (int h : dirty_slots)
			{
				if (!state[h].dirty)
					continue;
				row.clear();
				stateRow(row, h);
				appendJournal(state_log, ST_STATE, row);
				state[h].dirty = false;
			}
			dirty_slots.clear();

			for (const auto &key : stats_dirty_keys)
				appendJournal(stats_log, ST_STATS, stats_rows[key]);
			stats_dirty_keys.clear();

			if (state_log.log.is_open())
				state_log.log.flush();
			if (stats_log.log.is_open())
				stats_log.log.flush();
		}

		// safety valve for exceptional churn; the day rollover compacts via maintain()
		if (state_log.due())
			compactState();
		if (stats_log.due())
			compactStats();
	}

	// startup, shutdown and day rollover: merge journals, drop expired logs
	void CSV::maintain()
	{
		compactState();
		compactStats();
		pruneLogs();
	}

	void CSV::pruneLogs()
	{
		if (retention_days <= 0)
			return;

		// ISO dates sort lexically, so a string compare finds every expired log
		const std::string cutoff = Util::Convert::toDateStr(retentionCutoff());
		for (const auto &path : Util::Helper::getFilesWithExtension(dir, ".csv"))
		{
			const std::string name = path.substr(path.find_last_of("/\\") + 1);
			for (int st = 0; st < 3; st++)
			{
				const std::string prefix = std::string(TABLE_NAME[st]) + "-";
				if (name.size() == prefix.size() + 14 && name.compare(0, prefix.size(), prefix) == 0 &&
					name.substr(prefix.size(), 10) < cutoff)
					std::remove(path.c_str());
			}
		}
	}

	void CSV::closeDB()
	{
		for (int i = 0; i < 3; i++)
			if (log[i].is_open())
				log[i].close();

		if (state_log.log.is_open())
			state_log.log.close();
		if (stats_log.log.is_open())
			stats_log.log.close();
	}

	CSV::~CSV()
	{
		// before the files close: the worker calls back into this object
		stopWorker();
		maintain();
		closeDB();
	}
}
