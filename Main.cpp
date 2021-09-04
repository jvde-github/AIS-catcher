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
#include <cctype>
#include <iomanip>
#include <algorithm>

#include "Signal.h"

#include "DeviceFileRAW.h"
#include "DeviceFileWAV.h"
#include "DeviceRTLSDR.h"
#include "DeviceAIRSPYHF.h"
#include "DeviceAIRSPY.h"

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

// -------------------------------

bool isRateDefined(uint32_t s, std::vector<uint32_t> rates)
{
	for(uint32_t r : rates) if(r == s) return true;
	return false;
}

int setRateAutomatic(std::vector<uint32_t> dev_rates, std::vector<uint32_t> model_rates)
{
	for (auto r : model_rates) if (isRateDefined(r, dev_rates)) return r;

	throw "Sampling rate not available for this combination of model and device.";

	return 0;
}

int getNumber(std::string str, int min, int max)
{
	int sign = 1, ptr = 0, number = 0;

	if (str[ptr] == '-')
	{
		ptr++; sign = -1;
	}

	if (str[ptr] == '0' && str.length() != 1) throw "Error on command line. Not a valid integer.";

	while (ptr < str.length())
	{
		if (str[ptr] < '0' || str[ptr] > '9') throw "Error on command line. Not a number.";
		number = 10 * number + (str[ptr] - '0');
		ptr++;
	}

	number *= sign;

	if(number < min || number > max) throw "Error on command line. Number out of range.";

	return number;
}

void Usage()
{
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-h display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-s xxx sample rate in Hz (default: based on SDR device)]" << std::endl;
	std::cerr << "\t[-v [option: xx] enable verbose mode, optional to provide update frequency in seconds (default: false)]" << std::endl;
	std::cerr << "\t[-q surpress NMEA messages to screen (default: false)]" << std::endl;
	std::cerr << "\t[-u address port - UDP address and port (default: off)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-r filename - read IQ data from raw \'unsigned char\' file]" << std::endl;
	std::cerr << "\t[-r cu8 filename - read IQ data from raw \'unsigned char\' file]" << std::endl;
	std::cerr << "\t[-r cs16 filename - read IQ data from raw \'signed 16 bit integer\' file]" << std::endl;
	std::cerr << "\t[-r cf32 filename - read IQ data from WAV file in \'float\' format]" << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file in \'float\' format]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-l list available devices and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-d:x select device based on index (default: 0)]" << std::endl;
	std::cerr << "\t[-d xxxx select device based on serial number]" << std::endl;
#ifdef HASRTLSDR
	std::cerr << std::endl;
	std::cerr << "\t[-gr RTLSDR specic settings: TUNER [auto/0+] RTLAGC [on/off]" << std::endl;
	std::cerr << "\t[-p xx frequency correction for RTL SDR]" << std::endl;
#endif
#ifdef HASAIRSPY
	std::cerr << std::endl;
	std::cerr << "\t[-gm Airspy specific settings: SENSITIVITY [0-22] LINEARITY [0-22] VGA [0-15] LNA [auto/0-15] MIXER [auto/0-15] ]" << std::endl;
#endif
	std::cerr << std::endl;
	std::cerr << "\t[-m xx run specific decoding model (default: 2)]" << std::endl;
	std::cerr << "\t[\t0: Standard (non-coherent), 1: Base (non-coherent), 2: Default, 3: FM discrimator output]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models - for development purposes (default: off)]" << std::endl;
	std::cerr << std::endl;
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

int getDeviceFromSerial(std::vector<Device::Description>& device_list, std::string serial)
{
	for (int i = 0; i < device_list.size(); i++)
	{
		if (device_list[i].getSerial() == serial) return i;
	}
	std::cerr << "Searching for device with SN " << serial << "." << std::endl;
	return -1;
}

std::vector<AIS::Model*> setupModels(std::vector<int> &liveModelsSelected, Device::Control* control, Connection<CFLOAT32>* out)
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

void setupRates(int& sample_rate, int& model_rate, std::vector<AIS::Model*>& liveModels, Device::Control* control)
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

// Needs clean up and streamlining....
void parseAirspySettings(Device::SettingsAIRSPY& s, char* argv[],int &ptr, int argc)
{
	while (++ptr < argc - 1)
	{
		if (argv[ptr][0] == '-') break;

		if (strcmp(argv[ptr], "SENSITIVITY") == 0)
		{
			s.mode = Device::Sensitivity;
			s.gain = getNumber(argv[++ptr], 0, 22);
		}
		else if (strcmp(argv[ptr], "LINEARITY") == 0)
		{
			s.mode = Device::Linearity;
			s.gain = getNumber(argv[++ptr], 0, 22);
		}
		else if (strcmp(argv[ptr], "VGA") == 0)
		{
			s.mode = Device::Manual;
			ptr++;
			s.VGA_Gain = getNumber(argv[ptr], 0, 15);
		}
		else if (strcmp(argv[ptr], "MIXER") == 0)
		{
			s.mode = Device::Manual;
			ptr++;
			if (strcmp(argv[ptr], "auto") == 0)
				s.mixer_AGC = true;
			else
			{
				s.mixer_AGC = false;
				s.mixer_Gain = getNumber(argv[ptr], 0, 15);
			}
		}
		else if (strcmp(argv[ptr], "LNA") == 0)
		{
			s.mode = Device::Manual;
			ptr++;
			if (strcmp(argv[ptr], "auto") == 0)
				s.LNA_AGC = true;
			else
			{
				s.LNA_AGC = false;
				s.LNA_Gain = getNumber(argv[ptr], 0, 15);
			}
		}
		else
			throw " Invalid Gain setting for AIRSPY";
	}
	ptr --;
}

// Needs clean up and streamlining....
void parseRTLSDRSettings(Device::SettingsRTLSDR& s, char* argv[],int &ptr, int argc)
{
	while (++ptr < argc - 1)
	{
		if (argv[ptr][0] == '-') break;

		else if (strcmp(argv[ptr], "TUNER") == 0)
		{
			ptr++;
			if (strcmp(argv[ptr], "auto") == 0)
				s.tuner_AGC = true;
			else
			{
				s.tuner_AGC = false;
				s.tuner_Gain = getNumber(argv[ptr], 0, 100);
			}
		}
		else if (strcmp(argv[ptr], "RTLAGC") == 0)
		{
			ptr++;
			if (strcmp(argv[ptr], "on") == 0)
				s.RTL_AGC = true;
			else if (strcmp(argv[ptr], "off") == 0)
			{
				s.RTL_AGC = false;
			}
			else
				throw "Invalid RTLAGC switch on command line [on/off]";
		}
		else
			throw " Invalid Gain setting for RTLSDR";
	}
	ptr --;
}

int main(int argc, char* argv[])
{
	int sample_rate = 0;
	int model_rate = 0;

	int input_device = 0;

	bool list_devices = false;
	bool verbose = false;
	bool timer_on = false;
	bool NMEA_to_screen = true;
	bool RTLSDRfastDS = true;
	int verboseUpdateTime = 3000;

	Device::SettingsRTLSDR settingsRTL;
	Device::SettingsAIRSPYHF settingsAIRSPYHF;
	Device::SettingsAIRSPY settingsAIRSPY;


	uint64_t handle = 0;
	Device::Format RAWformat = Device::Format::CU8;

	std::string filename_in = "";
	std::string filename_out = "";
	std::string udp_address = "";
	std::string udp_port = "";

	std::vector<AIS::Model*> liveModels;
	std::vector<int> liveModelsSelected;
	Device::Type input_type = Device::Type::NONE;
	IO::UDP udp;
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

			std::string arg1 = ptr < argc - 1 ? std::string(argv[ptr + 1]) : "";
			std::string arg2 = ptr < argc - 2 ? std::string(argv[ptr + 2]) : "";

			if (param[0] != '-')
			{
				std::cerr << "Invalid argument : " << param << std::endl;
				return -1;
			}

			switch (param[1])
			{
			case 's':
				sample_rate = getNumber(arg1, 0, 3000000);
				ptr++;
				break;
			case 'm':
				liveModelsSelected.push_back(getNumber(arg1,0,5));
				ptr++;
				break;
			case 'v':
				verbose = true;
				if (arg1 != "" && arg1[0] != '-')
				{
					verboseUpdateTime = getNumber(arg1, 0, 3600) * 1000;
					ptr++;
				}
				break;
			case 'q':
				NMEA_to_screen = false;
				break;
			case 'b':
				timer_on = true;
				break;
			case 'w':
				input_type = Device::Type::WAVFILE;
				filename_in = arg1;
				ptr++;
				break;
			case 'r':
				input_type = Device::Type::RAWFILE;
				if(arg2 == "" || arg2[0] == '-')
				{
					filename_in = arg1;
					ptr++;
				}
				else
				{
					filename_in = arg2;

					if(arg1 == "cu8") RAWformat = Device::Format::CU8;
					else if(arg1 == "cs16") RAWformat = Device::Format::CS16;
					else if(arg1 == "cf32") RAWformat = Device::Format::CF32;
					else
					{
						std::cerr << "Unsupported RAW file format specified : " << arg1 << std::endl;
						return -1;
					}
					ptr += 2;
				}
				break;
			case 'l':
				list_devices = true;
				break;
			case 'd':
				if (param.length() == 4 && param[2] == ':')
				{
					input_device = (param[3] - '0');
				}
				else
				{
					input_device = getDeviceFromSerial(device_list, arg1);
					ptr++;
				}
				if (input_device < 0 || input_device >= device_list.size())
				{
					std::cerr << "Device does not exist." << std::endl;
					list_devices = true;
				}
				break;
			case 'u':
				udp_address = arg1; udp_port = arg2;
				ptr += 2;
				break;
			case 'h':
				Usage();
				return 0;
#ifdef HASRTLSDR
			case 'p':
				settingsRTL.FreqCorrection = getNumber(arg1, -100, 100);
				ptr++;
				break;
			case 'g':
				if(param == "-gm")
					parseAirspySettings(settingsAIRSPY, argv, ptr, argc);
				else if(param == "-gr")
					parseRTLSDRSettings(settingsRTL, argv, ptr, argc);
				else
					throw "Invalid -gx switch on command line";
				break;
#endif
			default:
				std::cerr << "Unknown option " << param << std::endl;
				return -1;
			}

			ptr++;
		}

		if (list_devices)
		{
			printDevices(device_list);
			return 0;
		}

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
		Device::Control* control = NULL;
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
			device->openDevice();

			control = device;
			out = &(device->out);

			device->setSettings(settingsAIRSPYHF);
#else
			std::cerr << "AIRSPYHF+ not included in this package. Please build version including AIRSPYHF+ support.";
#endif
			break;
		}
		case Device::Type::AIRSPY:
		{
#ifdef HASAIRSPY
			Device::AIRSPY* device = new Device::AIRSPY();
			device->openDevice();

			control = device;
			out = &(device->out);

			device->setSettings(settingsAIRSPY);
			if(verbose) settingsAIRSPY.Print();
#else
			std::cerr << "AIRSPY not included in this package. Please build version including AIRSPY support.";
#endif
			break;
		}
		case Device::Type::WAVFILE:
		{
			Device::WAVFile* device = new Device::WAVFile();
			device->openFile(filename_in);

			control = device;
			out = &(device->out);
			break;
		}
		case Device::Type::RAWFILE:
		{
			Device::RAWFile* device = new Device::RAWFile();
			device->openFile(filename_in);
			device->setFormat(RAWformat);

			control = device;
			out = &(device->out);
			break;
		}
		case Device::Type::RTLSDR:
		{
#ifdef HASRTLSDR

			Device::RTLSDR* device = new Device::RTLSDR();
			device->openDevice(handle);

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
			device->setSettings(settingsRTL);
			if(verbose) settingsRTL.Print();
			control = device;

#else
			std::cerr << "RTLSDR not included in this package. Please build version including RTLSDR support.";
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
		if (udp_address != "")
		{
			udp.openConnection(udp_address, udp_port);
			liveModels[0]->Output() >> udp;
		}

		if (NMEA_to_screen)
			liveModels[0]->Output() >> nmea_screen;

		// Set up Device
		control->setSampleRate(sample_rate);
		control->setFrequency((int)(162e6));

		if(verbose)
		{
			std::cerr << "Frequency (Hz)     : " << control->getFrequency() << std::endl;
			std::cerr << "Sampling rate (Hz) : " << control->getSampleRate() << std::endl;
		}

		// Main loop
		control->Play();

		while(control->isStreaming())
		{
			if (control->isCallback()) // don't go to sleep in case we are reading from a file
			{
				SleepSystem(verboseUpdateTime);

				if(verbose)
					for(int j = 0; j < liveModels.size(); j++)
						std::cerr << "[" << liveModels[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << std::setprecision(2) << statistics[j].getRate() << " msg/s" << std::endl;
			}
		}

		control->Pause();

		if (verbose)
		{
			std::cerr << "----------------------" << std::endl;
			for(int j = 0; j < liveModels.size(); j++)
				std::cerr << "[" << liveModels[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << std::setprecision(2) << statistics[j].getRate() << " msg/s" << std::endl;

		}

		if(timer_on)
			for (auto m : liveModels)
				std::cerr << "[" << m->getName() << "]\t: " << m->getTotalTiming() << " ms" << std::endl;

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
