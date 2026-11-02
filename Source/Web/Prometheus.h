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
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>

#include "AIS-catcher.h"
#include "JSONAIS.h"
#include "SlotTable.h"
#include "Statistics.h"
#include "Device/Device.h"

// Counts the messages one input (receiver/model pair) decodes. Models of one
// receiver share its device, so they report the bytes of the first model's counter.
struct PrometheusInput : public StreamIn<JSON::JSON> {
	std::string label, type;
	std::atomic<uint32_t> count{0};
	std::atomic<uint32_t> errors[3] = {}; // checksum, undersized, oversized
	std::atomic<int64_t> last{0};
	ByteCounter bytes_own;
	const ByteCounter *bytes = &bytes_own;
	Device::Device *device = nullptr;

	explicit PrometheusInput(const std::string &l) : label(l) {}
	void Receive(const JSON::JSON *, int len, TAG &tag)
	{
		count.fetch_add(len, std::memory_order_relaxed);
		last.store((int64_t)std::time(nullptr), std::memory_order_relaxed);
		if (tag.quality & MESSAGE_QUALITY_CHECKSUM)
			errors[0].fetch_add(1, std::memory_order_relaxed);
		if (tag.quality & MESSAGE_QUALITY_UNDERSIZED)
			errors[1].fetch_add(1, std::memory_order_relaxed);
		if (tag.quality & MESSAGE_QUALITY_OVERSIZED)
			errors[2].fetch_add(1, std::memory_order_relaxed);
	}
};

// Escapes a value for use inside a Prometheus label
std::string prometheusEscape(const std::string &s);

class PrometheusCounter : public StreamIn<JSON::JSON> {
	std::mutex mtx;

	unsigned int _count = 0;
	unsigned int _msg[28] = {};
	unsigned int _channel[4] = {};
	float _distance = 0;
	unsigned int _dropped = 0;

	// A vessel as heard from any receiver or station: per channel the latest ppm
	// and level, each with the type of the message that carried it, and the
	// station of the latest message for the station_id label
	struct Sample {
		char channel[4];
		uint8_t ppm_type[4], level_type[4];
		float ppm[4], level[4];
		std::time_t last;
		int station;
		uint8_t speed, shipclass;

		Sample() : channel(), ppm_type(), level_type(), last(0), station(0), speed(0), shipclass(0)
		{
			for (int c = 0; c < 4; c++) {
				ppm[c] = PPM_UNDEFINED;
				level[c] = LEVEL_UNDEFINED;
			}
		}
	};

	static const int CAPACITY = 768, BUCKETS = 1024;
	// seconds a vessel's values are written after it was last heard: as long as
	// Prometheus waits before it takes a series to be gone
	static const int WINDOW = 300;
	static const int WARN_EVERY = 3600;

	SlotTable<Sample, uint64_t> samples;
	// vessels are kept only once something reads the metrics
	bool scraped = false;
	std::time_t warned = 0;

	void Add(const AIS::Message &m, const TAG &tag);

public:
	void Receive(const JSON::JSON *json, int len, TAG &tag);
	std::string toPrometheus();
};
