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
#include <iostream>
#include <fstream>
#include <string.h>
#include <memory>

#include "Statistics.h"

template <int N, int INTERVAL>
class History : public StreamIn<JSON::JSON> {
	std::mutex mtx;

	struct {
		long int time = {};
		MessageStatistics stat;
	} history[N];

	int start, end;

	void create(long int t) {
		history[end].time = t;
		history[end].stat.Clear();
	}

	bool readInteger(std::ifstream& file, int& dest, int check = -1) {
		file.read((char*)&dest, sizeof(int));
		if (check != -1 && dest != check) return false;
		return true;
	}

public:
	virtual ~History() {}
	void setCutoff(int cutoff) {
		for (int i = 0; i < N; i++)
			history[i].stat.setCutoff(cutoff);
	}

	History() {
		Clear();
	}

	void Clear() {
		std::lock_guard<std::mutex> l{ this->mtx };

		start = end = 0;
		create((long int)time(nullptr) / (long int)INTERVAL);
	}

	void Receive(const JSON::JSON* j, int len, TAG& tag) {
		std::lock_guard<std::mutex> l{ this->mtx };

		for (int i = 0; i < len; i++) {
			if (!j[i].binary) return;

			const AIS::Message* msg = (AIS::Message*)j[i].binary;
			long int tm = ((long int)msg->getRxTimeUnix()) / (long int)INTERVAL;
			long int tp = ((long int)tag.previous_signal) / (long int)INTERVAL;

			if (history[end].time < tm) {
				end = (end + 1) % N;
				create(tm);
				if (start == end) start = (start + 1) % N;
			}

			history[end].stat.Add(*msg, tag, tm != tp);
		}
	}

	bool Save(std::ofstream& file) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int magic = 0x4f80b;
		int version = 1;
		int i = INTERVAL;
		int n = N;
		int s = sizeof(history);

		file.write((const char*)&magic, sizeof(int));
		file.write((const char*)&version, sizeof(int));
		file.write((const char*)&s, sizeof(int));
		file.write((const char*)&i, sizeof(int));
		file.write((const char*)&n, sizeof(int));
		file.write((const char*)&start, sizeof(int));
		file.write((const char*)&end, sizeof(int));

		for (int i = 0; i < N; i++) {
			file.write((const char*)&history[i].time, sizeof(history[i].time));
			history[i].stat.Save(file);
		}

		return true;
	}

	bool Load(std::ifstream& file) {
		std::lock_guard<std::mutex> l{ this->mtx };

		int tmp;

		if (!readInteger(file, tmp, 0x4f80b)) return false;
		if (!readInteger(file, tmp, 1)) return false;
		if (!readInteger(file, tmp, /*sizeof(history)*/ -1)) return false;
		if (!readInteger(file, tmp, INTERVAL)) return false;
		if (!readInteger(file, tmp, N)) return false;

		readInteger(file, start, -1);
		readInteger(file, end, -1);

		for (int i = 0; i < N; i++) {
			if (!file.read((char*)&history[i].time, sizeof(history[i].time))) return false;
			if (!history[i].stat.Load(file)) return false;
		}

		// as database is not persistent we cannot combine old and new
		history[end].stat.clearVessels();
		return true;
	}

	float getAverage() {
		std::lock_guard<std::mutex> l{ this->mtx };

		float avg = 0.0f;

		for (int idx = start; idx != end; idx = (idx + 1) % N)
			avg += history[idx].stat.getCount();

		int delta_time = 1 + (int)((long int)history[end].time - (long int)history[start].time);
		return avg / delta_time;
	}

	std::string lastStatToJSON() {
		std::lock_guard<std::mutex> l{ this->mtx };

		long int tm = (long int) std::time(nullptr) / (long int)INTERVAL -1;
		if (start == end || tm  > history[(end + N - 1) % N].time) return history[0].stat.toJSON(true);

		return history[(end + N - 1) % N].stat.toJSON(false);
	}

	std::string toJSON() {
		std::lock_guard<std::mutex> l{ this->mtx };

		std::string content;
		long int tm_now = ((long int)time(nullptr)) / (long int)INTERVAL;

		content += "{\"time\":[";
		for (int i = N, tm = tm_now, idx = end; i > 0; i--) {
			bool new_tm = history[idx].time >= tm;

			content += std::to_string(i - N) + ",";

			if (new_tm) {
				if (idx == start) break;
				idx = (idx + N - 1) % N;
			}
			tm--;
		}
		if (content[content.size() - 1] == ',') content.pop_back();
		content += "],\"stat\":[";

		for (int i = N, tm = tm_now, idx = end; i > 0; i--) {
			bool empty = history[idx].time < tm;
			content += history[idx].stat.toJSON(empty) + ",";

			if (!empty) {
				if (idx == start) break;
				idx = (idx + N - 1) % N;
			}
			tm--;
		}
		if (content[content.size() - 1] == ',') content.pop_back();
		content += "]}";
		return content;
	}
};
