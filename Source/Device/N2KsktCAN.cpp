/*
	Copyright(c) 2024 jvde.github@gmail.com

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

#include <cstring>

#include "N2KsktCAN.h"
#include "N2KInterface.h"

#ifdef __linux__
#include <ifaddrs.h>
#include <vector>
#include <string>
#include <iostream>
#endif

#ifdef HASNMEA2000
namespace Device
{

	N2KSCAN::~N2KSCAN()
	{
		Close();
	}

	void N2KSCAN::Close()
	{
	}

	void N2KSCAN::Open(uint64_t h)
	{
		if (_iface.empty())
		{
			if (h < available_intefaces.size())
				_iface = available_intefaces[h];
			else
			{
				Error() << "Requested interface #" << h << " is not available";
				for (auto i : available_intefaces)
					Error() << "Available interface: " << i;
				throw std::runtime_error("NMEA2000: No available interfaces.");
			}
		}
		N2K::N2KInterface.addInput(this);
		N2K::N2KInterface.setNetwork(_iface);
	}

	void N2KSCAN::Play()
	{
		N2K::N2KInterface.Start();
	}

	void N2KSCAN::Stop()
	{
		N2K::N2KInterface.Stop();
	}

	std::string N2KSCAN::Get()
	{
		return Device::Get() + " network " + _iface;
	}

	void N2KSCAN::getDeviceList(std::vector<Description> &DeviceList)
	{
		// 			DeviceList.push_back(Description(v, p, s, (uint64_t)i, Type::RTLSDR));
		struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;

		if (getifaddrs(&ifaddr) == -1)
		{
			return;
		}

		uint64_t i = 0;
		for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_name == nullptr)
				continue;

			// Check if it's either a CAN or virtual CAN interface by name
			bool is_can_by_name = (strncmp(ifa->ifa_name, "can", 3) == 0);
			bool is_vcan = (strncmp(ifa->ifa_name, "vcan", 4) == 0);

			// If it has no address but is a CAN or vCAN interface by name, include it
			if (ifa->ifa_addr == nullptr)
			{
				if (is_can_by_name || is_vcan)
				{
					DeviceList.push_back(Description("NMEA2000", "CANbus", ifa->ifa_name, i++, Type::N2K));
					available_intefaces.push_back(ifa->ifa_name);
				}
				continue;
			}

			// For interfaces with addresses, check if they're CAN interfaces by address family
			if (ifa->ifa_addr->sa_family == AF_CAN)
			{
				DeviceList.push_back(Description("NMEA2000", "CANbus", ifa->ifa_name, i++, Type::N2K));
				available_intefaces.push_back(ifa->ifa_name);
			}
		}

		freeifaddrs(ifaddr);
	}

}

#endif

Setting &Device::N2KSCAN::Set(std::string option, std::string arg)
{
	Util::Convert::toUpper(option);

	if (option == "INTERFACE")
	{
#ifdef HASNMEA2000
		_iface = arg;
#endif
	}
	else
		Device::Set(option, arg);

	return *this;
}