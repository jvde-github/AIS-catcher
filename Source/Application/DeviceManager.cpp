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

#include "DeviceManager.h"
#include "Logger.h"
#include "Parse.h"
#include "StringBuilder.h"
#include "JSON/JSONBuilder.h"

std::vector<Device::Description> DeviceManager::device_list;

void DeviceManager::refreshDevices()
{
	device_list.clear();

	RTLSDR().getDeviceList(device_list);
	AIRSPYHF().getDeviceList(device_list);
	AIRSPY().getDeviceList(device_list);
	SDRPLAY().getDeviceList(device_list);
	HACKRF().getDeviceList(device_list);
	SOAPYSDR().getDeviceList(device_list);
	N2KSCAN().getDeviceList(device_list);
	SerialPort().getDeviceList(device_list);
	HYDRASDR().getDeviceList(device_list);
}

Device::Device *DeviceManager::getDeviceByType(Type type)
{
	switch (type)
	{
	case Type::WAVFILE:
		return &_WAV;
	case Type::RAWFILE:
		return &_RAW;
	case Type::RTLTCP:
		return &_RTLTCP;
	case Type::SPYSERVER:
		return &_SpyServer;
	case Type::SERIALPORT:
		return &_SerialPort;
	case Type::UDP:
		return &_UDP;
#ifdef HASNMEA2000
	case Type::N2K:
		return &_N2KSCAN;
#endif
#ifdef HASZMQ
	case Type::ZMQ:
		return &_ZMQ;
#endif
#ifdef HASAIRSPYHF
	case Type::AIRSPYHF:
		return &_AIRSPYHF;
#endif
#ifdef HASAIRSPY
	case Type::AIRSPY:
		return &_AIRSPY;
#endif
#ifdef HASSDRPLAY
	case Type::SDRPLAY:
		return &_SDRPLAY;
#endif
#ifdef HASRTLSDR
	case Type::RTLSDR:
		return &_RTLSDR;
#endif
#ifdef HASHACKRF
	case Type::HACKRF:
		return &_HACKRF;
#endif
#ifdef HASHYDRASDR
	case Type::HYDRASDR:
		return &_HYDRASDR;
#endif
#ifdef HASSOAPYSDR
	case Type::SOAPYSDR:
		return &_SOAPYSDR;
#endif
	default:
		return nullptr;
	}
}

bool DeviceManager::openDevice(int sample_rate, int bandwidth, int ppm, int frequency, TAG &tag)
{
	int idx = device_list.empty() ? -1 : 0;
	uint64_t handle = device_list.empty() ? 0 : device_list[0].getHandle();

	if (!serial.empty())
		Info() << "Searching for device with SN " << serial << ".";

	if (!serial.empty() || type != Type::NONE)
	{
		idx = -1;
		for (int i = 0; i < device_list.size(); i++)
		{
			bool serial_match = device_list[i].getSerial() == serial && (type == Type::NONE || type == device_list[i].getType());
			bool type_match = serial.empty() && (type == device_list[i].getType());

			if (serial_match || type_match)
			{
				idx = i;
				handle = device_list[i].getHandle();
				break;
			}
		}
		if (idx == -1)
		{
			if (!serial.empty())
				throw std::runtime_error("Cannot find device with specified parameters");

			idx = 0;
			handle = 0;
		}
	}

	if (type == Type::NONE && device_list.size() == 0)
		throw std::runtime_error("No devices available");

	if (type != Type::NONE)
		device = getDeviceByType(type);
	else
		device = getDeviceByType(device_list[idx].getType());

	if (device == 0)
		return false;

	device->Open(handle);

	if (frequency)
		device->setFrequency(frequency);
	if (sample_rate)
		device->setSampleRate(sample_rate);
	if (ppm)
		device->Set("FREQOFFSET", std::to_string(ppm));
	if (bandwidth)
		device->Set("BW", std::to_string(bandwidth));

	tag.hardware = device->getProduct();
	tag.driver = device->getDriver();

	device->setTag(tag);
	return true;
}

void DeviceManager::printAvailableDevices(bool JSON)
{
	if (!JSON)
	{
		Info() << "Found " << device_list.size() << " device(s):";

		for (int i = 0; i < device_list.size(); i++)
		{
			Info() << i << ": " << device_list[i].toString();
		}
	}
	else
	{
		JSON::JSONBuilder json;
		json.start()
			.key("devices").startArray();

		for (int i = 0; i < device_list.size(); i++)
		{
			std::string type = Util::Parse::DeviceTypeString(device_list[i].getType());
			std::string serial = device_list[i].getSerial();
			std::string name = type + " [" + serial + "]";

			json.start()
				.add("input", type)
				.add("serial", serial)
				.add("name", name)
				.end();
		}

		json.endArray().end().nl();
		std::cout << json.str();
	}
}

void DeviceManager::selectDeviceByIndex(int index)
{
	if (index < 0 || index >= device_list.size())
		throw std::runtime_error("device does not exist");

	serial = device_list[index].getSerial();
}
