/*
Copyright(c) 2021 jvde.github@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <iostream>
#include <string.h>
#include <algorithm>

#include "AIS-catcher.h"
#include "Signal.h"

#include "DeviceFileRAW.h"
#include "DeviceFileWAV.h"
#include "DeviceRTLSDR.h"
#include "DeviceAIRSPYHF.h"
#include "DeviceHACKRF.h"
#include "DeviceRTLTCP.h"
#include "DeviceAIRSPY.h"
#include "DeviceSDRPLAY.h"

#include "IO.h"
#include "Model.h"

MessageHub<SystemMessage> SystemMessages;

#ifdef WIN32
BOOL WINAPI consoleHandler(DWORD signal)
{
	if (signal == CTRL_C_EVENT) SystemMessages.Send(SystemMessage::Stop);
	return TRUE;
}
#else
void consoleHandler(int signal)
{
	SystemMessages.Send(SystemMessage::Stop);
}
#endif

void Usage()
{
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-h display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-s xxx sample rate in Hz (default: based on SDR device)]" << std::endl;
	std::cerr << "\t[-v [option: 1+] enable verbose mode, optional to provide update frequency in seconds (default: false)]" << std::endl;
	std::cerr << "\t[-q suppress NMEA messages to screen (default: false)]" << std::endl;
	std::cerr << "\t[-n show NMEA messages on screen without detail]" << std::endl;
	std::cerr << "\t[-u xxx.xx.xx.xx yyy - UDP destination address and port (default: off)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-r [optional: yy] filename - read IQ data from file, short for -r -ga FORMAT yy FILE filename]" << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file, short for -w -gw FILE filename]" << std::endl;
	std::cerr << "\t[-t [host [port]] - read IQ data from remote RTL-TCP instance]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-l list available devices and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-L list supported SDR hardware and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-d:x select device based on index (default: 0)]" << std::endl;
	std::cerr << "\t[-d xxxx select device based on serial number]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-m xx run specific decoding model (default: 2)]" << std::endl;
	std::cerr << "\t[\t0: Standard (non-coherent), 1: Base (non-coherent), 2: Default, 3: FM discriminator output]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models for time - for development purposes (default: off)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\tDevice specific settings:" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-gr RTLSDRs: TUNER [auto/0.0-50.0] RTLAGC [on/off] BIASTEE [on/off] FREQOFFSET [-150-150]" << std::endl;
	std::cerr << "\t[-p xx equivalent to -gr FREQOFFSET xx]" << std::endl;
	std::cerr << "\t[-gm Airspy: SENSITIVITY [0-21] LINEARITY [0-21] VGA [0-14] LNA [auto/0-14] MIXER [auto/0-14] BIASTEE [on/off] ]" << std::endl;
	std::cerr << "\t[-gh Airspy HF+: TRESHOLD [low/high] PREAMP [on/off] ]" << std::endl;
	std::cerr << "\t[-gs SDRPLAY: GRDB [0-59] LNASTATE [0-9] AGC [on/off] ]" << std::endl;
	std::cerr << "\t[-gf HACKRF: LNA [0-40] VGA [0-62] PREAMP [on/off]" << std::endl;
	std::cerr << "\t[-gt RTLTCPs: HOST [address] PORT [port] TUNER [auto/0.0-50.0] RTLAGC [on/off] FREQOFFSET [-150-150]" << std::endl;
	std::cerr << "\t[-ga RAW file: FILE [filename] FORMAT [CF32/CS16/CU8/CS8]" << std::endl;
	std::cerr << "\t[-gw WAV file: FILE [filename]" << std::endl;
}

std::vector<Device::Description> getDevices()
{
	std::vector<Device::Description> device_list;

#ifdef HASRTLSDR
	Device::RTLSDR::pushDeviceList(device_list);
#endif
#ifdef HASAIRSPYHF
	Device::AIRSPYHF::pushDeviceList(device_list);
#endif
#ifdef HASAIRSPY
	Device::AIRSPY::pushDeviceList(device_list);
#endif
#ifdef HASSDRPLAY
	Device::SDRPLAY::pushDeviceList(device_list);
#endif
#ifdef HASRTLTCP
//	Device::RTLTCP::pushDeviceList(device_list);
#endif
#ifdef HASHACKRF
	Device::HACKRF::pushDeviceList(device_list);
#endif
	std::sort(device_list.begin(), device_list.end());

	return device_list;
}

std::string getDeviceDescription(Device::Description& d)
{
	return d.getVendor() + ", " + d.getProduct() + ", SN: " + d.getSerial();
}

void printDevices(std::vector<Device::Description>& device_list)
{
	std::cerr << "Found " << device_list.size() << " device(s):" << std::endl;
	for (int i = 0; i < device_list.size(); i++)
	{
		std::cerr << i << ": " << getDeviceDescription(device_list[i]) << std::endl;
	}
}

void printVersion()
{
	std::cerr << "AIS-catcher (build " << __DATE__ << ") " << VERSION <<std::endl ;
	std::cerr << "(C) Copyright 2021 " << COPYRIGHT << std::endl;
	std::cerr << "This is free software; see the source for copying conditions.There is NO"  << std::endl;
	std::cerr << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << std::endl;
}

void printSupportedDevices(std::vector<Device::Description>& device_list)
{
	std::cerr << "Supported SDR(s): ";
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
#ifdef HASRTLTCP
	std::cerr << "RTLTCP ";
#endif
#ifdef HASHACKRF
	std::cerr << "HACKRF ";
#endif
	std::cerr << std::endl;
}

int getDeviceFromSerial(std::vector<Device::Description>& device_list, std::string serial)
{
	for (int i = 0; i < device_list.size(); i++)
	{
		if (device_list[i].getSerial() == serial) return i;
	}
	std::cerr << "Searching for device with SN " << serial << "." << std::endl;
	return -1;
}

std::vector<AIS::Model*> setupModels(std::vector<int> &liveModelsSelected, Device::DeviceBase* control, Connection<CFLOAT32>* out)
{
	std::vector<AIS::Model*> liveModels;

	if (liveModelsSelected.size() == 0)
		liveModelsSelected.push_back(2);

	for (auto mi : liveModelsSelected)
	{
		switch (mi)
		{
		case 0: liveModels.push_back(new AIS::ModelStandard(control, out)); break;
		case 1: liveModels.push_back(new AIS::ModelBase(control, out)); break;
		case 2: liveModels.push_back(new AIS::ModelCoherent(control, out)); break;
		case 3: liveModels.push_back(new AIS::ModelDiscriminator(control, out)); break;
		case 4: liveModels.push_back(new AIS::ModelChallenger(control, out)); break;
		default: throw "Internal error: Model not implemented in this version. Check in later."; break;
		}
	}

	return liveModels;
}

// -------------------------------

bool isRateDefined(uint32_t s, std::vector<uint32_t> rates)
{
	for (uint32_t r : rates) if (r == s) return true;
	return false;
}

int setRateAutomatic(std::vector<uint32_t> dev_rates, std::vector<uint32_t> model_rates)
{
	for (auto r : dev_rates) if (isRateDefined(r, model_rates)) return r;
	throw "Sampling rate not available for this combination of model and device.";

	return 0;
}

void setupRates(int& sample_rate, int& model_rate, std::vector<AIS::Model*>& liveModels, Device::DeviceBase* control)
{

	std::vector<uint32_t> device_rates = control->SupportedSampleRates();
	std::vector<uint32_t> model_rates = liveModels[0]->SupportedSampleRates();

	if (sample_rate != 0)
	{
		if (model_rate == 0) model_rate = sample_rate;

		if (!isRateDefined(model_rate, model_rates)) throw "Sampling rate not supported in this version.";
		if (!isRateDefined(sample_rate, device_rates)) throw "Sampling rate not supported for this device.";
	}
	else
	{
		model_rate = sample_rate = setRateAutomatic(device_rates, model_rates);
	}
}

void parseDeviceSettings(Device::DeviceSettings& s, char* argv[], int ptr, int argc)
{
	ptr++;

	while (ptr < argc - 1 && argv[ptr][0] != '-')
	{
		std::string option = argv[ptr++];
		std::string arg = argv[ptr++];

		s.Set(option, arg);
	}
}

bool isoption(std::string s)
{
	return s.length() >= 2 && s[0] == '-' && std::isalpha(s[1]);
}

void Assert(bool b)
{
	if (!b) throw "Error on command line: syntax error.";
}

int main(int argc, char* argv[])
{
	int sample_rate = 0;
	int model_rate = 0;

	int input_device = 0;

	bool list_devices = false, list_support = false, list_options = false;
	bool verbose = false;
	bool timer_on = false;
	int NMEA_to_screen = 2;
	bool RTLSDRfastDS = true;
	int verboseUpdateTime = 3000;

	Device::SettingsRAWFile settingsRAW;
	Device::SettingsWAVFile settingsWAV;
	Device::SettingsRTLSDR settingsRTL;
	Device::SettingsRTLTCP settingsRTLTCP;
	Device::SettingsAIRSPYHF settingsAIRSPYHF;
	Device::SettingsAIRSPY settingsAIRSPY;
	Device::SettingsSDRPLAY settingsSDRPLAY;
	Device::SettingsHACKRF settingsHACKRF;

	uint64_t handle = 0;

	std::vector<IO::UDPEndPoint> UDPdestinations;
	std::vector<IO::UDP> UDPconnections;

	std::vector<AIS::Model*> liveModels;
	std::vector<int> liveModelsSelected;
	Device::Type input_type = Device::Type::NONE;

	IO::DumpScreen nmea_screen;

	try
	{
#ifdef WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw "ERROR: Could not set control handler";
#else
		signal(SIGINT, consoleHandler);
#endif

		std::vector<Device::Description> device_list = getDevices();

		int ptr = 1;

		while (ptr < argc)
		{
			std::string param = std::string(argv[ptr]);
			Assert(param[0] == '-');

			int count = 0;
			while (ptr + count + 1 < argc && !isoption(argv[ptr + 1 + count])) count++;

			std::string arg1 = count >= 1 ? std::string(argv[ptr + 1]) : "";
			std::string arg2 = count >= 2 ? std::string(argv[ptr + 2]) : "";

			switch (param[1])
			{
			case 's':
				Assert(count == 1);
				sample_rate = Util::Parse::Integer(arg1, 48000, 6144000);
				break;
			case 'm':
				Assert(count == 1);
				liveModelsSelected.push_back(Util::Parse::Integer(arg1, 0, 5));
				break;
			case 'v':
				Assert(count <= 1);
				verbose = true;
				if (count == 1) verboseUpdateTime = Util::Parse::Integer(arg1, 1, 3600) * 1000;
				break;
			case 'q':
				Assert(count == 0);
				NMEA_to_screen = 0;
				break;
			case 'n':
				Assert(count == 0);
				NMEA_to_screen = 1;
				break;
			case 't':
				input_type = Device::Type::RTLTCP;
				Assert(count <= 2);
				if(count >= 1) settingsRTLTCP.Set("host",arg1);
				if(count >= 2) settingsRTLTCP.Set("port",arg2);
				break;
			case 'b':
				Assert(count == 0);
				timer_on = true;
				break;
			case 'w':
				Assert(count <= 1);
				input_type = Device::Type::WAVFILE;
				if (count == 1) settingsWAV.Set("FILE", arg1);
				break;
			case 'r':
				Assert(count <= 2);
				input_type = Device::Type::RAWFILE;
				if (count == 1)
				{
					settingsRAW.Set("FILE", arg1);
				}
				else if (count == 2)
				{
					settingsRAW.Set("FORMAT", arg1);
					settingsRAW.Set("FILE", arg2);
				}
				break;
			case 'l':
				Assert(count == 0);
				list_devices = true;
				break;
			case 'L':
				Assert(count == 0);
				list_support = true;
				break;
			case 'd':
				if (param.length() == 4 && param[2] == ':')
				{
					Assert(count == 0);
					input_device = (param[3] - '0');
				}
				else
				{
					Assert(count == 1);
					input_device = getDeviceFromSerial(device_list, arg1);
				}
				if (input_device < 0 || input_device >= device_list.size())
				{
					std::cerr << "Device does not exist." << std::endl;
					return 0;
				}
				break;
			case 'u':
			case 'U':
				Assert(count == 2);
				UDPdestinations.push_back(IO::UDPEndPoint(arg1, arg2));
				break;
			case 'h':
				Assert(count == 0);
				list_options = true;
				break;
			case 'p':
				Assert(count == 1);
				settingsRTL.Set("FREQOFFSET", arg1);
				break;
			case 'g':
				Assert(count % 2 == 0 && param.length() == 3);
				switch (param[2])
				{
				case 'm': parseDeviceSettings(settingsAIRSPY, argv, ptr, argc); break;
				case 'r': parseDeviceSettings(settingsRTL, argv, ptr, argc); break;
				case 'h': parseDeviceSettings(settingsAIRSPYHF, argv, ptr, argc); break;
				case 's': parseDeviceSettings(settingsSDRPLAY, argv, ptr, argc); break;
				case 'a': parseDeviceSettings(settingsRAW, argv, ptr, argc); break;
				case 'w': parseDeviceSettings(settingsWAV, argv, ptr, argc); break;
				case 't': parseDeviceSettings(settingsRTLTCP, argv, ptr, argc); break;
				case 'f': parseDeviceSettings(settingsHACKRF, argv, ptr, argc); break;
				default: throw "Error on command line: invalid -g switch on command line";
				}
				break;
			default:
				std::cerr << "Unknown option on command line ignored (" << param[1] << ")." << std::endl;
				return 0;
			}

			ptr += count + 1;
		}

		if (verbose || list_devices || list_support || NMEA_to_screen == 2 || list_options) printVersion();
		if (list_devices) printDevices(device_list);
		if (list_support) printSupportedDevices(device_list);
		if (list_options) Usage();
		if (list_devices || list_support || list_options) return 0;

		// Select device

		if (input_type == Device::Type::NONE)
		{
			if (device_list.size() == 0) throw "No input device available.";

			Device::Description d = device_list[input_device];
			input_type = d.getType();
			handle = d.getHandle();

			std::cerr << "Device selected: " << getDeviceDescription(d) << std::endl;
		}

		// Device and output stream of device;
		Device::DeviceBase* control = NULL;
		Connection<CFLOAT32>* out = NULL;

		// Required for RTLSDR: conversion from usigned char to float
		DSP::ConvertCU8ToCFLOAT32 convertCU8;
		DSP::RTLSDRFastDownsample convertFastDS;

		switch (input_type)
		{
		case Device::Type::AIRSPYHF:
		{
#ifdef HASAIRSPYHF
			Device::AIRSPYHF* device = new Device::AIRSPYHF();
			device->Open(handle,settingsAIRSPYHF);

			control = device;
			out = &(device->out);

			if(verbose) settingsAIRSPYHF.Print();
#else
			throw "AIRSPYHF+ not included in this package. Please build version including AIRSPYHF+ support.";
#endif
			break;
		}
		case Device::Type::AIRSPY:
		{
#ifdef HASAIRSPY
			Device::AIRSPY* device = new Device::AIRSPY();
			device->Open(handle,settingsAIRSPY);

			control = device;
			out = &(device->out);

			if(verbose) settingsAIRSPY.Print();
#else
			throw "AIRSPY not included in this package. Please build version including AIRSPY support.";
#endif
			break;
		}
		case Device::Type::SDRPLAY:
		{
#ifdef HASSDRPLAY
			Device::SDRPLAY* device = new Device::SDRPLAY();
			device->Open(handle,settingsSDRPLAY);

			control = device;
			out = &(device->out);

			if(verbose) settingsSDRPLAY.Print();
#else
			throw "SDRPLAY not included in this package. Please build version including SDRPLAY support.";
#endif
			break;
		}
		case Device::Type::WAVFILE:
		{
			Device::WAVFile* device = new Device::WAVFile();

			control = device;
			out = &(device->out);

			device->Open(settingsWAV);
			if (verbose) settingsWAV.Print();

			break;
		}
		case Device::Type::RAWFILE:
		{
			Device::RAWFile* device = new Device::RAWFile();

			control = device;
			out = &(device->out);

			device->Open(settingsRAW);
			if (verbose) settingsRAW.Print();

			break;
		}
		case Device::Type::RTLSDR:
		{
#ifdef HASRTLSDR
			Device::RTLSDR* device = new Device::RTLSDR();
			device->Open(handle,settingsRTL);

			if(sample_rate == 0 and RTLSDRfastDS)
			{
				device->out >> convertFastDS;
				out = &(convertFastDS.out);

				sample_rate = 1536000;
				model_rate = 96000;
			}
			else
			{
				device->out >> convertCU8;
				out = &(convertCU8.out);
			}

			if(verbose) settingsRTL.Print();
			control = device;

#else
			throw "RTLSDR not included in this package. Please build version including RTLSDR support.";
#endif
			break;
		}
		case Device::Type::RTLTCP:
		{
#ifdef HASRTLTCP
			Device::RTLTCP* device = new Device::RTLTCP();
			device->Open(handle, settingsRTLTCP);

			device->out >> convertCU8;
			out = &(convertCU8.out);

			if (verbose) settingsRTLTCP.Print();
			control = device;

#else
			throw "RTLTCP not included in this package. Please build version including RTLTCP support.";
#endif
			break;
		}
		case Device::Type::HACKRF:
		{
#ifdef HASHACKRF
			Device::HACKRF* device = new Device::HACKRF();
			device->Open(handle, settingsHACKRF);

			control = device;
			out = &(device->out);

			if (verbose) settingsHACKRF.Print();

#else
			throw "HACKRF not included in this package. Please build version including RTLTCP support.";
#endif
			break;
		}
		default:
			throw "Error: invalid device selection";
		}
		if (control == 0) throw "Error: cannot set up device";

		SystemMessages.Connect(*control);

		// Create demodulation models
		liveModels = setupModels(liveModelsSelected, control, out);

		// set and check the sampling rate
		setupRates(sample_rate, model_rate, liveModels, control);

		// Build model and attach output to main model

		std::vector<IO::SampleCounter<NMEA>> statistics(verbose ? liveModels.size() : 0);

		for (int i = 0; i < liveModels.size(); i++)
		{
			liveModels[i]->buildModel(model_rate, timer_on);
			if (verbose) liveModels[i]->Output() >> statistics[i];
		}

		// Connect output to UDP stream
		UDPconnections.resize(UDPdestinations.size());

		for(int i = 0; i < UDPdestinations.size(); i++)
		{
			UDPconnections[i].openConnection(UDPdestinations[i]);
			liveModels[0]->Output() >> UDPconnections[i];
		}

		if (NMEA_to_screen > 0)
		{
			liveModels[0]->Output() >> nmea_screen;
			nmea_screen.setDetail(NMEA_to_screen);
		}

		// Set up generic device parameters
		control->setSampleRate(sample_rate);
		control->setFrequency((int)(162e6));

		if(verbose)
		{
			std::cerr << "Generic settings: " << "sample rate -s " << control->getSampleRate() << ", model rate " << model_rate/1000 << "K" << std::endl;
		}

		// -----------------
		// Main loop
		control->Play();

		while(control->isStreaming())
		{
			if (control->isCallback()) // don't go to sleep in case we are reading from a file
			{
				SleepSystem(verboseUpdateTime);

				if (verbose)
					for (int j = 0; j < liveModels.size(); j++)
						std::cerr << "[" << liveModels[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << statistics[j].getRate() << " msg/s" << std::endl;
			}
		}

		control->Stop();
		// End Main loop
		// -----------------

		if (verbose)
		{
			std::cerr << "----------------------" << std::endl;
			for (int j = 0; j < liveModels.size(); j++)
				std::cerr << "[" << liveModels[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << statistics[j].getRate() << " msg/s" << std::endl;

		}

		if(timer_on)
			for (auto m : liveModels)
				std::cerr << "[" << m->getName() << "]\t: " << m->getTotalTiming() << " ms" << std::endl;

		control->Close();

		for(auto model : liveModels) delete model;
		if(control) delete control;

	}
	catch (const char * msg)
	{
		std::cerr << msg << std::endl;
		return -1;
	}

	return 0;
}
