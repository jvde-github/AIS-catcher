/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include "Signals.h"
#include "Common.h"
#include "Model.h"
#include "IO.h"
#include "Network.h"
#include "AIS.h"
#include "JSONAIS.h"

#include "Device/FileRAW.h"
#include "Device/FileWAV.h"
#include "Device/RTLSDR.h"
#include "Device/AIRSPYHF.h"
#include "Device/HACKRF.h"
#include "Device/RTLTCP.h"
#include "Device/AIRSPY.h"
#include "Device/SDRPLAY.h"
#include "Device/SpyServer.h"
#include "Device/SoapySDR.h"
#include "Device/ZMQ.h"

std::atomic<bool> stop;

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT) stop = true;
	return TRUE;
}
#else
void consoleHandler(int signal) {
	stop = true;
}
#endif

struct Drivers {
	Device::RAWFile RAW;
	Device::WAVFile WAV;
	Device::RTLSDR RTLSDR;
	Device::RTLTCP RTLTCP;
	Device::SpyServer SpyServer;
	Device::AIRSPYHF AIRSPYHF;
	Device::AIRSPY AIRSPY;
	Device::SDRPLAY SDRPLAY;
	Device::HACKRF HACKRF;
	Device::SOAPYSDR SOAPYSDR;
	Device::ZMQ ZMQ;
};

void printVersion() {
	std::cerr << "AIS-catcher (build " << __DATE__ << ") " << VERSION << std::endl;
	std::cerr << "(C) Copyright 2021-2022 " << COPYRIGHT << std::endl;
	std::cerr << "This is free software; see the source for copying conditions.There is NO" << std::endl;
	std::cerr << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << std::endl;
}

void Usage() {
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-a xxx - set tuner bandwidth in Hz (default: off)]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models for time - for development purposes (default: off)]" << std::endl;
	std::cerr << "\t[-c [AB/CD] - [optional: AB] select AIS channels and optionally the NMEA channel designations]" << std::endl;
	std::cerr << "\t[-F run model optimized for speed at the cost of accuracy for slow hardware (default: off)]" << std::endl;
	std::cerr << "\t[-h display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-H [optional: url] - send messages via HTTP, for options see documentation]" << std::endl;
	std::cerr << "\t[-m xx - run specific decoding model (default: 2), see README for more details]" << std::endl;
	std::cerr << "\t[-M xxx - set additional meta data to generate: T = NMEA timestamp, D = decoder related (signal power, ppm) (default: none)]" << std::endl;
	std::cerr << "\t[-n show NMEA messages on screen without detail (-o 1)]" << std::endl;
	std::cerr << "\t[-o set output mode (0 = quiet, 1 = NMEA only, 2 = NMEA+, 3 = NMEA+ in JSON, 4 JSON Sparse, 5 JSON Full (default: 2)]" << std::endl;
	std::cerr << "\t[-p xxx - set frequency correction for device in PPM (default: zero)]" << std::endl;
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
	std::cerr << "\t[-t [host [port]] - read IQ data from remote RTL-TCP instance]" << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]" << std::endl;
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

std::vector<Device::Description> getDevices(Drivers& drivers) {
	std::vector<Device::Description> device_list;

	drivers.RTLSDR.getDeviceList(device_list);
	drivers.AIRSPYHF.getDeviceList(device_list);
	drivers.AIRSPY.getDeviceList(device_list);
	drivers.SDRPLAY.getDeviceList(device_list);
	drivers.HACKRF.getDeviceList(device_list);
	drivers.SOAPYSDR.getDeviceList(device_list);

	return device_list;
}

void printDevices(std::vector<Device::Description>& device_list) {
	std::cerr << "Found " << device_list.size() << " device(s):" << std::endl;
	for (int i = 0; i < device_list.size(); i++) {
		std::cerr << i << ": " << device_list[i].toString() << std::endl;
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

int getDeviceFromSerial(std::vector<Device::Description>& device_list, std::string serial) {
	for (int i = 0; i < device_list.size(); i++) {
		if (device_list[i].getSerial() == serial) return i;
	}
	std::cerr << "Searching for device with SN " << serial << "." << std::endl;
	return -1;
}

std::shared_ptr<AIS::Model> createModel(int m) {

	switch (m) {
	case 0:
		return std::make_shared<AIS::ModelStandard>();
		break;
	case 1:
		return std::make_shared<AIS::ModelBase>();
		break;
	case 2:
		return std::make_shared<AIS::ModelDefault>();
		break;
	case 3:
		return std::make_shared<AIS::ModelDiscriminator>();
		break;
	case 4:
		return std::make_shared<AIS::ModelChallenger>();
		break;
	case 5:
		return std::make_shared<AIS::ModelNMEA>();
		break;
	default:
		throw "Internal error: Model not implemented in this version. Check in later.";
		break;
	}
}

// -------------------------------

void parseTags(int& mode, std::string& s) {
	for (char c : s) {
		switch (toupper(c)) {
		case 'M':
			mode |= 4;
			break;
		case 'T':
			mode |= 2;
			break;
		case 'D':
			mode |= 1;
			break;
		default:
			throw "Error: illegal tag defined on command line [D/T]";
		}
	}
}

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
		std::cerr << "Error on command line in processing setting \"" << context << "\". ";
		if (msg != "") std::cerr << msg;
		std::cerr << std::endl;
		throw "Terminating.";
	}
}

int main(int argc, char* argv[]) {
	int sample_rate = 0, input_device = 0, bandwidth = 0, ppm = 0;
	int timeout = 0;
	int TAG_mode = 0;

	bool list_devices = false, list_support = false, list_options = false;
	bool verbose = false, timer_on = false;

	OutputLevel NMEA_to_screen = OutputLevel::FULL;
	int verboseUpdateTime = 3;

	AIS::Mode ChannelMode = AIS::Mode::AB;
	std::string NMEAchannels = "AB";
	TAG tag;

	// Device and output stream of device;
	Type input_type = Type::NONE;
	uint64_t handle = 0;

	Device::Device* device = NULL;
	Drivers drivers;

	std::vector<IO::UDP> UDP;
	std::vector<std::shared_ptr<AIS::Model>> models;

	// AIS message to properties
	std::vector<AIS::JSONAIS> jsonais;

	std::vector<std::shared_ptr<IO::HTTP>> http;
	IO::MessageToScreen msg2screen;
	IO::JSONtoScreen json2screen;

	try {
#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw "ERROR: Could not set control handler";
#else
		signal(SIGINT, consoleHandler);
#endif

		std::vector<Device::Description> device_list = getDevices(drivers);

		const std::string MSG_NO_PARAMETER = "Does not allow additional parameter.";
		int ptr = 1;

		while (ptr < argc) {
			std::string param = std::string(argv[ptr]);
			Assert(param[0] == '-', param, "Setting does not start with \"-\".");

			int count = 0;
			while (ptr + count + 1 < argc && !isOption(argv[ptr + 1 + count])) count++;

			std::string arg1 = count >= 1 ? std::string(argv[ptr + 1]) : "";
			std::string arg2 = count >= 2 ? std::string(argv[ptr + 2]) : "";

			switch (param[1]) {
			case 's':
				Assert(count == 1, param, "Does require one parameter [sample rate].");
				sample_rate = Util::Parse::Integer(arg1, 48000, 12288000);
				break;
			case 'm':
				Assert(count == 1, param, "Requires one parameter [model number].");
				models.push_back(createModel(Util::Parse::Integer(arg1, 0, 5)));
				break;
			case 'M':
				Assert(count <= 1, param, "Requires zero or one parameter [DT].");
				parseTags(TAG_mode, arg1);
				break;
			case 'c':
				Assert(count <= 2, param, "Requires one or two parameter [AB/CD]].");
				if (arg1 == "AB") {
					ChannelMode = AIS::Mode::AB;
					NMEAchannels = "AB";
				}
				else if (arg1 == "CD") {
					ChannelMode = AIS::Mode::CD;
					NMEAchannels = "CD";
				}
				else
					throw "Error: parameter needs to be AB or CD (-c)";
				if (count == 2) {
					Assert(arg2.length() == 2, param, "NMEA channel designation needs to be: XX.");
					Assert(std::isupper(arg2[0]) || std::isdigit(arg2[0]) || arg2[0] == '?', param, "NMEA channel designation invalid.");
					Assert(std::isupper(arg2[1]) || std::isdigit(arg2[1]) || arg2[1] == '?', param, "NMEA channel designation invalid.");
					NMEAchannels = arg2;
				}
				break;
			case 'v':
				Assert(count <= 1, param);
				verbose = true;
				if (count == 1) verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600);
				break;
			case 'T':
				Assert(count == 1, param, "Timeout parameter required.");
				timeout = Util::Parse::Integer(arg1, 1, 3600);
				break;
			case 'q':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				NMEA_to_screen = OutputLevel::NONE;
				break;
			case 'n':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				NMEA_to_screen = OutputLevel::NMEA;
				break;
			case 'o':
				Assert(count == 1, param, "Requires one parameter.");
				{
					switch (Util::Parse::Integer(arg1, 0, 5)) {
					case 0:
						NMEA_to_screen = OutputLevel::NONE;
						break;
					case 1:
						NMEA_to_screen = OutputLevel::NMEA;
						break;
					case 2:
						NMEA_to_screen = OutputLevel::FULL;
						break;
					case 3:
						NMEA_to_screen = OutputLevel::JSON_NMEA;
						break;
					case 4:
						NMEA_to_screen = OutputLevel::JSON_SPARSE;
						break;
					case 5:
						NMEA_to_screen = OutputLevel::JSON_FULL;
						break;
					default:
						throw "Error: unknown option 'o'";
					}
				}
				break;
			case 'F':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				models.push_back(createModel(2));
				models.back()->Set("FP_DS", "ON");
				models.back()->Set("PS_EMA", "ON");
				break;
			case 't':
				input_type = Type::RTLTCP;
				Assert(count <= 2, param, "Requires one or two parameters [host] [[port]].");
				if (count >= 1) drivers.RTLTCP.Set("host", arg1);
				if (count >= 2) drivers.RTLTCP.Set("port", arg2);
				break;
			case 'y':
				input_type = Type::SPYSERVER;
				Assert(count <= 2, param, "Requires one or two parameters [host] [[port]].");
				if (count >= 1) drivers.SpyServer.Set("host", arg1);
				if (count >= 2) drivers.SpyServer.Set("port", arg2);
				break;
			case 'z':
				input_type = Type::ZMQ;
				Assert(count <= 2, param, "Requires at most two parameters [[format]] [endpoint].");
				if (count == 1) {
					drivers.ZMQ.Set("ENDPOINT", arg1);
				}
				else if (count == 2) {
					drivers.ZMQ.Set("FORMAT", arg1);
					drivers.ZMQ.Set("ENDPOINT", arg2);
				}
				break;
			case 'b':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				timer_on = true;
				break;
			case 'w':
				Assert(count <= 1, param);
				input_type = Type::WAVFILE;
				if (count == 1) drivers.WAV.Set("FILE", arg1);
				break;
			case 'r':
				Assert(count <= 2, param, "Requires at most two parameters [[format]] [filename].");
				input_type = Type::RAWFILE;
				if (count == 1) {
					drivers.RAW.Set("FILE", arg1);
				}
				else if (count == 2) {
					drivers.RAW.Set("FORMAT", arg1);
					drivers.RAW.Set("FILE", arg2);
				}
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
				if (param.length() == 4 && param[2] == ':') {
					Assert(count == 0, param, MSG_NO_PARAMETER);
					input_device = (param[3] - '0');
				}
				else {
					Assert(count == 1, param, "Requires one parameter [serial number].");
					input_device = getDeviceFromSerial(device_list, arg1);
				}
				if (input_device < 0 || input_device >= device_list.size()) {
					std::cerr << "Device does not exist." << std::endl;
					return 0;
				}
				break;
			case 'u':
				Assert(count >= 2 && count % 2 == 0, param, "Requires at least two parameters [address] [port].");
				UDP.push_back(IO::UDP());
				UDP.back().Set("HOST", arg1);
				UDP.back().Set("PORT", arg2);
				if (count > 2) parseSettings(UDP.back(), argv, ptr + 2, argc);
				UDP.back().setSource(MAX(0, (int)models.size() - 1));
				break;
			case 'H':
				Assert(count > 0, param);
				http.push_back(std::make_shared<IO::HTTP>());
				if (count % 2) http.back()->Set("URL", arg1);
				parseSettings(*http.back(), argv, ptr + (count % 2), argc);
				http.back()->setSource(MAX(0, (int)models.size() - 1));
				TAG_mode |= 3;
				break;
			case 'h':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				list_options = true;
				break;
			case 'p':
				Assert(count == 1, param, "Requires one parameter [frequency offset].");
				ppm = Util::Parse::Integer(arg1, -150, 150);
				break;
			case 'a':
				Assert(count == 1, param, "Requires one parameter [bandwidth].");
				bandwidth = Util::Parse::Integer(arg1, 0, 20000000);
				break;
			case 'g':
				Assert(count % 2 == 0 && param.length() == 3, param);
				switch (param[2]) {
				case 'm':
					parseSettings(drivers.AIRSPY, argv, ptr, argc);
					break;
				case 'r':
					parseSettings(drivers.RTLSDR, argv, ptr, argc);
					break;
				case 'h':
					parseSettings(drivers.AIRSPYHF, argv, ptr, argc);
					break;
				case 's':
					parseSettings(drivers.SDRPLAY, argv, ptr, argc);
					break;
				case 'a':
					parseSettings(drivers.RAW, argv, ptr, argc);
					break;
				case 'w':
					parseSettings(drivers.WAV, argv, ptr, argc);
					break;
				case 't':
					parseSettings(drivers.RTLTCP, argv, ptr, argc);
					break;
				case 'y':
					parseSettings(drivers.SpyServer, argv, ptr, argc);
					break;
				case 'f':
					parseSettings(drivers.HACKRF, argv, ptr, argc);
					break;
				case 'u':
					parseSettings(drivers.SOAPYSDR, argv, ptr, argc);
					break;
				case 'z':
					parseSettings(drivers.ZMQ, argv, ptr, argc);
					break;
				case 'o':
					if (models.size() == 0) models.push_back(createModel(2));
					parseSettings(*models.back(), argv, ptr, argc);
					break;
					break;
				default:
					throw "Error on command line: invalid -g switch on command line";
				}
				break;
			default:
				std::cerr << "Unknown option on command line (" << param[1] << ")." << std::endl;
				return 0;
			}

			ptr += count + 1;
		}

		if (verbose || list_devices || list_support || NMEA_to_screen != OutputLevel::NONE || list_options) printVersion();
		if (list_devices) printDevices(device_list);
		if (list_support) printSupportedDevices();
		if (list_options) Usage();
		if (list_devices || list_support || list_options) return 0;

		// -------------
		// Select device

		if (input_type == Type::NONE) {
			if (device_list.size() == 0) throw "No input device available.";

			Device::Description d = device_list[input_device];
			input_type = d.getType();
			handle = d.getHandle();

			std::cerr << "Device selected: " << d.toString() << std::endl;
		}

		switch (input_type) {
		case Type::WAVFILE:
			device = &drivers.WAV;
			break;
		case Type::RAWFILE:
			device = &drivers.RAW;
			break;
		case Type::RTLTCP:
			device = &drivers.RTLTCP;
			break;
		case Type::SPYSERVER:
			device = &drivers.SpyServer;
			break;
#ifdef HASZMQ
		case Type::ZMQ:
			device = &drivers.ZMQ;
			break;
#endif
#ifdef HASAIRSPYHF
		case Type::AIRSPYHF:
			device = &drivers.AIRSPYHF;
			break;
#endif
#ifdef HASAIRSPY
		case Type::AIRSPY:
			device = &drivers.AIRSPY;
			break;
#endif
#ifdef HASSDRPLAY
		case Type::SDRPLAY:
			device = &drivers.SDRPLAY;
			break;
#endif
#ifdef HASRTLSDR
		case Type::RTLSDR:
			device = &drivers.RTLSDR;
			break;
#endif
#ifdef HASHACKRF
		case Type::HACKRF:
			device = &drivers.HACKRF;
			break;
#endif
#ifdef HASSOAPYSDR
		case Type::SOAPYSDR:
			device = &drivers.SOAPYSDR;
			break;
#endif
		default:
			throw "Error: invalid device selection";
		}
		if (device == 0) throw "Error: cannot set up device";

		// Derive TAG_mode which is a summary of the additional parameters
		// that will be calculated through the chain.

		tag.mode = TAG_mode;
		// ----------------------
		// Open and set up device

		device->setTag(tag);
		device->Open(handle);

		// override sample rate if defined by user
		if (sample_rate) device->setSampleRate(sample_rate);

		if (ChannelMode == AIS::Mode::AB)
			device->setFrequency((int)(162000000));
		else
			device->setFrequency((int)(156800000));

		if (ppm)
			device->Set("FREQOFFSET", std::to_string(ppm));

		if (bandwidth)
			device->Set("BW", std::to_string(bandwidth));

		if (verbose) {
			std::cerr << "Device: " << device->getProduct() << std::endl;
			std::cerr << "Settings: " << device->Get() << std::endl;
		}

		// ------------
		// Setup models
		if (!models.size()) models.push_back(createModel(2));

		// Attach output
		std::vector<IO::StreamCounter<AIS::Message>> statistics(verbose ? models.size() : 0);

		for (int i = 0; i < models.size(); i++) {
			models[i]->buildModel(NMEAchannels[0], NMEAchannels[1], device->getSampleRate(), timer_on, device);
			if (verbose) models[i]->Output() >> statistics[i];
		}

		// set up client thread to periodically submit msgs over HTTP
		// we will have a json decoder for every model
		jsonais.resize(models.size());
		for (auto& h : http) {

			h->Set("MODEL", models[h->getSource()]->getName());
			h->Set("MODEL_SETTING", models[h->getSource()]->Get());
			h->Set("DEVICE_SETTING", device->Get());
			h->Set("PRODUCT", device->getProduct());
			h->Set("VENDOR", device->getVendor());
			h->Set("SERIAL", device->getSerial());

			jsonais[h->getSource()] >> *h;
			h->Start();
		}

		// Create and connect output to UDP stream
		for (int i = 0; i < UDP.size(); i++) {
			models[UDP[i].getSource()]->Output() >> UDP[i];
			UDP[i].Start();
		}

		// Output
		if (NMEA_to_screen == OutputLevel::NMEA || NMEA_to_screen == OutputLevel::JSON_NMEA || NMEA_to_screen == OutputLevel::FULL) {
			models[0]->Output() >> msg2screen;
			msg2screen.setDetail(NMEA_to_screen);
		}
		else if (NMEA_to_screen == OutputLevel::JSON_SPARSE || NMEA_to_screen == OutputLevel::JSON_FULL) {
			jsonais[0] >> json2screen;

			if (NMEA_to_screen == OutputLevel::JSON_SPARSE) json2screen.setMap(JSON_DICT_SPARSE);
		}

		// connect property calculation to model only if it is needed (e.g. we connected it to a http server or screen output)
		// connection to either http or json screen output
		for (int i = 0; i < jsonais.size(); i++) {
			if (jsonais[i].out.isConnected()) {
				models[i]->Output() >> jsonais[i];
			}
		}

		// -----------------
		// Main loop

		device->Play();

		stop = false;

		const int SLEEP = 50;

		auto time_start = high_resolution_clock::now();
		auto time_last = time_start;

		while (device->isStreaming() && !stop) {

			if (device->isCallback()) // don't go to sleep in case we are reading from a file
				std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP));

			if (!verbose && !timeout) continue;

			auto time_now = high_resolution_clock::now();

			if (verbose && duration_cast<seconds>(time_now - time_last).count() >= verboseUpdateTime) {
				time_last = time_now;

				for (int j = 0; j < models.size(); j++) {
					statistics[j].Stamp();
					std::string name = models[j]->getName();
					std::cerr << "[" << name << "] " << std::string(37 - name.length(), ' ') << "received: " << statistics[j].getDeltaCount() << " msgs, total: " << statistics[j].getCount() << " msgs, rate: " << statistics[j].getRate() << " msg/s" << std::endl;
				}
			}

			if (timeout && duration_cast<seconds>(time_now - time_start).count() >= timeout) {
				stop = true;
				if (verbose)
					std::cerr << "Warning: Stop triggered by timeout after " << timeout << " seconds. (-T " << timeout << ")" << std::endl;
			}
		}

		device->Stop();

		// End Main loop
		// -----------------

		if (verbose) {
			std::cerr << "----------------------" << std::endl;
			for (int j = 0; j < models.size(); j++) {
				std::string name = models[j]->getName();
				statistics[j].Stamp();
				std::cerr << "[" << name << "] " << std::string(37 - name.length(), ' ') << "total: " << statistics[j].getCount() << " msgs" << std::endl;
			}
		}

		if (timer_on)
			for (auto m : models) {
				std::string name = m->getName();
				std::cerr << "[" << m->getName() << "]: " << std::string(37 - name.length(), ' ') << m->getTotalTiming() << " ms" << std::endl;
			}

		device->Close();
	}
	catch (const char* msg) {
		std::cerr << msg << std::endl;
		return -1;
	}

	return 0;
}
