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

#include <signal.h>

#include <iostream>
#include <fstream>
#include <string.h>
#include <memory>
#include <cctype>

#include "AIS-catcher.h"

#include "Receiver.h"
#ifdef HASWEBVIEWER
#include "WebViewer.h"
#endif
#include "RunState.h"
#include "Config.h"
#include "ControlCore.h"
#include "JSON.h"
#include "JSON/Parser.h"
#include "N2KStream.h"
#include "PostgreSQL.h"
#include "Logger.h"
#include "Screen.h"
#include "File.h"

// stop ends the current run (device end, timeout, engine stop); stop_process
// additionally ends the managed-mode supervisor loop (signals only)
static std::atomic<bool> stop;
static std::atomic<bool> stop_process;

void StopRequest()
{
	stop = true;
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
	{
		stop = true;
		stop_process = true;
	}
	return TRUE;
}
#else
static void consoleHandler(int signal)
{
	if (signal == SIGPIPE)
	{
		// Info() << "Termination request SIGPIPE ignored" ;
		return;
	}
	if (signal != SIGINT)
		std::cerr << "Termination request: " << signal;

	stop = true;
	stop_process = true;
}
#endif

static void printVersion()
{
	Info() << "AIS-catcher (build " << __DATE__ << ") " << VERSION_DESCRIBE << "\n"
		   << "(C) Copyright 2021-2026 " << COPYRIGHT << "\n"
		   << "This is free software; see the source for copying conditions. There is NO" << "\n"
		   << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";
}

static void Usage()
{
	Info() << "use: AIS-catcher [options]";
	Info() << "";
	Info() << "\t[-a xxx - set tuner bandwidth in Hz (default: off)]";
	Info() << "\t[-b benchmark demodulation models for time - for development purposes (default: off)]";
	Info() << "\t[-c [AB/CD] - [optional: AB] select AIS channels and optionally the NMEA channel designations]";
	Info() << "\t[-C [filename] - read configuration settings from file]";
	Info() << "\t[-D [connection string] - write messages to PostgreSQL database]";
	Info() << "\t[-e [baudrate] [serial port] - read NMEA from serial port at specified baudrate]";
	Info() << "\t[-E [filename] [port] - managed mode: run engine from config file with control server, must be only option (default port: 8110)]";
	Info() << "\t[-f [filename] write NMEA lines to file]";
	Info() << "\t[-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]";
	Info() << "\t[-G [LEVEL level] [SYSTEM on] - control logging (levels: DEBUG, INFO, WARNING, ERROR, CRITICAL) or enable system logging]";
	Info() << "\t[-h display this message and terminate (default: false)]";
	Info() << "\t[-H [optional: url] - send messages via HTTP, for options see documentation]";
	Info() << "\t[-i [interface] - read NMEA2000 data from socketCAN interface - Linux only]";
	Info() << "\t[-I [interface] - push messages as NMEA2000 data to a socketCAN interface - Linux only]";
	Info() << "\t[-m xx - run specific decoding model (default: 2), see README for more details]";
	Info() << "\t[-M xxx - set additional meta data to generate: T = NMEA timestamp, D = decoder related (signal power, ppm) (default: none)]";
	Info() << "\t[-n show NMEA messages on screen without detail (-o 1)]";
	Info() << "\t[-N [optional: port][optional settings] - start http server at port, see README for details]";
	Info() << "\t[-o set output mode (0 = quiet, 1 = NMEA only, 2 = NMEA+, 3 = NMEA+ in JSON, 4 JSON Sparse, 5 JSON Full (default: 2)]";
	Info() << "\t[-O MMSI - sets the own mmsi of the receiver]";
	Info() << "\t[-p xxx - set frequency correction for device in PPM (default: zero)]";
	Info() << "\t[-P xxx.xx.xx.xx yyy - TCP destination address and port (default: off)]";
	Info() << "\t[-q suppress NMEA messages to screen (-o 0)]";
	Info() << "\t[-s xxx - sample rate in Hz (default: based on SDR device)]";
	Info() << "\t[-S xxx - TCP server for NMEA lines at port xxx]";
	Info() << "\t[-T xx - auto terminate run with SDR after xxx seconds (default: off)]";
	Info() << "\t[-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]";
	Info() << "\t[-v [option: xx] - enable verbose mode, optional to provide update frequency of xx seconds (default: false)]";
	Info() << "\t[-X connect to AIS community feed at www.aiscatcher.org (default: off)]";
	Info() << "\t[-Q publish data to MQTT server]";
	Info() << "\t[-Z lat lon - set receiver location (latitude and longitude in decimal degrees)]";

	Info() << "";
	Info() << "\tDevice selection:";
	Info() << "";
	Info() << "\t[-d:x - select device based on index (default: 0)]";
	Info() << "\t[-d xxxx - select device based on serial number]";
	Info() << "\t[-e baudrate port - open device at serial port with given baudrate]";
	Info() << "\t[-l list available devices and terminate (default: off)]";
	Info() << "\t[-L list supported SDR hardware and terminate (default: off)]";
	Info() << "\t[-r [optional: yy] filename - read IQ data from file or stdin (.), short for -r -ga FORMAT yy FILE filename";
	Info() << "\t[-t [[protocol]] [host [port]] - read IQ data from remote RTL-TCP instance]";
	Info() << "\t[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]";
	Info() << "\t[-x [server][port] - UDP input of NMEA messages at port on server";
	Info() << "\t[-y [host [port]] - read IQ data from remote SpyServer]";
	Info() << "\t[-z [optional [format]] [optional endpoint] - read IQ data from [endpoint] in [format] via ZMQ (default: format is CU8)]";
	Info() << "";
	Info() << "\tDevice specific settings:";
	Info() << "";
	Info() << "\t[-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8] ]";
	Info() << "\t[-gd HydraSDR: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]";
	Info() << "\t[-ge Serial Port: PRINT [on/off] FLOWCONTROL [none/hardware/software] INIT_SEQ [string] ]";
	Info() << "\t[-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off] ]";
	Info() << "\t[-gh Airspy HF+: THRESHOLD [low/high] PREAMP [on/off] ]";
	Info() << "\t[-gm Airspy: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]";
	Info() << "\t[-gr RTLSDRs: TUNER [auto/0.0-50.0] RTLAGC [on/off] BIASTEE [on/off] ]";
	Info() << "\t[-gs SDRPLAY: GRDB [0-59] LNASTATE [0-9] AGC [on/off] ]";
	Info() << "\t[-gt RTLTCP: HOST [address] PORT [port] TUNER [auto/0.0-50.0] RTLAGC [on/off] FREQOFFSET [-150-150] PROTOCOL [none/rtltcp] TIMEOUT [1-60] ]";
	Info() << "\t[-gu SOAPYSDR: DEVICE [string] GAIN [string] AGC [on/off] STREAM [string] SETTING [string] CH [0+] PROBE [on/off] ANTENNA [string] ]";
	Info() << "\t[-gw WAV file: FILE [filename] ]";
	Info() << "\t[-gy SPYSERVER: HOST [address] PORT [port] GAIN [0-50] ]";
	Info() << "\t[-gz ZMQ: ENDPOINT [endpoint] FORMAT [CF32/CS16/CU8/CS8] ]";
	Info() << "";
	Info() << "\tModel specific settings:";
	Info() << "";
	Info() << "\t[-go Model: AFC_WIDE [on/off] FP_DS [on/off] PS_EMA [on/off] SOXR [on/off] SRC [on/off] DROOP [on/off] ]";
}

static void printBuildConfiguration()
{
	std::ostringstream sdr_support;
	sdr_support << "SDR support: ";
#ifdef HASRTLSDR
	sdr_support << "RTLSDR ";
#endif
#ifdef HASAIRSPY
	sdr_support << "AIRSPY ";
#endif
#ifdef HASHYDRASDR
	sdr_support << "HYDRASDR ";
#endif
#ifdef HASAIRSPYHF
	sdr_support << "AIRSPYHF+ ";
#endif
#ifdef HASSDRPLAY
	sdr_support << "SDRPLAY ";
#endif
	sdr_support << "RTLTCP SPYSERVER ";
#ifdef HASZMQ
	sdr_support << "ZMQ ";
#endif
#ifdef HASHACKRF
	sdr_support << "HACKRF ";
#endif
#ifdef HASSOAPYSDR
	sdr_support << "SOAPYSDR ";
#endif

	// Output all SDR support messages on one line
	Info() << sdr_support.str();

	std::ostringstream other_support;
	other_support << "Other support: ";
#ifdef HASSOXR
	other_support << "SOXR ";
#endif
#ifdef HASSYSLOG
	other_support << "SYSLOG ";
#endif
#ifdef HASNMEA2000
	other_support << "NMEA2000 ";
#endif
#ifdef HASOPENSSL
	other_support << "SSL ";
#endif
#ifdef HASPSQL
	other_support << "PostgreSQL ";
#endif
#ifdef HASZLIB
	other_support << "ZLIB ";
#endif
#ifdef HASSAMPLERATE
	other_support << "LIBSAMPLERATE ";
#endif
#ifdef HASSQLITE
	other_support << "SQLITE ";
#endif
#ifdef HASRTLSDR_BIASTEE
	other_support << "RTLSDR-BIASTEE ";
#endif
#ifdef HASRTLSDR_TUNERBW
	other_support << "RTLSDR-TUNERBW ";
#endif

	// Output all other support messages on one line
	Info() << other_support.str();
}

// -------------------------------
// Command line support functions


// Expand @filename response-file arguments (gcc/clang convention).
// Each line is whitespace-split; lines whose first non-whitespace
// character is '#' are skipped, as are blank lines. CR endings are
// stripped. Nested @file is allowed up to a small depth limit.
static void expandResponseFiles(int argc, char *argv[],
								std::vector<std::string> &out, int depth = 0)
{
	for (int i = 0; i < argc; i++)
	{
		const char *s = argv[i];
		if (!s || s[0] == '\0') continue;
		if (s[0] != '@')
		{
			out.emplace_back(s);
			continue;
		}
		if (depth >= 8)
			throw std::runtime_error(std::string("Response file recursion too deep: ") + s);

		std::ifstream f(s + 1);
		if (!f)
			throw std::runtime_error(std::string("Cannot open response file: ") + (s + 1));

		std::vector<std::string> tokens;
		std::string line;
		while (std::getline(f, line))
		{
			if (!line.empty() && line.back() == '\r') line.pop_back();

			size_t p = 0;
			while (p < line.size() && std::isspace((unsigned char)line[p])) p++;
			if (p >= line.size() || line[p] == '#') continue;

			while (p < line.size())
			{
				size_t e = p;
				while (e < line.size() && !std::isspace((unsigned char)line[e])) e++;
				tokens.emplace_back(line.substr(p, e - p));
				p = e;
				while (p < line.size() && std::isspace((unsigned char)line[p])) p++;
			}
		}

		std::vector<char *> tok_argv;
		tok_argv.reserve(tokens.size());
		for (auto &t : tokens) tok_argv.push_back(&t[0]);
		expandResponseFiles((int)tok_argv.size(), tok_argv.data(), out, depth + 1);
	}
}


static void parseSettings(Setting &s, char *argv[], int ptr, int argc)
{
	std::string flag_context;
	if (ptr >= 0 && ptr < argc && argv[ptr] != nullptr && argv[ptr][0] == '-')
		flag_context = argv[ptr];

	ptr++;

	while (ptr < argc - 1 && argv[ptr][0] != '-')
	{
		std::string option = argv[ptr++];
		std::string arg = argv[ptr++];

		Util::Convert::toUpper(option);
		AIS::Keys key = AIS::lookupSettingKey(option);

		try
		{
			if (key != (AIS::Keys)-1)
				s.SetKey(key, arg);
			else
			{
				std::string lower = option;
				Util::Convert::toLower(lower);
				std::string msg = s.getName().empty() ? "" : s.getName() + ": ";
				msg += "unknown setting \"" + lower + "\".";
				throw std::runtime_error(msg);
			}
		}
		catch (const std::exception &e)
		{
			std::string msg = e.what();
			if (!flag_context.empty())
				msg += " (in " + flag_context + " " + option + " " + arg + ")";

			throw std::runtime_error(msg);
		}
	}
}

static bool isOption(const std::string &s)
{
	return s.length() >= 2 && s[0] == '-' && std::isalpha((unsigned char)s[1]);
}

static void Assert(bool b, std::string &context, const std::string &msg = "")
{
	if (!b)
	{
		throw std::runtime_error("syntax error on command line with setting \"" + context + "\". " + msg + "\n");
	}
}

static void run(RunState &state)
{
	// -------------
	// set up the receiver and open the device

	state.stat.resize(state.receivers.size());
	state.msg_count.resize(state.receivers.size(), 0);

	bool has_server = false;
#ifdef HASWEBVIEWER
	for (auto &s : state.servers)
		has_server |= s->active();
#endif
	bool has_http = false;
	for (auto &o : state.msg)
		if (dynamic_cast<IO::HTTPStreamer *>(o.get())) { has_http = true; break; }

	int group = 0;

	for (int i = 0; i < (int)state.receivers.size(); i++)
	{
		Receiver &r = *state.receivers[i];
		r.SetKey(AIS::KEY_SETTING_OWN_MMSI, std::to_string(state.own_mmsi));

		if (has_server) r.setTags("DTM");
		if (has_http)   r.setTags("DT");

		r.setupDevice();

		// set up the decoding model(s), group is the last output group used
		r.setupModel(group, i);
	}

	// for community feed, restrict to local SDR hardware only
	if (state.comm_feed && !state.xshare_defined)
	{
		uint64_t live_groups = 0;
		for (const auto &r : state.receivers)
		{
			Type t = r->getDeviceManager().InputType();
			if (t == Type::RTLSDR || t == Type::AIRSPY || t == Type::AIRSPYHF ||
				t == Type::HACKRF || t == Type::SDRPLAY || t == Type::HYDRASDR ||
				t == Type::SOAPYSDR)
				live_groups |= r->getGroupMask();
		}

		state.comm_feed->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(live_groups));
	}

	// Resolve zone-based output filtering: compute GROUPS_IN from zone overlap
	auto resolveZones = [&](const std::vector<std::string> &zones) -> uint64_t {
		uint64_t mask = 0;
		for (const auto &r : state.receivers)
		{
			bool matched = false;
			for (const auto &rz : r->zones)
			{
				for (const auto &oz : zones)
					if (rz == oz) { matched = true; break; }
				if (matched) break;
			}
			if (matched) mask |= r->getGroupMask();
		}
		return mask;
	};

	for (auto &o : state.msg)
	{
		if (o->zones.empty()) continue;
		uint64_t mask = resolveZones(o->zones);
		if (!mask)
			Warning() << "Output has zone filter but no matching receivers — will receive nothing";
		o->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(mask));
	}

#ifdef HASWEBVIEWER
	for (auto &s : state.servers)
	{
		if (s->zones.empty()) continue;
		uint64_t mask = resolveZones(s->zones);
		if (!mask)
			Warning() << "Server has zone filter but no matching receivers — will receive nothing";
		s->SetKey(AIS::KEY_SETTING_GROUPS_IN, std::to_string(mask));
	}
#endif

	for (int i = 0; i < (int)state.receivers.size(); i++)
	{
		Receiver &r = *state.receivers[i];

		// set up all the output and connect to the receiver outputs
		for (auto &o : state.msg)
			o->Connect(r);

		state.screen.Connect(r);

		if (r.verbose || state.timeout_nomsg)
			state.stat[i].connect(r);
	}

	if (state.receivers.size() > 1)
	{
		Debug() << "Mutex: enabling exclusive on " << state.msg.size() << " message outputs (" << state.receivers.size() << " receivers); screen uses thread-local buffers";
		for (auto &o : state.msg)
			o->setExclusive(true);
	}
	else
	{
		Debug() << "Mutex: single receiver, all sinks lock-free";
	}

#ifdef HASWEBVIEWER
	for (auto &s : state.servers)
		if (s->active())
			s->connect(state.receivers);
#endif

	for (auto &o : state.msg)
		o->Start();

#ifdef HASWEBVIEWER
	for (auto &s : state.servers)
		if (s->active())
		{
			s->setOutputChannels(state.msg);
			s->setCommFeed(state.comm_feed);

			if (state.own_mmsi != -1)
			{
				s->SetKey(AIS::KEY_SETTING_SHARE_LOC, "true");
				s->SetKey(AIS::KEY_SETTING_OWN_MMSI, std::to_string(state.own_mmsi));
			}
			s->start();
		}
#endif

	Debug() << "Starting statistics";
	for (auto &s : state.stat)
		s.start();

	Debug() << "Starting receivers";
	for (auto &r : state.receivers)
		r->play();

	stop = false;
	const int SLEEP = 50;
	auto time_start = high_resolution_clock::now();
	auto time_timeout_start = time_start;
	auto time_last = time_start;
	bool oneverbose = false, iscallback = true;

	for (auto &r : state.receivers)
	{
		oneverbose |= r->verbose;
		iscallback &= r->getDeviceManager().getDevice()->isCallback();
	}

	Debug() << "Entering main loop";
	while (!stop)
	{
		for (auto &r : state.receivers)
			stop = stop || !(r->getDeviceManager().getDevice()->isStreaming());

		// for non-callback devices (WAV file), the isStreaming() poll above pumps
		// the data, so the loop must run unthrottled
		if (iscallback)
			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

		if (!oneverbose && !state.timeout)
			continue;

		auto time_now = high_resolution_clock::now();

		if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >= state.screen.verboseUpdateTime)
		{
			time_last = time_now;

			for (int i = 0; i < (int)state.receivers.size(); i++)
			{
				Receiver &r = *state.receivers[i];
				if (r.verbose)
				{
					for (int j = 0; j < r.Count(); j++)
					{
						state.stat[i].statistics[j]->Stamp();
						std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
						Info() << "[" << name << "] " << std::string(name.length() < 37 ? 37 - name.length() : 0, ' ') << "received: " << state.stat[i].statistics[j]->getDeltaCount() << " msgs, total: "
							   << state.stat[i].statistics[j]->getCount() << " msgs, rate: " << state.stat[i].statistics[j]->getRate() << " msg/s";
					}
				}
			}
		}

		// watchdog: stop when any receiver went without messages for the timeout period
		if (state.timeout && state.timeout_nomsg)
		{
			bool one_stale = false;

			for (int i = 0; i < (int)state.receivers.size(); i++)
			{
				uint64_t count = 0;
				for (auto &s : state.stat[i].statistics)
					count += s->getCount();

				if (count == state.msg_count[i])
					one_stale = true;

				state.msg_count[i] = count;
			}

			if (!one_stale)
				time_timeout_start = time_now;
		}

		if (state.timeout && duration_cast<seconds>(time_now - time_timeout_start).count() >= state.timeout)
		{
			stop = true;
			if (state.timeout_nomsg)
				Warning() << "Watchdog: a receiver had no messages for " << state.timeout << " seconds, stopping.";
			else
			{
				state.exit_code = -2;
				Warning() << "Stop triggered by timeout after " << state.timeout << " seconds. (-T " << state.timeout << ")";
			}
		}
	}

	for (int i = 0; i < (int)state.receivers.size(); i++)
	{
		Receiver &r = *state.receivers[i];
		r.stop();

		std::stringstream ss;
		if (r.verbose)
		{
			ss << "----------------------\n";
			for (int j = 0; j < r.Count(); j++)
			{
				std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
				state.stat[i].statistics[j]->Stamp();
				ss << "[" << name << "] " << std::string(name.length() < 37 ? 37 - name.length() : 0, ' ') << "total: " << state.stat[i].statistics[j]->getCount() << " msgs" << "\n";
			}
		}

		if (r.Timing())
			for (int j = 0; j < r.Count(); j++)
			{
				std::string name = r.Model(j)->getName();
				ss << "[" << r.Model(j)->getName() << "]: " << std::string(name.length() < 37 ? 37 - name.length() : 0, ' ') << r.Model(j)->getTotalTiming() << " ms" << "\n";
			}
		if (!ss.str().empty())
			Info() << ss.str();
	}

#ifdef HASWEBVIEWER
	for (auto &s : state.servers)
		s->close();
#endif
}

// Managed mode (-E): supervisor loop that builds a fresh RunState from the
// config file for every engine start and tears it down on stop/restart. The
// process survives a broken config; the user fixes it via the control API.
static int runManaged(const std::string &config_file, int port)
{
	ControlCore core(config_file, port);

	Info() << "Control: managed mode, config file \"" << config_file << "\", control port " << core.getControlPort();

	while (!stop_process)
	{
		if (core.engineDesired())
		{
			stop = false;

			RunState state;
			Config c(state);

			try
			{
				state.receivers.back()->getDeviceManager().refreshDevices();
				c.read(config_file);
				core.reportRunning();
				Info() << "Control: engine started";
				run(state);
			}
			catch (std::exception const &e)
			{
				Error() << "Control: engine failed: " << e.what();
				for (auto &r : state.receivers)
					r->stop();
				core.engineFailed();
			}
			core.reportStopped();
			Info() << "Control: engine stopped";
		}
		else
		{
			core.waitForCommand();
		}
	}
	return 0;
}

static void parseCLI(int argc, char *argv[], RunState &state, Config &c, int &cb)
{
	const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
	int ptr = 1;

	while (ptr < argc)
	{
		Receiver &receiver = *state.receivers.back();

		std::string param = std::string(argv[ptr]);
		Assert(param[0] == '-', param, "setting does not start with \"-\".");

		int count = 0;
		while (ptr + count + 1 < argc && !isOption(argv[ptr + 1 + count]))
			count++;

		std::string arg1 = count >= 1 ? std::string(argv[ptr + 1]) : "";
		std::string arg2 = count >= 2 ? std::string(argv[ptr + 2]) : "";
		std::string arg3 = count >= 3 ? std::string(argv[ptr + 3]) : "";

		switch (param[1])
		{
		case 'G':
			Assert(count % 2 == 0, param, "requires parameters in key/value pairs");
			Util::Convert::toUpper(arg1);
			Util::Convert::toUpper(arg2);

			if (cb != -1 && arg1 == "SYSTEM" && arg2 == "ON")
			{
				Logger::getInstance().removeLogListener(cb);
				cb = -1;
				// Enable DEBUG level when switching to system logging for journalctl filtering
				Logger::getInstance().setMinLevel(LogLevel::DEBUG);
			}
			parseSettings(Logger::getInstance(), argv, ptr, argc);
			break;
		case 's':
			Assert(count == 1, param, "does require one parameter [sample rate].");
			receiver.SetKey(AIS::KEY_SETTING_SAMPLE_RATE, arg1);
			break;
		case 'm':
			Assert(count == 1, param, "requires one parameter [model number].");
			receiver.addModel(Util::Parse::Integer(arg1, 0, 9));
			break;
		case 'M':
			Assert(count <= 1, param, "requires zero or one parameter [DT].");
			receiver.clearTags();
			receiver.SetKey(AIS::KEY_SETTING_META, arg1);
			break;
		case 'c':
			Assert(count <= 2 && count >= 1, param, "requires one or two parameter [AB/CD]].");
			if (count == 1)
				receiver.SetKey(AIS::KEY_SETTING_CHANNEL, arg1);
			if (count == 2)
				receiver.setChannel(arg1, arg2);
			break;
		case 'C':
			Assert(count == 1, param, "one parameter required: filename");

			if (!arg1.empty())
			{
				c.read(arg1);
			}
			break;
		case 'N':
#ifdef HASWEBVIEWER
			Assert(count > 0, param, "requires at least one parameter");
			if (state.servers.size() == 0)
				state.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));

			if (count % 2 == 1)
			{
				// -N port creates a new server assuming the previous one is complete (i.e. has a port set)
				if (state.servers.back()->isPortSet())
					state.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
				state.servers.back()->SetKey(AIS::KEY_SETTING_PORT, arg1);
			}
			state.servers.back()->active() = true;
			parseSettings(*state.servers.back(), argv, ptr + (count % 2), argc);
#else
			throw std::runtime_error("WebViewer support not compiled in.");
#endif
			break;
		case 'S':
			Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter [port].");
			{
				state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPlistenerStreamer()));
				IO::OutputMessage &u = *state.msg.back();
				u.SetKey(AIS::KEY_SETTING_PORT, arg1).SetKey(AIS::KEY_SETTING_TIMEOUT, "0");
				if (count > 1)
					parseSettings(u, argv, ptr + 1, argc);
			}
			break;
		case 'f':
		{
			state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::FileOutput()));
			IO::OutputMessage &f = *state.msg.back();
			if (count % 2 == 1)
			{
				f.SetKey(AIS::KEY_SETTING_FILE, arg1);
			}
			if (count > 1)
				parseSettings(f, argv, ptr + (count % 2), argc);
		}
		break;
		case 'v':
			Assert(count <= 1, param);
			if (param.length() == 3 && param[2] == '+')
			{
				// -v+ applies to last receiver only, no time parameter
				Assert(count == 0, param, "no parameters allowed with -v+");
				receiver.verbose = true;
			}
			else
			{
				// -v or -v* applies to all receivers (after loop)
				Assert(param.length() == 2 || (param.length() == 3 && param[2] == '*'), param, "invalid verbose option");
				state.verbose = true;
				if (count == 1)
					state.screen.verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
			}
			break;
		case 'O':
			Assert(count == 1, param);
			state.own_mmsi = Util::Parse::Integer(arg1, 1, 999999999);
			break;
		case 'T':
			Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
			state.timeout = Util::Parse::Integer(arg1, 1, 3600);
			if (count == 2)
				state.timeout_nomsg = true;
			break;
		case 'q':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			state.screen.setScreen("0");
			break;
		case 'n':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			state.screen.setScreen("1");
			break;
		case 'o':
			if (count % 2 == 1)
				state.screen.setScreen(arg1);
			parseSettings(state.screen, argv, ptr + (count % 2), argc);
			break;
		case 'F':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.addModel(2)->SetKey(AIS::KEY_SETTING_FP_DS, "ON").SetKey(AIS::KEY_SETTING_PS_EMA, "ON");
			receiver.removeTags("DT");
			break;
		case 't':
		{
			Assert(count > 0, param, "requires one parameter [url], or two or three parameters [[protocol]] [host] [port].");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::RTLTCP;
			if (count == 1)
				dm.RTLTCP().SetKey(AIS::KEY_SETTING_URL, arg1);
			else if ((count & 1) == 0)
				dm.RTLTCP().SetKey(AIS::KEY_SETTING_PORT, arg2).SetKey(AIS::KEY_SETTING_HOST, arg1);
			else if ((count & 1) == 1)
				dm.RTLTCP().SetKey(AIS::KEY_SETTING_PORT, arg3).SetKey(AIS::KEY_SETTING_HOST, arg2).SetKey(AIS::KEY_SETTING_PROTOCOL, arg1);

			if (count >= 4)
				parseSettings(dm.RTLTCP(), argv, ptr + 2 + (count & 1), argc);
		}
		break;
		case 'x':
		{
			Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [server] [port] (optionally followed by key value pairs).");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::UDP;
			dm.UDP().SetKey(AIS::KEY_SETTING_PORT, arg2).SetKey(AIS::KEY_SETTING_SERVER, arg1);
			if (count >= 4)
				parseSettings(dm.UDP(), argv, ptr + 2, argc);
		}
		break;
		case 'D':
		{
			state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::PostgreSQL()));
			IO::OutputMessage &d = *state.msg.back();

			if (count % 2 == 1)
			{
				d.SetKey(AIS::KEY_SETTING_CONN_STR, arg1);
				if (count > 1)
					parseSettings(d, argv, ptr + 1, argc);
			}
			else
			{
				if (count >= 2)
					parseSettings(d, argv, ptr, argc);
			}
		}
		break;
		case 'y':
		{
			Assert(count <= 2, param, "requires one or two parameters [url] or [host] [port].");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::SPYSERVER;
			if (count == 1)
				dm.SpyServer().SetKey(AIS::KEY_SETTING_URL, arg1);
			else if (count == 2)
				dm.SpyServer().SetKey(AIS::KEY_SETTING_PORT, arg2).SetKey(AIS::KEY_SETTING_HOST, arg1);
		}
		break;
		case 'z':
		{
			Assert(count > 0, param, "requires one parameter [endpoint] or two parameters [[format]] [endpoint].");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::ZMQ;
			if ((count & 1) == 1)
				dm.ZMQ().SetKey(AIS::KEY_SETTING_ENDPOINT, arg1);
			else if ((count & 1) == 0)
				dm.ZMQ().SetKey(AIS::KEY_SETTING_FORMAT, arg1).SetKey(AIS::KEY_SETTING_ENDPOINT, arg2);

			if (count >= 3)
				parseSettings(dm.ZMQ(), argv, ptr + 2 - (count & 1), argc);
		}
		break;
		case 'b':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.Timing() = true;
			break;
		case 'i':
		{
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::N2K;
			if ((count & 1) == 1)
				dm.N2KSCAN().SetKey(AIS::KEY_SETTING_INTERFACE, arg1);

			if (count >= 2)
				parseSettings(dm.N2KSCAN(), argv, ptr + (count & 1), argc);
		}
		break;

		case 'w':
		{
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::WAVFILE;
			if ((count & 1) == 1)
				dm.WAV().SetKey(AIS::KEY_SETTING_FILE, arg1);

			if (count >= 2)
				parseSettings(dm.WAV(), argv, ptr + (count & 1), argc);
		}
		break;
		case 'r':
		{
			Assert(count > 0, param, "requires one parameter [filename] or two parameters [[format]] [filename].");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::RAWFILE;
			if ((count & 1) == 1)
				dm.RAW().SetKey(AIS::KEY_SETTING_FILE, arg1);
			else if ((count & 1) == 0)
				dm.RAW().SetKey(AIS::KEY_SETTING_FORMAT, arg1).SetKey(AIS::KEY_SETTING_FILE, arg2);

			if (count >= 3)
				parseSettings(dm.RAW(), argv, ptr + 2 - (count & 1), argc);
		}
		break;
		case 'e':
		{
			Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [baudrate] [portname] (optionally followed by key value pairs).");
			DeviceManager &dm = state.newReceiver().getDeviceManager();
			dm.InputType() = Type::SERIALPORT;
			dm.SerialPort().SetKey(AIS::KEY_SETTING_BAUDRATE, arg1).SetKey(AIS::KEY_SETTING_PORT, arg2);
			if (count >= 4)
				parseSettings(dm.SerialPort(), argv, ptr + 2, argc);
		}
		break;
		case 'l':
			Assert(count == 0 || count == 2, param, "takes no parameters or [JSON on/off].");
			if (count == 2)
			{
				Assert(arg1 == "JSON", param, "requires JSON on/off");
				state.list_devices_JSON = Util::Parse::Switch(arg2);
			}
			state.list_devices = true;
			break;
		case 'L':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			state.list_support = true;
			break;
		case 'd':
		{
			DeviceManager &dm = state.newReceiver().getDeviceManager();

			if (param.length() >= 4 && param[2] == ':')
			{
				Assert(count == 0, param, MSG_NO_PARAMETER);
				int n = (int)Util::Parse::Integer(param.substr(3));
				dm.selectDeviceByIndex(n);
			}
			else
			{
				Assert(param.length() == 2, param, "syntax error in device setting");
				Assert(count == 1, param, "device setting requires one parameter [serial number]");
				dm.SerialNumber() = arg1;
			}
		}
		break;
		case 'u':
			Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
			{
				state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::UDPStreamer()));
				IO::OutputMessage &o = *state.msg.back();
				o.SetKey(AIS::KEY_SETTING_HOST, arg1).SetKey(AIS::KEY_SETTING_PORT, arg2);
				if (count > 2)
					parseSettings(o, argv, ptr + 2, argc);
			}
			break;
		case 'P':
			Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
			{
				state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
				IO::OutputMessage &p = *state.msg.back();
				p.SetKey(AIS::KEY_SETTING_HOST, arg1).SetKey(AIS::KEY_SETTING_PORT, arg2);
				if (count > 2)
					parseSettings(p, argv, ptr + 2, argc);
			}
			break;
		case 'Q':
			Assert(count >= 1, param, "invalid number of arguments");
			{
				state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::MQTTStreamer()));
				IO::OutputMessage &p = *state.msg.back();

				if (count % 2 == 1)
				{
					p.SetKey(AIS::KEY_SETTING_URL, arg1);
				}
				if (count >= 2)
					parseSettings(p, argv, ptr + (count % 2), argc);
			}
			break;
		case 'X':
			Assert(count <= 1, param, "Only one optional parameter [sharing key] allowed.");
			{
				state.xshare_defined = true;

				std::string xarg_upper = arg1;
				Util::Convert::toUpper(xarg_upper);

				if (count == 1 && xarg_upper == "OFF")
				{
					// Explicitly disable sharing if "off" is provided as second parameter
					Info() << "Community feed sharing disabled.";
					break;
				}

				bool xarg_is_on = (count == 1 && xarg_upper == "ON");

				if (!state.comm_feed)
					state.createCommunityFeed();

				if (count >= 1 && !xarg_is_on)
					state.comm_feed->SetKey(AIS::KEY_SETTING_UUID, arg1);
			}
			break;
		case 'H':
			Assert(count > 0, param);
			{
				state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::HTTPStreamer()));
				IO::OutputMessage &h = *state.msg.back();
				if (count % 2)
					h.SetKey(AIS::KEY_SETTING_URL, arg1);
				parseSettings(h, argv, ptr + (count % 2), argc);
			}
			break;
		case 'Z':
			Assert(count == 2, param, "Location Setting requires two parameters (lat/lon)");
			receiver.SetKey(AIS::KEY_SETTING_LAT, arg1).SetKey(AIS::KEY_SETTING_LON, arg2);
			break;
		case 'A':
			throw std::runtime_error("Option -A is obsolete. Please use -I instead.");
			break;
		case 'E':
			throw std::runtime_error("Option -E (managed mode) must be the only option: AIS-catcher -E <config file> [port].");
			break;
		case 'I':
		{
#ifdef HASNMEA2000
			state.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::N2KStreamer()));
			IO::OutputMessage &h = *state.msg.back();
			if (count % 2)
				h.SetKey(AIS::KEY_SETTING_DEVICE, arg1);

			if (count > 1)
				parseSettings(h, argv, ptr + (count % 2), argc);
#else
			throw std::runtime_error("NMEA2000 support not compiled in.");
#endif
		}
		break;
		case 'h':
			Assert(count == 0 || count == 1, param, "takes no parameters or one parameter [JSON/BUILD].");
			if (count == 1)
			{
				Util::Convert::toUpper(arg1);
				Assert(arg1 == "JSON" || arg1 == "BUILD", param, "parameter needs to be JSON or BUILD");

				if (arg1 == "JSON")
					std::cout << "{\"version\":\"" << VERSION << "\",\"version_describe\":\"" << VERSION_DESCRIBE << "\",\"version_code\":" << VERSION_NUMBER << "}\n";
				else
					std::cout << VERSION_DESCRIBE << "\n";

				state.no_run = true;
				state.show_copyright = false;
			}
			else
				state.list_options = true;
			break;
		case 'p':
			Assert(count == 1, param, "requires one parameter [frequency offset].");
			receiver.SetKey(AIS::KEY_SETTING_FREQOFFSET, arg1);
			break;
		case 'a':
			Assert(count == 1, param, "requires one parameter [bandwidth].");
			receiver.SetKey(AIS::KEY_SETTING_BANDWIDTH, arg1);
			break;
		case 'g':
			Assert(count % 2 == 0 && param.length() == 3, param);
			switch (param[2])
			{
			case 'e':
				parseSettings(receiver.getDeviceManager().SerialPort(), argv, ptr, argc);
				break;
			case 'm':
				parseSettings(receiver.getDeviceManager().AIRSPY(), argv, ptr, argc);
				break;
			case 'd':
				parseSettings(receiver.getDeviceManager().HYDRASDR(), argv, ptr, argc);
				break;
			case 'r':
				parseSettings(receiver.getDeviceManager().RTLSDR(), argv, ptr, argc);
				break;
			case 'h':
				parseSettings(receiver.getDeviceManager().AIRSPYHF(), argv, ptr, argc);
				break;
			case 's':
				parseSettings(receiver.getDeviceManager().SDRPLAY(), argv, ptr, argc);
				break;
			case 'a':
				parseSettings(receiver.getDeviceManager().RAW(), argv, ptr, argc);
				break;
			case 'w':
				parseSettings(receiver.getDeviceManager().WAV(), argv, ptr, argc);
				break;
			case 't':
				parseSettings(receiver.getDeviceManager().RTLTCP(), argv, ptr, argc);
				break;
			case 'y':
				parseSettings(receiver.getDeviceManager().SpyServer(), argv, ptr, argc);
				break;
			case 'f':
				parseSettings(receiver.getDeviceManager().HACKRF(), argv, ptr, argc);
				break;
			case 'u':
				parseSettings(receiver.getDeviceManager().SOAPYSDR(), argv, ptr, argc);
				break;
			case 'z':
				parseSettings(receiver.getDeviceManager().ZMQ(), argv, ptr, argc);
				break;
			case 'o':
				if (receiver.Count() == 0)
					receiver.addModel(receiver.getDeviceManager().isTXTformatSet() ? 5 : 2);
				parseSettings(*receiver.Model(receiver.Count() - 1), argv, ptr, argc);
				break;
			default:
				throw std::runtime_error("invalid -g switch on command line");
			}
			break;
		default:
			throw std::runtime_error("unknown option on command line (" + std::string(1, param[1]) + ").");
		}

		ptr += count + 1;
	}

	// Apply verbose setting to all receivers
	if (state.verbose)
	{
		for (auto &r : state.receivers)
		{
			r->SetKey(AIS::KEY_SETTING_VERBOSE, "on");
		}
	}

	if (state.show_copyright)
		printVersion();

	if ((!state.xshare_defined && !c.isSharingDefined()) && (state.msg.size() > 0
#ifdef HASWEBVIEWER
		|| state.servers.size() > 0
#endif
	))
	{
		Warning() << "Hint: Use '-X on' to share with aiscatcher.org community (enables community overlay) or '-X off' to disable. Currently ON by default.";

		if (!state.comm_feed)
			state.createCommunityFeed();
	}

	if (state.list_devices)
		state.receivers.back()->getDeviceManager().printAvailableDevices(state.list_devices_JSON);
	if (state.list_support)
		printBuildConfiguration();
	if (state.list_options)
		Usage();
}

int main(int argc, char *argv[])
{
	RunState state;
	Config c(state);
	int cb = -1;

	try
	{
		Logger::getInstance().setMaxBufferSize(50);
		cb = Logger::getInstance().addLogListener([](const LogMessage &msg)
												  { std::cerr << msg.message << "\n"; });

#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("could not set control handler");
#else
		signal(SIGINT, consoleHandler);
		signal(SIGTERM, consoleHandler);
		signal(SIGHUP, consoleHandler);
		signal(SIGPIPE, consoleHandler);
#endif

		state.receivers.back()->getDeviceManager().refreshDevices();

		std::vector<std::string> expanded;
		expandResponseFiles(argc, argv, expanded);

		bool managed = false;
		for (const auto &a : expanded)
			managed |= (a == "-E");

		if (managed)
		{
			if (expanded.size() < 3 || expanded.size() > 4 || expanded[1] != "-E")
				throw std::runtime_error("in managed mode all settings live in the config file: AIS-catcher -E <config file> [port]");

			std::string file;
			int port = 0;

			for (std::size_t i = 2; i < expanded.size(); i++)
			{
				const std::string &a = expanded[i];
				if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos)
					port = Util::Parse::Integer(a, 1, 65535);
				else if (file.empty())
					file = a;
				else
					throw std::runtime_error("managed mode takes one config file and optionally one port: AIS-catcher -E <config file> [port]");
			}

			if (file.empty())
				throw std::runtime_error("managed mode requires a config file: AIS-catcher -E <config file> [port]");

			return runManaged(file, port);
		}

		std::vector<char *> argv_expanded;
		argv_expanded.reserve(expanded.size() + 1);
		for (auto &s : expanded) argv_expanded.push_back(&s[0]);
		argv_expanded.push_back(nullptr);

		parseCLI((int)expanded.size(), argv_expanded.data(), state, c, cb);

		if (state.list_devices || state.list_support || state.list_options || state.no_run)
			return 0;

		run(state);
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		for (auto &r : state.receivers)
			r->stop();
		state.exit_code = -1;
	}

	return state.exit_code;
}
