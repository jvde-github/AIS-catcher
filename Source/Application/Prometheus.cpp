/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include "Prometheus.h"

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

PromotheusCounter::PromotheusCounter()
{
	Reset();
	Clear();
}

void PromotheusCounter::Clear()
{

	m.lock();
	std::memset(_msg, 0, sizeof(_msg));
	std::memset(_channel, 0, sizeof(_channel));

	_count = 0;
	_distance = 0;

	m.unlock();
}

void PromotheusCounter::Add(const AIS::Message &m, const TAG &tag, bool new_vessel)
{

	if (m.type() > 27 || m.type() < 1)
		return;
	if (tag.shipclass < 0 || tag.shipclass > 13)
		return;

	std::string speed = tag.speed < 0 ? "Unknown" : (tag.speed > 0.5 ? "Moving" : "Stationary");

	if (tag.ppm < 1000)
		ppm += "ais_msg_ppm{type=\"" + std::to_string(m.type()) + "\",mmsi=\"" + std::to_string(m.mmsi()) + "\",station_id=\"" + std::to_string(m.getStation()) + "\",speed=\"" + speed + "\",shipclass=\"" + ShippingClassNames[tag.shipclass] + "\",channel=\"" + std::string(1, m.getChannel()) + "\"} " + std::to_string(tag.ppm) + "\n";

	if (tag.level < 1000)
		level += "ais_msg_level{type=\"" + std::to_string(m.type()) + "\",mmsi=\"" + std::to_string(m.mmsi()) + "\",station_id=\"" + std::to_string(m.getStation()) + "\",speed=\"" + speed + "\",shipclass=\"" + ShippingClassNames[tag.shipclass] + "\",channel=\"" + std::string(1, m.getChannel()) + "\"} " + std::to_string(tag.level) + "\n";

	_count++;
	_msg[m.type() - 1]++;

	if (m.getChannel() >= 'A' && m.getChannel() <= 'D')
		_channel[m.getChannel() - 'A']++;

	if (tag.distance > _distance)
	{
		_distance = tag.distance;
	}
}

void PromotheusCounter::Receive(const JSON::JSON *json, int len, TAG &tag)
{
	AIS::Message &data = *((AIS::Message *)json[0].binary);

	if (ppm.size() > 32768 || level.size() > 32768)
	{
		return;
	}

	m.lock();

	Add(data, tag);

	m.unlock();
}

void PromotheusCounter::Reset()
{
	m.lock();
	ppm = "# HELP ais_msg_ppm\n# TYPE ais_msg_ppm gauge\n";
	level = "# HELP ais_msg_level\n# TYPE ais_msg_level gauge\n";
	m.unlock();
}

std::string PromotheusCounter::toPrometheus()
{
	m.lock();
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

	for (int i = 0; i < 27; i++)
	{
		std::string type = std::to_string(i + 1);
		element += "# HELP ais_stat_count_type_" + type + " Total number of messages of type " + type + "\n";
		element += "# TYPE ais_stat_count_type_" + type + " counter\n";
		element += "ais_stat_count_type_" + type + " " + std::to_string(_msg[i]) + "\n";
	}

	element += ppm + level;
	m.unlock();
	return element;
}
