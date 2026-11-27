/*
	Copyright(c) 2026 jvde.github@gmail.com

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
#include <cmath>
#include <functional>

#include "Convert.h"
#include "Gnss.h"

static const uint32_t GNSS_VERSION = 1;
static const std::size_t MAX_ROWS = 320;

static int group(int system, int band)
{
	static const int index[] = {-1, 0, 1, 2, 3}; // OTHER, GPS, GLONASS, GALILEO, BEIDOU
	return system >= 1 && system <= 4 ? index[system] * AIS::Sky::BANDS + band : -1;
}

// the average signal kept in a byte: 1 to 99 dB-Hz, 0 for not heard
static uint8_t level(float sum, uint32_t n)
{
	return n ? (uint8_t)std::max(1.0f, std::min(99.0f, std::round(sum / n))) : 0;
}

// NORTH: metres a degree of latitude; a degree of longitude is that times the cosine of the latitude
static const double RAD = 3.14159265358979323846 / 180, NORTH = 6371008.8 * RAD;

GnssHistory::Slot GnssHistory::Sums::slot() const
{
	Slot s;
	s.t = t;
	s.reports = (uint16_t)std::min<uint32_t>(reports, 65535);
	s.fixes = (uint16_t)std::min<uint32_t>(fixes, 65535);
	s.fix = (uint8_t)fix;
	if (reports)
	{
		s.used = (uint8_t)std::min<uint32_t>(255, (used + reports / 2) / reports);
		s.seen = (uint8_t)std::min<uint32_t>(255, (seen + reports / 2) / reports);
	}
	s.hdop = (uint8_t)std::min(255.0f, std::round(hdop * 10));
	for (int g = 0; g < GROUPS; g++)
		s.signal[g] = level(signal[g], heard[g]);
	if (fixes)
	{
		s.lat = lat / fixes;
		s.lon = lon / fixes;
		s.at_lat = at_lat;
		s.at_lon = at_lon;
	}
	return s;
}

bool GnssHistory::Span::at(int32_t t, Slot &out) const
{
	if (t == open.t)
		out = open.slot();
	else if (t >= 0 && ring[t % places].t == t)
		out = ring[t % places];
	else
		return false;
	return out.reports || out.fixes;
}

// the slots and rows of one satellite and band are closed together with the day's slot
void GnssHistory::closeRows(int32_t closed, int32_t next)
{
	for (auto it = rows.begin(); it != rows.end();)
	{
		Row &r = it->second;
		r.v[closed % DAY_SLOTS] = level(r.sum, r.n);
		for (int32_t t = closed + 1; t < next && t < closed + DAY_SLOTS; t++)
			r.v[t % DAY_SLOTS] = 0;
		r.sum = 0;
		r.n = 0;
		if (r.last <= next - DAY_SLOTS)
			it = rows.erase(it);
		else
			++it;
	}
}

void GnssHistory::roll(Span &s, int32_t t)
{
	if (s.open.t == t)
		return;
	if (s.open.t >= 0)
	{
		s.ring[s.open.t % s.places] = s.open.slot();
		if (&s == &day)
		{
			closeRows(s.open.t, t);
			mark(s.open.t);
		}
	}
	s.open = Sums();
	s.open.t = t;
}

void GnssHistory::add(std::time_t now, const AIS::GPS &gps)
{
	const int32_t t = (int32_t)(now / day.interval);
	roll(day, t);
	roll(week, (int32_t)(now / week.interval));
	heard = now;
	if (gps.hasPosition())
		fixed = now;

	for (Span *s : {&day, &week})
	{
		Sums &o = s->open;
		if (gps.sky)
		{
			const AIS::Sky &sky = *gps.sky;
			o.reports++;
			if (sky.fix > 0)
				o.fix = o.fix ? std::min(o.fix, sky.fix) : sky.fix;
			o.hdop = std::max(o.hdop, sky.hdop);
			for (const auto &sat : sky.satellites)
			{
				o.used += sat.used;
				o.seen++;
				for (int b = 0; b < AIS::Sky::BANDS; b++)
				{
					const int g = group(sat.system, b);
					if (g >= 0 && sat.signal[b] > 0)
					{
						o.signal[g] += sat.signal[b];
						o.heard[g]++;
					}
				}
			}
		}
		if (gps.hasPosition())
		{
			o.fixes++;
			o.lat += gps.getLat();
			o.lon += gps.getLon();
			o.at_lat = gps.getLat();
			o.at_lon = gps.getLon();
		}
	}

	if (!gps.sky)
		return;
	for (const auto &sat : gps.sky->satellites)
		for (int b = 0; b < AIS::Sky::BANDS; b++)
		{
			if (sat.signal[b] <= 0 || sat.id <= 0 || sat.id > 16383)
				continue;
			const uint32_t key = (uint32_t)sat.system << 16 | (uint32_t)sat.id << 2 | (uint32_t)b;
			auto it = rows.find(key);
			if (it == rows.end())
			{
				if (rows.size() >= MAX_ROWS)
					continue;
				it = rows.emplace(key, Row()).first;
			}
			it->second.sum += sat.signal[b];
			it->second.n++;
			it->second.last = t;
		}
}

double GnssHistory::median(std::vector<double> v)
{
	if (v.empty())
		return 0;
	std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
	return v[v.size() / 2];
}

void GnssHistory::mark(int32_t t)
{
	const auto held = [&](int32_t k) { return k >= 0 && day.ring[k % DAY_SLOTS].t == k; };
	const auto jammed = [&](int32_t k) { return k >= 0 && marks[k % DAY_SLOTS].t == k && marks[k % DAY_SLOTS].jam; };
	const auto clamp = [](double v) { return (int8_t)std::max(-99.0, std::min(99.0, std::round(v))); };
	std::vector<double> d1, d5, levels;
	for (const auto &r : rows)
	{
		const int b = (int)(r.first & 3);
		const uint8_t now = r.second.v[t % DAY_SLOTS];
		if (b != AIS::Sky::L1 && b != AIS::Sky::L5)
			continue;
		if (b == AIS::Sky::L1 && now)
			levels.push_back(now);
		// a signal gone while the GPS still reports is the deepest drop there is; one that was not there the
		// slot before is a satellite that set, or one a jam already took
		if (!now && !(b == AIS::Sky::L1 && held(t - 1) && r.second.v[(t - 1) % DAY_SLOTS]))
			continue;
		// against its own half hour before; slots of a jam that goes on are passed over, not counted
		std::vector<double> before;
		int seen = 0;
		for (int32_t k = t - 1; k >= t - 90 && seen < 15 && before.size() < 8; k--)
		{
			if (jammed(k))
				continue;
			seen++;
			if (held(k) && r.second.v[k % DAY_SLOTS])
				before.push_back(r.second.v[k % DAY_SLOTS]);
		}
		if (before.size() >= 3)
			(b == AIS::Sky::L1 ? d1 : d5).push_back(median(before) - now);
	}
	Mark m;
	m.t = t;
	m.level = levels.empty() ? 0 : (uint8_t)median(levels);
	// with no satellite left to compare, a jam goes on for as long as the signals stay under what they were before it
	if (d1.size() < 4 && jammed(t - 1))
	{
		int32_t k = t - 1;
		while (k > t - DAY_SLOTS && jammed(k))
			k--;
		const Mark &was = marks[(k < 0 ? 0 : k) % DAY_SLOTS];
		if (k >= 0 && was.t == k && was.level && was.level - m.level >= 6)
		{
			m.jam = true;
			m.drop1 = clamp(was.level - m.level);
			m.sats = (uint8_t)std::min<std::size_t>(255, levels.size());
		}
	}
	if (d1.size() >= 4)
	{
		const long big = std::count_if(d1.begin(), d1.end(), [](double x) { return x >= 6; });
		m.jam = big >= std::max(4.0, 0.6 * d1.size());
		m.drop1 = clamp(median(d1));
		m.sats = (uint8_t)std::min<std::size_t>(255, d1.size());
		if ((m.has5 = d5.size() >= 3))
			m.drop5 = clamp(median(d5));
	}
	marks[t % DAY_SLOTS] = m;
}

void GnssHistory::json(JSON::Writer &w, std::time_t now, bool weekly, int last) const
{
	const Span &s = weekly ? week : day;
	const int32_t end = (int32_t)(now / s.interval), first = end - s.places + 1;
	// the slots told: all of them, or the newest ones. The reference and the events are of the whole span either way.
	const int from = last > 0 && last < s.places ? s.places - last : 0;
	std::vector<Slot> slots(s.places);
	std::vector<char> held(s.places);
	std::vector<double> lats, lons;
	for (int i = 0; i < s.places; i++)
		if ((held[i] = s.at(first + i, slots[i])) && slots[i].fixes)
		{
			lats.push_back(slots[i].lat);
			lons.push_back(slots[i].lon);
		}
	// the reference is the median position of the span, so that an hour astray does not move it
	const double ref_lat = median(lats), ref_lon = median(lons), east = NORTH * std::cos(ref_lat * RAD);
	const auto metres = [](double v) { return std::round(v * 100) / 100; };

	w.beginObject().kv("interval", s.interval).kv("end", (long long)end * s.interval).kv("now", (long long)now);
	const auto series = [&](const char *name, int (*value)(const Slot &)) {
		w.key(name).beginArray();
		for (int i = from; i < s.places; i++)
			held[i] ? w.val(value(slots[i])) : w.val_null();
		w.endArray();
	};
	series("reports", [](const Slot &x) { return (int)x.reports; });
	series("fix", [](const Slot &x) { return (int)x.fix; });
	series("used", [](const Slot &x) { return (int)x.used; });
	series("seen", [](const Slot &x) { return (int)x.seen; });
	series("hdop", [](const Slot &x) { return (int)x.hdop; });

	w.key("signal").beginObject();
	for (int g = 0; g < GROUPS; g++)
	{
		bool any = false;
		for (int i = from; i < s.places && !any; i++)
			any = held[i] && slots[i].signal[g];
		if (!any)
			continue;
		const std::string name = std::to_string(g / AIS::Sky::BANDS + 1) + "." + std::to_string(g % AIS::Sky::BANDS);
		w.key(name).beginArray();
		for (int i = from; i < s.places; i++)
			held[i] && slots[i].signal[g] ? w.val((int)slots[i].signal[g]) : w.val_null();
		w.endArray();
	}
	w.endObject();

	const auto offsets = [&](const char *name, bool last) {
		w.key(name).beginArray();
		for (int i = from; i < s.places; i++)
		{
			const Slot &x = slots[i];
			if (!held[i] || !x.fixes)
			{
				w.val_null();
				continue;
			}
			const double lat = last ? x.at_lat : x.lat, lon = last ? x.at_lon : x.lon;
			w.beginArray().val(metres((lon - ref_lon) * east)).val(metres((lat - ref_lat) * NORTH)).endArray();
		}
		w.endArray();
	};
	offsets("position", false);
	offsets("fixes", true);

	if (!weekly)
	{
		w.key("satellites").beginArray();
		std::string bytes(s.places - from, '\0');
		for (const auto &r : rows)
		{
			bool any = false;
			for (int i = from; i < s.places; i++)
			{
				const uint8_t v = value(r.second, first + i);
				bytes[i - from] = (char)v;
				any |= v != 0;
			}
			if (any)
				w.beginArray().val((int)(r.first >> 16)).val((int)(r.first >> 2 & 16383)).val((int)(r.first & 3)).val(Util::Convert::toBase64(bytes)).endArray();
		}
		w.endArray();

		w.key("events").beginArray();
		for (const Event &e : events(first, slots, held, ref_lat, ref_lon))
		{
			static const char *kinds[] = {"jam", "gap", "jump"};
			w.beginObject().kv("kind", kinds[e.kind == 'j' ? 0 : e.kind == 'g' ? 1 : 2]).kv("from", e.from).kv("to", e.to);
			if (e.kind == 'j')
			{
				w.kv("l1", e.l1).kv("used_min", e.used_min).kv("fix", e.fix).kv("satellites", e.satellites).kv("off", std::round(e.off * 10) / 10);
				e.has_l5 ? w.kv("l5", e.l5) : w.kv_null("l5");
				e.used_before >= 0 ? w.kv("used_before", e.used_before) : w.kv_null("used_before");
			}
			else if (e.kind == 's')
				w.kv("off", std::round(e.off * 10) / 10);
			w.endObject();
		}
		w.endArray();
	}
	w.endObject();
}

uint8_t GnssHistory::value(const Row &r, int32_t t) const
{
	if (t == day.open.t)
		return level(r.sum, r.n);
	if (t >= 0 && day.ring[t % DAY_SLOTS].t == t)
		return r.v[t % DAY_SLOTS];
	return 0;
}

std::vector<GnssHistory::Event> GnssHistory::events(int32_t first, const std::vector<Slot> &slots, const std::vector<char> &held, double ref_lat, double ref_lon) const
{
	const int n = day.places;
	std::vector<char> flagged(n, 0);
	std::vector<const Mark *> at(n, nullptr);
	for (int i = 0; i < n; i++)
	{
		const Mark &m = marks[(first + i) % DAY_SLOTS];
		if (first + i >= 0 && m.t == first + i)
		{
			at[i] = &m;
			flagged[i] = m.jam;
		}
	}

	const double east = NORTH * std::cos(ref_lat * RAD);
	const auto off = [&](int i) { return held[i] && slots[i].fixes ? std::hypot((slots[i].lon - ref_lon) * east, (slots[i].lat - ref_lat) * NORTH) : 0.0; };

	std::vector<Event> out;
	const auto runs = [&](char kind, int gap, std::function<bool(int)> in) {
		for (int i = 0; i < n; i++)
		{
			if (!in(i))
				continue;
			if (!out.empty() && out.back().kind == kind && i - out.back().to <= gap + 1)
				out.back().to = i;
			else
			{
				Event e;
				e.kind = kind;
				e.from = e.to = i;
				out.push_back(e);
			}
		}
	};
	runs('j', 1, [&](int i) { return flagged[i] != 0; });
	for (Event &e : out)
	{
		std::vector<double> before, l5s, count;
		for (int k = e.from - 1; k >= 0 && before.size() < 5; k--)
			if (held[k])
				before.push_back(slots[k].used);
		e.used_before = before.empty() ? -1 : (int)median(before);
		e.used_min = 255;
		for (int i = e.from; i <= e.to; i++)
		{
			e.l1 = std::max(e.l1, (int)at[i]->drop1);
			if (at[i]->has5)
				l5s.push_back(at[i]->drop5);
			if (held[i])
			{
				e.used_min = std::min(e.used_min, (int)slots[i].used);
				if (slots[i].fix)
					e.fix = e.fix ? std::min(e.fix, (int)slots[i].fix) : slots[i].fix;
			}
			e.off = std::max(e.off, off(i));
			count.push_back(at[i]->sats);
		}
		e.has_l5 = !l5s.empty();
		e.l5 = (int)std::lround(median(l5s));
		e.satellites = (int)median(count);
		if (e.used_min == 255)
			e.used_min = 0;
	}
	int first_held = -1, last_held = -1;
	for (int i = 0; i < n; i++)
		if (held[i])
		{
			if (first_held < 0)
				first_held = i;
			last_held = i;
		}
	const std::size_t jams = out.size();
	runs('g', 0, [&](int i) { return i > first_held && i < last_held && !held[i]; });
	out.erase(std::remove_if(out.begin() + jams, out.end(), [](const Event &e) { return e.to == e.from; }), out.end());
	const std::size_t gaps = out.size();
	runs('s', 0, [&](int i) { return !flagged[i] && off(i) > 25 && slots[i].hdop < 30; });
	for (std::size_t k = gaps; k < out.size(); k++)
		for (int i = out[k].from; i <= out[k].to; i++)
			out[k].off = std::max(out[k].off, off(i));
	std::sort(out.begin(), out.end(), [](const Event &a, const Event &b) { return a.from < b.from; });
	return out;
}

GnssHistory::Now GnssHistory::read(std::time_t now, bool has_place, double lat, double lon) const
{
	Now r;
	r.heard = heard;
	r.fixed = fixed;
	const int n = day.places;
	const int32_t t = (int32_t)(now / day.interval), first = t - n + 1;
	// the day's median leaves out what a jam moved
	std::vector<double> lats, lons;
	int first_held = -1;
	Slot x;
	for (int i = 0; i < n; i++)
	{
		if (first + i < 0 || !day.at(first + i, x))
			continue;
		if (first_held < 0)
			first_held = i;
		const Mark &m = marks[(first + i) % DAY_SLOTS];
		if (x.fixes && !(m.t == first + i && m.jam))
		{
			lats.push_back(x.lat);
			lons.push_back(x.lon);
		}
	}
	r.hours = first_held < 0 ? 0 : (n - first_held) * day.interval / 3600.0;
	for (const auto &row : rows)
		r.signals |= (row.first & 3) == AIS::Sky::L1;
	// a slot is judged as it closes: the one before this, which holds for the one under way
	const Mark &m = marks[(t - 1) % DAY_SLOTS];
	if (t > 0 && m.t == t - 1 && m.jam)
	{
		r.jam = true;
		r.drop = m.drop1;
	}

	if (!has_place && lats.size() >= 30)
	{
		lat = median(lats);
		lon = median(lons);
		has_place = true;
	}
	r.placed = has_place;
	const bool here = day.open.t == t && day.open.fixes;
	if (r.placed && (here || (day.at(t - 1, x) && x.fixes)))
	{
		r.has_off = true;
		r.off = std::hypot(((here ? day.open.at_lon : x.at_lon) - lon) * NORTH * std::cos(lat * RAD), ((here ? day.open.at_lat : x.at_lat) - lat) * NORTH);
	}
	return r;
}

template <typename T>
static bool put(std::ofstream &f, const T &v)
{
	return (bool)f.write((const char *)&v, sizeof(v));
}

template <typename T>
static bool get(std::ifstream &f, T &v)
{
	return (bool)f.read((char *)&v, sizeof(v));
}

bool GnssHistory::save(std::ofstream &f) const
{
	if (!put(f, GNSS_VERSION))
		return false;
	for (const Span *s : {&day, &week})
		if (!f.write((const char *)s->ring.data(), (std::streamsize)(s->ring.size() * sizeof(Slot))) || !put(f, s->open))
			return false;
	const uint32_t n = (uint32_t)rows.size();
	if (!put(f, n))
		return false;
	for (const auto &r : rows)
		if (!put(f, r.first) || !put(f, r.second))
			return false;
	return true;
}

bool GnssHistory::load(std::ifstream &f)
{
	uint32_t version = 0, n = 0;
	if (!get(f, version) || version != GNSS_VERSION)
		return false;
	for (Span *s : {&day, &week})
		if (!f.read((char *)s->ring.data(), (std::streamsize)(s->ring.size() * sizeof(Slot))) || !get(f, s->open))
			return false;
	if (!get(f, n) || n > MAX_ROWS)
		return false;
	rows.clear();
	for (uint32_t i = 0; i < n; i++)
	{
		uint32_t key = 0;
		Row r;
		if (!get(f, key) || !get(f, r))
			return false;
		rows[key] = r;
	}
	std::vector<int32_t> closed;
	for (const Slot &x : day.ring)
		if (x.t >= 0)
			closed.push_back(x.t);
	std::sort(closed.begin(), closed.end());
	marks.assign(DAY_SLOTS, Mark());
	for (int32_t t : closed)
		mark(t);
	heard = fixed = 0;
	for (const Slot &x : day.ring)
	{
		const std::time_t end = x.t < 0 ? 0 : (std::time_t)(x.t + 1) * day.interval;
		heard = std::max(heard, end);
		if (x.fixes)
			fixed = std::max(fixed, end);
	}
	return true;
}
