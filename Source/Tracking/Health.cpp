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
#include <cstring>
#include <iterator>

#include "Figures.h"
#include "Health.h"
#include "SystemFacts.h"

const char *Health::name(Status s)
{
	static const char *names[] = {"ok", "degraded", "down"};
	return names[s];
}

// Every issue there is: whose it is, how bad, the seconds it must last to count and be gone to be
// over, the check of a station's AIS it fails, and what the journal says of it as it begins. One
// without words is told elsewhere: the lost device and the quiet station where they are noticed,
// the stopped engine when it stops.
namespace
{
	enum Of { OF_AIS, OF_GPS, OF_OUTPUT };
	typedef Health::Station::Checks Checks;
	struct Kind
	{
		const char *code;
		Of of;
		Health::Status level;
		int hold, recover;
		const char *Checks::*check;
		const char *words;
	};
	const Kind KINDS[] = {
		{"engine_stopped", OF_AIS, Health::DOWN, 0, 0, nullptr, nullptr},
		{"input_lost", OF_AIS, Health::DOWN, 0, 0, nullptr, nullptr},
		// a connection that drops and comes back within the half minute is no failure
		{"input_not_streaming", OF_AIS, Health::DOWN, 30, 0, nullptr, "a receiver does not stream"},
		{"no_messages", OF_AIS, Health::DEGRADED, 0, 0, &Checks::reception, nullptr},
		{"rate_low", OF_AIS, Health::DEGRADED, 0, 300, &Checks::rate, "fewer messages than usual for this hour"},
		{"channel_a_low", OF_AIS, Health::DEGRADED, 0, 300, &Checks::channels, "little heard on channel A"},
		{"channel_b_low", OF_AIS, Health::DEGRADED, 0, 300, &Checks::channels, "little heard on channel B"},
		{"rf_noise_high", OF_AIS, Health::DEGRADED, 60, 300, &Checks::gain, "the noise floor rose"},
		{"rf_noise_low", OF_AIS, Health::DEGRADED, 60, 300, &Checks::gain, "the noise floor fell"},
		{"rf_clipping", OF_AIS, Health::DEGRADED, 60, 300, &Checks::gain, "the signal clips"},
		{"freq_offset", OF_AIS, Health::DEGRADED, 0, 300, &Checks::frequency, "the receiver is off frequency"},
		{"no_data", OF_GPS, Health::DOWN, 0, 0, nullptr, "the GPS is silent"},
		{"no_fix", OF_GPS, Health::DEGRADED, 30, 0, nullptr, "the GPS has no fix"},
		{"fix_2d", OF_GPS, Health::DEGRADED, 30, 0, nullptr, "the GPS has a 2D fix only"},
		{"jamming_suspected", OF_GPS, Health::DEGRADED, 0, 0, nullptr, "GPS jamming suspected"},
		{"spoofing_suspected", OF_GPS, Health::DEGRADED, 60, 300, nullptr, "GPS position off, spoofing suspected"},
		{"not_connected", OF_OUTPUT, Health::DOWN, 60, 0, nullptr, "not connected"},
		{"not_sending", OF_OUTPUT, Health::DEGRADED, 0, 0, nullptr, "sends nothing"},
		{"dropping", OF_OUTPUT, Health::DEGRADED, 0, 300, nullptr, "drops messages"},
		{"reconnecting", OF_OUTPUT, Health::DEGRADED, 0, 300, nullptr, "keeps reconnecting"},
		{"not_listening", OF_OUTPUT, Health::DOWN, 0, 0, nullptr, "cannot open its port"},
	};

	const Kind &kindOf(const char *code)
	{
		return *std::find_if(std::begin(KINDS), std::end(KINDS), [&](const Kind &k) { return std::strcmp(k.code, code) == 0; });
	}
}

Health::Station &Health::station(int id)
{
	for (auto &s : stations)
		if (s.id == id)
			return s;
	stations.push_back(Station());
	stations.back().id = id;
	return stations.back();
}

std::size_t Health::Station::listening(std::size_t &down) const
{
	const bool all = std::all_of(receivers.begin(), receivers.end(), [](const Receiver &r) { return r.other; });
	std::size_t n = 0;
	down = 0;
	for (const auto &r : receivers)
		if (all || !r.other)
		{
			n++;
			down += r.status == DOWN;
		}
	return n;
}

Health::Issue &Health::note(const char *code, int station, int receiver, int output)
{
	const Kind &k = kindOf(code);
	auto it = std::find_if(issues.begin(), issues.end(), [&](const Issue &i) { return i.station == station && i.receiver == receiver && i.output == output && i.code == code; });
	if (it == issues.end())
	{
		Issue i;
		i.gps = k.of == OF_GPS;
		i.station = station;
		i.receiver = receiver;
		i.output = output;
		i.code = code;
		i.since = at;
		it = issues.insert(issues.end(), i);
	}
	it->seen = at;
	it->level = k.level;
	it->hold = k.hold;
	it->recover = k.recover;
	it->words = k.words;
	return *it;
}

bool Health::counts(bool gps, int station, const char *code) const
{
	for (const auto &i : issues)
		if (i.active && i.gps == gps && i.station == station && i.code == code)
			return true;
	return false;
}

// A receiver is down when its device is lost or it does not stream; one that streams and hears
// nothing is not: a quiet connection is no failure. A station whose receivers stream and that
// heard nothing for a quarter of an hour is degraded, as the journal says it.
void Health::readReceivers(const SystemFacts &f)
{
	running = f.running;
	born = f.born;
	session = f.session;
	if (!running)
	{
		note("engine_stopped");
		return;
	}
	const bool settled = at - session > STATION_QUIET_S;
	for (const auto &r : f.receivers)
	{
		Receiver x;
		x.index = r.index;
		x.label = r.label;
		x.input = r.status();
		x.heard = r.last_message;
		x.per_minute = (long long)r.per_minute;
		x.bytes = r.bytes;
		x.aircraft = r.aircraft;
		x.other = r.aircraft || (r.text && r.streaming && r.bytes && !r.last_message);
		station(r.station).receivers.push_back(x);
		if (r.lost)
			note("input_lost", r.station, r.index);
		else if (!r.streaming && at - session > 30)
			note("input_not_streaming", r.station, r.index);
	}
	for (auto &s : stations)
	{
		// a station that only takes in aircraft is as quiet as its inputs are: no AIS is expected of it
		const bool planes = !s.receivers.empty() && std::all_of(s.receivers.begin(), s.receivers.end(), [](const Receiver &r) { return r.aircraft; });
		std::time_t heard = s.heard;
		for (const auto &r : s.receivers)
			if (planes)
				heard = std::max(heard, r.heard);
		// a station only heard of through a feed goes by its messages alone
		const std::time_t since = heard ? heard : s.receivers.empty() ? at : session;
		if (settled && at - since > STATION_QUIET_S)
		{
			note("no_messages", s.id);
			continue;
		}
		// an input that counts no messages, as a GPS, has no last one
		if (s.receivers.size() > 1 && settled)
			for (const auto &r : s.receivers)
				if (r.heard && at - r.heard > STATION_QUIET_S && std::strcmp(r.input, "streaming") == 0)
					note("no_messages", s.id, r.index);
	}
}

static int localHour(std::time_t t)
{
	struct tm local;
#ifdef _WIN32
	localtime_s(&local, &t);
#else
	localtime_r(&t, &local);
#endif
	return local.tm_hour;
}

// the latest minutes and the usual hour of a reported measure, of the channel that stands out most
struct Measured
{
	bool known = false, usual_known = false;
	double now = 0, usual = 0;
	const char *unit = "";
};

static Measured measured(const MetricHistory *m, int key, std::time_t now, bool rise)
{
	Measured best;
	if (!m)
		return best;
	for (const auto &s : m->series)
	{
		if (s.key != key)
			continue;
		std::vector<double> recent, hours;
		float v;
		for (int i = 1; i <= 5; i++)
			if (MetricHistory::at(s, 0, (long int)now - i * 60, v))
				recent.push_back(v);
		for (int i = 1; i <= 168; i++)
			if (MetricHistory::at(s, 1, (long int)now - i * 3600, v))
				hours.push_back(v);
		if (recent.empty())
			continue;
		Measured x;
		x.known = true;
		x.now = GnssHistory::median(recent);
		x.usual_known = hours.size() >= 6;
		x.usual = x.usual_known ? GnssHistory::median(hours) : x.now;
		x.unit = s.unit;
		if (!best.known || (rise ? std::fabs(x.now - x.usual) > std::fabs(best.now - best.usual) : x.now > best.now))
			best = x;
	}
	return best;
}

// Messages against the same hour on earlier days, both channels over a quarter of an hour, what
// the receiver reports of the band: a noise floor far from its usual, or clipping, both of which
// point at the gain or at something nearby, and how far off frequency the messages come in.
void Health::readAIS(Station &s, Figures &f)
{
	std::vector<MessageStatistics::Values> minute;
	f.minute.forEachSlot([&](long int, MessageStatistics &m, bool empty) { minute.push_back(empty ? MessageStatistics::Values() : m.values()); });
	std::vector<std::pair<long int, int>> hour;
	f.hour.forEachSlot([&](long int start, MessageStatistics &m, bool empty) { hour.push_back(std::make_pair(start, empty ? 0 : m.getCount())); });
	if (minute.size() >= 2)
	{
		// newest first: [0] is the minute under way
		std::reverse(minute.begin(), minute.end());
		s.msgs = minute[1].count;
		s.a = minute[1].channel[0];
		s.b = minute[1].channel[1];
		// only the minutes of this session count: after a restart the ring still holds the empty ones before it
		const std::size_t ran = (std::size_t)std::max<long long>(0, (long long)(at - session) / 60);
		const std::size_t n = std::min(minute.size() - 1, ran);
		long long hourly = 0, a = 0, b = 0;
		for (std::size_t i = 1; i < minute.size(); i++)
		{
			if (i <= n)
				hourly += minute[i].count;
			if (i <= 15)
			{
				a += minute[i].channel[0];
				b += minute[i].channel[1];
			}
		}
		// this hour of the day on the days before: the usual traffic, needing three of them, and a
		// quarter of an hour of this session to hold against it
		std::vector<double> seen;
		const int this_hour = localHour(at);
		for (std::size_t i = 0; i + 24 < hour.size(); i++)
			if (hour[i].second > 0 && localHour((std::time_t)hour[i].first) == this_hour)
				seen.push_back(hour[i].second / 60.0);
		if (seen.size() >= 3 && n >= 15)
		{
			s.checks.rate = "ok";
			const double usual = GnssHistory::median(seen), ratio = usual > 0 ? (double)hourly / n / usual : 1;
			if (usual >= 1 && ratio < 0.4)
				note("rate_low", s.id).measured(std::round(ratio * 100), "%", "same_hour_median");
		}
		if (a + b >= 60)
		{
			s.checks.channels = "ok";
			if (a < 0.2 * b || b < 0.2 * a)
				note(a < b ? "channel_a_low" : "channel_b_low", s.id).measured(std::round(100.0 * std::min(a, b) / (a + b)), "%", "share_15m");
		}
	}

	const Measured floor = measured(f.reception.get(), AIS::KEY_NOISE_FLOOR, at, true);
	const Measured clip = measured(f.reception.get(), AIS::KEY_CLIPPING, at, false);
	// the receiver's own offset shows in the median of many transmitters, each a little off itself
	std::vector<double> ppms;
	float mean;
	for (const auto &m : minute)
		if (m.meanPpm(mean))
			ppms.push_back(mean);
	s.has_ppm = ppms.size() >= 5;
	s.rf = floor.known || clip.known || s.has_ppm;
	if (s.has_ppm)
	{
		s.ppm = std::round(GnssHistory::median(ppms) * 10) / 10;
		s.checks.frequency = "ok";
		if (std::fabs(s.ppm) >= 3)
			note("freq_offset", s.id).measured(s.ppm, "ppm", "median_1h");
	}
	// the gain is judged by what it does: a noise floor away from its usual, or clipping
	if (floor.known)
	{
		s.noise = std::round(floor.now * 10) / 10;
		s.noise_unit = floor.unit;
		s.has_delta = floor.usual_known;
		s.checks.gain = s.has_delta ? "ok" : "learning";
		s.noise_delta = std::round((floor.now - floor.usual) * 10) / 10;
		if (s.has_delta && std::fabs(s.noise_delta) >= 6)
			note(s.noise_delta > 0 ? "rf_noise_high" : "rf_noise_low", s.id).measured(s.noise_delta, "dB", "week_median");
	}
	if (clip.known)
	{
		s.has_clip = true;
		if (!floor.known)
			s.checks.gain = "ok";
		s.clip = std::round(clip.now * 10) / 10;
		if (clip.now >= 1)
			note("rf_clipping", s.id).measured(s.clip, "%", "samples_5m");
	}
}

void Health::GPS::see(const AIS::Sky &sky)
{
	fix = sky.fix;
	hdop = sky.hdop;
	sats = 0;
	for (const auto &sat : sky.satellites)
		sats += sat.used;
}

// a GPS further than this from where its station is has a doubtful position
static const double POSITION_OK_M = 50;

// A GPS that went silent is down; no fix or a 2D one, signals that fall together, and a good fix
// away from where the station is degrade.
void Health::readGPS(Station &s, const GnssHistory::Now &x, const AIS::Sky *sky, bool placed, bool moves)
{
	s.has_gps = true;
	GPS &p = s.gps;
	p.heard = x.heard;
	p.fixed = x.fixed;
	const bool silent = at - x.heard >= 120;
	if (!silent && sky)
		p.see(*sky);
	if (silent)
		note("no_data", s.id);
	else if (p.fix == 1 || p.fix == 2)
		note(p.fix == 1 ? "no_fix" : "fix_2d", s.id);
	p.checks.data = silent ? "down" : "ok";
	p.checks.fix = p.fix == 3 ? "ok" : p.fix ? "degraded" : "unknown";
	// without signal levels a jam cannot be told; with them it takes an hour to know what is usual
	p.checks.jamming = !x.signals ? "unknown" : x.hours >= 1 ? "ok" : "learning";
	// a station that moves has no place to be held against; one that stays learns its own in an hour
	p.checks.position = moves ? "unknown" : "learning";
	if (x.signals && x.hours >= 1)
		p.jamming = "none";
	if (x.jam && !silent)
		note("jamming_suspected", s.id).measured(-x.drop, "dB", "cn0_drop");
	if (x.placed && !moves)
	{
		p.pos_ref = placed ? "configured" : "learned";
		if (x.has_off && !silent)
			p.pos_ok = x.off <= POSITION_OK_M;
	}
	// a good fix in the wrong place, with no jam to explain it
	if (p.pos_ok >= 0 && x.hours >= 1)
	{
		p.spoofing = "none";
		p.checks.position = "ok";
	}
	if (p.pos_ok == 0 && p.fix == 3 && !x.jam)
		note("spoofing_suspected", s.id).measured(std::round(x.off), "m", "position_offset");
}

// An output that dials out and is not connected is down. One that keeps dropping messages or
// reconnecting, or that sent nothing for a quarter of an hour while messages came in, is degraded.
// An output that never sent is not judged on sending: a file or the screen count no bytes.
void Health::readOutputs(const SystemFacts &f, const std::vector<Output> &before)
{
	const bool receiving = std::any_of(stations.begin(), stations.end(), [](const Station &s) { return s.msgs > 0; });
	// an issue of an output or a server, named for the journal while its name is at hand
	const auto wrong = [&](const Output &x, const char *code) {
		note(code, -1, -1, x.index).of = x.kind == Output::SERVICE ? "Service on port " + std::to_string(x.port) : "Output " + x.label;
	};
	for (std::size_t k = 0; k < f.outputs.size(); k++)
	{
		const OutputFacts &o = f.outputs[k];
		Output x;
		x.index = (int)k + 1;
		x.type = o.type;
		x.label = o.label;
		x.feed = o.feed;
		x.kind = o.type == "TCP Listener" ? Output::LISTENER : o.connection && o.type != "HTTP" ? Output::CONNECTION : Output::SENDER;
		x.up = x.kind != Output::CONNECTION || o.connected;
		// a listener's connections are its clients, as a server's are
		if (x.kind == Output::LISTENER)
			x.clients = o.connected;
		x.bytes_out = o.bytes_out;
		x.dropped = o.dropped;
		x.reconnects = o.reconnects;
		x.messages = o.messages;
		x.sent = at;
		for (const auto &was : before)
			if (was.index == x.index && was.type == x.type && was.label == x.label)
			{
				x.drops = was.drops;
				x.flaps = was.flaps;
				if (x.dropped > was.dropped)
					x.drops.saw(at);
				if (x.reconnects > was.reconnects)
					x.flaps.saw(at);
				if (receiving && x.bytes_out == was.bytes_out)
					x.sent = was.sent;
			}
		if (!x.up && at - born > 30)
			wrong(x, "not_connected");
		// one that is not connected keeps trying: that is the one trouble, not two
		if (x.up && x.flaps.holds(at))
			wrong(x, "reconnecting");
		if (x.up && x.drops.holds(at))
			wrong(x, "dropping");
		if (x.up && x.bytes_out && at - x.sent >= STATION_QUIET_S)
			wrong(x, "not_sending");
		outputs.push_back(x);
	}
	// the servers follow the outputs: one that could not open its port is down, and no more is judged of them
	for (const auto &v : f.services)
	{
		Output x;
		x.index = (int)outputs.size() + 1;
		x.kind = Output::SERVICE;
		x.type = "Service";
		x.label = "Port " + std::to_string(v.port);
		x.port = v.port;
		x.offers = v.offers;
		x.up = v.listening;
		x.clients = v.clients;
		x.bytes_out = v.bytes_out;
		x.dropped = v.dropped;
		if (!x.up)
			wrong(x, "not_listening");
		outputs.push_back(x);
	}
}

std::vector<Health::Change> Health::settle()
{
	std::vector<Change> changes;
	for (auto it = issues.begin(); it != issues.end();)
	{
		const bool holds = it->seen == at;
		if (holds && !it->active && at - it->since >= it->hold)
		{
			it->active = true;
			changes.push_back(Change{*it, true});
		}
		if (!holds && (!it->active || at - it->seen >= it->recover))
		{
			if (it->active)
				changes.push_back(Change{*it, false});
			it = issues.erase(it);
		}
		else
			++it;
	}

	for (auto &s : stations)
	{
		bool any = false;
		Status gps = OK;
		Station::Checks &c = s.checks;
		c.reception = s.heard ? "ok" : "unknown";
		for (const auto &i : issues)
		{
			if (!i.active || i.station != s.id)
				continue;
			if (i.gps)
				gps = std::max(gps, i.level);
			else
			{
				any = true;
				for (auto &r : s.receivers)
					if (r.index == i.receiver)
						r.status = std::max(r.status, i.level);
				if (const char *Checks::*check = kindOf(i.code.c_str()).check)
					c.*check = "degraded";
			}
		}
		std::size_t down;
		const std::size_t listening = s.listening(down);
		s.ais = listening && down == listening ? DOWN : any ? DEGRADED : OK;
		c.input = !listening ? "unknown" : down == listening ? "down" : down ? "degraded" : "ok";
		s.gps.status = gps;
		if (counts(true, s.id, "jamming_suspected"))
		{
			s.gps.jamming = "suspected";
			s.gps.checks.jamming = "degraded";
		}
		if (counts(true, s.id, "spoofing_suspected"))
		{
			s.gps.spoofing = "suspected";
			s.gps.checks.position = "degraded";
		}
	}
	// the outputs: each as bad as its worst issue, all of them down only when none that dials is connected;
	// a server's trouble counts where the server is told, which is for whoever answers to say
	const auto has = [&](const Output &o, const char *code) {
		return std::any_of(issues.begin(), issues.end(), [&](const Issue &i) { return i.active && i.output == o.index && i.code == code; });
	};
	std::size_t dials = 0, cut = 0, feeds = 0;
	bool silent = false, dropping = false, flapping = false, sends = false;
	for (auto &o : outputs)
	{
		for (const auto &i : issues)
			if (i.active && i.output == o.index)
				o.status = std::max(o.status, i.level);
		if (o.kind == Output::SERVICE)
			continue;
		feeds++;
		dials += o.kind == Output::CONNECTION;
		cut += has(o, "not_connected");
		silent = silent || has(o, "not_sending");
		dropping = dropping || has(o, "dropping");
		flapping = flapping || has(o, "reconnecting");
		sends = sends || o.bytes_out;
		if (o.feed)
			out_checks.feed = name(o.status);
	}
	const bool any = std::any_of(outputs.begin(), outputs.end(), [](const Output &o) { return o.kind != Output::SERVICE && o.status != OK; });
	out = dials && cut == dials ? DOWN : any ? DEGRADED : OK;
	if (dials)
	{
		out_checks.connected = cut == dials ? "down" : cut ? "degraded" : "ok";
		out_checks.reconnects = flapping ? "degraded" : "ok";
	}
	if (sends)
		out_checks.sending = silent ? "degraded" : "ok";
	if (feeds)
		out_checks.dropped = dropping ? "degraded" : "ok";
	return changes;
}
