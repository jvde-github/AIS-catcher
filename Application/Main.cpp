/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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
#include "Config.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/StringBuilder.h"

#include "TCP.h"
std::atomic<bool> stop;

void StopRequest() {
	stop = true;
}

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT) stop = true;
	return TRUE;
}
#else
void consoleHandler(int signal) {
	if (signal == SIGPIPE) {
		// std::cerr << "Termination request SIGPIPE ignored" << std::endl;
		return;
	}
	if (signal != SIGINT)
		std::cerr << "Termination request: " << signal << std::endl;

	stop = true;
}
#endif

void printVersion() {
	std::cerr << "AIS-catcher (build " << __DATE__ << ") " << VERSION_DESCRIBE << std::endl;
	std::cerr << "(C) Copyright 2021-2023 " << COPYRIGHT << std::endl;
	std::cerr << "This is free software; see the source for copying conditions.There is NO" << std::endl;
	std::cerr << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << std::endl;
}

void Usage() {
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-a xxx - set tuner bandwidth in Hz (default: off)]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models for time - for development purposes (default: off)]" << std::endl;
	std::cerr << "\t[-c [AB/CD] - [optional: AB] select AIS channels and optionally the NMEA channel designations]" << std::endl;
	std::cerr << "\t[-C [filename] - read configuration settings from file]" << std::endl;
	std::cerr << "\t[-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]" << std::endl;
	std::cerr << "\t[-h display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-H [optional: url] - send messages via HTTP, for options see documentation]" << std::endl;
	std::cerr << "\t[-m xx - run specific decoding model (default: 2), see README for more details]" << std::endl;
	std::cerr << "\t[-M xxx - set additional meta data to generate: T = NMEA timestamp, D = decoder related (signal power, ppm) (default: none)]" << std::endl;
	std::cerr << "\t[-n show NMEA messages on screen without detail (-o 1)]" << std::endl;
	std::cerr << "\t[-N [optional: port][optional settings] - start http server at port, see README for details]" << std::endl;
	std::cerr << "\t[-o set output mode (0 = quiet, 1 = NMEA only, 2 = NMEA+, 3 = NMEA+ in JSON, 4 JSON Sparse, 5 JSON Full (default: 2)]" << std::endl;
	std::cerr << "\t[-p xxx - set frequency correction for device in PPM (default: zero)]" << std::endl;
	std::cerr << "\t[-P xxx.xx.xx.xx yyy - TCP destination address and port (default: off)]" << std::endl;
	std::cerr << "\t[-q suppress NMEA messages to screen (-o 0)]" << std::endl;
	std::cerr << "\t[-s xxx - sample rate in Hz (default: based on SDR device)]" << std::endl;
	std::cerr << "\t[-T xx - auto terminate run with SDR after xxx seconds (default: off)]" << std::endl;
	std::cerr << "\t[-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]" << std::endl;
	std::cerr << "\t[-v [option: xx] - enable verbose mode, optional to provide update frequency of xx seconds (default: false)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\tDevice selection:" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-d:x - select device based on index (default: 0)]" << std::endl;
	std::cerr << "\t[-d xxxx - select device based on serial number]" << std::endl;
	std::cerr << "\t[-l list available devices and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-L list supported SDR hardware and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-r [optional: yy] filename - read IQ data from file or stdin (.), short for -r -ga FORMAT yy FILE filename" << std::endl;
	std::cerr << "\t[-t [[protocol]] [host [port]] - read IQ data from remote RTL-TCP instance]" << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]" << std::endl;
	std::cerr << "\t[-x [server][port] - UDP input of NMEA messages at port on server" << std::endl;
	std::cerr << "\t[-y [host [port]] - read IQ data from remote SpyServer]" << std::endl;
	std::cerr << "\t[-z [optional [format]] [optional endpoint] - read IQ data from [endpoint] in [format] via ZMQ (default: format is CU8)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\tDevice specific settings:" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8] ]" << std::endl;
	std::cerr << "\t[-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off] ]" << std::endl;
	std::cerr << "\t[-gh Airspy HF+: TRESHOLD [low/high] PREAMP [on/off] ]" << std::endl;
	std::cerr << "\t[-gm Airspy: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]" << std::endl;
	std::cerr << "\t[-gr RTLSDRs: TUNER [auto/0.0-50.0] RTLAGC [on/off] BIASTEE [on/off] ]" << std::endl;
	std::cerr << "\t[-gs SDRPLAY: GRDB [0-59] LNASTATE [0-9] AGC [on/off] ]" << std::endl;
	std::cerr << "\t[-gt RTLTCP: HOST [address] PORT [port] TUNER [auto/0.0-50.0] RTLAGC [on/off] FREQOFFSET [-150-150] PROTOCOL [none/rtltcp] TIMEOUT [1-60] ]" << std::endl;
	std::cerr << "\t[-gu SOAPYSDR: DEVICE [string] GAIN [string] AGC [on/off] STREAM [string] SETTING [string] CH [0+] PROBE [on/off] ANTENNA [string] ]" << std::endl;
	std::cerr << "\t[-gw WAV file: FILE [filename] ]" << std::endl;
	std::cerr << "\t[-gy SPYSERVER: HOST [address] PORT [port] GAIN [0-50] ]" << std::endl;
	std::cerr << "\t[-gz ZMQ: ENDPOINT [endpoint] FORMAT [CF32/CS16/CU8/CS8] ]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\tModel specific settings:" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-go Model: AFC_WIDE [on/off] FP_DS [on/off] PS_EMA [on/off] SOXR [on/off] SRC [on/off] DROOP [on/off] ]" << std::endl;
}

void printDevices(Receiver& r) {
	std::cerr << "Found " << r.device_list.size() << " device(s):" << std::endl;
	for (int i = 0; i < r.device_list.size(); i++) {
		std::cerr << i << ": " << r.device_list[i].toString() << std::endl;
	}
}

void printSupportedDevices() {
	std::cerr << "SDR support: ";
#ifdef HASRTLSDR
	std::cerr << "RTLSDR ";
#endif
#ifdef HASAIRSPY
	std::cerr << "AIRSPY ";
#endif
#ifdef HASAIRSPYHF
	std::cerr << "AIRSPYHF+ ";
#endif
#ifdef HASSDRPLAY
	std::cerr << "SDRPLAY ";
#endif
	std::cerr << "RTLTCP SPYSERVER ";
#ifdef HASZMQ
	std::cerr << "ZMQ ";
#endif
#ifdef HASHACKRF
	std::cerr << "HACKRF ";
#endif
#ifdef HASSOAPYSDR
	std::cerr << "SOAPYSDR ";
#endif
	std::cerr << std::endl;

	std::cerr << "Other support: ";
#ifdef HASSOXR
	std::cerr << "SOXR ";
#endif
#ifdef HASCURL
	std::cerr << "CURL ";
#endif
#ifdef HASPSQL
	std::cerr << "PostgreSQL ";
#endif
#ifdef HASZLIB
	std::cerr << "ZLIB ";
#endif
#ifdef HASSAMPLERATE
	std::cerr << "LIBSAMPLERATE ";
#endif
#ifdef HASRTLSDR_BIASTEE
	std::cerr << "RTLSDR-BIASTEE ";
#endif
#ifdef HASRTLSDR_TUNERBW
	std::cerr << "RTLSDR-TUNERBW ";
#endif
	std::cerr << std::endl;
}

// -------------------------------
// Command line support functions

void parseSettings(Setting& s, char* argv[], int ptr, int argc) {
	ptr++;

	while (ptr < argc - 1 && argv[ptr][0] != '-') {
		std::string option = argv[ptr++];
		std::string arg = argv[ptr++];

		s.Set(option, arg);
	}
}

bool isOption(std::string s) {
	return s.length() >= 2 && s[0] == '-' && std::isalpha(s[1]);
}

void Assert(bool b, std::string& context, std::string msg = "") {
	if (!b) {
		throw std::runtime_error("syntax error on command line with setting \"" + context + "\". " + msg + "\n");
	}
}

int main(int argc, char* argv[]) {

	std::string file_config;

	std::vector<std::unique_ptr<Receiver>> _receivers;
	_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));

	OutputScreen screen;
	OutputHTTP http;
	OutputUDP udp;
	OutputTCP tcp;
	OutputDBMS db;
	std::vector<OutputStatistics> stat;
	std::vector<std::unique_ptr<WebClient>> servers;

	bool list_devices = false, list_support = false, list_options = false;
	int timeout = 0, nrec = 0;

	try {
#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("could not set control handler");
#else
		signal(SIGINT, consoleHandler);
		signal(SIGTERM, consoleHandler);
		signal(SIGHUP, consoleHandler);
		signal(SIGPIPE, consoleHandler);
#endif

		printVersion();
		_receivers.back()->refreshDevices();

		const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
		int ptr = 1;

		while (ptr < argc) {
			Receiver& receiver = *_receivers.back();

			std::string param = std::string(argv[ptr]);
			Assert(param[0] == '-', param, "setting does not start with \"-\".");

			int count = 0;
			while (ptr + count + 1 < argc && !isOption(argv[ptr + 1 + count])) count++;

			std::string arg1 = count >= 1 ? std::string(argv[ptr + 1]) : "";
			std::string arg2 = count >= 2 ? std::string(argv[ptr + 2]) : "";
			std::string arg3 = count >= 3 ? std::string(argv[ptr + 3]) : "";

			switch (param[1]) {
			case 's':
				Assert(count == 1, param, "does require one parameter [sample rate].");
				receiver.setSampleRate(Util::Parse::Integer(arg1, 48000, 12288000));
				break;
			case 'm':
				Assert(count == 1, param, "requires one parameter [model number].");
				receiver.addModel(Util::Parse::Integer(arg1, 0, 5));
				break;
			case 'M':
				Assert(count <= 1, param, "requires zero or one parameter [DT].");
				receiver.setTags(arg1);
				break;
			case 'c':
				Assert(count <= 2 && count >= 1, param, "requires one or two parameter [AB/CD]].");
				if (count == 1) receiver.setChannel(arg1);
				if (count == 2) receiver.setChannel(arg1, arg2);
				break;
			case 'C':
				Assert(count == 1, param, "one parameter required: filename");
				file_config = arg1;
				break;
			case 'N':
				Assert(count > 0, param, "requires at least one parameter");
				if (servers.size() == 0)
					servers.push_back(std::unique_ptr<WebClient>(new WebClient()));

				if (count % 2 == 1) {
					// -N port creates a new server assuming the previous one is complete (i.e. has a port set)
					if (servers.back()->isPortSet())
						servers.push_back(std::unique_ptr<WebClient>(new WebClient()));
					servers.back()->Set("PORT", arg1);
				}
				servers.back()->active() = true;
				parseSettings(*servers.back(), argv, ptr + (count % 2), argc);
				break;
			case 'v':
				Assert(count <= 1, param);
				receiver.verbose = true;
				if (count == 1) screen.verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
				break;
			case 'T':
				Assert(count == 1, param, "timeout parameter required.");
				timeout = Util::Parse::Integer(arg1, 1, 3600);
				break;
			case 'q':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				screen.setScreen(OutputLevel::NONE);
				break;
			case 'n':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				screen.setScreen(OutputLevel::NMEA);
				break;
			case 'o':
				Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter.");
				screen.setScreen(arg1);
				if (count > 1) {
					parseSettings(screen.msg2screen, argv, ptr + 1, argc);
					parseSettings(screen.json2screen, argv, ptr + 1, argc);
				}
				break;
			case 'F':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				receiver.addModel(2)->Set("FP_DS", "ON").Set("PS_EMA", "ON");
				break;
			case 't':
				Assert(count <= 3, param, "requires one, two or three parameters [protocol] [host] [port].");
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::RTLTCP;
				if (count == 1) _receivers.back()->RTLTCP().Set("host", arg1);
				if (count == 2) _receivers.back()->RTLTCP().Set("port", arg2).Set("host", arg1);
				if (count == 3) _receivers.back()->RTLTCP().Set("port", arg3).Set("host", arg2).Set("PROTOCOL",arg1);
				break;
			case 'x':
				Assert(count == 2, param, "requires two parameters [server] [port].");
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::UDP;
				_receivers.back()->UDP().Set("port", arg2).Set("server", arg1);
				break;
			case 'D': {
				IO::PostgreSQL& d = db.add();
				if (count % 2 == 1) {
					d.Set("CONN_STR", arg1);
					if (count > 1)
						parseSettings(d, argv, ptr + 1, argc);
				}
				else {
					if (count >= 2)
						parseSettings(d, argv, ptr, argc);
				}
			} break;
			case 'y':
				Assert(count <= 2, param, "requires one or two parameters [host] [[port]].");
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::SPYSERVER;
				if (count == 1) _receivers.back()->SpyServer().Set("host", arg1);
				if (count == 2) _receivers.back()->SpyServer().Set("port", arg2).Set("host", arg1);
				break;
			case 'z':
				Assert(count <= 2, param, "requires at most two parameters [[format]] [endpoint].");
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::ZMQ;
				if (count == 1) _receivers.back()->ZMQ().Set("ENDPOINT", arg1);
				if (count == 2) _receivers.back()->ZMQ().Set("FORMAT", arg1).Set("ENDPOINT", arg2);
				break;
			case 'b':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				receiver.Timing() = true;
				break;
			case 'w':
				Assert(count <= 1, param);
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::WAVFILE;
				if (count == 1) _receivers.back()->WAV().Set("FILE", arg1);
				break;
			case 'r':
				Assert(count <= 2, param, "requires at most two parameters [[format]] [filename].");
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}
				_receivers.back()->InputType() = Type::RAWFILE;
				if (count == 1) _receivers.back()->RAW().Set("FILE", arg1);
				if (count == 2) _receivers.back()->RAW().Set("FORMAT", arg1).Set("FILE", arg2);
				break;
			case 'l':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				list_devices = true;
				break;
			case 'L':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				list_support = true;
				break;
			case 'd':
				if (++nrec > 1) {
					_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
				}

				if (param.length() == 4 && param[2] == ':') {
					Assert(count == 0, param, MSG_NO_PARAMETER);
					int n = param[3] - '0';
					Assert(n >= 0 && n < receiver.device_list.size(), param, "device does not exist");
					_receivers.back()->Serial() = receiver.device_list[n].getSerial();
				}
				else {
					Assert(param.length() == 2, param, "syntax error in device setting");
					Assert(count == 1, param, "device setting requires one parameter [serial number]");
					_receivers.back()->Serial() = arg1;
				}
				break;
			case 'u':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				{
					IO::UDP& u = udp.add(arg1, arg2);
					if (count > 2) parseSettings(u, argv, ptr + 2, argc);
				}
				break;
			case 'P':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				{
					IO::TCP& u = tcp.add(arg1, arg2);
					if (count > 2) parseSettings(u, argv, ptr + 2, argc);
				}
				break;
			case 'H':
				Assert(count > 0, param);
				{
					auto& h = http.add(AIS::KeyMap, JSON_DICT_FULL);
					if (count % 2) h->Set("URL", arg1);
					parseSettings(*h, argv, ptr + (count % 2), argc);
					receiver.setTags("DT");
				}
				break;
			case 'h':
				Assert(count == 0, param, MSG_NO_PARAMETER);
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
				switch (param[2]) {
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
					if (receiver.Count() == 0) receiver.addModel(receiver.isTXTformatSet() ? 5 : 2);
					parseSettings(*receiver.Model(receiver.Count() - 1), argv, ptr, argc);
					break;
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

		// -------------
		// Read config file

		if (!file_config.empty()) {
			if (++nrec > 1) {
				_receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));
			}
			if (!servers.size())
				servers.push_back(std::unique_ptr<WebClient>(new WebClient()));

			Config c(*_receivers.back(), screen, http, udp, tcp, *servers.back());
			c.read(file_config);
		}

		if (list_devices) printDevices(*_receivers.back());
		if (list_support) printSupportedDevices();
		if (list_options) Usage();
		if (list_devices || list_support || list_options) return 0;

		// -------------
		// set up the receiver and open the device

		stat.resize(_receivers.size());

		for (int i = 0; i < _receivers.size(); i++) {
			Receiver& r = *_receivers[i];

			if (servers.size() > 0 && servers[0]->active()) r.setTags("DTM");

			r.setupDevice();
			// set up the decoding model(s)
			r.setupModel();

			// set up all the output and connect to the receiver outputs
			udp.connect(r);
			tcp.connect(r);
			http.connect(r);
			screen.connect(r);
			db.connect(r);

			for (auto& s : servers)
				if (s->active()) s->connect(r);

			if (r.verbose) stat[i].connect(r);
		}

		udp.start();
		tcp.start();
		http.start();
		screen.start();
		db.start();

		for (auto& s : servers)
			if (s->active()) s->start();

		for (auto& s : stat) s.start();
		for (auto& r : _receivers) r->play();

		stop = false;
		const int SLEEP = 50;
		auto time_start = high_resolution_clock::now();
		auto time_last = time_start;
		bool oneverbose = false, iscallback = true;

		for (auto& r : _receivers) {
			oneverbose |= r->verbose;
			iscallback &= r->device->isCallback();
		}

		while (!stop) {

			bool onerunning = false;
			for (auto& r : _receivers) onerunning |= r->device->isStreaming();
			if (!onerunning) {
				stop = true;
				continue;
			}

			if (iscallback) // don't go to sleep in case we are reading from a file
				std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

			if (!oneverbose && !timeout) continue;

			auto time_now = high_resolution_clock::now();

			if (oneverbose && duration_cast<seconds>(time_now - time_last).count() >= screen.verboseUpdateTime) {
				time_last = time_now;

				for (int i = 0; i < _receivers.size(); i++) {
					Receiver& r = *_receivers[i];
					if (r.verbose) {
						for (int j = 0; j < r.Count(); j++) {
							stat[i].statistics[j].Stamp();
							std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
							std::cerr << "[" << name << "] " << std::string(37 - name.length(), ' ') << "received: " << stat[i].statistics[j].getDeltaCount() << " msgs, total: "
									  << stat[i].statistics[j].getCount() << " msgs, rate: " << stat[i].statistics[j].getRate() << " msg/s" << std::endl;
						}
					}
				}
			}

			if (timeout && duration_cast<seconds>(time_now - time_start).count() >= timeout) {
				stop = true;
				std::cerr << "Warning: Stop triggered by timeout after " << timeout << " seconds. (-T " << timeout << ")" << std::endl;
			}
		}

		for (int i = 0; i < _receivers.size(); i++) {
			Receiver& r = *_receivers[i];
			r.stop();

			// End Main loop
			// -----------------

			if (r.verbose) {
				std::cerr << "----------------------" << std::endl;
				for (int j = 0; j < r.Count(); j++) {
					std::string name = r.Model(j)->getName() + " #" + std::to_string(i) + "-" + std::to_string(j);
					stat[i].statistics[j].Stamp();
					std::cerr << "[" << name << "] " << std::string(37 - name.length(), ' ') << "total: " << stat[i].statistics[j].getCount() << " msgs" << std::endl;
				}
			}

			if (r.Timing())
				for (int j = 0; j < r.Count(); j++) {
					std::string name = r.Model(j)->getName();
					std::cerr << "[" << r.Model(j)->getName() << "]: " << std::string(37 - name.length(), ' ')
							  << r.Model(j)->getTotalTiming() << " ms" << std::endl;
				}
		}

		for (auto& s : servers)
			s->close();
	}
	catch (std::exception const& e) {
		std::cout << "Error: " << e.what() << std::endl;
		for (auto& r : _receivers) r->stop();
		return -1;
	}
	return 0;
}
