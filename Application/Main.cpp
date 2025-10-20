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
#include "version.h"
#include "WebViewer.h"
#include "Config.h"
#include "JSON/JSON.h"
#include "MsgOut.h"
#include "N2KStream.h"
#include "PostgreSQL.h"
#include "Logger.h"

static std::atomic<bool> stop;

void StopRequest()
{
	stop = true;
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT)
		stop = true;
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
}
#endif

static void printVersion()
{
	Info() << "AIS-catcher (build " << __DATE__ << ") " << VERSION_DESCRIBE << "\n"
		   << "(C) Copyright 2021-2025 " << COPYRIGHT << "\n"
		   << "This is free software; see the source for copying conditions.There is NO" << "\n"
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
	Info() << "\t[-G control logging";
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

static void printDevices(Receiver &r, bool JSON = false)
{
	if (!JSON)
	{
		Info() << "Found " << r.device_list.size() << " device(s):";

		for (int i = 0; i < r.device_list.size(); i++)
		{
			Info() << i << ": " << r.device_list[i].toString();
		}
	}
	else
	{
		std::cout << "{\"devices\":[";

		for (int i = 0; i < r.device_list.size(); i++)
		{
			std::string type = Util::Parse::DeviceTypeString(r.device_list[i].getType());
			std::cout << "{\"input\":\"" + type;
			std::cout << "\",\"serial\":\"" + r.device_list[i].getSerial();
			std::cout << "\",\"name\":\"" + type + " [" + r.device_list[i].getSerial() + "]\"";

			std::cout << "}" << (i == r.device_list.size() - 1 ? "" : ",");
		}
		std::cout << "]}\n";
	}
}
static void printSupportedDevices()
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

int main(int argc, char *argv[])
{

	// std::string file_config;

	std::vector<std::unique_ptr<Receiver>> _receivers;
	_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));

	OutputScreen screen;
	std::vector<OutputStatistics> stat;
	std::vector<int> msg_count;

	std::vector<std::unique_ptr<WebViewer>> servers;
	std::vector<std::unique_ptr<IO::OutputMessage>> msg;
	std::vector<std::unique_ptr<IO::OutputJSON>> json;

	bool list_devices = false, list_support = false, list_options = false;
	int timeout = 0, nrec = 0, exit_code = 0;
	bool timeout_nomsg = false, list_devices_JSON = false, no_run = false, show_copyright = true;
	int own_mmsi = -1;
	int cb = -1;

	Config c(_receivers, nrec, msg, json, screen, servers, own_mmsi);
	extern IO::OutputMessage *commm_feed;

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

		_receivers.back()->refreshDevices();

		const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
		int ptr = 1;

		while (ptr < argc)
		{
			Receiver &receiver = *_receivers.back();

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
				if (cb != -1)
				{
					Logger::getInstance().removeLogListener(cb);
					cb = -1;
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
				// file_config = arg1;
				if (!arg1.empty())
				{
					c.read(arg1);
				}
				break;
			case 'N':
				Assert(count > 0, param, "requires at least one parameter");
				if (servers.size() == 0)
					servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));

				if (count % 2 == 1)
				{
					// -N port creates a new server assuming the previous one is complete (i.e. has a port set)
					if (servers.back()->isPortSet())
						servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
					servers.back()->Set("PORT", arg1);
				}
				servers.back()->active() = true;
				parseSettings(*servers.back(), argv, ptr + (count % 2), argc);
				break;
			case 'S':
				Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter [port].");
				{
					msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPlistenerStreamer()));
					IO::OutputMessage &u = *msg.back();
					u.Set("PORT", arg1).Set("TIMEOUT", "0");
					if (count > 1)
						parseSettings(u, argv, ptr + 1, argc);
				}
				break;
			case 'B':
				Assert(count == 0, param, "requires no parameters.");
				{
					msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::BluetoothStreamer()));
				}
				break;
			case 'f':
			{
				msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::MessageToFile()));
				IO::OutputMessage &f = *msg.back();
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
					screen.verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
				break;
			case 'O':
				Assert(count == 1, param);
				own_mmsi = Util::Parse::Integer(arg1, 1, 999999999);
				break;
			case 'T':
				Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
				timeout = Util::Parse::Integer(arg1, 1, 3600);
				if (count == 2)
					timeout_nomsg = true;
				break;
			case 'q':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				screen.setScreen(MessageFormat::SILENT);
				break;
			case 'n':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				screen.setScreen(MessageFormat::NMEA);
				break;
			case 'o':
				Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter.");
				screen.setScreen(arg1);
				if (count > 1)
				{
					parseSettings(screen.msg2screen, argv, ptr + 1, argc);
					parseSettings(screen.json2screen, argv, ptr + 1, argc);
				}
				break;
			case 'F':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				receiver.addModel(2)->Set("FP_DS", "ON").Set("PS_EMA", "ON");
				receiver.removeTags("DT");
				break;
			case 't':
				Assert(count <= 3, param, "requires one parameter [url], or two or three parameters [[protocol]] [host] [port].");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::RTLTCP;
				if (count == 1)
					_receivers.back()->RTLTCP().Set("url", arg1);
				if (count == 2)
					_receivers.back()->RTLTCP().Set("port", arg2).Set("host", arg1);
				if (count == 3)
					_receivers.back()->RTLTCP().Set("port", arg3).Set("host", arg2).Set("PROTOCOL", arg1);
				break;
			case 'x':
				Assert(count == 2, param, "requires two parameters [server] [port].");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::UDP;
				_receivers.back()->UDP().Set("port", arg2).Set("server", arg1);
				break;
			case 'D':
			{
				json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::PostgreSQL()));
				IO::OutputJSON &d = *json.back();

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
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::SPYSERVER;
				if (count == 1)
					_receivers.back()->SpyServer().Set("url", arg1);
				else if (count == 2)
					_receivers.back()->SpyServer().Set("port", arg2).Set("host", arg1);
				break;
			case 'z':
				Assert(count <= 2, param, "requires at most two parameters [[format]] [endpoint].");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::ZMQ;
				if (count == 1)
					_receivers.back()->ZMQ().Set("ENDPOINT", arg1);
				if (count == 2)
					_receivers.back()->ZMQ().Set("FORMAT", arg1).Set("ENDPOINT", arg2);
				break;
			case 'b':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				receiver.Timing() = true;
				break;
			case 'i':
				Assert(count <= 1, param, "requires at most one option parameter.");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::N2K;
				if (count == 1)
					_receivers.back()->N2KSCAN().Set("INTERFACE", arg1);
				break;

			case 'w':
				Assert(count <= 1, param);
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::WAVFILE;
				if (count == 1)
					_receivers.back()->WAV().Set("FILE", arg1);
				break;
			case 'r':
				Assert(count <= 2, param, "requires at most two parameters [[format]] [filename].");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::RAWFILE;
				if (count == 1)
					_receivers.back()->RAW().Set("FILE", arg1);
				if (count == 2)
					_receivers.back()->RAW().Set("FORMAT", arg1).Set("FILE", arg2);
				break;
			case 'e':
				Assert(count == 2, param, "requires two parameters [baudrate] [portname].");
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::SERIALPORT;
				_receivers.back()->SerialPort().Set("BAUDRATE", arg1).Set("PORT", arg2);
				break;
			case 'l':
				Assert(count == 0 || count == 2, param, MSG_NO_PARAMETER);
				if (count == 2)
				{
					Assert(arg1 == "JSON", param, "requires JSON on/off");
					list_devices_JSON = Util::Parse::Switch(arg2);
				}
				list_devices = true;
				break;
			case 'L':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				list_support = true;
				break;
			case 'd':
				if (++nrec > 1)
				{
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}

				if (param.length() == 4 && param[2] == ':')
				{
					Assert(count == 0, param, MSG_NO_PARAMETER);
					int n = param[3] - '0';
					Assert(n >= 0 && n < receiver.device_list.size(), param, "device does not exist");
					_receivers.back()->Serial() = receiver.device_list[n].getSerial();
				}
				else
				{
					Assert(param.length() == 2, param, "syntax error in device setting");
					Assert(count == 1, param, "device setting requires one parameter [serial number]");
					_receivers.back()->Serial() = arg1;
				}
				break;
			case 'u':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				{
					msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::UDPStreamer()));
					IO::OutputMessage &o = *msg.back();
					o.Set("HOST", arg1).Set("PORT", arg2);
					if (count > 2)
						parseSettings(o, argv, ptr + 2, argc);
				}
				break;
			case 'P':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				{
					msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
					IO::OutputMessage &p = *msg.back();
					p.Set("HOST", arg1).Set("PORT", arg2);
					if (count > 2)
						parseSettings(p, argv, ptr + 2, argc);
				}
				break;
			case 'Q':
				Assert(count >= 1, param, "invalid number of arguments");
				{
					msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::MQTTStreamer()));
					IO::OutputMessage &p = *msg.back();

					if (count % 2 == 1)
					{
						p.Set("URL", arg1);
					}
					if (count >= 2)
						parseSettings(p, argv, ptr + (count % 2), argc);
				}
				break;
			case 'X':
				Assert(count <= 1, param, "Only one optional parameter [sharing key] allowed.");
				{
					if (!commm_feed)
					{
						msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
						commm_feed = msg.back().get();
						commm_feed->Set("HOST", "aiscatcher.org").Set("PORT", "4242").Set("JSON", "on").Set("FILTER", "on").Set("GPS", "off").Set("KEEP_ALIVE", "on").Set("DOWNSAMPLE", "on").Set("INCLUDE_SAMPLE_START", "on");
					}

					if (count == 1 && commm_feed)
						commm_feed->Set("UUID", arg1);
				}
				break;
			case 'H':
				Assert(count > 0, param);
				{
					json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::HTTPStreamer()));
					IO::OutputJSON &h = *json.back();
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
			case 'I':
			case 'E':
			{
#ifdef HASNMEA2000
				json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::N2KStreamer()));
				IO::OutputJSON &h = *json.back();
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

					no_run = true;
					show_copyright = false;
				}
				else
					list_options = true;
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
					parseSettings(receiver.SerialPort(), argv, ptr, argc);
					break;
				case 'm':
					parseSettings(receiver.AIRSPY(), argv, ptr, argc);
					break;
				case 'r':
					parseSettings(receiver.RTLSDR(), argv, ptr, argc);
					break;
				case 'h':
					parseSettings(receiver.AIRSPYHF(), argv, ptr, argc);
					break;
				case 's':
					parseSettings(receiver.SDRPLAY(), argv, ptr, argc);
					break;
				case 'a':
					parseSettings(receiver.RAW(), argv, ptr, argc);
					break;
				case 'w':
					parseSettings(receiver.WAV(), argv, ptr, argc);
					break;
				case 't':
					parseSettings(receiver.RTLTCP(), argv, ptr, argc);
					break;
				case 'y':
					parseSettings(receiver.SpyServer(), argv, ptr, argc);
					break;
				case 'f':
					parseSettings(receiver.HACKRF(), argv, ptr, argc);
					break;
				case 'u':
					parseSettings(receiver.SOAPYSDR(), argv, ptr, argc);
					break;
				case 'z':
					parseSettings(receiver.ZMQ(), argv, ptr, argc);
					break;
				case 'o':
					if (receiver.Count() == 0)
						receiver.addModel(receiver.isTXTformatSet() ? 5 : 2);
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

		if (show_copyright)
			printVersion();

		/*
		// -------------
		// Read config file

		if (!file_config.empty())
		{
			Config c(_receivers, nrec, msg, json, screen, servers, own_mmsi);
			c.read(file_config);
		}
		*/
		if (list_devices)
			printDevices(*_receivers.back(), list_devices_JSON);
		if (list_support)
			printSupportedDevices();
		if (list_options)
			Usage();
		if (list_devices || list_support || list_options || no_run)
			return 0;

		// -------------
		// set up the receiver and open the device

		stat.resize(_receivers.size());
		msg_count.resize(_receivers.size(), 0);

		int group = 0;

		for (int i = 0; i < _receivers.size(); i++)
		{

			Receiver &r = *_receivers[i];
			r.setOwnMMSI(own_mmsi);

			if (servers.size() > 0 && servers[0]->active())
				r.setTags("DTM");

			r.setupDevice();
			// set up the decoding model(s), group is the last output group used
			r.setupModel(group);

			// set up all the output and connect to the receiver outputs
			for (auto &o : msg)
				o->Connect(r);
			for (auto &j : json)
				j->Connect(r);

			screen.connect(r);

			for (auto &s : servers)
				if (s->active())
					s->connect(r);

			if (r.verbose || timeout_nomsg)
				stat[i].connect(r);
		}

		for (auto &o : msg)
			o->Start();

		for (auto &j : json)
			j->Start();

		screen.start();

		for (auto &s : servers)
			if (s->active())
			{
				if (own_mmsi != -1)
				{
					s->Set("SHARE_LOC", "true");
					s->Set("OWN_MMSI", std::to_string(own_mmsi));
				}
				s->start();
			}

		DBG("Starting statistics");
		for (auto &s : stat)
			s.start();

		DBG("Starting receivers");
		for (auto &r : _receivers)
			r->play();

		stop = false;
		const int SLEEP = 50;
		auto time_start = high_resolution_clock::now();
		auto time_timeout_start = time_start;
		auto time_last = time_start;
		bool oneverbose = false, iscallback = true;

		for (auto &r : _receivers)
		{
			oneverbose |= r->verbose;
			iscallback &= r->device->isCallback();
		}

		DBG("Entering main loop");
		while (!stop)
		{
			for (auto &r : _receivers)
				stop = stop || !(r->device->isStreaming());

			if (iscallback) // don't go to sleep in case we are reading from a file
				std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

			if (!oneverbose && !timeout)
				continue;

			auto time_now = high_resolution_clock::now();

			if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >= screen.verboseUpdateTime)
			{
				time_last = time_now;

				for (int i = 0; i < _receivers.size(); i++)
				{
					Receiver &r = *_receivers[i];
					if (r.verbose)
					{
						for (int j = 0; j < r.Count(); j++)
						{
							stat[i].statistics[j].Stamp();
							std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
							Info() << "[" << name << "] " << std::string(37 - name.length(), ' ') << "received: " << stat[i].statistics[j].getDeltaCount() << " msgs, total: "
								   << stat[i].statistics[j].getCount() << " msgs, rate: " << stat[i].statistics[j].getRate() << " msg/s";
						}
					}
				}
			}

			if (timeout && timeout_nomsg)
			{
				bool one_stale = false;

				for (int i = 0; i < _receivers.size(); i++)
				{
					if (stat[i].statistics[0].getCount() == msg_count[i])
						one_stale = true;

					msg_count[i] = stat[i].statistics[0].getCount();
				}

				if (!one_stale)
					time_timeout_start = time_now;
			}

			if (timeout && duration_cast<seconds>(time_now - time_timeout_start).count() >= timeout)
			{
				stop = true;
				if (timeout_nomsg)
					Warning() << "Stop triggered, no messages were received for " << timeout << " seconds.";
				else
				{
					exit_code = -2;
					Warning() << "Stop triggered by timeout after " << timeout << " seconds. (-T " << timeout << ")";
				}
			}
		}

		std::stringstream ss;
		for (int i = 0; i < _receivers.size(); i++)
		{
			Receiver &r = *_receivers[i];
			r.stop();

			// End Main loop
			// -----------------

			if (r.verbose)
			{
				ss << "----------------------\n";
				for (int j = 0; j < r.Count(); j++)
				{
					std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
					stat[i].statistics[j].Stamp();
					ss << "[" << name << "] " << std::string(37 - name.length(), ' ') << "total: " << stat[i].statistics[j].getCount() << " msgs" << "\n";
				}
			}

			if (r.Timing())
				for (int j = 0; j < r.Count(); j++)
				{
					std::string name = r.Model(j)->getName();
					ss << "[" << r.Model(j)->getName() << "]: " << std::string(37 - name.length(), ' ') << r.Model(j)->getTotalTiming() << " ms" << "\n";
				}
			Info() << ss.str();
		}

		for (auto &s : servers)
			s->close();
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		for (auto &r : _receivers)
			r->stop();
		exit_code = -1;
	}

	return exit_code;
}
