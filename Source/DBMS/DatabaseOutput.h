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

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "MsgOut.h"
#include "Stream.h"
#include "Keys.h"
#include "AIS.h"
#include "JSON/JSON.h"

// Database-agnostic part of the message log: queue, worker thread, row
// building, statistics. Must compile with neither libpq nor sqlite3 present.

namespace IO
{

	class DatabaseOutput : public OutputMessage
	{
	public:
		enum Statement
		{
			ST_MESSAGE = 0,
			ST_POSITION,
			ST_STATIC,
			ST_STATE,
			ST_STATS,
			ST_COUNT
		};

		// ST_STATE layout: ST_STATE_FIXED fields, both column blocks, two bitmasks
		enum
		{
			N_POSITION = 8,
			N_STATIC = 12,
			ST_STATE_FIXED = 5
		};

	protected:
		// generated from one key table; parameters are numbered $1..$n
		static const std::string &sqlTemplate(int st);
		static const std::string &columnList(int st);
		static int sqlParamCount(int st);

		virtual void connectDB() = 0;

		// Reconnect and re-prepare if needed. False means unusable this round.
		virtual bool ensureConnection() = 0;
		virtual bool prepareAll() = 0;

		virtual bool exec(int st, const std::vector<const char *> &params) = 0;
		virtual bool execReturningId(int st, const std::vector<const char *> &params, std::string &id) = 0;

		virtual bool begin() = 0;
		virtual bool commit() = 0;
		virtual bool rollback() = 0;

		// false when begin/commit/rollback are no-ops: a failed batch is then never replayed
		virtual bool transactional() const { return true; }

		// end of a flush cycle
		virtual void flushed() {}

		// daily housekeeping: the default prunes SQL tables to `retention` days
		virtual void maintain();

		// one $1-parameterized statement, returns affected rows
		virtual long execDelete(const char *sql, const char *param) { return 0; }

		std::time_t retentionCutoff() const { return std::time(nullptr) - (std::time_t)retention_days * 86400; }


		// targets heard since `since` (base timestamp format); restores only the
		// hour's vessel count after a restart, the other columns restart by design
		virtual void collectVesselsSince(const std::string &since, std::set<uint32_t> &out) {}

		static std::time_t hourOf(std::time_t t) { return t - (t % 3600); }

		// derived destructors must call this first: the worker calls their virtuals
		void stopWorker();

		struct QueuedEntry
		{
			struct KV
			{
				int key;
				std::string value;
			};

			std::string mmsi, station_id, msg_type, timestamp, channel, level, ppm;
			std::string type_bit, channel_bit;
			std::string nmea;
			int msg_type_int = 0;
			std::vector<KV> kvs;
		};

		struct StatsBucket
		{
			std::time_t hour = 0;
			int msgs = 0;
			int channel[4] = {0, 0, 0, 0};
			int level_count = 0, ppm_count = 0;
			float level_min = 0, level_max = 0;
			double ppm_sum = 0;
			int vessel_count = 0; // vessels.size() frozen at hand-off
			std::set<uint32_t> vessels;

			// copy without the vessels set
			StatsBucket scalars() const
			{
				StatsBucket b;
				b.hour = hour;
				b.msgs = msgs;
				for (int i = 0; i < 4; i++)
					b.channel[i] = channel[i];
				b.level_count = level_count;
				b.ppm_count = ppm_count;
				b.level_min = level_min;
				b.level_max = level_max;
				b.ppm_sum = ppm_sum;
				b.vessel_count = (int)vessels.size();
				return b;
			}
		};

		int station_id = 0;
		int conn_fails = 0;
		int MAX_FAILS = 10;
		int INTERVAL = 60;

		bool POSITION = false, STATIC = false, NMEA = false;
		bool STATE = true, STATS = true;

		std::string conn_string;

		// CSV-only, but every backend accepts it: the hub writes the whole object
		int capacity = 8192;

		// days of history maintain() keeps, 0 keeps everything
		int retention_days = 0;

		bool needMessageTable() const { return POSITION || STATIC || NMEA; }

		void startWorker();

	private:
		std::atomic<bool> terminate{false}, running{false};
		std::thread run_thread;

		long maintain_day = 0;

		std::vector<QueuedEntry> message_queue;
		static const size_t MAX_QUEUE_SIZE = 2048;
		std::mutex queue_mutex;

		StatsBucket stats_current;
		std::vector<StatsBucket> stats_pending;

		size_t queueSize();

		bool writeEntry(const QueuedEntry &entry);
		bool writePosition(const QueuedEntry &entry, const char *msg_id);
		bool writeStatic(const QueuedEntry &entry, const char *msg_id);
		bool writeState(const QueuedEntry &entry);
		bool writeStats(const StatsBucket &bucket);

		void accumulateStats(const AIS::Message &msg, const TAG &tag);
		void postStats();
		void post();
		void process();

	public:
		DatabaseOutput(const char *name) : OutputMessage(name) { fmt = MessageFormat::JSON_FULL; }

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<AIS::GPS>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;

		void setup();
		void Start() override { setup(); }

		Setting &SetKey(AIS::Keys key, const std::string &arg) override;
	};
}
