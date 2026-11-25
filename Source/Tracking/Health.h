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

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "Gnss.h"

struct Figures;
struct SystemFacts;

// Whether the installation works, station by station: AIS from its receivers and what they hear,
// the GPS from its fix and its signals, and the outputs, which are the installation's, from their
// connections and counters. Worked out every half minute, roughly, from figures kept
// anyway. A status is ok, degraded or down, and down only when something stopped working: a lost
// device, a GPS gone silent. Little traffic, noise or a doubtful position degrade. Everything
// behind a status is an issue, which holds from when it was first seen until it is over.
struct Health
{
	enum Status { OK, DEGRADED, DOWN };
	static const char *name(Status s);

	struct Issue
	{
		Status level = DEGRADED;
		bool gps = false;
		int station = -1, receiver = -1; // -1: the installation's, the station's
		int output = -1;				 // an output's, by its number
		std::string code;
		std::time_t since = 0, seen = 0;
		// seconds it must last to count, and be gone to be over
		int hold = 0, recover = 0;
		bool active = false, valued = false;
		double value = 0;
		const char *unit = "", *method = "";
		// what the journal says of it as it begins; nullptr when that is told elsewhere
		const char *words = nullptr;
		// whose it is in the journal's words, of an output's or a server's
		std::string of;

		const char *type() const { return output >= 0 ? "output" : gps ? "gps" : "ais"; }
		void measured(double v, const char *u, const char *m)
		{
			valued = true;
			value = v;
			unit = u;
			method = m;
		}
	};

	// an issue that began to count, or ended
	struct Change
	{
		Issue issue;
		bool raised;
	};

	struct Receiver
	{
		int index = 0;
		std::string label;
		const char *input = "";
		Status status = OK;
		std::time_t heard = 0;
		long long per_minute = 0;
		// everything it took in: an input with bytes and no messages carries something else, as a GPS
		unsigned long long bytes = 0;
		// it streams text and never passed on an AIS message: no receiver of AIS, whatever else it brings
		bool other = false;
	};

	struct GPS
	{
		Status status = OK;
		int fix = 0, sats = 0; // fix: 0 unknown, 1 none, 2 2D, 3 3D
		float hdop = -1;
		std::time_t heard = 0, fixed = 0;
		const char *jamming = "unknown", *spoofing = "unknown", *pos_ref = "unknown";
		int pos_ok = -1; // -1 unknown
		// what was judged, each ok, degraded, down, learning (not long enough yet) or unknown (cannot be told)
		struct Checks
		{
			const char *data = "ok", *fix = "unknown", *jamming = "unknown", *position = "unknown";
		} checks;
		// the fix and the satellites as a sky has them
		void see(const AIS::Sky &sky);
	};

	struct Station
	{
		int id = 0;
		Status ais = OK;
		std::time_t heard = 0;
		// the last whole minute
		int msgs = 0, a = 0, b = 0;
		// what was judged, each ok, degraded, down, learning (no baseline yet) or unknown (not measured)
		struct Checks
		{
			const char *input = "unknown", *reception = "unknown", *rate = "learning", *channels = "unknown", *gain = "unknown", *frequency = "unknown";
		} checks;
		bool rf = false, has_delta = false, has_clip = false, has_ppm = false;
		// the frequency offset: the median over the hour of each minute's mean
		double ppm = 0;
		double noise = 0, noise_delta = 0, clip = 0;
		std::string noise_unit;
		std::vector<Receiver> receivers;
		// the receivers that are there for AIS, and how many of them are down; all of them when none has
		// passed on a message yet
		std::size_t listening(std::size_t &down) const;
		bool has_gps = false;
		GPS gps;
	};

	// drops or reconnects seen at several verdicts close together: one alone is no trouble
	struct Run
	{
		std::time_t first = 0, last = 0;
		void saw(std::time_t at)
		{
			if (at - last > 300)
				first = at;
			last = at;
		}
		bool holds(std::time_t at) const { return last && at - last <= 120 && last - first >= 60; }
	};

	struct Output
	{
		// It sends without a connection of its own, as UDP, HTTP or a file; it dials out and holds a
		// connection; it listens for clients; or it is a server of the installation, numbered after the outputs.
		enum Kind { SENDER, CONNECTION, LISTENER, SERVICE };
		int index = 0;
		Kind kind = SENDER;
		std::string type, label;
		Status status = OK;
		bool up = true;	   // false for a connection that is not connected, a server that could not open its port
		bool feed = false; // the community feed
		// a server's port and what it offers there; a listener's and a server's clients now
		int port = 0;
		std::vector<std::string> offers;
		unsigned long long clients = 0;
		const char *kindName() const
		{
			static const char *names[] = {"sender", "connection", "listener", "service"};
			return names[kind];
		}
		const char *state() const
		{
			switch (kind)
			{
			case CONNECTION: return up ? "connected" : "disconnected";
			case LISTENER: return "listening";
			case SERVICE: return up ? "listening" : "closed";
			default: return bytes_out ? "sending" : "idle";
			}
		}
		unsigned long long bytes_out = 0, dropped = 0, reconnects = 0;
		std::time_t sent = 0; // when it last sent, or had nothing to send
		Run drops, flaps;
	};

	bool running = false;
	std::time_t born = 0, session = 0, at = 0;
	std::vector<Station> stations;
	std::vector<Output> outputs;
	Status out = OK;
	// each ok, degraded, down or unknown (nothing to judge it by)
	struct OutputChecks
	{
		const char *connected = "unknown", *sending = "unknown", *dropped = "unknown", *reconnects = "unknown", *feed = "unknown";
	} out_checks;
	std::vector<Issue> issues; // those that count, and those on their way in or out

	Station &station(int id);
	// an issue seen now, by its code in the table of them; -1: the installation's, the station's, no output's
	Issue &note(const char *code, int station = -1, int receiver = -1, int output = -1);
	bool counts(bool gps, int station, const char *code) const;

	void readReceivers(const SystemFacts &f);
	void readAIS(Station &s, Figures &f);
	// x: what the history of the station's GPS tells; sky: its sky when it is fresh; placed: the station has
	// a configured place to hold the fix against; moves: it rides a vessel and has none
	void readGPS(Station &s, const GnssHistory::Now &x, const AIS::Sky *sky, bool placed, bool moves);
	// after the stations were read; before: the outputs of the verdict before this one
	void readOutputs(const SystemFacts &f, const std::vector<Output> &before);
	// after everything was read: the issues begin and end, and the statuses follow from them
	std::vector<Change> settle();
};
