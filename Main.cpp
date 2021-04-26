/*
Copyright(c) 2021 Jasper van den Eshof

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
#include <string>
#include <sstream>
#include <chrono>
#include <typeinfo>

#include "Signal.h"
#include "Device.h"
#include "DSP.h"
#include "Model.h"
#include "AIS.h"
#include "Filters.h"


MessageHub<SystemMessage> SystemMessages;

#ifdef WIN32
BOOL WINAPI consoleHandler(DWORD signal) {

	if (signal == CTRL_C_EVENT)
	{
		SystemMessages.Send(SystemMessage::Stop);
	}
	return TRUE;
}
#else
void consoleHandler(int signum) {
	SystemMessages.Send(SystemMessage::Stop);
}
#endif

// -------------------------------

bool rateDefined(uint32_t s, std::vector<uint32_t> rates)
{
	for(uint32_t r : rates) if(r == s) return true;

	return false;
}

void Usage()
{
	std::cerr << "use: AIS-catcher [options]" << std::endl;
	std::cerr << "\t[-s sample rate in Hz (default: based on SDR device)]" << std::endl;
	std::cerr << "\t[-d:x device index (default: 0)]" << std::endl;
	std::cerr << "\t[-v enable verbose mode (default: false)]" << std::endl;
	std::cerr << "\t[-r filename - read IQ data from raw \'unsigned char\' file]" << std::endl;
	std::cerr << "\t[-w filename - read IQ data from WAV file in \'float\' format]" << std::endl;
	std::cerr << "\t[-l list available devices and terminate (default: off)]" << std::endl;
	std::cerr << "\t[-q surpress NMEA messages to screen (default: false)]" << std::endl;
	std::cerr << "\t[-p:xx frequency offset (reserved for future version)]" << std::endl;
	std::cerr << "\t[-u UDP address and host (reserved for future version)]" << std::endl;
	std::cerr << "\t[-u display this message and terminate (default: false)]" << std::endl;
	std::cerr << "\t[-c run challenger model - for development purposes (default: off)]" << std::endl;
	std::cerr << "\t[-b benchmark demodulation models - for development purposes (default: off)]" << std::endl;
}

int main(int argc, char* argv[])
{
	int sample_rate = 0;
	int input_device = -1;
	bool list_devices = false;
	bool run_challenger = false;
	bool verbose = false;
	bool timer_on = false;
	bool NMEA_to_screen = true;
	uint64_t handle = 0;

	Device::Type input_type = Device::Type::NONE;
	std::string filename_in = "";
	std::string filename_out = "";
	std::string udp_address = "";
	std::string udp_port = "";

	std::vector<uint32_t> model_rates{ 288000, 384000, 768000, 1536000 };
	std::vector<Model*> model;

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
			bool has_arg = ptr < argc - 1 && argv[ptr + 1][0] != '-';
			std::string arg = has_arg ? std::string(argv[ptr + 1]) : "";

			if (param[0] != '-')
			{
				std::cerr << "Invalid argument : " << param << std::endl;
				return -1;
			}

			switch (param[1])
			{
			case 's':
				sample_rate = std::stoi(arg);
				ptr++;
				break;
			case 'v':
				verbose = true;
				break;
			case 'c':
				run_challenger = true;
				break;
			case 'q':
				NMEA_to_screen = false;
				break;
			case 'b':
				timer_on = true;
				break;
			case 'w':
				input_type = Device::Type::WAVFILE;
				filename_in = arg;
				ptr++;
				break;
			case 'r':
				input_type = Device::Type::RAWFILE;
				filename_in = arg;
				ptr++;
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
				Usage();
				return 0;

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

		if (device_list.size() == 0 && input_type == Device::Type::NONE) throw "No input device available.";

		if (input_type != Device::Type::RAWFILE && input_type != Device::Type::WAVFILE)
		{
			Device::Description device_selected = device_list[0];

			if (input_device >= 0) device_selected = device_list[input_device];

			input_type = device_selected.getType();
			handle = device_selected.getHandle();
		}

		// device and output stream of device;
		Device::Control* control = NULL;
		// RTLSDR conversion from usigned char to float
		DSP::ConvertCU8ToCFLOAT32 conversion;
		Connection<CFLOAT32>* out = NULL; 
		DSP::DumpScreen nmea_print;

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
			device->out.Connect(&conversion);

			control = device;
			out = &(conversion.out);
			break;
		}
		case Device::Type::RTLSDR:
		{
#ifdef HASRTLSDR
			Device::RTLSDR* device = new Device::RTLSDR();
			device->openDevice(handle);
			device->out.Connect(&conversion);

			control = device;
			out = &(conversion.out);

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

		// set and check the sampling rate

		std::vector<uint32_t> device_rates;
		control->getAvailableSampleRates(device_rates);

		if (sample_rate != 0)
		{
			if (!rateDefined(sample_rate,model_rates)) throw "Sampling rate not supported in this version.";
		}
		else
		{
			bool found = false;
			for(int i = 0; i < model_rates.size() && !found; i++)
			{
				if(rateDefined(model_rates[i], device_rates))
				{
					sample_rate = model_rates[i];
					found = true;
				}
			}
			if (!found) throw "Sampling rate not available for this device.";
		}

		std::vector<DSP::SampleCounter<NMEA>> statistics(2);

		Model *m = new ModelStandard(sample_rate, control, out);
		m->BuildModel(timer_on);
		if(verbose) m->Output() >> statistics[0];
		if(NMEA_to_screen) m->Output() >> nmea_print;
		model.push_back(m);

		if (run_challenger)
		{
			m = new ModelChallenge(sample_rate, control, out);
			m->BuildModel(timer_on);
			if (verbose) m->Output() >> statistics[1];
			model.push_back(m);
		}
		// Set up Device

		control->setSampleRate(sample_rate);
		control->setAGCtoAuto();
		control->setFrequency((int)(162e6));

		std::cerr << "Frequency     : " << control->getFrequency() << std::endl;
		std::cerr << "Sampling rate : " << control->getSampleRate() << std::endl;

		control->Play();

		for (int i = 0; (i < 3600 * 24 || !control->isCallback()) && control->isStreaming(); i++)
		{
			if (control->isCallback()) // don't go to sleep in case we are reading from a file
			{
				SleepSystem(3000);

				if(verbose)
					for(int j = 0; j < model.size(); j++)
						std::cerr << "[" << model[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << statistics[j].getRate() << " msg/s" << std::endl;
			}
		}

		control->Pause();

		if (verbose)
		{
			std::cerr << "----------------------" << std::endl;
			for(int j = 0; j < model.size(); j++)
				std::cerr << "[" << model[j]->getName() << "]\t: " << statistics[j].getCount() << " msgs at " << statistics[j].getRate() << " msg/s" << std::endl;

		}

		if(timer_on)
			for (Model* m : model)
				std::cerr << "[" << m->getName() << "]\t: " << m->getTotalTiming() << " ms" << std::endl;

		for(Model *m : model) delete m;
		if(control) delete control;		
	}
	catch (const char * msg)
	{
		std::cerr << msg << std::endl;		
	}
	
	return 0;
}
