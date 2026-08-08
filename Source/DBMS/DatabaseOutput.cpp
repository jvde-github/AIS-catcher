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

#include <cstring>

#include "DatabaseOutput.h"
#include "Logger.h"
#include "Common.h"
#include "Convert.h"
#include "Helper.h"
#include "Parse.h"

namespace IO
{
	// Column order of the position/static blocks; statement text, column lists
	// and column maps are all generated from these two tables.
	static const int keys_position[] = {AIS::KEY_LAT, AIS::KEY_LON, AIS::KEY_SPEED, AIS::KEY_COURSE,
										AIS::KEY_HEADING, AIS::KEY_STATUS, AIS::KEY_TURN, AIS::KEY_ALT};

	static const int keys_static[] = {AIS::KEY_SHIPNAME, AIS::KEY_CALLSIGN, AIS::KEY_IMO, AIS::KEY_SHIPTYPE,
									  AIS::KEY_AID_TYPE, AIS::KEY_TO_BOW, AIS::KEY_TO_STERN, AIS::KEY_TO_PORT,
									  AIS::KEY_TO_STARBOARD, AIS::KEY_DRAUGHT, AIS::KEY_DESTINATION, AIS::KEY_ETA};

	static_assert(sizeof(keys_position) / sizeof(int) == DatabaseOutput::N_POSITION, "keys_position must match N_POSITION");
	static_assert(sizeof(keys_static) / sizeof(int) == DatabaseOutput::N_STATIC, "keys_static must match N_STATIC");

	static bool carriesPosition(int type)
	{
		return type == 1 || type == 2 || type == 3 || type == 4 || type == 9 ||
			   type == 18 || type == 19 || type == 21 || type == 27;
	}

	static bool carriesStatic(int type)
	{
		return type == 5 || type == 19 || type == 21 || type == 24;
	}

	static std::string colName(int key)
	{
		const AIS::KeyStr &k = AIS::KeyMap[key][JSON_DICT_FULL];
		return std::string(k.p, k.n);
	}

	static std::string joinKeyNames(const int *keys, int n)
	{
		std::string out;
		for (int i = 0; i < n; i++)
		{
			if (i)
				out += ',';
			out += colName(keys[i]);
		}
		return out;
	}

	static std::string placeholders(int n)
	{
		std::string out;
		for (int i = 1; i <= n; i++)
		{
			if (i > 1)
				out += ',';
			out += '$' + std::to_string(i);
		}
		return out;
	}

	struct Templates
	{
		std::string sql[DatabaseOutput::ST_COUNT];
		std::string cols[DatabaseOutput::ST_COUNT];
	};

	// No RETURNING clause: the shared text stays portable.
	static Templates buildTemplates()
	{
		const int NP = DatabaseOutput::N_POSITION, NS = DatabaseOutput::N_STATIC;
		Templates t;

		const std::string pos = joinKeyNames(keys_position, NP);
		const std::string sta = joinKeyNames(keys_static, NS);

		t.cols[DatabaseOutput::ST_MESSAGE] = "mmsi,received_at,station_id,type,channel,signal_level,ppm,nmea";
		t.cols[DatabaseOutput::ST_POSITION] = "msg_id," + pos;
		t.cols[DatabaseOutput::ST_STATIC] = "msg_id," + sta;
		t.cols[DatabaseOutput::ST_STATE] = "mmsi,first_seen,received_at,station_id,signalpower,ppm," + pos + "," + sta + ",count,msg_types,channels";
		t.cols[DatabaseOutput::ST_STATS] = "station_id,bucket,msgs,vessels,channel_a,channel_b,channel_c,channel_d,level_min,level_max,ppm";

		t.sql[DatabaseOutput::ST_MESSAGE] = "INSERT INTO ais_message (" + t.cols[DatabaseOutput::ST_MESSAGE] +
											") VALUES (" + placeholders(8) + ")";
		t.sql[DatabaseOutput::ST_POSITION] = "INSERT INTO ais_position (" + t.cols[DatabaseOutput::ST_POSITION] +
											 ") VALUES (" + placeholders(1 + NP) + ")";
		t.sql[DatabaseOutput::ST_STATIC] = "INSERT INTO ais_static (" + t.cols[DatabaseOutput::ST_STATIC] +
										   ") VALUES (" + placeholders(1 + NS) + ")";

		// $2 fills first_seen and received_at alike; count starts at 1
		std::string vals = "$1,$2,$2,$3,$4,$5";
		for (int i = 0; i < NP + NS; i++)
			vals += ",$" + std::to_string(6 + i);
		vals += ",1,$" + std::to_string(6 + NP + NS) + ",$" + std::to_string(7 + NP + NS);

		// COALESCE: a NULL for a field the message did not carry must not wipe the stored value
		std::string upd = "received_at=EXCLUDED.received_at,station_id=EXCLUDED.station_id,"
						  "signalpower=COALESCE(EXCLUDED.signalpower,ais_state.signalpower),"
						  "ppm=COALESCE(EXCLUDED.ppm,ais_state.ppm)";
		for (int i = 0; i < NP + NS; i++)
		{
			const std::string c = colName(i < NP ? keys_position[i] : keys_static[i - NP]);
			upd += ',' + c + "=COALESCE(EXCLUDED." + c + ",ais_state." + c + ')';
		}
		upd += ",count=ais_state.count+1,"
			   "msg_types=EXCLUDED.msg_types|ais_state.msg_types,"
			   "channels=EXCLUDED.channels|ais_state.channels";

		t.sql[DatabaseOutput::ST_STATE] = "INSERT INTO ais_state (" + t.cols[DatabaseOutput::ST_STATE] +
										  ") VALUES (" + vals + ") ON CONFLICT (mmsi) DO UPDATE SET " + upd;

		t.sql[DatabaseOutput::ST_STATS] = "INSERT INTO ais_stats_hourly (" + t.cols[DatabaseOutput::ST_STATS] +
										  ") VALUES (" + placeholders(11) + ") "
										  "ON CONFLICT (station_id,bucket) DO UPDATE SET "
										  "msgs=EXCLUDED.msgs,vessels=EXCLUDED.vessels,channel_a=EXCLUDED.channel_a,"
										  "channel_b=EXCLUDED.channel_b,channel_c=EXCLUDED.channel_c,channel_d=EXCLUDED.channel_d,"
										  "level_min=EXCLUDED.level_min,level_max=EXCLUDED.level_max,ppm=EXCLUDED.ppm";

		return t;
	}

	static const Templates &templates()
	{
		static const Templates t = buildTemplates();
		return t;
	}

	const std::string &DatabaseOutput::sqlTemplate(int st)
	{
		return templates().sql[st];
	}

	const std::string &DatabaseOutput::columnList(int st)
	{
		return templates().cols[st];
	}

	int DatabaseOutput::sqlParamCount(int st)
	{
		switch (st)
		{
		case ST_MESSAGE:
			return 8;
		case ST_POSITION:
			return 1 + N_POSITION;
		case ST_STATIC:
			return 1 + N_STATIC;
		case ST_STATE:
			return ST_STATE_FIXED + N_POSITION + N_STATIC + 2;
		default:
			return 11;
		}
	}

	size_t DatabaseOutput::queueSize()
	{
		const std::lock_guard<std::mutex> lock(queue_mutex);
		return message_queue.size();
	}

	// AIS key -> column index within the position/static blocks, or -1
	struct ColumnMaps
	{
		std::vector<int> position, stat;
	};

	static const ColumnMaps &columnMaps()
	{
		static const ColumnMaps m = []() {
			ColumnMaps m;
			m.position.assign(AIS::KEY_COUNT, -1);
			m.stat.assign(AIS::KEY_COUNT, -1);

			for (int i = 0; i < DatabaseOutput::N_POSITION; i++)
				m.position[keys_position[i]] = i;

			for (int i = 0; i < DatabaseOutput::N_STATIC; i++)
				m.stat[keys_static[i]] = i;

			// an aton reports its name in KEY_NAME, sharing the shipname column
			m.stat[AIS::KEY_NAME] = m.stat[AIS::KEY_SHIPNAME];
			return m;
		}();
		return m;
	}

	bool DatabaseOutput::writePosition(const QueuedEntry &entry, const char *msg_id)
	{
		std::vector<const char *> params(sqlParamCount(ST_POSITION), nullptr);
		params[0] = msg_id;

		for (const auto &kv : entry.kvs)
		{
			int col = columnMaps().position[kv.key];
			if (col >= 0)
				params[1 + col] = kv.value.c_str();
		}

		return exec(ST_POSITION, params);
	}

	bool DatabaseOutput::writeStatic(const QueuedEntry &entry, const char *msg_id)
	{
		std::vector<const char *> params(sqlParamCount(ST_STATIC), nullptr);
		params[0] = msg_id;

		for (const auto &kv : entry.kvs)
		{
			int col = columnMaps().stat[kv.key];
			if (col >= 0)
				params[1 + col] = kv.value.c_str();
		}

		return exec(ST_STATIC, params);
	}

	bool DatabaseOutput::writeState(const QueuedEntry &entry)
	{
		std::vector<const char *> params(sqlParamCount(ST_STATE), nullptr);

		params[0] = entry.mmsi.c_str();
		params[1] = entry.timestamp.c_str();
		params[2] = entry.station_id.c_str();
		params[3] = entry.level.empty() ? nullptr : entry.level.c_str();
		params[4] = entry.ppm.empty() ? nullptr : entry.ppm.c_str();

		for (const auto &kv : entry.kvs)
		{
			int col = columnMaps().position[kv.key];
			if (col >= 0)
				params[ST_STATE_FIXED + col] = kv.value.c_str();

			col = columnMaps().stat[kv.key];
			if (col >= 0)
				params[ST_STATE_FIXED + N_POSITION + col] = kv.value.c_str();
		}

		params[ST_STATE_FIXED + N_POSITION + N_STATIC] = entry.type_bit.c_str();
		params[ST_STATE_FIXED + N_POSITION + N_STATIC + 1] = entry.channel_bit.c_str();

		return exec(ST_STATE, params);
	}

	bool DatabaseOutput::writeStats(const StatsBucket &bucket)
	{
		const std::string bucket_at = Util::Convert::toTimestampStr(bucket.hour);
		const std::string station = std::to_string(station_id);
		const std::string msgs = std::to_string(bucket.msgs);
		const std::string vessels = std::to_string(bucket.vessel_count);
		const std::string ch[4] = {std::to_string(bucket.channel[0]), std::to_string(bucket.channel[1]),
								   std::to_string(bucket.channel[2]), std::to_string(bucket.channel[3])};

		std::string lmin, lmax, ppm;
		if (bucket.level_count > 0)
		{
			lmin = std::to_string(bucket.level_min);
			lmax = std::to_string(bucket.level_max);
		}
		if (bucket.ppm_count > 0)
			ppm = std::to_string(bucket.ppm_sum / bucket.ppm_count);

		return exec(ST_STATS, {station.c_str(), bucket_at.c_str(), msgs.c_str(), vessels.c_str(),
							   ch[0].c_str(), ch[1].c_str(), ch[2].c_str(), ch[3].c_str(),
							   lmin.empty() ? nullptr : lmin.c_str(),
							   lmax.empty() ? nullptr : lmax.c_str(),
							   ppm.empty() ? nullptr : ppm.c_str()});
	}

	bool DatabaseOutput::writeEntry(const QueuedEntry &entry)
	{
		const bool pos = POSITION && carriesPosition(entry.msg_type_int);
		const bool sta = STATIC && carriesStatic(entry.msg_type_int);

		std::string msg_id;

		if (needMessageTable())
		{
			if (!execReturningId(ST_MESSAGE,
								 {entry.mmsi.c_str(), entry.timestamp.c_str(), entry.station_id.c_str(),
								  entry.msg_type.c_str(), entry.channel.c_str(),
								  entry.level.empty() ? nullptr : entry.level.c_str(),
								  entry.ppm.empty() ? nullptr : entry.ppm.c_str(),
								  NMEA && !entry.nmea.empty() ? entry.nmea.c_str() : nullptr},
								 msg_id))
				return false;
		}

		const char *msg_id_ptr = msg_id.empty() ? nullptr : msg_id.c_str();

		if (pos && !writePosition(entry, msg_id_ptr))
			return false;

		if (sta && !writeStatic(entry, msg_id_ptr))
			return false;

		if (STATE && entry.mmsi != "0" && !writeState(entry))
			return false;

		stats.bytes_out += entry.mmsi.size() + entry.station_id.size() + entry.msg_type.size() +
						   entry.timestamp.size() + entry.channel.size() + entry.level.size() +
						   entry.ppm.size() + entry.nmea.size();
		for (const auto &kv : entry.kvs)
			stats.bytes_out += kv.value.size();

		return true;
	}

	// caller holds queue_mutex
	void DatabaseOutput::accumulateStats(const AIS::Message &msg, const TAG &tag)
	{
		std::time_t t = msg.getRxTimeUnix();
		if (t <= 0)
			t = std::time(nullptr);

		const std::time_t hour = hourOf(t);

		if (stats_current.hour != hour)
		{
			if (stats_current.msgs > 0)
			{
				stats_current.vessel_count = (int)stats_current.vessels.size();
				stats_pending.push_back(std::move(stats_current));
			}

			stats_current = StatsBucket();
			stats_current.hour = hour;
		}

		stats_current.msgs++;
		stats_current.vessels.insert(msg.mmsi());

		const int ch = msg.getChannel() - 'A';
		if (ch >= 0 && ch < 4)
			stats_current.channel[ch]++;

		if (tag.level != LEVEL_UNDEFINED)
		{
			if (stats_current.level_count == 0)
				stats_current.level_min = stats_current.level_max = tag.level;
			else
			{
				stats_current.level_min = MIN(stats_current.level_min, tag.level);
				stats_current.level_max = MAX(stats_current.level_max, tag.level);
			}
			stats_current.level_count++;
		}

		if (tag.ppm != PPM_UNDEFINED)
		{
			stats_current.ppm_sum += tag.ppm;
			stats_current.ppm_count++;
		}
	}

	void DatabaseOutput::postStats()
	{
		std::vector<StatsBucket> buckets;
		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			buckets.swap(stats_pending);
			if (stats_current.msgs > 0)
				buckets.push_back(stats_current.scalars());
		}

		for (size_t i = 0; i < buckets.size(); i++)
		{
			if (writeStats(buckets[i]))
				continue;

			// closed hours go back for the next cycle; the current bucket is re-copied anyway
			const std::lock_guard<std::mutex> lock(queue_mutex);
			for (size_t j = i; j < buckets.size(); j++)
				if (buckets[j].hour != stats_current.hour)
					stats_pending.push_back(std::move(buckets[j]));
			break;
		}
	}

	void DatabaseOutput::post()
	{
		// conn_fails counts consecutive failed cycles; any usable cycle resets it
		if (!connected && !tryConnect())
		{
			conn_fails++;
			return;
		}

		if (!ensureConnection())
		{
			conn_fails++;
			return;
		}

		if (STATS)
			postStats();

		std::vector<QueuedEntry> batch;
		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			batch.swap(message_queue);
		}

		if (batch.empty())
		{
			conn_fails = 0;
			return;
		}

		// no rollback exists: skip failed entries in place, a replay would double-write
		if (!transactional())
		{
			int failed = 0;
			for (const auto &entry : batch)
				if (!writeEntry(entry))
					failed++;

			if (failed)
				Warning() << "DBMS: dropped " << failed << " of " << batch.size() << " messages the backend rejected";

			conn_fails = failed == (int)batch.size() ? conn_fails + 1 : 0;
			return;
		}

		if (!begin())
		{
			conn_fails++;
			return;
		}

		bool ok = true;
		for (const auto &entry : batch)
			if (!writeEntry(entry))
			{
				ok = false;
				break;
			}

		if (ok)
		{
			// a failed commit means nothing landed; count the cycle so the
			// watchdog fires on a persistently failing backend
			if (commit())
			{
				conn_fails = 0;
				return;
			}

			rollback();
			conn_fails++;
			Warning() << "DBMS: dropped " << batch.size() << " messages, commit failed";
			return;
		}

		rollback();

		// nothing landed: replay row by row so one refused message cannot block the rest
		int failed = 0;
		for (const auto &entry : batch)
		{
			if (!begin())
			{
				conn_fails++;
				return;
			}

			if (writeEntry(entry) && commit())
				continue;

			rollback();
			failed++;
		}

		if (failed)
			Warning() << "DBMS: dropped " << failed << " of " << batch.size() << " messages the database rejected";

		conn_fails = failed == (int)batch.size() ? conn_fails + 1 : 0;
	}

	void DatabaseOutput::process()
	{
		while (!terminate)
		{
			for (int i = 0; !terminate && i < (conn_fails == 0 ? INTERVAL : retryDelay()) && queueSize() < MAX_QUEUE_SIZE / 2; i++)
				SleepSystem(1000);

			// no backend may take the receiver down: log, back off, retry
			try
			{
				post();
				flushed();

				const long day = std::time(nullptr) / 86400;
				if (day != maintain_day)
				{
					maintain_day = day;
					maintain();
				}
			}
			catch (const std::exception &e)
			{
				Error() << "DBMS: " << e.what();
				conn_fails++;
			}

			if (terminate)
				break;
		}
	}

	void DatabaseOutput::maintain()
	{
		if (retention_days <= 0)
			return;

		const auto t0 = std::chrono::steady_clock::now();
		const std::string cutoff = Util::Convert::toTimestampStr(retentionCutoff());

		// chunked so a backlog never holds one long transaction; cascade covers children
		long total = 0, rows;
		do
		{
			rows = execDelete("DELETE FROM ais_message WHERE id IN "
							  "(SELECT id FROM ais_message WHERE received_at < $1 LIMIT 5000)",
							  cutoff.c_str());
			total += rows;
		} while (rows == 5000);

		execDelete("DELETE FROM ais_stats_hourly WHERE bucket < $1", cutoff.c_str());
		execDelete("DELETE FROM ais_state WHERE received_at < $1", cutoff.c_str());

		if (total)
			Info() << "DBMS: retention removed " << total << " messages older than " << cutoff << " in " << Util::Helper::msSince(t0) << " ms";
	}

	void DatabaseOutput::startWorker()
	{
		if (running)
			return;

		running = true;
		terminate = false;
		run_thread = std::thread(&DatabaseOutput::process, this);

		Debug() << "DBMS: start thread, filter: " << Util::Convert::toString(filter.isOn());

		if (filter.isOn())
			Debug() << ", Allowed: " << filter.getAllowed();

		Debug() << ", state " << Util::Convert::toString(STATE)
				<< ", position " << Util::Convert::toString(POSITION)
				<< ", static " << Util::Convert::toString(STATIC)
				<< ", nmea " << Util::Convert::toString(NMEA)
				<< ", stats " << Util::Convert::toString(STATS);
	}

	void DatabaseOutput::stopWorker()
	{
		if (!running)
			return;

		running = false;
		terminate = true;
		run_thread.join();

		Debug() << "DBMS: stop thread and database closed.";
	}

	// false leaves the output disconnected for the worker to retry with backoff
	bool DatabaseOutput::tryConnect()
	{
		try
		{
			connectDB();

			if (!prepareAll())
				throw std::runtime_error("DBMS: cannot prepare statements, is the schema loaded? See create_pg.sql / create_sqlite.sql");
		}
		catch (const std::exception &e)
		{
			Error() << e.what();
			stats.connect_fail++;
			return false;
		}

		connected = true;
		conn_fails = 0;
		stats.connected = 1;
		stats.connect_ok++;

		if (STATS)
		{
			const std::time_t now = std::time(nullptr);
			stats_current.hour = hourOf(now);
			collectVesselsSince(Util::Convert::toTimestampStr(stats_current.hour), stats_current.vessels);

			if (!stats_current.vessels.empty())
				Debug() << "DBMS: resumed hour bucket with " << stats_current.vessels.size() << " vessels already heard";
		}

		maintain();
		maintain_day = std::time(nullptr) / 86400;
		return true;
	}

	void DatabaseOutput::setup()
	{
		if (!tryConnect())
		{
			conn_fails = 1;
			Warning() << "DBMS: output not available, the receiver continues and retries with backoff";
		}

		startWorker();
	}

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

	void DatabaseOutput::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		const JSON::JSON &json = data[0];
		const AIS::Message &msg = *(AIS::Message *)json.binary;

		if (!filter.include(msg))
			return;

		// nothing below the statistics is enabled: no entry to build or queue
		if (!needMessageTable() && !STATE)
		{
			if (STATS)
			{
				const std::lock_guard<std::mutex> lock(queue_mutex);
				accumulateStats(msg, tag);
			}
			return;
		}

		// build outside the lock: the flush thread must not wait on the string
		// work below, nor the decoder on a flush in progress
		QueuedEntry entry;
		entry.mmsi = std::to_string(msg.mmsi());
		entry.station_id = std::to_string(station_id ? station_id : msg.getStation());
		entry.msg_type = std::to_string(msg.type());
		entry.msg_type_int = msg.type();
		entry.timestamp = Util::Convert::toTimestampStr(msg.getRxTimeUnix());
		entry.channel = std::string(1, (char)msg.getChannel());

		// a tag without measurements stays empty and is bound as NULL, so a
		// non-SDR source cannot write the 1024 sentinel over real values
		if (tag.level != LEVEL_UNDEFINED)
			entry.level = std::to_string(tag.level);
		if (tag.ppm != PPM_UNDEFINED)
			entry.ppm = std::to_string(tag.ppm);

		if (NMEA)
		{
			for (const auto &s : msg.sentences())
			{
				if (!entry.nmea.empty())
					entry.nmea += '\n';
				entry.nmea += s;
			}
		}

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

		if (STATE)
		{
			int ch = msg.getChannel() - 'A';
			if (ch < 0 || ch > 4)
				ch = 4;
			entry.type_bit = std::to_string(1 << msg.type());
			entry.channel_bit = std::to_string(1 << ch);
		}

		const std::lock_guard<std::mutex> lock(queue_mutex);

		if (STATS)
			accumulateStats(msg, tag);

		if (message_queue.size() >= MAX_QUEUE_SIZE)
		{
			// drop the oldest quarter rather than the whole buffer: a slow database
			// should cost the least recent history, not everything still queued
			const size_t drop = MAX_QUEUE_SIZE / 4;
			Warning() << "DBMS: writing to database slow or failed, dropped " << drop << " oldest messages.";
			message_queue.erase(message_queue.begin(), message_queue.begin() + drop);
		}

		message_queue.push_back(std::move(entry));
	}

	// removed table settings get an error naming their replacement
	static const char *replacedBy(AIS::Keys key)
	{
		switch (key)
		{
		case AIS::KEY_SETTING_VP:
		case AIS::KEY_SETTING_BS:
		case AIS::KEY_SETTING_SAR:
		case AIS::KEY_SETTING_ATON:
			return "position";
		case AIS::KEY_SETTING_VS:
			return "static";
		case AIS::KEY_SETTING_V:
			return "state";
		case AIS::KEY_SETTING_MSGS:
			return "position, static or nmea";
		default:
			return nullptr;
		}
	}

	Setting &DatabaseOutput::SetKey(AIS::Keys key, const std::string &arg)
	{
		if (const char *use = replacedBy(key))
			throw std::runtime_error("DBMS: this setting was removed with the new schema, use \"" +
									 std::string(use) + "\" instead.");

		switch (key)
		{
		case AIS::KEY_SETTING_CONN_STR:
			conn_string = arg;
			break;
		case AIS::KEY_SETTING_CAPACITY:
			capacity = Util::Parse::Integer(arg, 64, 1000000);
			break;
		case AIS::KEY_SETTING_RETENTION:
			retention_days = Util::Parse::Integer(arg, 0, 36500);
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
			Warning() << "DBMS: max_fails is ignored, a database output no longer stops the receiver";
			break;
		case AIS::KEY_SETTING_NMEA:
			NMEA = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_POSITION:
			POSITION = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_STATIC:
			STATIC = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_STATE:
			STATE = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_STATS:
			STATS = Util::Parse::Switch(arg);
			break;
		default:
			if (!setOptionKey(key, arg) && !filter.SetOptionKey(key, arg))
				throw std::runtime_error("DBMS: unknown option.");
			break;
		}
		return *this;
	}
}
