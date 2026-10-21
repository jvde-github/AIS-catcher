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


#include "ViewFilter.h"
#include "Ships.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

const float ViewFilter::MOVING_KNOTS = 0.5f;

namespace
{
	const char *const KEYS[] = {"stations", "hidden_buckets", "hidden_classes", "hidden_statuses", "speed_min", "speed_max",
								"seen", "distance_min", "distance_max", "validated", "repeated", "mmsi_allow", "mmsi_deny"};
	const char *const BUCKETS[] = {"am", "as", "bm", "bs", "aton", "base", "sarte", "air"};

	std::vector<std::string> split(const std::string &s)
	{
		std::vector<std::string> out;
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, ','))
			if (!item.empty())
				out.push_back(item);
		return out;
	}

	long whole(const std::string &key, const std::string &v, long min, long max)
	{
		char *end = nullptr;
		const long n = std::strtol(v.c_str(), &end, 10);
		if (v.empty() || !end || *end || n < min || n > max)
			throw std::runtime_error(key + " takes whole numbers from " + std::to_string(min) + " to " + std::to_string(max));
		return n;
	}

	float real(const std::string &key, const std::string &v)
	{
		char *end = nullptr;
		const double n = std::strtod(v.c_str(), &end);
		if (v.empty() || !end || *end || n < 0 || n > 100000)
			throw std::runtime_error(key + " takes a number from 0");
		return (float)n;
	}

	template <typename T>
	bool has(const std::vector<T> &list, const T &v) { return std::find(list.begin(), list.end(), v) != list.end(); }
}

bool ViewFilter::isKey(const std::string &key)
{
	for (const char *k : KEYS)
		if (key == k)
			return true;
	return false;
}

void ViewFilter::set(const std::string &key, const std::string &value)
{
	if (key == "stations")
		for (const auto &v : split(value))
			stations.push_back((int)whole(key, v, 0, 2147483647));
	else if (key == "hidden_buckets")
		for (const auto &v : split(value))
		{
			bool known = false;
			for (const char *b : BUCKETS)
				known |= v == b;
			if (!known)
				throw std::runtime_error("hidden_buckets takes am, as, bm, bs, aton, base, sarte or air");
			hidden_buckets.push_back(v);
		}
	else if (key == "hidden_classes")
		for (const auto &v : split(value))
			hidden_classes.push_back((int)whole(key, v, 0, 13));
	else if (key == "hidden_statuses")
		for (const auto &v : split(value))
			hidden_statuses.push_back((int)whole(key, v, 0, 15));
	else if (key == "speed_min")
		has_speed_min = true, speed_min = real(key, value);
	else if (key == "speed_max")
		has_speed_max = true, speed_max = real(key, value);
	else if (key == "seen")
		seen = (int)whole(key, value, 0, 7 * 24 * 60);
	else if (key == "distance_min")
		has_distance_min = true, distance_min = real(key, value);
	else if (key == "distance_max")
		has_distance_max = true, distance_max = real(key, value);
	else if (key == "validated")
	{
		if (value != "any" && value != "yes" && value != "no" && value != "pending")
			throw std::runtime_error("validated takes any, yes, no or pending");
		validated = value;
	}
	else if (key == "repeated")
	{
		if (value != "any" && value != "only" && value != "never")
			throw std::runtime_error("repeated takes any, only or never");
		repeated = value;
	}
	else if (key == "mmsi_allow" || key == "mmsi_deny")
		for (const auto &v : split(value))
			(key == "mmsi_allow" ? mmsi_allow : mmsi_deny).push_back((uint32_t)whole(key, v, 0, 999999999));
	else
		throw std::runtime_error("\"" + key + "\" is not a filter term");
}

const char *ViewFilter::bucketOf(int shipclass, float speed)
{
	const bool underway = speed != SPEED_UNDEFINED && speed > MOVING_KNOTS;
	switch (shipclass)
	{
	case CLASS_ATON: return "aton";
	case CLASS_STATION: return "base";
	case CLASS_SARTEPIRB: return "sarte";
	case CLASS_PLANE:
	case CLASS_HELICOPTER: return "air";
	case CLASS_B: return underway ? "bm" : "bs";
	default: return underway ? "am" : "as";
	}
}

int ViewFilter::classFilterId(int shipclass)
{
	switch (shipclass)
	{
	case CLASS_CARGO:
	case CLASS_TANKER:
	case CLASS_PASSENGER:
	case CLASS_HIGHSPEED:
	case CLASS_SPECIAL:
	case CLASS_FISHING:
	case CLASS_B:
		return shipclass;
	default:
		return CLASS_OTHER;
	}
}

bool ViewFilter::passes(const Ship &s, std::time_t now) const
{
	if (!stations.empty())
	{
		bool heard = false;
		s.forEachHeard([&](int station, std::time_t t) { heard |= t >= now - Ship::STATION_WINDOW && has(stations, station); });
		if (!heard)
			return false;
	}
	if (!mmsi_allow.empty() && !has(mmsi_allow, s.mmsi))
		return false;
	if (has(mmsi_deny, s.mmsi))
		return false;

	const bool speed = s.speed != SPEED_UNDEFINED;
	if (!hidden_buckets.empty() && has(hidden_buckets, std::string(bucketOf(s.shipclass, s.speed))))
		return false;
	if (!hidden_classes.empty() && has(hidden_classes, classFilterId(s.shipclass)))
		return false;
	if (has_speed_min && !(speed && s.speed >= speed_min))
		return false;
	if (has_speed_max && !(speed && s.speed <= speed_max))
		return false;

	const int status = s.status >= 0 && s.status <= 15 ? s.status : 15;
	if (!hidden_statuses.empty() && has(hidden_statuses, status))
		return false;
	if (seen > 0 && !(now - s.last_signal <= (std::time_t)seen * 60))
		return false;
	// no distance without a position
	const bool distance = isValidCoord(s.lat, s.lon) && s.distance != DISTANCE_UNDEFINED;
	if (has_distance_min && !(distance && s.distance >= distance_min))
		return false;
	if (has_distance_max && !(distance && s.distance <= distance_max))
		return false;

	if (validated != "any")
	{
		const int want = validated == "yes" ? 1 : validated == "no" ? 2 : 0;
		if (s.getValidated() != want)
			return false;
	}
	if (repeated != "any" && (s.getRepeat() > 0) != (repeated == "only"))
		return false;
	return true;
}

bool ViewFilter::operator==(const ViewFilter &o) const
{
	return stations == o.stations && hidden_buckets == o.hidden_buckets && hidden_classes == o.hidden_classes &&
		   hidden_statuses == o.hidden_statuses && has_speed_min == o.has_speed_min && has_speed_max == o.has_speed_max &&
		   speed_min == o.speed_min && speed_max == o.speed_max && seen == o.seen && has_distance_min == o.has_distance_min &&
		   has_distance_max == o.has_distance_max && distance_min == o.distance_min && distance_max == o.distance_max &&
		   validated == o.validated && repeated == o.repeated && mmsi_allow == o.mmsi_allow && mmsi_deny == o.mmsi_deny;
}
