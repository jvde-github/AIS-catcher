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

#include <fstream>
#include <vector>

#include "History.h"
#include "Statistics.h"
#include "Writer.h"

// What one stream of messages adds up to: totals kept across restarts and for
// this run, and the figures by second, minute, hour and day. The tracker keeps
// one for everything it hears and one for each station.
struct Figures : public StreamIn<JSON::JSON>
{
	Counter total, session;
	History<60, 1> second;
	History<60, 60> minute;
	History<24, 3600> hour;
	History<90, 86400> day;

	void Receive(const JSON::JSON *data, int len, TAG &tag) override
	{
		total.Receive(data, len, tag);
		session.Receive(data, len, tag);
		second.Receive(data, len, tag);
		minute.Receive(data, len, tag);
		hour.Receive(data, len, tag);
		day.Receive(data, len, tag);
	}

	void setCutoff(int cutoff)
	{
		total.setCutOff(cutoff);
		session.setCutOff(cutoff);
		second.setCutoff(cutoff);
		minute.setCutoff(cutoff);
		hour.setCutoff(cutoff);
		day.setCutoff(cutoff);
	}

	// several streams' figures as one, in place of these: what they count adds
	// up, their extremes take the extreme. The parts must not change meanwhile.
	void combine(const std::vector<Figures *> &parts)
	{
		total.Clear();
		session.Clear();
		std::vector<History<60, 1> *> seconds;
		std::vector<History<60, 60> *> minutes;
		std::vector<History<24, 3600> *> hours;
		std::vector<History<90, 86400> *> days;
		for (Figures *f : parts)
		{
			total.add(f->total);
			session.add(f->session);
			seconds.push_back(&f->second);
			minutes.push_back(&f->minute);
			hours.push_back(&f->hour);
			days.push_back(&f->day);
		}
		second.combine(seconds);
		minute.combine(minutes);
		hour.combine(hours);
		day.combine(days);
	}

	void clear()
	{
		total.Clear();
		session.Clear();
		second.Clear();
		minute.Clear();
		hour.Clear();
		day.Clear();
	}

	// the session counter starts again with every run, so it is not kept
	bool save(std::ofstream &f) { return total.Save(f) && second.Save(f) && minute.Save(f) && hour.Save(f) && day.Save(f); }
	bool load(std::ifstream &f) { return total.Load(f) && second.Load(f) && minute.Load(f) && hour.Load(f) && day.Load(f); }

	void writeCountersJSON(JSON::Writer &w)
	{
		w.key("total");
		total.writeJSON(w);
		w.key("session");
		session.writeJSON(w);
		w.key("last_day");
		day.writeLastStatJSON(w);
		w.key("last_hour");
		hour.writeLastStatJSON(w);
		w.key("last_minute");
		minute.writeLastStatJSON(w);
		w.kv("msg_rate", second.getAverage());
	}

	void writeHistoryJSON(JSON::Writer &w)
	{
		w.beginObject();
		w.key("second");
		second.writeJSON(w);
		w.key("minute");
		minute.writeJSON(w);
		w.key("hour");
		hour.writeJSON(w);
		w.key("day");
		day.writeJSON(w);
		w.endObject();
	}
};
