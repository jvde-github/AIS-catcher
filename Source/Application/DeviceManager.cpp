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
#include "Writer.h"

std::vector<Device::Description> DeviceManager::device_list;
std::mutex DeviceManager::list_mtx;

namespace
{
	// One row per device type: its "-g<flag>" command line switch (0 when it has
	// none), its config file key, its Type, and whether refreshDevices() asks it
	// to enumerate hardware. Network/file devices must not enumerate: they push
	// an unconditional pseudo-entry that would pollute the device list. Rows are
	// ordered enumerating-first; that order fixes the "-d:x" indices.
	struct DeviceEntry
	{
		char flag;
		AIS::Keys key;
		Type type;
		bool enumerate;
		Device::Device &(*device)(DeviceManager &);
	};

	const DeviceEntry device_settings[] = {
		{'r', AIS::KEY_SETTING_RTLSDR, Type::RTLSDR, true, [](DeviceManager &d) -> Device::Device & { return d.RTLSDR(); }},
		{'h', AIS::KEY_SETTING_AIRSPYHF, Type::AIRSPYHF, true, [](DeviceManager &d) -> Device::Device & { return d.AIRSPYHF(); }},
		{'m', AIS::KEY_SETTING_AIRSPY, Type::AIRSPY, true, [](DeviceManager &d) -> Device::Device & { return d.AIRSPY(); }},
		{'s', AIS::KEY_SETTING_SDRPLAY, Type::SDRPLAY, true, [](DeviceManager &d) -> Device::Device & { return d.SDRPLAY(); }},
		{'f', AIS::KEY_SETTING_HACKRF, Type::HACKRF, true, [](DeviceManager &d) -> Device::Device & { return d.HACKRF(); }},
		{'u', AIS::KEY_SETTING_SOAPYSDR, Type::SOAPYSDR, true, [](DeviceManager &d) -> Device::Device & { return d.SOAPYSDR(); }},
		{0, AIS::KEY_SETTING_NMEA2000, Type::N2K, true, [](DeviceManager &d) -> Device::Device & { return d.N2KSCAN(); }},
		{'e', AIS::KEY_SETTING_SERIALPORT, Type::SERIALPORT, true, [](DeviceManager &d) -> Device::Device & { return d.SerialPort(); }},
		{'d', AIS::KEY_SETTING_HYDRASDR, Type::HYDRASDR, true, [](DeviceManager &d) -> Device::Device & { return d.HYDRASDR(); }},
		{'a', AIS::KEY_SETTING_FILE, Type::RAWFILE, false, [](DeviceManager &d) -> Device::Device & { return d.RAW(); }},
		{'w', AIS::KEY_SETTING_WAVFILE, Type::WAVFILE, false, [](DeviceManager &d) -> Device::Device & { return d.WAV(); }},
		{'t', AIS::KEY_SETTING_RTLTCP, Type::RTLTCP, false, [](DeviceManager &d) -> Device::Device & { return d.RTLTCP(); }},
		{'y', AIS::KEY_SETTING_SPYSERVER, Type::SPYSERVER, false, [](DeviceManager &d) -> Device::Device & { return d.SpyServer(); }},
		{'z', AIS::KEY_SETTING_ZMQ, Type::ZMQ, false, [](DeviceManager &d) -> Device::Device & { return d.ZMQ(); }},
		{0, AIS::KEY_SETTING_UDPSERVER, Type::UDP, false, [](DeviceManager &d) -> Device::Device & { return d.UDP(); }}};
}

Setting *DeviceManager::settingForFlag(char flag)
{
	for (const auto &d : device_settings)
		if (d.flag && d.flag == flag)
			return &d.device(*this);

	return nullptr;
}

Setting *DeviceManager::settingForKey(AIS::Keys key)
{
	for (const auto &d : device_settings)
		if (d.key == key)
			return &d.device(*this);

	return nullptr;
}

void DeviceManager::refreshDevices()
{
	std::lock_guard<std::mutex> lock(list_mtx);

	device_list.clear();

	for (const auto &d : device_settings)
		if (d.enumerate)
			d.device(*this).getDeviceList(device_list);
}

Device::Device *DeviceManager::getDeviceByType(Type type)
{
	for (const auto &d : device_settings)
		if (d.type == type)
			return &d.device(*this);

	return nullptr;
}

bool DeviceManager::openDevice(int sample_rate, int bandwidth, int ppm, int frequency, TAG &tag)
{
	uint64_t handle;

	{
		std::lock_guard<std::mutex> lock(list_mtx);

		int idx = device_list.empty() ? -1 : 0;
		handle = device_list.empty() ? 0 : device_list[0].getHandle();

		if (!serial.empty()) {
			Info() << "Searching for device with SN " << serial << (type != Type::NONE ? " and type " + Util::Parse::DeviceTypeString(type) : "") << ".";
		}

		if (!serial.empty() || type != Type::NONE)
		{
			std::vector<int> matches;
			for (int i = 0; i < device_list.size(); i++)
			{
				bool serial_match = device_list[i].getSerial() == serial && (type == Type::NONE || type == device_list[i].getType());
				bool type_match = serial.empty() && (type == device_list[i].getType());

				if (serial_match || type_match)
					matches.push_back(i);
			}

			idx = -1;
			for (int m : matches)
				if (!device_list[m].isClaimed()) { idx = m; break; }

			if (idx == -1 && !matches.empty())
			{
				Device::Description &d = device_list[matches.front()];
				Error() << "Device Manager: configuration opens the same device twice ("
						<< Util::Parse::DeviceTypeString(d.getType()) << " SN " << d.getSerial()
						<< "). Each receiver must select a distinct device (set a unique \"serial\").";
				return false;
			}

			if (idx == -1)
			{
				if (!serial.empty())
				{
					Error() << "Device Manager: cannot find device with SN " << serial << ".";
					printAvailableDevices_locked();
					return false;
				}

				idx = 0;
				handle = 0;
			}
			else
			{
				handle = device_list[idx].getHandle();
				device_list[idx].setClaimed();

				if (serial.empty() && matches.size() > 1)
				{
					Device::Description &d = device_list[idx];
					Warning() << "Device Manager: multiple devices match type "
							  << Util::Parse::DeviceTypeString(type) << "; selecting SN " << d.getSerial()
							  << ". Set \"serial\" to choose a specific device.";
				}
			}
		}

		if (type == Type::NONE && device_list.size() == 0)
		{
			Error() << "Device Manager: no devices available.";
			return false;
		}

		if (type == Type::NONE)
			type = device_list[idx].getType();
	}

	device = getDeviceByType(type);

	if (device == 0)
		return false;

	device->Open(handle);

	if (frequency)
		device->setFrequency(frequency);
	if (sample_rate)
		device->setSampleRate(sample_rate);
	if (ppm)
		device->SetKey(AIS::KEY_SETTING_FREQOFFSET, std::to_string(ppm));
	if (bandwidth)
		device->SetKey(AIS::KEY_SETTING_BANDWIDTH, std::to_string(bandwidth));

	tag.hardware = device->getProduct();
	tag.driver = device->getDriver();
	tag.replay = device->isReplay();

	device->setTag(tag);
	return true;
}

void DeviceManager::printAvailableDevices_locked()
{
	Info() << "Found " << device_list.size() << " device(s):";

	for (int i = 0; i < device_list.size(); i++)
	{
		Info() << i << ": " << device_list[i].toString();
	}
}

void DeviceManager::printAvailableDevices(bool JSON)
{
	if (!JSON)
	{
		std::lock_guard<std::mutex> lock(list_mtx);
		printAvailableDevices_locked();
	}
	else
	{
		std::cout << getDeviceListJSON() << "\n";
	}
}

std::string DeviceManager::getDeviceListJSON()
{
	std::lock_guard<std::mutex> lock(list_mtx);

	std::string s;
	JSON::Writer w(s);
	w.beginObject().key("devices").beginArray();
	for (int i = 0; i < device_list.size(); i++)
	{
		std::string type = Util::Parse::DeviceTypeString(device_list[i].getType());
		std::string serial = device_list[i].getSerial();
		w.beginObject()
			.kv("input", type)
			.kv("serial", serial)
			.kv("name", type + " [" + serial + "]")
			.endObject();
	}
	w.endArray().endObject().finish();
	return s;
}

void DeviceManager::selectDeviceByIndex(int index)
{
	std::lock_guard<std::mutex> lock(list_mtx);

	if (index < 0 || index >= device_list.size())
		throw std::runtime_error("Device Manager: device does not exist");

	serial = device_list[index].getSerial();
	type = device_list[index].getType();
}
