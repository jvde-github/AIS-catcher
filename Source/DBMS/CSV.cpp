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
		Debug() << "DBMS: writing CSV files to " << dir;
	}

	bool CSV::openLog(int st, const std::string &day)
	{
		if (log[st].is_open())
			log[st].close();

		const std::string path = dir + TABLE_NAME[st] + "-" + day + ".csv";

		// only a new file gets a header, so a restart appends to the same day
		std::ifstream probe(path.c_str());
		const bool fresh = !probe.good();
		probe.close();

		log[st].open(path.c_str(), std::ios::out | std::ios::app);
		if (!log[st].is_open())
		{
			Error() << "DBMS: cannot open " << path;
			return false;
		}

		if (fresh)
			log[st] << header(st) << '\n';

		log_day[st] = day;
		return true;
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

		state.touch(h);
		state_dirty = true;
	}

	bool CSV::appendLine(int st, const std::string &line)
	{
		const std::string &day = today();
		if (log_day[st] != day && !openLog(st, day))
			return false;

		log[st] << line << '\n';
		return log[st].good();
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
			stats_rows[params[1] ? params[1] : ""] = join(params);
			stats_dirty = true;
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

	// atomic replace: a reader never sees a half written table
	void CSV::writeSnapshot(const char *name, const std::string &content)
	{
		std::string err;
		if (!Util::Helper::writeFileAtomic(dir + name + ".csv", content, err))
			Error() << "DBMS: " << err;
	}

	void CSV::writeStateFile()
	{
		std::string out;
		out.reserve(state_bytes ? state_bytes + state_bytes / 4 : 4096);
		out += header(ST_STATE);
		out += '\n';

		// forEach walks keyed slots newest first and stops at the first unused one
		state.forEach([&](int h) {
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
			out += ',' + std::to_string(r.count) + ',' + std::to_string(r.msg_types) + ',' + std::to_string(r.channels) + '\n';
			return true;
		});

		state_bytes = out.size();
		writeSnapshot(TABLE_NAME[ST_STATE], out);
	}

	void CSV::writeStatsFile()
	{
		std::string out = header(ST_STATS) + "\n";
		for (const auto &kv : stats_rows)
			out += kv.second + "\n";

		writeSnapshot(TABLE_NAME[ST_STATS], out);
	}

	void CSV::flushed()
	{
		for (int i = 0; i < 3; i++)
			if (log[i].is_open())
				log[i].flush();

		if (state_dirty)
		{
			writeStateFile();
			state_dirty = false;
		}

		if (stats_dirty)
		{
			writeStatsFile();
			stats_dirty = false;
		}
	}

	void CSV::closeDB()
	{
		for (int i = 0; i < 3; i++)
			if (log[i].is_open())
				log[i].close();
	}

	CSV::~CSV()
	{
		// before the files close: the worker calls back into this object
		stopWorker();
		flushed();
		closeDB();
	}
}
