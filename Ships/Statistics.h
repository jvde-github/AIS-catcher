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

// ----------------------------
// Class to log message count stat

class Statistics {
	int count;
	int msg[27];
	int channel[4];

	float level, ppm, distance;
	float radar[36];

public:
	int getCount() { return count; }
	void Clear() {
		std::memset(msg, 0, sizeof(msg));
		std::memset(channel, 0, sizeof(channel));
		std::memset(radar, 0, sizeof(radar));

		count = 0;
		distance = level = ppm = 0;
	}

	void Add(const AIS::Message& m, const TAG& tag) {
		if (m.type() > 27 || m.type() < 1) return;

		count++;
		msg[m.type() - 1]++;
		if (m.getChannel() >= 'A' || m.getChannel() <= 'D') channel[m.getChannel() - 'A']++;

		level += tag.level;
		ppm += tag.ppm;

		// for range we ignore atons for now
		if (m.type() == 21) return;

		if (tag.distance > distance && (tag.distance < 50 || tag.validated)) {
			distance = tag.distance;
		}

		if (tag.angle >= 0 && tag.angle < 360) {
			int bucket = tag.angle / 10;
			if (tag.distance > radar[bucket] && (tag.distance < 50 || tag.validated))
				radar[bucket] = tag.distance;
		}
	}

	std::string toJSON(bool empty = false) {
		std::string element;

		element += "{\"count\":" + std::to_string(empty ? 0 : count) +
				   ",\"level\":" + std::to_string(empty || !count ? 0 : level / count) +
				   ",\"ppm\":" + std::to_string(empty || !count ? 0 : ppm / count) +
				   ",\"dist\":" + std::to_string(empty ? 0 : distance) +
				   ",\"channel\":[";

		for (int i = 0; i < 4; i++) element += std::to_string(empty ? 0 : channel[i]) + ",";
		element.pop_back();
		element += "],\"radar\":[";
		for (int i = 0; i < 36; i++) element += std::to_string(empty ? 0 : radar[i]) + ",";
		element.pop_back();
		element += "],\"msg\":[";
		for (int i = 0; i < 27; i++) element += std::to_string(empty ? 0 : msg[i]) + ",";
		element.pop_back();
		element += "]}";
		return element;
	}

	bool Save(std::ofstream& file) {
		int magic = 0x4f81b;
		int version = 1;

		file.write((const char*)&magic, sizeof(int));
		file.write((const char*)&version, sizeof(int));
		file.write((const char*)&count, sizeof(int));
		file.write((const char*)&msg, sizeof(msg));
		file.write((const char*)&channel, sizeof(channel));
		file.write((const char*)&level, sizeof(level));
		file.write((const char*)&ppm, sizeof(ppm));
		file.write((const char*)&distance, sizeof(distance));
		file.write((const char*)&radar, sizeof(radar));

		return true;
	}

	bool ReadInteger(std::ifstream& file, int& dest, int check = -1) {
		file.read((char*)&dest, sizeof(int));
		if (check != -1 && dest != check) return false;
		return true;
	}

	bool Load(std::ifstream& file) {
		int tmp;

		if (!ReadInteger(file, tmp, 0x4f81b)) return false;
		if (!ReadInteger(file, tmp, 1)) return false;
		file.read((char*)&count, sizeof(int));

		file.read((char*)&msg, sizeof(msg));
		file.read((char*)&channel, sizeof(channel));
		file.read((char*)&level, sizeof(level));
		file.read((char*)&ppm, sizeof(ppm));
		file.read((char*)&distance, sizeof(distance));
		file.read((char*)&radar, sizeof(radar));
		return true;
	}
};
