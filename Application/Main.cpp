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
#include "Application.h"



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

	Command() << "stop";
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
	Info() << "\t[-X connect to AIS community feed at aiscatcher.org (default: off)]";
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

int main(int argc, char *argv[])
{
	int exit_code = 0;

	try
	{
		Logger::getInstance().setMaxBufferSize(50);

#ifdef _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("could not set control handler");
#else
		signal(SIGINT, consoleHandler);
		signal(SIGTERM, consoleHandler);
		signal(SIGHUP, consoleHandler);
		signal(SIGPIPE, consoleHandler);
#endif

		//_receivers.back()->refreshDevices();

		printVersion();

		app.argc = argc;
		app.argv = argv;

		do
		{
			app.Setup();

			if (app.list_devices)
				printDevices(*app.run->_receivers.back(), app.list_devices_JSON);
			if (app.list_support)
				printSupportedDevices();
			if (app.list_options)
				Usage();
			if (app.list_devices || app.list_support || app.list_options || app.no_run)
				return 0;

			// -------------
			// set up the receiver and open the device
			app.Start();
			app.RunConsole();
			app.Stop();
		} while(app.restart);
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		app.Stop();
		exit_code = -1;
	}

	app.Close();
	    //Logger::getInstance().shutdown();
	return exit_code;
}
