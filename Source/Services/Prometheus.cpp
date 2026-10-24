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
#include <vector>

#include "Prometheus.h"
#include "Logger.h"

const std::string ShippingClassNames[] = {
	"Other",							  // CLASS_OTHER
	"Unknown",							  // CLASS_UNKNOWN
	"Cargo",							  // CLASS_CARGO
	"Class B",							  // CLASS_B
	"Passenger",						  // CLASS_PASSENGER
	"Special",							  // CLASS_SPECIAL
	"Tanker",							  // CLASS_TANKER
	"High Speed",						  // CLASS_HIGHSPEED
	"Fishing",							  // CLASS_FISHING
	"Plane",							  // CLASS_PLANE
	"Helicopter",						  // CLASS_HELICOPTER
	"Station",							  // CLASS_STATION
	"Aid-to-Navigation",				  // CLASS_ATON
	"Search and Rescue Transponder EPIRB" // CLASS_SARTEPIRB
};

void PrometheusCounter::Add(const AIS::Message &m, const TAG &tag)
{
	if (m.type() > 28 || m.type() < 1)
		return;
	if (tag.shipclass < 0 || tag.shipclass > 13)
		return;

	const bool has_ppm = tag.ppm < 1000, has_level = tag.level < 1000;

	if (scraped && (has_ppm || has_level))
	{
		const std::time_t now = std::time(nullptr);
		const uint64_t key = (uint64_t)m.mmsi() + 1; // 0 marks an empty slot
		int h = samples.find(key);
		if (h == samples.NIL)
		{
			h = samples.create(key);
			// the vessel heard longest ago gives way; losing one heard within the window is a drop
			if (samples[h].last && now - samples[h].last < WINDOW)
			{
				_dropped++;
				if (now - warned >= WARN_EVERY)
				{
					Warning() << "Prometheus: more than " << (int)CAPACITY << " vessels heard within five minutes, ais_msg_ppm and ais_msg_level leave out the ones heard longest ago (ais_stat_samples_dropped)";
					warned = now;
				}
			}
			samples[h] = Sample();
		}
		else
			samples.touch(h);

		// the scrape walks the table in the order last heard, so `last` must follow every touch
		Sample &s = samples[h];
		s.last = now;
		s.station = m.getStationOrDefault();
		const char ch = m.getChannel() ? m.getChannel() : '?';
		int c = 0;
		while (c < 4 && s.channel[c] && s.channel[c] != ch)
			c++;

		if (c == 4)
			_dropped++;
		else
		{
			s.channel[c] = ch;
			s.speed = tag.speed < 0 ? 0 : (tag.speed > 0.5 ? 1 : 2);
			s.shipclass = (uint8_t)tag.shipclass;
			if (has_ppm)
			{
				s.ppm[c] = tag.ppm;
				s.ppm_type[c] = (uint8_t)m.type();
			}
			if (has_level)
			{
				s.level[c] = tag.level;
				s.level_type[c] = (uint8_t)m.type();
			}
		}
	}

	_count++;
	_msg[m.type() - 1]++;

	if (m.getChannel() >= 'A' && m.getChannel() <= 'D')
		_channel[m.getChannel() - 'A']++;

	if (tag.distance > _distance)
		_distance = tag.distance;
}

void PrometheusCounter::Receive(const JSON::JSON *json, int len, TAG &tag)
{
	AIS::Message &data = *((AIS::Message *)json[0].binary);

	std::lock_guard<std::mutex> lock(mtx);
	Add(data, tag);
}

std::string PrometheusCounter::toPrometheus()
{
	// copied under the lock, written outside it
	struct Row
	{
		uint32_t mmsi;
		Sample s;
	};
	std::vector<Row> rows;
	unsigned int count, msg[28], channel[4], dropped;
	float distance;
	const std::time_t now = std::time(nullptr);
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (!scraped)
		{
			samples.setup(CAPACITY, BUCKETS);
			scraped = true;
		}
		count = _count;
		dropped = _dropped;
		distance = _distance;
		std::memcpy(msg, _msg, sizeof(msg));
		std::memcpy(channel, _channel, sizeof(channel));
		rows.reserve(samples.size());
		// newest first, so the first vessel outside the window ends the walk
		samples.forEach([&](int h) {
			if (now - samples[h].last >= WINDOW)
				return false;
			rows.push_back({(uint32_t)(samples.key(h) - 1), samples[h]});
			return true;
		});
	}

	std::string element;

	element += "# HELP ais_stat_count Total number of messages\n";
	element += "# TYPE ais_stat_count counter\n";
	element += "ais_stat_count " + std::to_string(count) + "\n";

	element += "# HELP ais_stat_distance Longest distance\n";
	element += "# TYPE ais_stat_distance gauge\n";
	element += "ais_stat_distance " + std::to_string(distance) + "\n";

	for (int i = 0; i < 4; i++)
	{
		std::string ch(1, i + 'A');
		element += "# HELP ais_stat_count_channel_" + ch + " Total number of messages on channel " + ch + "\n";
		element += "# TYPE ais_stat_count_channel_" + ch + " counter\n";
		element += "ais_stat_count_channel_" + ch + " " + std::to_string(channel[i]) + "\n";
	}

	for (int i = 0; i < 28; i++)
	{
		std::string type = std::to_string(i + 1);
		element += "# HELP ais_stat_count_type_" + type + " Total number of messages of type " + type + "\n";
		element += "# TYPE ais_stat_count_type_" + type + " counter\n";
		element += "ais_stat_count_type_" + type + " " + std::to_string(msg[i]) + "\n";
	}

	static const char *speeds[] = {"Unknown", "Moving", "Stationary"};

	auto write = [&](const char *name, bool ppm) {
		element += std::string("# HELP ") + name + "\n# TYPE " + name + " gauge\n";
		char line[256];
		// in the order heard: the walk collected them newest first
		for (auto it = rows.rbegin(); it != rows.rend(); ++it)
		{
			const Row &r = *it;
			for (int c = 0; c < 4 && r.s.channel[c]; c++)
			{
				const float value = ppm ? r.s.ppm[c] : r.s.level[c];
				if (value == (ppm ? PPM_UNDEFINED : LEVEL_UNDEFINED))
					continue;
				const int n = snprintf(line, sizeof(line), "%s{type=\"%d\",mmsi=\"%u\",station_id=\"%d\",speed=\"%s\",shipclass=\"%s\",channel=\"%c\"} %f\n",
									   name, ppm ? r.s.ppm_type[c] : r.s.level_type[c], (unsigned)r.mmsi, r.s.station, speeds[r.s.speed],
									   ShippingClassNames[r.s.shipclass].c_str(), r.s.channel[c], (double)value);
				if (n > 0 && n < (int)sizeof(line))
					element.append(line, n);
			}
		}
	};

	write("ais_msg_ppm", true);
	write("ais_msg_level", false);

	element += "# HELP ais_stat_samples_dropped Messages left out of ais_msg_ppm and ais_msg_level: more vessels heard within five minutes than the table holds, or a vessel on more than four channels\n";
	element += "# TYPE ais_stat_samples_dropped counter\n";
	element += "ais_stat_samples_dropped " + std::to_string(dropped) + "\n";

	return element;
}

namespace Services
{
	int Metrics::scope(const Request &request) const
	{
		return request.path == "/metrics" ? (int)ApiKey::DATA : NOT_MINE;
	}

	bool Metrics::handle(const Request &request, Response &response)
	{
		if (scope(request) == NOT_MINE)
			return false;
		if (request.method != "GET")
		{
			response = Response::problem(405, "Method not allowed", "The counts are read here, not written.");
			response.header = "Allow: GET, HEAD";
			return true;
		}
		response.type = "text/plain";
		response.body = counter.toPrometheus();
		response.cors = true;
		return true;
	}
}
