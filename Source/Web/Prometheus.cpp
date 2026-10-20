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

#include <algorithm>

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

PrometheusCounter::PrometheusCounter()
{
	Reset();
	Clear();
}

void PrometheusCounter::Clear()
{

	mtx.lock();
	std::memset(_msg, 0, sizeof(_msg));
	std::memset(_channel, 0, sizeof(_channel));

	_count = 0;
	_distance = 0;
	_dropped = 0;

	mtx.unlock();
}

void PrometheusCounter::Add(const AIS::Message &m, const TAG &tag)
{

	if (m.type() > 28 || m.type() < 1)
		return;
	if (tag.shipclass < 0 || tag.shipclass > 13)
		return;

	bool has_ppm = tag.ppm < 1000, has_level = tag.level < 1000;

	if (has_ppm || has_level)
	{
		if (samples.empty())
			samples.resize(SLOTS);

		uint32_t mmsi = m.mmsi();
		int station = m.getStation();
		char ch = m.getChannel() ? m.getChannel() : '?';

		// MAX_USED < SLOTS, so a free slot always ends the probe
		Sample *s = nullptr;
		uint32_t h = (mmsi ^ ((uint32_t)station << 20)) * 2654435761u;
		for (int i = h >> (32 - SLOT_BITS);; i = (i + 1) & (SLOTS - 1))
		{
			Sample &e = samples[i];

			if (!e.channel[0])
			{
				if (used < MAX_USED)
				{
					used++;
					e.mmsi = mmsi;
					e.station = station;
					s = &e;
				}
				break;
			}
			if (e.mmsi == mmsi && e.station == station)
			{
				s = &e;
				break;
			}
		}

		if (!s && !_warned)
		{
			Warning() << "Prometheus: more than " << (int)MAX_USED << " ships between two scrapes, ais_msg_ppm and ais_msg_level leave out the rest (ais_stat_samples_dropped); scrape more often";
			_warned = true;
		}

		int c = 0;
		while (s && c < 4 && s->channel[c] && s->channel[c] != ch)
			c++;

		if (!s || c == 4)
			_dropped++;
		else
		{
			s->channel[c] = ch;
			s->types[c] |= 1u << (m.type() - 1);
			s->speed = tag.speed < 0 ? 0 : (tag.speed > 0.5 ? 1 : 2);
			s->shipclass = (uint8_t)tag.shipclass;
			if (has_ppm)
			{
				s->ppm[c] = tag.ppm;
				s->has_ppm |= 1 << c;
			}
			if (has_level)
			{
				s->level[c] = tag.level;
				s->has_level |= 1 << c;
			}
		}
	}

	_count++;
	_msg[m.type() - 1]++;

	if (m.getChannel() >= 'A' && m.getChannel() <= 'D')
		_channel[m.getChannel() - 'A']++;

	if (tag.distance > _distance)
	{
		_distance = tag.distance;
	}
}

void PrometheusCounter::Receive(const JSON::JSON *json, int len, TAG &tag)
{
	AIS::Message &data = *((AIS::Message *)json[0].binary);

	mtx.lock();
	Add(data, tag);
	mtx.unlock();
}

void PrometheusCounter::resetSamples()
{
	std::fill(samples.begin(), samples.end(), Sample());
	used = 0;
}

void PrometheusCounter::Reset()
{
	mtx.lock();
	resetSamples();
	mtx.unlock();
}

std::string PrometheusCounter::toPrometheus()
{
	mtx.lock();
	std::string element;

	element += "# HELP ais_stat_count Total number of messages\n";
	element += "# TYPE ais_stat_count counter\n";
	element += "ais_stat_count " + std::to_string(_count) + "\n";

	element += "# HELP ais_stat_distance Longest distance\n";
	element += "# TYPE ais_stat_distance gauge\n";
	element += "ais_stat_distance " + std::to_string(_distance) + "\n";

	for (int i = 0; i < 4; i++)
	{
		std::string ch(1, i + 'A');
		element += "# HELP ais_stat_count_channel_" + ch + " Total number of messages on channel " + ch + "\n";
		element += "# TYPE ais_stat_count_channel_" + ch + " counter\n";
		element += "ais_stat_count_channel_" + ch + " " + std::to_string(_channel[i]) + "\n";
	}

	for (int i = 0; i < 28; i++)
	{
		std::string type = std::to_string(i + 1);
		element += "# HELP ais_stat_count_type_" + type + " Total number of messages of type " + type + "\n";
		element += "# TYPE ais_stat_count_type_" + type + " counter\n";
		element += "ais_stat_count_type_" + type + " " + std::to_string(_msg[i]) + "\n";
	}

	element += "# HELP ais_stat_samples_dropped Messages left out of ais_msg_ppm and ais_msg_level: more ships between scrapes than the table holds\n";
	element += "# TYPE ais_stat_samples_dropped counter\n";
	element += "ais_stat_samples_dropped " + std::to_string(_dropped) + "\n";

	static const char *speeds[] = {"Unknown", "Moving", "Stationary"};

	auto write = [&](const std::string &name, bool ppm) {
		element += "# HELP " + name + "\n# TYPE " + name + " gauge\n";
		for (const Sample &s : samples)
			for (int c = 0; c < 4 && s.channel[c]; c++)
			{
				if (!(((ppm ? s.has_ppm : s.has_level) >> c) & 1))
					continue;

				std::string rest = "\",mmsi=\"" + std::to_string(s.mmsi) + "\",station_id=\"" + std::to_string(s.station) + "\",speed=\"" + speeds[s.speed] + "\",shipclass=\"" + ShippingClassNames[s.shipclass] + "\",channel=\"" + std::string(1, s.channel[c]) + "\"} " + std::to_string(ppm ? s.ppm[c] : s.level[c]) + "\n";

				for (int t = 1; t <= 28; t++)
					if ((s.types[c] >> (t - 1)) & 1)
						element += name + "{type=\"" + std::to_string(t) + rest;
			}
	};

	write("ais_msg_ppm", true);
	write("ais_msg_level", false);

	// drain must share the lock with the read or samples in between are lost
	resetSamples();
	mtx.unlock();
	return element;
}
