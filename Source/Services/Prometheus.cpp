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
#include "Tracker.h"
#include "Logger.h"

const std::string ShippingClassNames[] = {
	"Other",
	"Unknown",
	"Cargo",
	"Class B",
	"Passenger",
	"Special",
	"Tanker",
	"High Speed",
	"Fishing",
	"Plane",
	"Helicopter",
	"Station",
	"Aid-to-Navigation",
	"Search and Rescue Transponder EPIRB"
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

std::string PrometheusCounter::toPrometheus(bool hide_range)
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

	if (!(hide_range && distance > 0))
	{
		element += "# HELP ais_stat_distance Longest distance\n";
		element += "# TYPE ais_stat_distance gauge\n";
		element += "ais_stat_distance " + std::to_string(distance) + "\n";
	}

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

	// a label value: backslash, quote and newline escaped
	static std::string label(const std::string &s)
	{
		std::string r;
		for (char c : s)
		{
			if (c == '\\' || c == '"')
				r += '\\';
			if (c == '\n')
				r += "\\n";
			else
				r += c;
		}
		return r;
	}

	std::string Metrics::engineMetrics()
	{
		if (!facts)
			return std::string();
		const SystemFacts f = facts();
		std::string out;
		const auto head = [&](const char *name, const char *type, const char *help) {
			out += std::string("# HELP ") + name + " " + help + "\n# TYPE " + name + " " + type + "\n";
		};
		const auto inputLabels = [](const ReceiverState &r, const std::string &extra) {
			return "{input=\"" + std::to_string(r.index) + "\",type=\"" + label(r.input) + "\",description=\"" + label(r.label) + "\"" + extra + "}";
		};
		const auto inputs = [&](const char *name, const char *type, const char *help, bool connection, const std::function<uint64_t(const ReceiverState &)> &value) {
			head(name, type, help);
			for (const auto &r : f.receivers)
				if (!connection || r.connection)
					out += std::string(name) + inputLabels(r, "") + " " + std::to_string(value(r)) + "\n";
		};
		inputs("ais_input_count", "counter", "Total number of messages decoded per input", false, [](const ReceiverState &r) { return r.messages; });
		inputs("ais_input_bytes_in", "counter", "Total number of raw bytes received per input", false, [](const ReceiverState &r) { return r.bytes; });
		inputs("ais_input_up", "gauge", "Whether the input is streaming (1) or not (0)", false, [](const ReceiverState &r) { return (uint64_t)(r.streaming && !r.lost); });
		inputs("ais_input_last_message_timestamp_seconds", "gauge", "Unix time of the last message decoded per input, 0 if none yet", false, [&](const ReceiverState &r) { return (uint64_t)tracker.lastHeard(r.station); });
		head("ais_input_errors", "counter", "Messages decoded with errors per input");
		static const char *const kinds[] = {"checksum", "undersized", "oversized"};
		for (const auto &r : f.receivers)
		{
			uint64_t errors[3] = {0, 0, 0};
			tracker.withStation(r.station, [&](Figures &fig) {
				const MessageStatistics::Values v = fig.total.values();
				errors[0] = v.errors[3];
				errors[1] = v.errors[1];
				errors[2] = v.errors[2];
			});
			for (int k = 0; k < 3; k++)
				out += "ais_input_errors" + inputLabels(r, std::string(",error=\"") + kinds[k] + "\"") + " " + std::to_string(errors[k]) + "\n";
		}
		inputs("ais_input_connect_ok", "counter", "Successful connections per input", true, [](const ReceiverState &r) { return r.connect_ok; });
		inputs("ais_input_connect_fail", "counter", "Failed connections per input", true, [](const ReceiverState &r) { return r.connect_fail; });
		inputs("ais_input_reconnects", "counter", "Reconnects per input", true, [](const ReceiverState &r) { return r.reconnects; });
		inputs("ais_input_connected", "gauge", "Whether the input has an established connection (1) or not (0)", true, [](const ReceiverState &r) { return r.connected; });

		const auto outputs = [&](const char *name, const char *type, const char *help, bool connection, const std::function<uint64_t(const OutputFacts &)> &value) {
			head(name, type, help);
			int k = 0;
			for (const auto &o : f.outputs)
			{
				k++;
				if (!connection || o.connection)
					out += std::string(name) + "{output=\"" + std::to_string(k) + "\",type=\"" + label(o.type) + "\",description=\"" + label(o.label) + "\"} " + std::to_string(value(o)) + "\n";
			}
		};
		outputs("ais_output_count", "counter", "Total number of messages passed to the output after filtering", false, [](const OutputFacts &o) { return o.messages; });
		outputs("ais_output_bytes_out", "counter", "Bytes sent per output", false, [](const OutputFacts &o) { return o.bytes_out; });
		outputs("ais_output_bytes_in", "counter", "Bytes received per output", false, [](const OutputFacts &o) { return o.bytes_in; });
		outputs("ais_output_connect_ok", "counter", "Successful connections per output", true, [](const OutputFacts &o) { return o.connect_ok; });
		outputs("ais_output_connect_fail", "counter", "Failed connections per output", true, [](const OutputFacts &o) { return o.connect_fail; });
		outputs("ais_output_reconnects", "counter", "Reconnects per output", true, [](const OutputFacts &o) { return o.reconnects; });
		outputs("ais_output_connected", "gauge", "Connections currently established per output", true, [](const OutputFacts &o) { return o.connected; });
		outputs("ais_output_dropped", "counter", "Messages dropped per output", false, [](const OutputFacts &o) { return o.dropped; });
		return out;
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
		response.body = counter.toPrometheus(!located(tracker, spec, stations()).all({})) + engineMetrics();
		response.cors = true;
		return true;
	}
}
