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

	void create(long int t) {
		history[end].time = t;
		history[end].stat.clear();
	}

	History() : start(0), end(0) {
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
		int idx;

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
