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
#include <cstdint>
#include <iostream>
#include <string.h>
#include <mutex>
#include <vector>

#include "AIS-catcher.h"
#include "JSONAIS.h"

class PrometheusCounter : public StreamIn<JSON::JSON> {
	std::mutex mtx;

	unsigned int _count;
	unsigned int _msg[28];
	unsigned int _channel[4];

	float _distance;
	unsigned int _dropped;

	struct Sample {
		uint32_t mmsi;
		int station;
		char channel[4];
		uint32_t types[4];
		float ppm[4], level[4];
		uint8_t has_ppm, has_level;
		uint8_t speed, shipclass;
	};

	static const int SLOT_BITS = 10;
	static const int SLOTS = 1 << SLOT_BITS;
	static const int MAX_USED = SLOTS * 3 / 4;

	std::vector<Sample> samples;
	int used = 0;
	bool _warned = false;

	void Add(const AIS::Message& m, const TAG& tag);
	void Clear();
	void resetSamples(); // caller must hold mtx

public:
	PrometheusCounter();
	virtual ~PrometheusCounter() {}

	void Reset();

	int getCount() { return _count; }

	void Receive(const JSON::JSON* json, int len, TAG& tag);
	std::string toPrometheus();
};
