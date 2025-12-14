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

#include <signal.h>

#include <iostream>
#include <string.h>
#include <memory>

#include "AIS-catcher.h"

#include "Receiver.h"
#include "WebViewer.h"
#include "Config.h"
#include "ApplicationContext.h"
#include "JSON.h"
#include "N2KStream.h"
#include "PostgreSQL.h"
#include "Logger.h"
#include "Screen.h"
#include "File.h"

enum class ApplicationState
{
	Busy,	   // Starting or stopping - transitional state
	Running,   // Actively processing
	Idle,	   // Stopped
	Terminated // Final state, exiting
};

static std::atomic<bool> stop;
static std::atomic<bool> terminate;
static std::atomic<bool> paused{false};
static std::atomic<ApplicationState> app_state{ApplicationState::Busy};
static int log_callback = -1;

void StopRequest()
{
	stop = true;
}

ApplicationState GetApplicationState()
{
	return app_state;
}

void SetPaused(bool pause)
{
	if (pause)
	{
		// Request to pause - stop current processing
		if (app_state == ApplicationState::Running)
		{
			Info() << "Pause requested";
			stop = true;
			paused = true;
		}
	}
	else
	{
		// Request to play/resume
		if (paused)
		{
			Info() << "Resume requested";
			paused = false;
		}
	}
}

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

static struct termios original_termios;
static bool termios_saved = false;

static void restoreTerminal()
{
	if (termios_saved)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
		termios_saved = false;
	}
}

static void keyboardMonitor()
{
	// Set terminal to non-canonical mode to read individual keypresses
	struct termios newt;
	tcgetattr(STDIN_FILENO, &original_termios);
	termios_saved = true;
	newt = original_termios;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	while (!terminate)
	{
		char ch;
		if (read(STDIN_FILENO, &ch, 1) == 1 && ch == ' ')
		{
			if (GetApplicationState() == ApplicationState::Running)
			{
				SetPaused(true);
			}
			else if (paused)
			{
				SetPaused(false);
			}
		}
		SleepSystem(100);
	}

	// Restore terminal settings
	restoreTerminal();
}
#endif

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
	{
		stop = true;
		terminate = true;
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

	restoreTerminal();
	stop = true;
	terminate = true;
}
#endif

static void printVersion()
{
	Info() << "AIS-catcher (build " << __DATE__ << ") " << VERSION_DESCRIBE << "\n"
		   << "(C) Copyright 2021-2025 " << COPYRIGHT << "\n"
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
	Info() << "\t[-ge Serial Port: PRINT [on/off]";
	Info() << "\t[-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off] ]";
	Info() << "\t[-gh Airspy HF+: TRESHOLD [low/high] PREAMP [on/off] ]";
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
#ifdef HASCURL
	other_support << "CURL ";
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

static void parseSettings(Setting &s, char *argv[], int ptr, int argc)
{
	ptr++;

	while (ptr < argc - 1 && argv[ptr][0] != '-')
	{
		std::string option = argv[ptr++];
		std::string arg = argv[ptr++];

		s.Set(option, arg);
	}
}

static bool isOption(std::string s)
{
	return s.length() >= 2 && s[0] == '-' && std::isalpha(s[1]);
}

static void Assert(bool b, std::string &context, std::string msg = "")
{
	if (!b)
	{
		throw std::runtime_error("syntax error on command line with setting \"" + context + "\". " + msg + "\n");
	}
}

static void parseCommandLine(ApplicationContext &ctx, int argc, char *argv[])
{
	extern IO::OutputMessage *commm_feed;

	ctx.receivers.back()->getDeviceManager().refreshDevices();

	const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
	int ptr = 1;

	while (ptr < argc)
	{
		Receiver &receiver = *ctx.receivers.back();

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

			if (log_callback != -1 && arg1 == "SYSTEM" && arg2 == "ON")
			{
				Logger::getInstance().removeLogListener(log_callback);
				log_callback = -1;
				// Enable DEBUG level when switching to system logging for journalctl filtering
				Logger::getInstance().setMinLevel(LogLevel::_DEBUG);
			}
			parseSettings(Logger::getInstance(), argv, ptr, argc);
			break;
		case 's':
			Assert(count == 1, param, "does require one parameter [sample rate].");
			receiver.setSampleRate(Util::Parse::Integer(arg1, 12500, 12288000));
			break;
		case 'm':
			Assert(count == 1, param, "requires one parameter [model number].");
			receiver.addModel(Util::Parse::Integer(arg1, 0, 9));
			break;
		case 'M':
			Assert(count <= 1, param, "requires zero or one parameter [DT].");
			receiver.clearTags();
			receiver.setTags(arg1);
			break;
		case 'c':
			Assert(count <= 2 && count >= 1, param, "requires one or two parameter [AB/CD]].");
			if (count == 1)
				receiver.setChannel(arg1);
			if (count == 2)
				receiver.setChannel(arg1, arg2);
			break;
		case 'C':
			Assert(count == 1, param, "one parameter required: filename");
			if (!arg1.empty())
				ctx.config_file = arg1;
			break;
		case 'N':
			Assert(count > 0, param, "requires at least one parameter");
			if (ctx.servers.size() == 0)
				ctx.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));

			if (count % 2 == 1)
			{
				// -N port creates a new server assuming the previous one is complete (i.e. has a port set)
				if (ctx.servers.back()->isPortSet())
					ctx.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
				ctx.servers.back()->Set("PORT", arg1);
			}
			ctx.servers.back()->active() = true;
			parseSettings(*ctx.servers.back(), argv, ptr + (count % 2), argc);
			break;
		case 'S':
			Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter [port].");
			{
				ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPlistenerStreamer()));
				IO::OutputMessage &u = *ctx.msg.back();
				u.Set("PORT", arg1).Set("TIMEOUT", "0");
				if (count > 1)
					parseSettings(u, argv, ptr + 1, argc);
			}
			break;
		case 'f':
		{
			ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::FileOutput()));
			IO::OutputMessage &f = *ctx.msg.back();
			if (count % 2 == 1)
			{
				f.Set("FILE", arg1);
			}
			if (count > 1)
				parseSettings(f, argv, ptr + (count % 2), argc);
		}
		break;
		case 'v':
			Assert(count <= 1, param);
			receiver.verbose = true;
			if (count == 1)
				ctx.screen.verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
			break;
		case 'O':
			Assert(count == 1, param);
			ctx.own_mmsi = Util::Parse::Integer(arg1, 1, 999999999);
			break;
		case 'T':
			Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
			ctx.timeout = Util::Parse::Integer(arg1, 1, 3600);
			if (count == 2)
				ctx.timeout_nomsg = true;
			break;
		case 'q':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			ctx.screen.setScreen("0");
			break;
		case 'n':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			ctx.screen.setScreen("2");
			break;
		case 'o':
			Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter.");
			ctx.screen.setScreen(arg1);
			if (count > 1)
			{
				parseSettings(ctx.screen, argv, ptr + 1, argc);
			}
			break;
		case 'F':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.addModel(2)->Set("FP_DS", "ON").Set("PS_EMA", "ON");
			receiver.removeTags("DT");
			break;
		case 't':
			Assert(count <= 3, param, "requires one parameter [url], or two or three parameters [[protocol]] [host] [port].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::RTLTCP;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().RTLTCP().Set("url", arg1);
			if (count == 2)
				ctx.receivers.back()->getDeviceManager().RTLTCP().Set("port", arg2).Set("host", arg1);
			if (count == 3)
				ctx.receivers.back()->getDeviceManager().RTLTCP().Set("port", arg3).Set("host", arg2).Set("PROTOCOL", arg1);
			break;
		case 'x':
			Assert(count == 2, param, "requires two parameters [server] [port].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::UDP;
			ctx.receivers.back()->getDeviceManager().UDP().Set("port", arg2).Set("server", arg1);
			break;
		case 'D':
		{
			ctx.json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::PostgreSQL()));
			IO::OutputJSON &d = *ctx.json.back();

			if (count % 2 == 1)
			{
				d.Set("CONN_STR", arg1);
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
			Assert(count <= 2, param, "requires one or two parameters [url] or [host] [port].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::SPYSERVER;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().SpyServer().Set("url", arg1);
			else if (count == 2)
				ctx.receivers.back()->getDeviceManager().SpyServer().Set("port", arg2).Set("host", arg1);
			break;
		case 'z':
			Assert(count <= 2, param, "requires at most two parameters [[format]] [endpoint].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::ZMQ;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().ZMQ().Set("ENDPOINT", arg1);
			if (count == 2)
				ctx.receivers.back()->getDeviceManager().ZMQ().Set("FORMAT", arg1).Set("ENDPOINT", arg2);
			break;
		case 'b':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.Timing() = true;
			break;
		case 'i':
			Assert(count <= 1, param, "requires at most one option parameter.");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::N2K;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().N2KSCAN().Set("INTERFACE", arg1);
			break;

		case 'w':
			Assert(count <= 1, param);
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::WAVFILE;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().WAV().Set("FILE", arg1);
			break;
		case 'r':
			Assert(count <= 2, param, "requires at most two parameters [[format]] [filename].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::RAWFILE;
			if (count == 1)
				ctx.receivers.back()->getDeviceManager().RAW().Set("FILE", arg1);
			if (count == 2)
				ctx.receivers.back()->getDeviceManager().RAW().Set("FORMAT", arg1).Set("FILE", arg2);
			break;
		case 'e':
			Assert(count == 2, param, "requires two parameters [baudrate] [portname].");
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			ctx.receivers.back()->getDeviceManager().InputType() = Type::SERIALPORT;
			ctx.receivers.back()->getDeviceManager().SerialPort().Set("BAUDRATE", arg1).Set("PORT", arg2);
			break;
		case 'l':
			Assert(count == 0 || count == 2, param, MSG_NO_PARAMETER);
			if (count == 2)
			{
				Assert(arg1 == "JSON", param, "requires JSON on/off");
				ctx.list_devices_JSON = Util::Parse::Switch(arg2);
			}
			ctx.list_devices = true;
			break;
		case 'L':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			ctx.list_support = true;
			break;
		case 'd':
			if (++ctx.nrec > 1)
			{
				ctx.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}

			if (param.length() == 4 && param[2] == ':')
			{
				Assert(count == 0, param, MSG_NO_PARAMETER);
				int n = param[3] - '0';
				ctx.receivers.back()->getDeviceManager().selectDeviceByIndex(n);
			}
			else
			{
				Assert(param.length() == 2, param, "syntax error in device setting");
				Assert(count == 1, param, "device setting requires one parameter [serial number]");
				ctx.receivers.back()->getDeviceManager().SerialNumber() = arg1;
			}
			break;
		case 'u':
			Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
			{
				ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::UDPStreamer()));
				IO::OutputMessage &o = *ctx.msg.back();
				o.Set("HOST", arg1).Set("PORT", arg2);
				if (count > 2)
					parseSettings(o, argv, ptr + 2, argc);
			}
			break;
		case 'P':
			Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
			{
				ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
				IO::OutputMessage &p = *ctx.msg.back();
				p.Set("HOST", arg1).Set("PORT", arg2);
				if (count > 2)
					parseSettings(p, argv, ptr + 2, argc);
			}
			break;
		case 'Q':
			Assert(count >= 1, param, "invalid number of arguments");
			{
				ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::MQTTStreamer()));
				IO::OutputMessage &p = *ctx.msg.back();

				if (count % 2 == 1)
				{
					p.Set("URL", arg1);
				}
				if (count >= 2)
					parseSettings(p, argv, ptr + (count % 2), argc);
			}
			break;
		case 'X':
			Assert(count <= 1 || (count == 2 && arg2 == "EXP"), param, "Only one optional parameter [sharing key] allowed.");
			{
				if (!commm_feed)
				{
					ctx.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
					commm_feed = ctx.msg.back().get();
					commm_feed->Set("HOST", AISCATCHER_URL).Set("PORT", AISCATCHER_PORT).Set("FILTER", "on").Set("GPS", "off").Set("REMOVE_EMPTY", "on").Set("KEEP_ALIVE", "on").Set("DOWNSAMPLE", "on").Set("INCLUDE_SAMPLE_START", "on");
					if (count == 2 || true)
					{
						// Warning() << "Experimental feature - using COMMUNITY_HUB message format.";
						commm_feed->Set("MSGFORMAT", "COMMUNITY_HUB");
					}
					else
					{
						commm_feed->Set("MSGFORMAT", "JSON_NMEA");
					}
				}

				if (count >= 1 && commm_feed)
					commm_feed->Set("UUID", arg1);
			}
			break;
		case 'H':
			Assert(count > 0, param);
			{
				ctx.json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::HTTPStreamer()));
				IO::OutputJSON &h = *ctx.json.back();
				if (count % 2)
					h.Set("URL", arg1);
				parseSettings(h, argv, ptr + (count % 2), argc);
				receiver.setTags("DT");
			}
			break;
		case 'Z':
			Assert(count == 2, param, "Location Setting requires two parameters (lat/lon)");
			receiver.setLatLon(Util::Parse::Float(arg1), Util::Parse::Float(arg2));
			break;
		case 'A':
		case 'E':
			throw std::runtime_error("Option -" + std::string(1, param[1]) + " is obsolete. Please use -I instead.");
			break;
		case 'I':
		{
#ifdef HASNMEA2000
			ctx.json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::N2KStreamer()));
			IO::OutputJSON &h = *ctx.json.back();
			if (count % 2)
				h.Set("DEVICE", arg1);

			if (count > 1)
				parseSettings(h, argv, ptr + (count % 2), argc);
#else
			throw std::runtime_error("NMEA2000 support not compiled in.");
#endif
		}
		break;
		case 'h':
			Assert(count == 0 || count == 1, param, MSG_NO_PARAMETER);
			if (count == 1)
			{
				Util::Convert::toUpper(arg1);
				Assert(arg1 == "JSON" || arg1 == "BUILD", param, "parameter needs to be JSON or BUILD");

				if (arg1 == "JSON")
					std::cout << "{\"version\":\"" << VERSION << "\",\"version_describe\":\"" << VERSION_DESCRIBE << "\",\"version_code\":" << VERSION_NUMBER << "}\n";
				else
					std::cout << VERSION_DESCRIBE << "\n";

				ctx.no_run = true;
				ctx.show_copyright = false;
			}
			else
				ctx.list_options = true;
			break;
		case 'p':
			Assert(count == 1, param, "requires one parameter [frequency offset].");
			receiver.setPPM(Util::Parse::Integer(arg1, -150, 150));
			break;
		case 'a':
			Assert(count == 1, param, "requires one parameter [bandwidth].");
			receiver.setBandwidth(Util::Parse::Integer(arg1, 0, 20000000));
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
}

int main(int argc, char *argv[])
{

	int exit_code = 0;

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
		throw std::runtime_error("could not set control handler");
#else
	signal(SIGINT, consoleHandler);
	signal(SIGTERM, consoleHandler);
	signal(SIGHUP, consoleHandler);
	signal(SIGPIPE, consoleHandler);
#endif

	Logger::getInstance().setMaxBufferSize(50);
	log_callback = Logger::getInstance().addLogListener([](const LogMessage &msg)
														{ std::cerr << msg.message << "\n"; });

	terminate = false;

#ifndef _WIN32
	// Start keyboard monitor thread for spacebar pause/resume
	std::thread kbd_thread(keyboardMonitor);
	kbd_thread.detach();
#endif

	do
	{
		if (paused)
		{
			SleepSystem(100);
			continue;
		}

		exit_code = 0;
		app_state = ApplicationState::Busy;

		std::unique_ptr<ApplicationContext> ctx(new ApplicationContext());

		try
		{
			parseCommandLine(*ctx, argc, argv);

			if (!ctx->config_file.empty())
			{
				Config c(*ctx);
				c.read(ctx->config_file);
			}
		}
		catch (std::exception const &e)
		{
			Error() << e.what();
			return -1;
		}

		if (ctx->show_copyright)
			printVersion();

		if (ctx->list_devices)
			ctx->receivers.back()->getDeviceManager().printAvailableDevices(ctx->list_devices_JSON);

		if (ctx->list_support)
			printBuildConfiguration();

		if (ctx->list_options)
			Usage();

		if (!ctx->shouldRun())
			break;

		try
		{
			// -------------
			// set up the receiver and open the device

			ctx->Setup();
			ctx->Start();

			app_state = ApplicationState::Running;

			stop = false;
			const int SLEEP = 50;
			auto time_start = high_resolution_clock::now();
			auto time_timeout_start = time_start;
			auto time_last = time_start;

			while (!stop)
			{
				auto time_now = high_resolution_clock::now();

				for (auto &r : ctx->receivers)
					stop = stop || !(r->getDeviceManager().getDevice()->isStreaming());

				if (ctx->isAllCallback()) // don't go to sleep in case we are reading from a file
					std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

				if (!ctx->isAnyVerbose() && !ctx->timeout)
					continue;

				if (ctx->isAnyVerbose() && duration_cast<seconds>(time_now - time_last).count() >= ctx->screen.verboseUpdateTime)
				{
					time_last = time_now;
					ctx->printVerboseStats();
				}

				if (ctx->timeout && ctx->timeout_nomsg)
				{
					bool one_stale = false;

					for (int i = 0; i < ctx->receivers.size(); i++)
					{
						if (ctx->stat[i].statistics[0].getCount() == ctx->msg_count[i])
							one_stale = true;

						ctx->msg_count[i] = ctx->stat[i].statistics[0].getCount();
					}

					if (!one_stale)
						time_timeout_start = time_now;
				}

				if (ctx->timeout && duration_cast<seconds>(time_now - time_timeout_start).count() >= ctx->timeout)
				{
					stop = true;

					if (ctx->timeout_nomsg)
						Warning() << "Stop triggered, no messages were received for " << ctx->timeout << " seconds.";
					else
					{
						exit_code = -2;
						Warning() << "Stop triggered by timeout after " << ctx->timeout << " seconds. (-T " << ctx->timeout << ")";
					}
				}
			}

			// Save paused state before cleanup
			app_state = ApplicationState::Busy;

			ctx->Stop();
			ctx->printFinalStats();

			for (auto &s : ctx->servers)
				s->close();

			
			app_state = ApplicationState::Idle;
		}
		catch (std::exception const &e)
		{
			Error() << e.what();
			exit_code = -1;
			app_state = ApplicationState::Idle;
			SleepSystem(2000);
		}

	} while (!terminate);

	app_state = ApplicationState::Terminated;
#ifndef _WIN32
	restoreTerminal();
#endif
	return exit_code;
}