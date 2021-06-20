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

#include "Signal.h"
#include "Device.h"
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

bool checkNetworkAddress(std::string s)
{
	bool correct = true;
	int count = 0;

	char str[20];
	int len = s.copy(str, 20);  str[len] = '\0';

	char* token = strtok(str, ".");

	while (token)
	{
		correct &= getNumber(std::string(token),0,255)>=0;
		token = strtok(NULL, ".");
		count++;
	}

	if (correct && count == 4) return true;
	return false;
}

void Usage()
{
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << "\t[-s xxx sample rate in Hz (default: based on SDR device)]" << std::endl;
	std::cerr << "\t[-d:x device index (default: 0)]" << std::endl;
	std::cerr << "\t[-v enable verbose mode (default: false)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-r filename - read IQ data from raw \'unsigned char\' file]" << std::endl;
	std::cerr << "\t[-r cu8 filename - read IQ data from raw \'unsigned char\' file]" << std::endl;
	std::cerr << "\t[-r cs16 filename - read IQ data from raw \'signed 16 bit integer\' file]" << std::endl;
	std::cerr << "\t[-r cf32 filename - read IQ data from WAV file in \'float\' format]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file in \'float\' format]" << std::endl;
	std::cerr << "\t[-l list available devices and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-q surpress NMEA messages to screen (default: false)]" << std::endl;
#ifdef HASRTLSDR
	std::cerr << "\t[-p xx frequency correction for RTL SDR]" << std::endl;
#endif
	std::cerr << "\t[-u xx.xx.xx.xx yyy UDP address and port (default: off)]" << std::endl;
	std::cerr << "\t[-h display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-m xx run specific decoding model - 0: standard, 1: base, 2: coherent (default: 0)]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models - for development purposes (default: off)]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "Note: if sample rate is set at 48 KHz, input is assumed to be the output of a FM discriminator" << std::endl;
}

int main(int argc, char* argv[])
{
	int sample_rate = 0;
	int input_device = -1;
	bool list_devices = false;
	bool verbose = false;
	bool timer_on = false;
	bool NMEA_to_screen = true;
	int ppm_correction = 0;
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

		std::vector<Device::Description> device_list;
#ifdef HASRTLSDR
		Device::RTLSDR::pushDeviceList(device_list);
#endif
#ifdef HASAIRSPYHF
		Device::AIRSPYHF::pushDeviceList(device_list);
#endif

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
				sample_rate = getNumber(arg1,0,1536000);
				ptr++;
				break;
			case 'm':
				liveModelsSelected.push_back(getNumber(arg1,0,3));
				ptr++;
				break;
			case 'v':
				verbose = true;
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
					if (input_device < 0 || input_device >= device_list.size())
					{
						std::cerr << "Device does not exist : " << param << std::endl;
						list_devices = true;
					}
				}
				else
				{
					std::cerr << "Invalid syntax : " << param << std::endl;
					list_devices = true;
				}
				break;
			case 'u':
				udp_address = arg1; udp_port = arg2;

				if(!checkNetworkAddress(udp_address) || getNumber(udp_port,0,65535)<0)
				{
					std::cerr << "UDP network address and/or port not valid." << std::endl;
					return -1;
				}
				ptr += 2;
				break;
			case 'h':
				Usage();
				return 0;
#ifdef HASRTLSDR
			case 'p':
				ppm_correction = getNumber(arg1, -50, 50);
				ptr++;
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
			std::cerr << "Available devices:" << std::endl;
			for(int i = 0; i < device_list.size(); i++)
			{
				std::cerr << "-d:"<< i << " " << device_list[i].getDescription() << std::endl;
			}
			return 0;
		}

		// Select device

		if (input_type == Device::Type::NONE)
		{
			if (device_list.size() == 0) throw "No input device available.";

			Device::Description sel = device_list[0];
			if (input_device >= 0) sel = device_list[input_device];

			input_type = sel.getType();
			handle = sel.getHandle();
		}

		// Device and output stream of device;
		Device::Control* control = NULL;
		Connection<CFLOAT32>* out = NULL;

		// Required for RTLSDR: conversion from usigned char to float
		DSP::ConvertCU8ToCFLOAT32 convertCU8;

		switch (input_type)
		{
		case Device::Type::AIRSPYHF:
		{
#ifdef HASAIRSPYHF
			Device::AIRSPYHF* device = new Device::AIRSPYHF();
			device->openDevice();

			control = device;
			out = &(device->out);
#else
			std::cerr << "AIRSPYHF+ not included in this package. Please build version including AIRSPYHF+ support.";
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
			device->out >> convertCU8;

			control = device;
			out = &(convertCU8.out);

			device->setFrequencyCorrection(ppm_correction);
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

		if(liveModelsSelected.size() == 0)
			liveModelsSelected.push_back(0);

		for (int i = 0; i < liveModelsSelected.size(); i++)
		{
			switch(liveModelsSelected[i])
			{
			case 0: liveModels.push_back(new AIS::ModelStandard(control, out)); break;
			case 1: liveModels.push_back(new AIS::ModelChallenge(control, out)); break;
			case 2: liveModels.push_back(new AIS::ModelCoherent(control, out)); break;
			default: throw "Model not implemented in this version. Check in later.."; break;
			}
		}

		// set and check the sampling rate
		std::vector<uint32_t> device_rates = control->SupportedSampleRates();
		std::vector<uint32_t> model_rates = liveModels[0]->SupportedSampleRates();

		if (sample_rate != 0)
		{
			if (!isRateDefined(sample_rate,model_rates)) throw "Sampling rate not supported in this version.";
			if (!isRateDefined(sample_rate,device_rates)) throw "Sampling rate not supported for this device.";
		}
		else
		{
			sample_rate = setRateAutomatic(device_rates, model_rates);
		}

		// Build model and attach output to main model

		std::vector<IO::SampleCounter<NMEA>> statistics(verbose ? liveModels.size() : 0);

		for (int i = 0; i < liveModels.size(); i++)
		{
			liveModels[i]->buildModel(sample_rate, timer_on);
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
		control->setAGCtoAuto();
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
				SleepSystem(3000);

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
