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

#include <iostream>
#include <fstream>
#include <string.h>
#include <memory>
#include <cctype>

#include "AIS-catcher.h"

#include "CommandLine.h"
#include "Receiver.h"
#ifdef HASWEBVIEWER
#include "WebViewer.h"
#endif
#include "Engine.h"
#include "Config.h"
#include "JSON.h"
#include "JSON/Parser.h"
#include "N2KStream.h"
#include "Logger.h"
#include "Screen.h"
#include "File.h"

namespace CommandLine
{

void printVersion()
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
	Info() << "\t[-D [connection string] - write messages to a database: libpq string, sqlite:[file] or csv:[directory]]";
	Info() << "\t[-e [baudrate] [serial port] - read NMEA from serial port at specified baudrate]";
	Info() << "\t[-E [config file] [bind address:port] - managed mode: engine run from config file with control server, must be only option (defaults: config.json, control port 8118, viewer on control port + 1, local access without password; use 0.0.0.0:port for LAN access with password)]";
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
	Info() << "\t[-go Model: AFC_WIDE [on/off] FP_DS [on/off] PS_EMA [on/off] SOXR [on/off] SRC [on/off] DROOP [on/off] DD_TRAIN [weight] DD_WEIGHT [weight] ]";
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

// Assign the positional arguments to keys, then hand any remaining key/value
// pairs to parseSettings. The caller has already asserted the argument count.
static void setDeviceArgs(Setting &dev, std::initializer_list<AIS::Keys> keys, char *argv[], int ptr, int count, int argc)
{
	int i = 0;
	for (AIS::Keys k : keys)
		dev.SetKey(k, argv[ptr + 1 + i++]);

	if (count > (int)keys.size())
		parseSettings(dev, argv, ptr + (int)keys.size(), argc);
}

template <typename T>
static IO::OutputMessage &addOutput(Engine &engine)
{
	engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(new T()));
	return *engine.msg.back();
}

static void Assert(bool b, std::string &context, const std::string &msg = "")
{
	if (!b)
	{
		throw std::runtime_error("syntax error on command line with setting \"" + context + "\". " + msg + "\n");
	}
}

static void parseCLI(int argc, char *argv[], Engine &engine, Config &c, int &cb)
{
	const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
	int ptr = 1;

	auto newDevice = [&engine](Type t) -> DeviceManager &
	{
		DeviceManager &dm = engine.newReceiver().getDeviceManager();
		dm.InputType() = t;
		return dm;
	};

	while (ptr < argc)
	{
		Receiver &receiver = *engine.receivers.back();

		std::string param = std::string(argv[ptr]);
		Assert(param[0] == '-', param, "setting does not start with \"-\".");

		int count = 0;
		while (ptr + count + 1 < argc && !isOption(argv[ptr + 1 + count]))
			count++;

		std::string arg1 = count >= 1 ? std::string(argv[ptr + 1]) : "";
		std::string arg2 = count >= 2 ? std::string(argv[ptr + 2]) : "";

		switch (param[1])
		{
		case 'G':
			Assert(count % 2 == 0, param, "requires parameters in key/value pairs");

			if (cb != -1)
			{
				for (int k = 0; k + 1 < count; k += 2)
				{
					std::string key = argv[ptr + 1 + k];
					Util::Convert::toUpper(key);
					if (key == "SYSTEM" && Util::Parse::Switch(argv[ptr + 2 + k]))
					{
						Logger::getInstance().removeLogListener(cb);
						cb = -1;
						Logger::getInstance().setMinLevel(LogLevel::DEBUG);
						break;
					}
				}
			}
			parseSettings(Logger::getInstance(), argv, ptr, argc);
			break;
		case 's':
			Assert(count == 1, param, "does require one parameter [sample rate].");
			receiver.SetKey(AIS::KEY_SETTING_SAMPLE_RATE, arg1);
			break;
		case 'm':
			Assert(count == 1, param, "requires one parameter [model number].");
			receiver.addModel(Util::Parse::Integer(arg1, 0, AIS::MODEL_MAX));
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
			if (engine.servers.size() == 0)
				engine.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));

			if (count % 2 == 1)
			{
				// -N port creates a new server assuming the previous one is complete (i.e. has a port set)
				if (engine.servers.back()->isPortSet())
					engine.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
				engine.servers.back()->SetKey(AIS::KEY_SETTING_PORT, arg1);
			}
			engine.servers.back()->setActive(true);
			parseSettings(*engine.servers.back(), argv, ptr + (count % 2), argc);
#else
			throw std::runtime_error("WebViewer support not compiled in.");
#endif
			break;
		case 'S':
			Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter [port].");
			{
				IO::OutputMessage &u = addOutput<IO::TCPlistenerStreamer>(engine);
				u.SetKey(AIS::KEY_SETTING_PORT, arg1).SetKey(AIS::KEY_SETTING_TIMEOUT, "0");
				if (count > 1)
					parseSettings(u, argv, ptr + 1, argc);
			}
			break;
		case 'f':
		{
			IO::OutputMessage &f = addOutput<IO::FileOutput>(engine);
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
				engine.verbose = true;
				if (count == 1)
					engine.screen.verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
			}
			break;
		case 'O':
			Assert(count == 1, param);
			engine.own_mmsi = Util::Parse::Integer(arg1, 1, 999999999);
			break;
		case 'T':
			Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
			engine.timeout = Util::Parse::Integer(arg1, 1, 3600);
			if (count == 2)
				engine.timeout_nomsg = true;
			break;
		case 'q':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			engine.screen.setScreen("0");
			break;
		case 'n':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			engine.screen.setScreen("1");
			break;
		case 'o':
			if (count % 2 == 1)
				engine.screen.setScreen(arg1);
			parseSettings(engine.screen, argv, ptr + (count % 2), argc);
			break;
		case 'F':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.addModel(AIS::MODEL_V1_BASE)->SetKey(AIS::KEY_SETTING_FP_DS, "ON").SetKey(AIS::KEY_SETTING_PS_EMA, "ON");
			receiver.removeTags("DT");
			break;
		case 't':
		{
			Assert(count > 0, param, "requires one parameter [url], or two or three parameters [[protocol]] [host] [port].");
			DeviceManager &dm = newDevice(Type::RTLTCP);
			if (count == 1)
				setDeviceArgs(dm.RTLTCP(), {AIS::KEY_SETTING_URL}, argv, ptr, count, argc);
			else if ((count & 1) == 0)
				setDeviceArgs(dm.RTLTCP(), {AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, argv, ptr, count, argc);
			else
				setDeviceArgs(dm.RTLTCP(), {AIS::KEY_SETTING_PROTOCOL, AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, argv, ptr, count, argc);
		}
		break;
		case 'x':
		{
			Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [server] [port] (optionally followed by key value pairs).");
			DeviceManager &dm = newDevice(Type::UDP);
			setDeviceArgs(dm.UDP(), {AIS::KEY_SETTING_SERVER, AIS::KEY_SETTING_PORT}, argv, ptr, count, argc);
		}
		break;
		case 'D':
		{
			// bare target = libpq string; a "sqlite:" or "csv:" prefix picks the backend
			std::string type = "postgres";
			std::string target = count % 2 == 1 ? arg1 : std::string();

			if (target.compare(0, 7, "sqlite:") == 0)
			{
				type = "sqlite";
				target = target.substr(7);
			}
			else if (target.compare(0, 4, "csv:") == 0)
			{
				type = "csv";
				target = target.substr(4);
			}

			engine.msg.push_back(std::unique_ptr<IO::OutputMessage>(Config::newDatabaseOutput(type)));
			IO::OutputMessage &d = *engine.msg.back();

			if (count % 2 == 1)
			{
				d.SetKey(AIS::KEY_SETTING_CONN_STR, target);
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
			DeviceManager &dm = newDevice(Type::SPYSERVER);
			if (count == 1)
				setDeviceArgs(dm.SpyServer(), {AIS::KEY_SETTING_URL}, argv, ptr, count, argc);
			else if (count == 2)
				setDeviceArgs(dm.SpyServer(), {AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, argv, ptr, count, argc);
		}
		break;
		case 'z':
		{
			Assert(count > 0, param, "requires one parameter [endpoint] or two parameters [[format]] [endpoint].");
			DeviceManager &dm = newDevice(Type::ZMQ);
			if ((count & 1) == 1)
				setDeviceArgs(dm.ZMQ(), {AIS::KEY_SETTING_ENDPOINT}, argv, ptr, count, argc);
			else
				setDeviceArgs(dm.ZMQ(), {AIS::KEY_SETTING_FORMAT, AIS::KEY_SETTING_ENDPOINT}, argv, ptr, count, argc);
		}
		break;
		case 'b':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			receiver.Timing() = true;
			break;
		case 'i':
		{
			DeviceManager &dm = newDevice(Type::N2K);
			if ((count & 1) == 1)
				setDeviceArgs(dm.N2KSCAN(), {AIS::KEY_SETTING_INTERFACE}, argv, ptr, count, argc);
			else
				setDeviceArgs(dm.N2KSCAN(), {}, argv, ptr, count, argc);
		}
		break;

		case 'w':
		{
			DeviceManager &dm = newDevice(Type::WAVFILE);
			if ((count & 1) == 1)
				setDeviceArgs(dm.WAV(), {AIS::KEY_SETTING_FILE}, argv, ptr, count, argc);
			else
				setDeviceArgs(dm.WAV(), {}, argv, ptr, count, argc);
		}
		break;
		case 'r':
		{
			Assert(count > 0, param, "requires one parameter [filename] or two parameters [[format]] [filename].");
			DeviceManager &dm = newDevice(Type::RAWFILE);
			if ((count & 1) == 1)
				setDeviceArgs(dm.RAW(), {AIS::KEY_SETTING_FILE}, argv, ptr, count, argc);
			else
				setDeviceArgs(dm.RAW(), {AIS::KEY_SETTING_FORMAT, AIS::KEY_SETTING_FILE}, argv, ptr, count, argc);
		}
		break;
		case 'e':
		{
			Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [baudrate] [portname] (optionally followed by key value pairs).");
			DeviceManager &dm = newDevice(Type::SERIALPORT);
			setDeviceArgs(dm.SerialPort(), {AIS::KEY_SETTING_BAUDRATE, AIS::KEY_SETTING_PORT}, argv, ptr, count, argc);
		}
		break;
		case 'l':
			Assert(count == 0 || count == 2, param, "takes no parameters or [JSON on/off].");
			if (count == 2)
			{
				Assert(arg1 == "JSON", param, "requires JSON on/off");
				engine.list_devices_JSON = Util::Parse::Switch(arg2);
			}
			engine.list_devices = true;
			break;
		case 'L':
			Assert(count == 0, param, MSG_NO_PARAMETER);
			engine.list_support = true;
			break;
		case 'd':
		{
			DeviceManager &dm = engine.newReceiver().getDeviceManager();

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
				IO::OutputMessage &o = addOutput<IO::UDPStreamer>(engine);
				o.SetKey(AIS::KEY_SETTING_HOST, arg1).SetKey(AIS::KEY_SETTING_PORT, arg2);
				if (count > 2)
					parseSettings(o, argv, ptr + 2, argc);
			}
			break;
		case 'P':
			Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
			{
				IO::OutputMessage &p = addOutput<IO::TCPClientStreamer>(engine);
				p.SetKey(AIS::KEY_SETTING_HOST, arg1).SetKey(AIS::KEY_SETTING_PORT, arg2);
				if (count > 2)
					parseSettings(p, argv, ptr + 2, argc);
			}
			break;
		case 'Q':
			Assert(count >= 1, param, "invalid number of arguments");
			{
				IO::OutputMessage &p = addOutput<IO::MQTTStreamer>(engine);

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
				engine.xshare_defined = true;

				std::string xarg_upper = arg1;
				Util::Convert::toUpper(xarg_upper);

				if (count == 1 && xarg_upper == "OFF")
				{
					// Explicitly disable sharing if "off" is provided as second parameter
					Info() << "Community feed sharing disabled.";
					break;
				}

				bool xarg_is_on = (count == 1 && xarg_upper == "ON");

				if (!engine.comm_feed)
					engine.createCommunityFeed();

				if (count >= 1 && !xarg_is_on)
					engine.comm_feed->SetKey(AIS::KEY_SETTING_UUID, arg1);
			}
			break;
		case 'H':
			Assert(count > 0, param);
			{
				IO::OutputMessage &h = addOutput<IO::HTTPStreamer>(engine);
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
			throw std::runtime_error("Option -E (managed mode) must be the only option: AIS-catcher -E [config file] [bind address:port].");
			break;
		case 'I':
		{
#ifdef HASNMEA2000
			IO::OutputMessage &h = addOutput<IO::N2KStreamer>(engine);
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

				engine.no_run = true;
				engine.show_copyright = false;
			}
			else
				engine.list_options = true;
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
			if (param[2] == 'o')
			{
				if (receiver.Count() == 0)
					receiver.addModel(receiver.getDeviceManager().isTXTformatSet() ? AIS::MODEL_NMEA : AIS::MODEL_V1_BASE);
				parseSettings(*receiver.Model(receiver.Count() - 1), argv, ptr, argc);
			}
			else
			{
				Setting *device = receiver.getDeviceManager().settingForFlag(param[2]);
				if (!device)
					throw std::runtime_error("invalid -g switch on command line");

				parseSettings(*device, argv, ptr, argc);
			}
			break;
		default:
			throw std::runtime_error("unknown option on command line (" + std::string(1, param[1]) + ").");
		}

		ptr += count + 1;
	}

	// Apply verbose setting to all receivers
	if (engine.verbose)
	{
		for (auto &r : engine.receivers)
		{
			r->SetKey(AIS::KEY_SETTING_VERBOSE, "on");
		}
	}

	if (engine.show_copyright)
		printVersion();

	if (engine.list_devices)
		engine.receivers.back()->getDeviceManager().printAvailableDevices(engine.list_devices_JSON);
	if (engine.list_support)
		printBuildConfiguration();
	if (engine.list_options)
		Usage();
}


int run(const std::vector<std::string> &args, int &cb)
{
	Engine engine;
	Config c(engine);

	try
	{
		engine.receivers.back()->getDeviceManager().refreshDevices();

		// parseCLI() still speaks argv; the copy gives it writable storage
		std::vector<std::string> storage = args;
		std::vector<char *> argv;
		argv.reserve(storage.size() + 1);
		for (auto &s : storage)
			argv.push_back(&s[0]);
		argv.push_back(nullptr);

		parseCLI((int)storage.size(), argv.data(), engine, c, cb);

		if (engine.list_devices || engine.list_support || engine.list_options || engine.no_run)
			return 0;

		engine.run();
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		engine.detach();
		engine.exit_code = -1;
	}

	return engine.exit_code;
}

}
