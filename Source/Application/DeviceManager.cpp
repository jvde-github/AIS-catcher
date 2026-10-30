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
#include "Device/Serial.h"
#include "Device/ZMQ.h"
#include "Device/UDP.h"
#include "Device/N2KsktCAN.h"
#include "Device/HYDRASDR.h"

std::vector<Device::Description> DeviceManager::device_list;
std::mutex DeviceManager::list_mtx;

const DeviceManager::DeviceEntry DeviceManager::device_settings[] = {
	{'r', AIS::KEY_SETTING_RTLSDR, Type::RTLSDR, true, true, "RTL-SDR", []() -> Device::Device * { return new Device::RTLSDR(); }},
	{'h', AIS::KEY_SETTING_AIRSPYHF, Type::AIRSPYHF, true, true, "Airspy HF+", []() -> Device::Device * { return new Device::AIRSPYHF(); }},
	{'m', AIS::KEY_SETTING_AIRSPY, Type::AIRSPY, true, true, "Airspy", []() -> Device::Device * { return new Device::AIRSPY(); }},
	{'s', AIS::KEY_SETTING_SDRPLAY, Type::SDRPLAY, true, true, "SDRplay", []() -> Device::Device * { return new Device::SDRPLAY(); }},
	{'f', AIS::KEY_SETTING_HACKRF, Type::HACKRF, true, true, "HackRF", []() -> Device::Device * { return new Device::HACKRF(); }},
	{'u', AIS::KEY_SETTING_SOAPYSDR, Type::SOAPYSDR, true, true, "SoapySDR", []() -> Device::Device * { return new Device::SOAPYSDR(); }},
	{0, AIS::KEY_SETTING_NMEA2000, Type::N2K, true, false, "NMEA 2000", []() -> Device::Device * { return new Device::N2KSCAN(); }},
	{'e', AIS::KEY_SETTING_SERIALPORT, Type::SERIALPORT, true, false, "Serial", []() -> Device::Device * { return new Device::SerialPort(); }},
	{'d', AIS::KEY_SETTING_HYDRASDR, Type::HYDRASDR, true, true, "HydraSDR", []() -> Device::Device * { return new Device::HYDRASDR(); }},
	{'a', AIS::KEY_SETTING_FILE, Type::RAWFILE, false, false, "File", []() -> Device::Device * { return new Device::RAWFile(); }},
	{'w', AIS::KEY_SETTING_WAVFILE, Type::WAVFILE, false, false, "WAV file", []() -> Device::Device * { return new Device::WAVFile(); }},
	{'t', AIS::KEY_SETTING_RTLTCP, Type::RTLTCP, false, false, "Network", []() -> Device::Device * { return new Device::RTLTCP(); }},
	{'y', AIS::KEY_SETTING_SPYSERVER, Type::SPYSERVER, false, false, "SpyServer", []() -> Device::Device * { return new Device::SpyServer(); }},
	{'z', AIS::KEY_SETTING_ZMQ, Type::ZMQ, false, false, "ZMQ", []() -> Device::Device * { return new Device::ZMQ(); }},
	{0, AIS::KEY_SETTING_UDPSERVER, Type::UDP, false, false, "UDP", []() -> Device::Device * { return new Device::UDP(); }}};

Device::Device &DeviceManager::object(const DeviceEntry &d)
{
	static_assert(sizeof(device_settings) / sizeof(device_settings[0]) == ROWS, "one device object per row of the table");

	std::unique_ptr<Device::Device> &slot = objects[&d - device_settings];
	if (!slot)
		slot.reset(d.make());
	return *slot;
}

const DeviceManager::DeviceEntry *DeviceManager::row(Type type)
{
	for (const auto &d : device_settings)
		if (d.type == type)
			return &d;
	return nullptr;
}

const DeviceManager::DeviceEntry *DeviceManager::row(AIS::Keys key)
{
	for (const auto &d : device_settings)
		if (d.key == key)
			return &d;
	return nullptr;
}

const DeviceManager::DeviceEntry *DeviceManager::row(char flag)
{
	for (const auto &d : device_settings)
		if (d.flag && d.flag == flag)
			return &d;
	return nullptr;
}

Setting *DeviceManager::settingForFlag(char flag)
{
	const DeviceEntry *d = row(flag);
	return d ? &object(*d) : nullptr;
}

AIS::Keys DeviceManager::keyForFlag(char flag)
{
	const DeviceEntry *d = row(flag);
	return d ? d->key : AIS::KEY_COUNT;
}

bool DeviceManager::isDeviceKey(AIS::Keys key)
{
	return row(key) != nullptr;
}

AIS::Keys DeviceManager::keyForType(Type type)
{
	const DeviceEntry *d = row(type);
	return d ? d->key : AIS::KEY_COUNT;
}

Setting *DeviceManager::settingForKey(AIS::Keys key)
{
	const DeviceEntry *d = row(key);
	return d ? &object(*d) : nullptr;
}

bool DeviceManager::isRadio(Type type)
{
	const DeviceEntry *d = row(type);
	return d && d->radio;
}

const char *DeviceManager::label(Type type)
{
	const DeviceEntry *d = row(type);
	return d ? d->label : "Input";
}

void DeviceManager::refreshDevices()
{
	std::lock_guard<std::mutex> lock(list_mtx);

	device_list.clear();

	for (const auto &d : device_settings)
		if (d.enumerate)
			object(d).getDeviceList(device_list);
}

Device::Device *DeviceManager::getDeviceByType(Type type)
{
	const DeviceEntry *d = row(type);
	return d ? &object(*d) : nullptr;
}

bool DeviceManager::openDevice(int sample_rate, int bandwidth, int ppm, int frequency, TAG &tag)
{
	uint64_t handle = 0;

	{
		std::lock_guard<std::mutex> lock(list_mtx);

		int idx = -1;

		if (!serial_.empty()) {
			Info() << "Searching for device with SN " << serial_ << (type != Type::NONE ? " and type " + Util::Parse::DeviceTypeString(type) : "") << ".";
		}

		if (!serial_.empty() || type != Type::NONE)
		{
			std::vector<int> matches;
			for (int i = 0; i < device_list.size(); i++)
			{
				bool serial_match = device_list[i].getSerial() == serial_ && (type == Type::NONE || type == device_list[i].getType());
				bool type_match = serial_.empty() && (type == device_list[i].getType());

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
				if (!serial_.empty())
				{
					Error() << "Device Manager: cannot find device with SN " << serial_ << ".";
					printLocked();
					return false;
				}

				idx = 0;
				handle = 0;
			}
			else
			{
				handle = device_list[idx].getHandle();
				device_list[idx].setClaimed();

				if (serial_.empty() && matches.size() > 1)
				{
					Device::Description &d = device_list[idx];
					Warning() << "Device Manager: multiple devices match type "
							  << Util::Parse::DeviceTypeString(type) << "; selecting SN " << d.getSerial()
							  << ". Set \"serial\" to choose a specific device.";
				}
			}
		}
		else
		{
			// Nothing specified: auto-select the first available SDR radio only.
			idx = -1;
			for (int i = 0; i < device_list.size(); i++)
				if (isRadio(device_list[i].getType()) && !device_list[i].isClaimed())
				{
					idx = i;
					handle = device_list[i].getHandle();
					device_list[i].setClaimed();
					break;
				}

			if (idx == -1)
			{
				if (device_list.empty())
					Error() << "Device Manager: no devices available.";
				else
				{
					Error() << "Device Manager: no SDR device found, please select a device.";
					printLocked();
				}
				return false;
			}
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

void DeviceManager::printLocked()
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
		printLocked();
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

	serial_ = device_list[index].getSerial();
	type = device_list[index].getType();
}
