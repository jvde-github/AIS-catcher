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

#include "ReceiverTracker.h"
#include "Device/Device.h"

void ReceiverTracker::applyConfig(const TrackingConfig &cfg, const AIS::Filter &f)
{
	setStationPosition(cfg.lat, cfg.lon, cfg.use_gps);
	ships.setShareLatLon(cfg.latlon_share);
	ships.setServerMode(cfg.server_mode);
	ships.setMsgSave(cfg.msg_save);
	ships.setOwnMMSI(cfg.own_mmsi);
	ships.setTimeHistory(cfg.time_history);
	ships.setTrackTime(cfg.track_time);
	ships.setExpireFields(cfg.expire_fields);
	ships.setTrackMemory(cfg.track_memory);
	ships.setFilter(f);

	// applied unconditionally: a config that drops "cutoff" must go back to the
	// default rather than keep the value from the previous configuration
	const int cutoff = cfg.cutoff > 0 ? cfg.cutoff : LONG_RANGE_CUTOFF_DEFAULT;

	hist_second.setCutoff(cutoff);
	hist_minute.setCutoff(cutoff);
	hist_hour.setCutoff(cutoff);
	hist_day.setCutoff(cutoff);
	counter.setCutOff(cutoff);
	counter_session.setCutOff(cutoff);
}

void ReceiverTracker::setup()
{
	ships.setup();
}

void ReceiverTracker::wireStreams()
{
	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;
	ships >> counter;
	ships >> counter_session;
}

void ReceiverTracker::clearHistories()
{
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

void ReceiverTracker::clear()
{
	counter.Clear();
	clearHistories();
}

void ReceiverTracker::reset()
{
	clearHistories();
	ships.setup();
}

bool ReceiverTracker::save(std::ofstream &f, bool include_ships)
{
	return counter.Save(f) && hist_second.Save(f) && hist_minute.Save(f) && hist_hour.Save(f) && hist_day.Save(f) && (!include_ships || ships.Save(f));
}

bool ReceiverTracker::load(std::ifstream &f)
{
	if (!counter.Load(f) || !hist_second.Load(f) || !hist_minute.Load(f) || !hist_hour.Load(f) || !hist_day.Load(f))
		return false;

	if (f.peek() != EOF && !ships.Load(f))
		Warning() << "Backup: ship positions could not be restored from backup (format change?).";

	return true;
}

void ReceiverTracker::writeHistoryJSON(JSON::Writer &w)
{
	w.beginObject();
	w.key("second");
	hist_second.writeJSON(w);
	w.key("minute");
	hist_minute.writeJSON(w);
	w.key("hour");
	hist_hour.writeJSON(w);
	w.key("day");
	hist_day.writeJSON(w);
	w.endObject();
}

void ReceiverTracker::writeCountersJSON(JSON::Writer &w)
{
	w.key("total");
	counter.writeJSON(w);
	w.key("session");
	counter_session.writeJSON(w);
	w.key("last_day");
	hist_day.writeLastStatJSON(w);
	w.key("last_hour");
	hist_hour.writeLastStatJSON(w);
	w.key("last_minute");
	hist_minute.writeLastStatJSON(w);
	w.kv("msg_rate", hist_second.getAverage());
	w.kv("vessel_count", ships.getCount());
	w.kv("vessel_max", ships.getMaxCount());
}

std::string ReceiverTracker::toHistoryJSON()
{
	std::string s;
	JSON::Writer w(s);
	writeHistoryJSON(w);
	w.finish();
	return s;
}

void ReceiverTracker::writeSummary(std::ostream &out)
{
	auto tidy = [](std::string s) -> std::string {
		size_t pos;
		while ((pos = s.find("<br>")) != std::string::npos)  s.replace(pos, 4, ", ");
		while ((pos = s.find("<br/>")) != std::string::npos) s.replace(pos, 5, ", ");
		while (!s.empty() && (s.back() == ' ' || s.back() == ',' ||
		                       s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		while (!s.empty() && (s.front() == ' ' || s.front() == ','))
			s.erase(s.begin());
		return s;
	};

	out << "=== state: " << (label.empty() ? "(unnamed)" : label) << " ===\n";

	std::string p = tidy(product), v = tidy(vendor), sn = tidy(serial), sr = tidy(sample_rate);
	if (!p.empty())
	{
		out << "  device:   " << p;
		bool has_extra = (!v.empty() && v != "-") || (!sn.empty() && sn != "-") || !sr.empty();
		if (has_extra)
		{
			out << " [";
			bool first = true;
			auto sep = [&]() { if (!first) out << ", "; first = false; };
			if (!v.empty() && v != "-")  { sep(); out << v; }
			if (!sn.empty() && sn != "-") { sep(); out << "sn=" << sn; }
			if (!sr.empty())              { sep(); out << "rate=" << sr; }
			out << "]";
		}
		out << "\n";
	}
	std::string mn = tidy(model_name);
	if (!mn.empty())
		out << "  model:    " << mn << "\n";

	out << "  vessels:  " << ships.getCount()
	    << "   msg/s: " << hist_second.getAverage() << "\n";
	counter_session.print(out, "  ");
}

static std::string orDash(const std::string &s)
{
	return s.empty() ? "-" : s;
}

// How a device names itself. attachEngine() needs this before it has a tracker to
// put it in, because the name is what identifies a tracker across engine runs.
std::string deviceLabel(Device::Device *device)
{
	const std::string serial = orDash(device->getSerial());

	if (serial == ".")
		return "Console";

	std::string label = device->getProduct();
	if (serial != "-")
		label += " " + serial;

	return label;
}

void ReceiverTracker::setDevice(Device::Device *device)
{
	product = device->getProduct();
	vendor = orDash(device->getVendor());
	serial = orDash(device->getSerial());
	sample_rate = device->getRateDescription();

	label = deviceLabel(device);
}

void ReceiverTracker::appendDevice(Device::Device *device, const std::string &newline)
{
	product += device->getProduct() + newline;
	vendor += orDash(device->getVendor()) + newline;
	serial += orDash(device->getSerial()) + newline;
	sample_rate += device->getRateDescription() + newline;
}
