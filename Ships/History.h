/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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
struct History : public StreamIn<JSON::JSON> {

	struct {
		long int time;
		Statistics stat;
	} history[N];

	int start, end;

	void setCutoff(int cutoff) {
		for (int i = 0; i < N; i++) 
			history[i].stat.setCutoff(cutoff);
	}

	void setLog(bool b) {
		for (int i = 0; i < N; i++)
			history[i].stat.setLog(b);
			
	}
	void create(long int t) {
		history[end].time = t;
		history[end].stat.Clear();
	}

	History() {
		Clear();
	}

	void Clear() {
		start = end = 0;
		create((long int)time(NULL) / (long int)INTERVAL);
	}

	void Receive(const JSON::JSON* j, int len, TAG& tag) {

		for (int i = 0; i < len; i++) {
			if (!j[i].binary) return;

			const AIS::Message* msg = (AIS::Message*)j[i].binary;
			long int tm = ((long int)msg->getRxTimeUnix()) / (long int)INTERVAL;

			if (history[end].time != tm) {
				end = (end + 1) % N;
				create(tm);
				if (start == end) start = (start + 1) % N;
			}

			history[end].stat.Add(*msg, tag);
		}
	}

	bool Save(std::ofstream& file) {
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

	bool ReadInteger(std::ifstream& file, int& dest, int check = -1) {
		file.read((char*)&dest, sizeof(int));
		if (check != -1 && dest != check) return false;
		return true;
	}

	bool Load(std::ifstream& file) {
		int tmp;

		if (!ReadInteger(file, tmp, 0x4f80b)) return false;
		if (!ReadInteger(file, tmp, 1)) return false;
		if (!ReadInteger(file, tmp, /*sizeof(history)*/ -1)) return false;
		if (!ReadInteger(file, tmp, INTERVAL)) return false;
		if (!ReadInteger(file, tmp, N)) return false;

		ReadInteger(file, start, -1);
		ReadInteger(file, end, -1);

		for (int i = 0; i < N; i++) {
			if (!file.read((char*)&history[i].time, sizeof(history[i].time))) return false;
			if (!history[i].stat.Load(file)) return false;
		}
		return true;
	}

	float getAverage() {
		float avg = 0.0f;

		for (int idx = start; idx != end; idx = (idx + 1) % N)
			avg += history[idx].stat.getCount();

		int delta_time = 1 + (int)((long int)history[end].time - (long int)history[start].time);
		return avg / delta_time;
	}

	std::string lastStatToJSON() {
		// needs change, now it returns last interval in which we received messages
		if (start == end) return history[0].stat.toJSON(true);
		return history[(end + N - 1) % N].stat.toJSON(false);
	}

	std::string toJSON() {
		std::string content;
		long int tm, tm_now = ((long int)time(nullptr)) / (long int)INTERVAL;

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
