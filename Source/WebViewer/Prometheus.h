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
#include <string.h>
#include <mutex>

#include "AIS-catcher.h"
#include "JSONAIS.h"

class PromotheusCounter : public StreamIn<JSON::JSON> {
	std::mutex m;

	int _LONG_RANGE_CUTOFF = 2500;

	unsigned int _count;
	unsigned int _msg[27];
	unsigned int _channel[4];

	float _distance;

	std::string ppm;
	std::string level;

	void Add(const AIS::Message& m, const TAG& tag, bool new_vessel = false);
	void Clear();

public:
	void setCutOff(int c) { _LONG_RANGE_CUTOFF = c; }

	PromotheusCounter();
	virtual ~PromotheusCounter() {}

	void Reset();

	int getCount() { return _count; }
	void setCutoff(int cutoff) { _LONG_RANGE_CUTOFF = cutoff; }

	void Receive(const JSON::JSON* json, int len, TAG& tag);
	std::string toPrometheus();
};
